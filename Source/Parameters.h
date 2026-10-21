#pragma once

#include "Preset.h"
#include "Settings.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace ember::params
{
    inline const juce::StringArray arpNames = presets::arpNames();
    inline const juce::StringArray octaveShiftNames { "-2", "-1", "0", "+1", "+2" };
    inline const juce::StringArray captureBarNames { "1 bar", "2 bars", "4 bars", "8 bars", "16 bars", "32 bars" };
    inline constexpr int captureBars[] { 1, 2, 4, 8, 16, 32 };

    struct StepInfo { const char* id; const char* name; int min, max; };
    inline constexpr StepInfo stepInfo[seq::numKinds] {
        { "pitch", "Pitch",       -24, 24  },
        { "vel",   "Velocity",    0,   127 },
        { "gate",  "Gate",        10,  200 },
        { "prob",  "Probability", 0,   100 },
        { "ratch", "Ratchet",     1,   4   },
    };

    inline juce::String engineId (int engine, const char* name) { return "e" + juce::String (engine + 1) + "_" + name; }
    inline juce::String stepId (int engine, int kind, int step)
    {
        return engineId (engine, stepInfo[kind].id) + "_" + juce::String (step + 1).paddedLeft ('0', 2);
    }
    inline juce::String chordId (int slot, const char* name) { return "c" + juce::String (slot + 1) + "_" + name; }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Sets a parameter from its plain value (an index for choices). Unchanged values are skipped, so loading a preset only
    // tells the host about what actually moved.
    void setPlain (juce::AudioProcessorValueTreeState&, const juce::String& id, float plainValue);

    // The current sound as a preset: engines that are not muted, scale and humanize, and the chord settings.
    presets::Preset capturePreset (juce::AudioProcessorValueTreeState&, bool includeChords);

    // Writes a preset into the parameters. Engines it does not use are muted; routing (channels, solo) is left alone.
    void applyPreset (juce::AudioProcessorValueTreeState&, const presets::Preset&, bool includeChords);

    // Writes one engine's pattern (steps, rate, ratio, step lanes) into the parameters; voicing and routing stay as they are.
    void applyPattern (juce::AudioProcessorValueTreeState&, int engine, const seq::EngineSettings&);

    // Cached raw parameter pointers, so the audio thread never looks parameters up by name.
    class Reader
    {
    public:
        explicit Reader (juce::AudioProcessorValueTreeState&);
        void read (seq::Settings&) const noexcept;

    private:
        struct Engine
        {
            std::atomic<float> *steps, *rate, *num, *den, *octave, *mute, *solo, *arp, *channel;
            std::array<std::array<std::atomic<float>*, seq::maxSteps>, seq::numKinds> step;
        };
        struct Slot { std::atomic<float> *degree, *extension; };

        std::array<Engine, seq::numEngines> engine {};
        std::array<Slot, seq::maxChordSlots> slot {};
        std::atomic<float> *chordSource, *chordLength, *chordBars, *chordTiming, *chordOut, *chordChannel;
        std::atomic<float> *root, *baseOctave, *scale, *borrowScale, *scaleLock, *borrowAmount, *jitter, *slip, *run, *bpm;
    };
}
