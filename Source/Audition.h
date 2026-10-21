#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

// A small sample player so the patterns can be heard without another instrument. It plays back short samples (made by
// tools/make_audition_samples.py and compiled into the plugin) by plain resampling: no synthesis, so it costs next to nothing. It listens to the MIDI
// the plugin itself generates and plays each MIDI channel with the sound its part is set to. It is off by default; the audio output is silent otherwise.
namespace ember::audition
{
    enum Sound { soundOff, soundPluck, soundBass, soundPad, soundKeys, soundBell, numSounds };

    inline const juce::StringArray& soundNames()
    {
        static const juce::StringArray n { "Off", "Pluck", "Bass", "Pad", "Keys", "Bell" };
        return n;
    }

    struct Route { int sound = soundOff; float pan = 0.0f; };   // per MIDI channel; pan -1 (left) to +1 (right)

    struct Zone;   // one sample and the pitch it was recorded at

    class Player
    {
    public:
        Player();

        void prepare (double sampleRate);
        void reset();

        // Adds the sound of the notes in `midi` to `out` (the buffer is not cleared). `level` is 0 to 1.
        void render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const std::array<Route, 16>& routes, float level);

        bool loaded() const noexcept;       // the embedded samples decoded
        int activeVoices() const noexcept;

    private:
        struct Voice
        {
            bool active = false, releasing = false;
            int channel = 1, note = 60, sound = 0;
            const Zone* zone = nullptr;
            double position = 0.0, ratio = 1.0;
            float gainL = 0.0f, gainR = 0.0f, envelope = 0.0f, attackStep = 0.0f, releaseStep = 0.0f;
            std::uint64_t age = 0;
        };

        void noteOn (int channel, int note, int velocity, const Route&, float level);
        void noteOff (int channel, int note);
        void renderRange (juce::AudioBuffer<float>& out, int from, int to);

        static constexpr int maxVoices = 32;
        double sampleRate = 44100.0;
        std::uint64_t counter = 0;
        std::array<Voice, maxVoices> voices {};
    };
}
