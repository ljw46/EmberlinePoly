#include "SequencerEngine.h"
#include "Chords.h"
#include "Scales.h"

namespace ember::seq
{
    namespace
    {
        constexpr double lookaheadSeconds = 0.025;   // matches the largest jitter
        constexpr int chordVelocity = 90;

        double engineBeats (const EngineSettings& e) noexcept { return rates::stepBeats (e.rate, e.num, e.den); }

        // First grid point at or after `from` (inclusive) or strictly after it.
        double gridPoint (double from, double stepBeats, bool inclusive) noexcept
        {
            return inclusive ? std::ceil (from / stepBeats - 1.0e-6) * stepBeats
                             : (std::floor (from / stepBeats + 1.0e-6) + 1.0) * stepBeats;
        }

        int foldNote (int n) noexcept
        {
            while (n > 127) n -= 12;
            while (n < 0)   n += 12;
            return n;
        }

        bool audible (const Settings& s, int e) noexcept
        {
            bool anySolo = false;
            for (auto& en : s.engine)
                anySolo = anySolo || en.solo;
            return ! s.engine[(size_t) e].mute && (! anySolo || s.engine[(size_t) e].solo);
        }

        int baseNote (const Settings& s) noexcept { return 12 * (s.baseOctave + 2) + s.root; }
    }

