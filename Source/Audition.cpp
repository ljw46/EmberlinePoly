#include "Audition.h"
#include "AssetData.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>

namespace ember::audition
{
    struct Zone
    {
        int root = 60;
        std::vector<float> data;
        double rate = 24000.0;
        int loopStart = -1, loopEnd = -1;   // frames; -1 means the sample plays once
    };

    namespace
    {
        // Keep in sync with PAD_LOOP_START in tools/make_audition_samples.py: the pad loops from there to the end of the file.
        constexpr double padLoopStartSeconds = 2.0;

        constexpr float soundGain[numSounds] { 0.0f, 0.9f, 0.85f, 0.5f, 0.8f, 0.6f };
        constexpr float releaseSeconds[numSounds] { 0.05f, 0.12f, 0.15f, 0.7f, 0.3f, 0.6f };

        // The decoded samples, built once on first use (the processor's constructor, never the audio thread) and only read afterwards.
        struct Bank
        {
            Bank()
            {
                juce::WavAudioFormat wav;
                for (int i = 0; i < AssetData::namedResourceListSize; ++i)
                {
                    const char* name = AssetData::namedResourceList[i];
                    const juce::String file = AssetData::getNamedResourceOriginalFilename (name);
                    if (! file.endsWithIgnoreCase (".wav"))
                        continue;

                    // "<sound>_<midi note>.wav"
                    const auto stem = file.upToLastOccurrenceOf (".", false, false);
                    const auto sound = stem.upToFirstOccurrenceOf ("_", false, false).toLowerCase();
                    const int root = stem.fromLastOccurrenceOf ("_", false, false).getIntValue();
                    int index = -1;
                    for (int s = 1; s < numSounds; ++s)
                        if (soundNames()[s].toLowerCase() == sound)
                            index = s;
                    if (index < 0)
                        continue;

                    int size = 0;
                    const char* bytes = AssetData::getNamedResource (name, size);
                    auto stream = std::make_unique<juce::MemoryInputStream> (bytes, (size_t) size, false);
                    std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (stream.release(), true));
                    if (reader == nullptr || reader->lengthInSamples < 2)
                        continue;

                    Zone z;
                    z.root = root;
                    z.rate = reader->sampleRate;
                    z.data.resize ((size_t) reader->lengthInSamples + 1, 0.0f);   // one spare frame so interpolation never reads past the end
                    juce::AudioBuffer<float> buffer (1, (int) reader->lengthInSamples);
                    reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, false);
                    std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + reader->lengthInSamples, z.data.begin());
                    if (index == soundPad)
                    {
                        z.loopStart = (int) std::llround (padLoopStartSeconds * z.rate);
                        z.loopEnd = (int) reader->lengthInSamples;
                    }
                    zones[(size_t) index].push_back (std::move (z));
                    ok = true;
                }
            }

            std::array<std::vector<Zone>, numSounds> zones;
            bool ok = false;
        };

        const Bank& bank()
        {
            static const Bank b;
            return b;
        }

        const Zone* nearestZone (int sound, int note)
        {
            const Zone* best = nullptr;
            for (const auto& z : bank().zones[(size_t) sound])
                if (best == nullptr || std::abs (z.root - note) < std::abs (best->root - note))
                    best = &z;
            return best;
        }
    }

    Player::Player() { (void) bank(); }

    bool Player::loaded() const noexcept { return bank().ok; }

    int Player::activeVoices() const noexcept
    {
        int n = 0;
        for (auto& v : voices)
            n += v.active ? 1 : 0;
        return n;
    }

    void Player::prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        reset();
    }

    void Player::reset()
    {
        for (auto& v : voices)
            v.active = false;
    }

    void Player::noteOn (int channel, int note, int velocity, const Route& route, float level)
    {
        const auto* zone = nearestZone (route.sound, note);
        if (zone == nullptr || velocity <= 0)
            return;

        // a free voice, or the oldest one
        Voice* slot = &voices[0];
        for (auto& v : voices)
        {
            if (! v.active) { slot = &v; break; }
            if (v.age < slot->age) slot = &v;
        }

        const float angle = (juce::jlimit (-1.0f, 1.0f, route.pan) + 1.0f) * 0.25f * juce::MathConstants<float>::pi;   // constant-power pan
        const float gain = level * soundGain[route.sound] * std::pow ((float) velocity / 127.0f, 0.8f) * 0.6f;

        slot->active = true;
        slot->releasing = false;
        slot->channel = channel;
        slot->note = note;
        slot->sound = route.sound;
        slot->zone = zone;
        slot->position = 0.0;
        slot->ratio = std::pow (2.0, (double) (note - zone->root) / 12.0) * zone->rate / sampleRate;
        slot->gainL = gain * std::cos (angle);
        slot->gainR = gain * std::sin (angle);
        slot->envelope = 0.0f;
        slot->attackStep = (float) (1.0 / (0.002 * sampleRate));
        slot->releaseStep = (float) (1.0 / (releaseSeconds[route.sound] * sampleRate));
        slot->age = ++counter;
    }

    void Player::noteOff (int channel, int note)
    {
        for (auto& v : voices)
            if (v.active && ! v.releasing && v.channel == channel && v.note == note)
                v.releasing = true;
    }

    void Player::renderRange (juce::AudioBuffer<float>& out, int from, int to)
    {
        if (to <= from)
            return;

        auto* left = out.getWritePointer (0);
        auto* right = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;

        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            const auto& z = *v.zone;
            const int last = (int) z.data.size() - 2;
            for (int i = from; i < to; ++i)
            {
                if (v.releasing)
                {
                    v.envelope -= v.releaseStep;
                    if (v.envelope <= 0.0f)
                    {
                        v.active = false;
                        break;
                    }
                }
                else if (v.envelope < 1.0f)
                    v.envelope = juce::jmin (1.0f, v.envelope + v.attackStep);

                if (z.loopEnd > 0 && v.position >= (double) z.loopEnd)
                    v.position -= (double) (z.loopEnd - z.loopStart);
                const int index = (int) v.position;
                if (index >= last)
                {
                    v.active = false;
                    break;
                }

                const float frac = (float) (v.position - (double) index);
                const float sample = (z.data[(size_t) index] + (z.data[(size_t) index + 1] - z.data[(size_t) index]) * frac) * v.envelope;
                left[i] += sample * v.gainL;
                if (right != nullptr)
                    right[i] += sample * v.gainR;
                v.position += v.ratio;
            }
        }
    }

    void Player::render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const std::array<Route, 16>& routes, float level)
    {
        int cursor = 0;
        const int numSamples = out.getNumSamples();
        for (const auto meta : midi)
        {
            const int position = juce::jlimit (0, numSamples, meta.samplePosition);
            renderRange (out, cursor, position);
            cursor = position;

            const auto m = meta.getMessage();
            if (m.isNoteOn())
            {
                const auto& route = routes[(size_t) juce::jlimit (1, 16, m.getChannel()) - 1];
                if (route.sound != soundOff)
                    noteOn (m.getChannel(), m.getNoteNumber(), m.getVelocity(), route, level);
            }
            else if (m.isNoteOff())
                noteOff (m.getChannel(), m.getNoteNumber());
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                for (auto& v : voices)
                    v.releasing = v.active;
        }
        renderRange (out, cursor, numSamples);
    }
}
