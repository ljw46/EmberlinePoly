#include "MidiClip.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace ember::clip
{
    Window chooseWindow (const std::vector<seq::CapturedNote>& notes, int maxBars, int beatsPerBar)
    {
        if (notes.empty())
            return {};

        double first = notes.front().startBeats, last = first;
        for (auto& n : notes)
        {
            first = std::min (first, n.startBeats);
            last = std::max (last, n.startBeats);
        }

        const double bar = (double) beatsPerBar;
        const double end = std::floor (last / bar + 1.0e-9) * bar + bar;   // bar after the last note starts
        const double start = std::max (std::floor (first / bar + 1.0e-9) * bar, end - (double) std::max (1, maxBars) * bar);
        return { start, end };
    }

    const char* sourceName (int source) noexcept
    {
        static const char* names[] { "Chords", "Engine 1", "Engine 2", "Engine 3", "Engine 4" };
        return names[juce::jlimit (0, seq::numSources - 1, source)];
    }

    namespace
    {
        bool inWindow (const seq::CapturedNote& n, const Window& w) noexcept
        {
            return n.startBeats >= w.startBeats - 1.0e-9 && n.startBeats < w.endBeats;
        }
    }

    int countInWindow (const std::vector<seq::CapturedNote>& notes, unsigned sourceMask, int maxBars)
    {
        const auto w = chooseWindow (notes, maxBars);
        int count = 0;
        for (auto& n : notes)
            count += ((sourceMask >> n.source) & 1u) != 0 && inWindow (n, w);
        return count;
    }

    int build (const std::vector<seq::CapturedNote>& notes, unsigned sourceMask, int maxBars, double bpm, juce::MidiFile& result)
    {
        const auto window = chooseWindow (notes, maxBars);

        juce::MidiMessageSequence tempoTrack;
        tempoTrack.addEvent (juce::MidiMessage::textMetaEvent (3, "Emberline Poly"), 0.0);
        tempoTrack.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::llround (60000000.0 / std::max (20.0, bpm))), 0.0);
        tempoTrack.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
        tempoTrack.addEvent (juce::MidiMessage::endOfTrack(), (window.endBeats - window.startBeats) * ticksPerQuarter);

        std::vector<juce::MidiMessageSequence> tracks;
        int written = 0;
        for (int source = 0; source < seq::numSources; ++source)
        {
            if (((sourceMask >> source) & 1u) == 0)
                continue;

            juce::MidiMessageSequence track;
            int count = 0;
            for (auto& n : notes)
            {
                if (n.source != source || ! inWindow (n, window))
                    continue;

                const double on = std::round ((n.startBeats - window.startBeats) * ticksPerQuarter);
                const double off = std::max (on + 1.0, std::round ((n.startBeats + n.lengthBeats - window.startBeats) * ticksPerQuarter));
                const int channel = juce::jlimit (1, 16, n.channel);
                track.addEvent (juce::MidiMessage::noteOn (channel, n.note, (juce::uint8) juce::jlimit (1, 127, n.velocity)), on);
                track.addEvent (juce::MidiMessage::noteOff (channel, n.note), off);
                ++count;
            }

            if (count == 0)
                continue;

            track.addEvent (juce::MidiMessage::textMetaEvent (3, sourceName (source)), 0.0);
            track.sort();
            track.updateMatchedPairs();
            tracks.push_back (std::move (track));
            written += count;
        }

        if (written == 0)
            return 0;

        result.clear();
        result.setTicksPerQuarterNote (ticksPerQuarter);
        result.addTrack (tempoTrack);
        for (auto& t : tracks)
            result.addTrack (t);
        return written;
    }

    int write (const std::vector<seq::CapturedNote>& notes, unsigned sourceMask, int maxBars, double bpm, const juce::File& file)
    {
        juce::MidiFile midi;
        const int count = build (notes, sourceMask, maxBars, bpm, midi);
        if (count == 0)
            return 0;

        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::FileOutputStream out (file);
        return out.openedOk() && midi.writeTo (out) ? count : 0;
    }

    bool importFile (const juce::File& file, Imported& result)
    {
        juce::FileInputStream in (file);
        juce::MidiFile midi;
        if (! in.openedOk() || ! midi.readFrom (in))
            return false;

        struct Note { double start, length; int pitch, velocity; };
        std::map<long long, Note> byStart;   // keeps the highest note of each chord

        for (int t = 0; t < midi.getNumTracks(); ++t)
        {
            juce::MidiMessageSequence seq (*midi.getTrack (t));
            seq.updateMatchedPairs();
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* e = seq.getEventPointer (i);
                if (! e->message.isNoteOn())
                    continue;

                const double start = e->message.getTimeStamp();
                const double end = e->noteOffObject != nullptr ? e->noteOffObject->message.getTimeStamp() : start + 1.0;
                const Note n { start, std::max (1.0, end - start), e->message.getNoteNumber(), e->message.getVelocity() };
                const auto key = std::llround (start);
                const auto found = byStart.find (key);
                if (found == byStart.end() || n.pitch > found->second.pitch)
                    byStart[key] = n;
            }
        }

        if (byStart.empty())
            return false;

        std::vector<Note> notes;
        for (auto& kv : byStart)
            notes.push_back (kv.second);
        if ((int) notes.size() > seq::maxSteps)
            notes.resize ((size_t) seq::maxSteps);

        result = {};
        result.count = (int) notes.size();
        for (size_t i = 0; i < notes.size(); ++i)
        {
            const double gap = i + 1 < notes.size() ? notes[i + 1].start - notes[i].start
                                                    : (i > 0 ? notes[i].start - notes[i - 1].start : notes[i].length);
            result.pitch[i] = juce::jlimit (-24, 24, notes[i].pitch - notes[0].pitch);
            result.velocity[i] = juce::jlimit (1, 127, notes[i].velocity);
            result.gate[i] = juce::jlimit (10, 200, (int) std::lround (100.0 * notes[i].length / std::max (1.0, gap)));
        }
        return true;
    }

    void deleteStaleExports (const juce::File& folder)
    {
        for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.mid"))
            if (f.getLastModificationTime() < juce::Time::getCurrentTime() - juce::RelativeTime::days (1.0))
                f.deleteFile();
    }
}