    void Engine::prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        reset();
    }

    void Engine::reset()
    {
        for (auto& e : queue)
            e.used = false;
        for (auto& ch : sounding)
            ch.fill (0);
        held.fill (false);
        modWheel = 0;
        wasPlaying = false;
        haveBoundary.fill (false);
        haveChord = false;
        arpCounter.fill (0);
        arpHigh.fill (false);
        internalPpq = expectedPpq = 0.0;
        sampleClock = 0;
        for (auto& p : playhead)
            p.store (-1, std::memory_order_relaxed);
        chordPlayhead.store (-1, std::memory_order_relaxed);
    }

    void Engine::scanInput (const juce::MidiBuffer& in)
    {
        for (const auto meta : in)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn())
                held[(size_t) m.getNoteNumber()] = true;
            else if (m.isNoteOff())
                held[(size_t) m.getNoteNumber()] = false;
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                held.fill (false);
            else if (m.isController() && m.getControllerNumber() == 1)
                modWheel = m.getControllerValue();
        }
    }

    // The chord buffer, sorted low to high: held MIDI notes in live mode, otherwise the chord the progression is on.
    Engine::Buffer Engine::bufferAt (const Settings& s, double beat) const noexcept
    {
        Buffer b;
        if (s.chords.source == chordLive)
        {
            for (int n = 0; n < 128 && b.count < maxBufferNotes; ++n)
                if (held[(size_t) n])
                    b.note[(size_t) b.count++] = n;
            return b;
        }

        const double len = chords::chordBeats (s.chords.barsIndex);
        const auto index = chords::indexAt (beat, len, chords::timingShift (s.chords.timing));
        const auto& slot = s.chords.slot[(size_t) chords::slotOf (index, s.chords.length)];
        const auto v = chords::voice (s.scale, slot.degree, slot.extension);
        for (int i = 0; i < v.count; ++i)
            b.note[(size_t) b.count++] = foldNote (baseNote (s) + v.semis[(size_t) i]);
        std::sort (b.note.begin(), b.note.begin() + b.count);
        return b;
    }

    // Scale and chord guard: snaps a note to the (main or borrowed) scale, or passes it through in direct-override mode.
    int Engine::guard (const Settings& s, int note, bool borrow) noexcept
    {
        if (s.scaleLock)
            note = scales::snap (borrow ? s.borrowScale : s.scale, note - s.root) + s.root;
        return foldNote (note);
    }

    void Engine::push (std::int64_t sample, int note, int velocity, int channel, bool on)
    {
        for (auto& e : queue)
            if (! e.used)
            {
                e = { sample, note, velocity, channel, on, true };
                return;
            }
        // queue full: drop (only reachable with extreme settings)
    }

    void Engine::flush (juce::MidiBuffer& out, int position)
    {
        for (int ch = 0; ch < 16; ++ch)
            for (int n = 0; n < 128; ++n)
                if (sounding[(size_t) ch][(size_t) n] > 0)
                    out.addEvent (juce::MidiMessage::noteOff (ch + 1, n), position);

        for (auto& ch : sounding)
            ch.fill (0);
        for (auto& e : queue)
            e.used = false;
    }

    // Queues a note-on / note-off pair at a position in beats, with jitter and gate slip, and records it for export.
    void Engine::addNote (double onsetBeat, double lengthBeats, int note, int velocity, int channel, int source,
                          double ppq, double bps, double jitterMs, double slipFactor, CaptureRing* capture)
    {
        double onsetD = (double) sampleClock + (onsetBeat - ppq) / bps;
        onsetD += jitterMs * 0.001 * sampleRate;
        const auto onset = std::max ((std::int64_t) std::llround (onsetD), sampleClock);

        const auto length = std::max ((std::int64_t) 1, (std::int64_t) std::llround (lengthBeats * slipFactor / bps));

        // a retrigger of a pitch that is still sounding ends the old note first
        for (auto& e : queue)
            if (e.used && ! e.on && e.note == note && e.channel == channel && e.sample > onset)
                e.sample = onset;

        push (onset, note, velocity, channel, true);
        push (onset + length, note, 0, channel, false);

        if (capture != nullptr)
            capture->push ({ ppq + (double) (onset - sampleClock) * bps, (double) length * bps, note, velocity, channel, source });
    }

    void Engine::scheduleEngineStep (const Settings& s, int e, double boundary, double ppq, double bps, CaptureRing* capture)
    {
        const auto& en = s.engine[(size_t) e];
        if (! audible (s, e))
            return;

        const double stepLen = engineBeats (en);
        const int step = rates::stepAt (boundary, stepLen, en.steps);
        auto value = [&] (int kind) { return en.step[(size_t) kind][(size_t) step]; };

        const int velocity = value (kindVelocity), gate = value (kindGate), probability = value (kindProbability);
        const int ratchet = juce::jlimit (1, 4, value (kindRatchet));
        if (velocity <= 0 || gate <= 0)
            return;
        if (probability < 100 && rng.uniform() * 100.0 >= (double) probability)
            return;

        const auto buffer = bufferAt (s, boundary);
        if (buffer.count == 0)
            return;

        // note pick
        std::array<int, maxBufferNotes> picked {};
        int numPicked = 0;
        switch (en.arp)
        {
            case arpChordHold:
                for (int i = 0; i < buffer.count; ++i) picked[(size_t) numPicked++] = buffer.note[(size_t) i];
                break;
            case arpLowHigh:
                picked[(size_t) numPicked++] = arpHigh[(size_t) e] ? buffer.note[(size_t) buffer.count - 1] : buffer.note[0];
                arpHigh[(size_t) e] = ! arpHigh[(size_t) e];
                break;
            case arpRandom:
                picked[(size_t) numPicked++] = buffer.note[(size_t) juce::jmin (buffer.count - 1, (int) (rng.uniform() * buffer.count))];
                break;
            case arpRoot:
                picked[(size_t) numPicked++] = buffer.note[0];
                break;
            default:   // as played / cyclic
                picked[(size_t) numPicked++] = buffer.note[(size_t) (arpCounter[(size_t) e] % buffer.count)];
                ++arpCounter[(size_t) e];
                break;
        }

        const bool borrow = modWheel >= 64 || (s.borrowAmount > 0.0f && rng.uniform() * 100.0 < (double) s.borrowAmount);
        std::array<int, maxBufferNotes> notes {};
        int numNotes = 0;
        for (int i = 0; i < numPicked; ++i)
        {
            const int n = guard (s, picked[(size_t) i] + value (kindPitch) + 12 * en.octave, borrow);
            if (std::find (notes.begin(), notes.begin() + numNotes, n) == notes.begin() + numNotes)
                notes[(size_t) numNotes++] = n;
        }

        // a ratchet splits the step into equal retriggers, each a little softer than the last
        const double sub = stepLen / ratchet;
        const int channel = juce::jlimit (1, 16, en.channel);
        for (int r = 0; r < ratchet; ++r)
        {
            const int v = juce::jmax (1, (int) std::lround ((double) velocity * (1.0 - 0.08 * r)));
            // one jitter and one slip per trigger, so the notes of a chord stay together
            const double jitter = humanize::jitterMs (rng, (double) s.jitterMs);
            const double slip = s.slipPercent > 0.0f ? humanize::slipFactor (rng, (double) s.slipPercent) : 1.0;
            for (int i = 0; i < numNotes; ++i)
                addNote (boundary + r * sub, sub * (double) gate * 0.01, notes[(size_t) i], v, channel, e + 1, ppq, bps, jitter, slip, capture);
        }
    }

    void Engine::scheduleChord (const Settings& s, long long index, double startBeat, double ppq, double bps, CaptureRing* capture)
    {
        const double len = chords::chordBeats (s.chords.barsIndex);
        const double endBeat = (double) (index + 1) * len + chords::timingShift (s.chords.timing);
        const auto& slot = s.chords.slot[(size_t) chords::slotOf (index, s.chords.length)];
        const auto v = chords::voice (s.scale, slot.degree, slot.extension);
        const int channel = juce::jlimit (1, 16, s.chords.channel);

        for (int i = 0; i < v.count; ++i)
            addNote (startBeat, endBeat - startBeat, foldNote (baseNote (s) + v.semis[(size_t) i]), chordVelocity, channel, 0,
                     ppq, bps, 0.0, 1.0, capture);
    }

    void Engine::emit (juce::MidiBuffer& out, int numSamples)
    {
        const auto blockEnd = sampleClock + numSamples;
        auto position = [&] (const Event& e) { return (int) juce::jlimit<std::int64_t> (0, numSamples - 1, e.sample - sampleClock); };

        // offs first, so a note that ends where the next begins is closed before the new one opens
        for (auto& e : queue)
            if (e.used && ! e.on && e.sample < blockEnd)
            {
                out.addEvent (juce::MidiMessage::noteOff (e.channel, e.note), position (e));
                --sounding[(size_t) e.channel - 1][(size_t) e.note];
                e.used = false;
            }

        for (auto& e : queue)
            if (e.used && e.on && e.sample < blockEnd)
            {
                out.addEvent (juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.velocity), position (e));
                ++sounding[(size_t) e.channel - 1][(size_t) e.note];
                e.used = false;
            }
    }

    void Engine::process (const Settings& s, const Transport& t, const juce::MidiBuffer& midiIn, juce::MidiBuffer& midiOut,
                          int numSamples, CaptureRing* capture)
    {
        scanInput (midiIn);

        const bool hostClock = t.hasPosition;
        const bool playing = hostClock ? t.playing : s.run;
        const double bpm = hostClock ? (t.bpm > 0.0 ? t.bpm : 120.0) : s.internalBpm;
        const double ppq = hostClock ? t.ppq : internalPpq;
        const double bps = bpm / 60.0 / sampleRate;   // beats per sample
        bpmSeen.store (bpm, std::memory_order_relaxed);

        if (! playing)
        {
            if (wasPlaying)
                flush (midiOut, 0);
            wasPlaying = false;
            haveBoundary.fill (false);
            haveChord = false;
            internalPpq = 0.0;
            for (auto& p : playhead)
                p.store (-1, std::memory_order_relaxed);
            chordPlayhead.store (-1, std::memory_order_relaxed);
            sampleClock += numSamples;
            return;
        }

        // start, loop or jump: drop whatever is sounding and restart every grid from here
        if (! wasPlaying || std::abs (ppq - expectedPpq) > 0.02)
        {
            if (! wasPlaying && capture != nullptr)
                capture->clear();
            flush (midiOut, 0);
            haveBoundary.fill (false);
            haveChord = false;
            arpCounter.fill (0);
            arpHigh.fill (false);
        }

        const double horizon = ppq + ((double) numSamples + lookaheadSeconds * sampleRate) * bps;

        // base chords (internal progression only; in live mode the host already plays the chords)
        const double chordLen = chords::chordBeats (s.chords.barsIndex), shift = chords::timingShift (s.chords.timing);
        if (haveChord && (std::abs (chordLen - chordLenSeen) > 1.0e-9 || std::abs (shift - chordShiftSeen) > 1.0e-9))
            haveChord = false;   // the chord grid changed: pick up the current chord again

        if (s.chords.source == chordInternal && s.chords.out)
        {
            if (! haveChord)
            {
                const auto k = chords::indexAt (ppq, chordLen, shift);
                scheduleChord (s, k, std::max ((double) k * chordLen + shift, ppq), ppq, bps, capture);
                nextChord = k + 1;
                haveChord = true;
                chordLenSeen = chordLen;
                chordShiftSeen = shift;
            }

            for (int guardCount = 0; (double) nextChord * chordLen + shift < horizon && guardCount < 8; ++guardCount, ++nextChord)
                scheduleChord (s, nextChord, (double) nextChord * chordLen + shift, ppq, bps, capture);
        }
        else
            haveChord = false;

        // motif engines, each on its own grid
        for (int e = 0; e < numEngines; ++e)
        {
            const double stepLen = engineBeats (s.engine[(size_t) e]);
            double boundary = haveBoundary[(size_t) e] ? gridPoint (lastBoundary[(size_t) e], stepLen, false) : gridPoint (ppq, stepLen, true);

            for (int guardCount = 0; boundary < horizon && guardCount < 256; ++guardCount)
            {
                scheduleEngineStep (s, e, boundary, ppq, bps, capture);
                lastBoundary[(size_t) e] = boundary;
                haveBoundary[(size_t) e] = true;
                boundary = gridPoint (boundary, stepLen, false);
            }
        }

        emit (midiOut, numSamples);

        for (int e = 0; e < numEngines; ++e)
            playhead[(size_t) e].store (rates::stepAt (ppq, engineBeats (s.engine[(size_t) e]), s.engine[(size_t) e].steps), std::memory_order_relaxed);
        chordPlayhead.store (s.chords.source == chordInternal ? chords::slotOf (chords::indexAt (ppq, chordLen, shift), s.chords.length) : -1,
                             std::memory_order_relaxed);

        wasPlaying = true;
        expectedPpq = ppq + (double) numSamples * bps;
        if (! hostClock)
            internalPpq = expectedPpq;
        sampleClock += numSamples;
    }
}
