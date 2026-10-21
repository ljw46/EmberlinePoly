#pragma once

#include "Audition.h"
#include "Pattern.h"
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
        { "late",  "Late",        -50, 50  },
        { "harm",  "Harmony",     -12, 12  },
    };

    inline const juce::StringArray noteRuleNames { "Allow", "Block", "Up", "Down" };
    inline constexpr int maxLaneDivide = 8;

    inline juce::String engineId (int engine, const char* name) { return "e" + juce::String (engine + 1) + "_" + name; }
    inline juce::String stepId (int engine, int kind, int step)
    {
        return engineId (engine, stepInfo[kind].id) + "_" + juce::String (step + 1).paddedLeft ('0', 2);
    }
    inline juce::String laneLengthId (int engine, int kind) { return engineId (engine, "len_") + stepInfo[kind].id; }
    inline juce::String laneDivideId (int engine, int kind) { return engineId (engine, "div_") + stepInfo[kind].id; }
    inline juce::String ruleId (int pitchClass) { return "rule_" + juce::String (pitchClass).paddedLeft ('0', 2); }
    inline juce::String chordId (int slot, const char* name) { return "c" + juce::String (slot + 1) + "_" + name; }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Sets a parameter from its plain value (an index for choices). Unchanged values are skipped, so loading a preset only
    // tells the host about what actually moved.
    void setPlain (juce::AudioProcessorValueTreeState&, const juce::String& id, float plainValue);

    // The current sound as a preset: engines that are not muted, scale and humanize, and the chord settings.
    presets::Preset capturePreset (juce::AudioProcessorValueTreeState&, bool includeChords);

    // Writes a preset into the parameters. Engines it does not use are muted; routing (channels, solo) is left alone.
    void applyPreset (juce::AudioProcessorValueTreeState&, const presets::Preset&, bool includeChords);

    // Writes one engine's pattern (steps, rate, ratio, step lanes with their lengths and divides, pitch mask) into the parameters; voicing and routing stay as they are.
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
            std::atomic<float> *steps, *rate, *num, *den, *octave, *mute, *solo, *arp, *channel, *pitchMask;
            std::array<std::array<std::atomic<float>*, seq::maxSteps>, seq::numKinds> step;
            std::array<std::atomic<float>*, seq::numKinds> laneLength, laneDivide;
        };
        struct Slot { std::atomic<float> *degree, *extension; };

        std::array<Engine, seq::numEngines> engine {};
        std::array<Slot, seq::maxChordSlots> slot {};
        std::array<std::atomic<float>*, 12> rule {};
        std::atomic<float> *auditionOn, *auditionLevel, *chordsSound;
        std::array<std::atomic<float>*, seq::numEngines> engineSound {};
        std::atomic<float> *chordSource, *chordLength, *chordBars, *chordTiming, *chordOut, *chordChannel;
        std::atomic<float> *root, *baseOctave, *scale, *borrowScale, *scaleLock, *borrowAmount, *jitter, *slip, *run, *bpm;
    };

    // The pattern slots of the four engines. The slot being edited is the live parameters; the others are kept in the plugin state (so they are saved
    // with the project). Switching to a slot that has never been used starts it as a copy of the current pattern, which is how a variation begins.
    class PatternBank
    {
    public:
        explicit PatternBank (juce::AudioProcessorValueTreeState& a) : apvts (a) {}

        int active (int engine) const;
        bool filled (int engine, int slot) const;                  // the active slot, or one that holds a pattern
        void select (int engine, int slot);
        void copy (int engine, int from, int to);                  // `to` becomes a copy of `from`; copying onto the active slot changes the live pattern
        void clear (int engine, int slot);                         // an empty slot starts as a copy again the next time it is selected
        seq::EngineSettings read (int engine, int slot) const;     // the live pattern for the active slot, the stored one otherwise

    private:
        static juce::Identifier slotKey (int engine, int slot) { return juce::Identifier ("patSlot_" + juce::String (engine + 1) + "_" + patterns::slotName (slot)); }
        static juce::Identifier activeKey (int engine) { return juce::Identifier ("patActive_" + juce::String (engine + 1)); }
        seq::EngineSettings live (int engine) const;

        juce::AudioProcessorValueTreeState& apvts;
    };
}
