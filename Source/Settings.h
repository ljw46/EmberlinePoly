#pragma once

#include "Rates.h"
#include <array>

// A plain snapshot of everything the sequencer engine needs. The processor fills it from the parameter tree once
// per block; the engine and its tests never touch JUCE parameters.
namespace ember::seq
{
    inline constexpr int numEngines = 4;
    inline constexpr int maxSteps = 32;
    inline constexpr int maxChordSlots = 8;
    inline constexpr int maxBufferNotes = 16;

    // Per-step values of a motif engine.
    // Late is a timing offset in % of the step (negative is early); Harmony adds a second note that many semitones from the first (0 is off).
    enum StepKind { kindPitch, kindVelocity, kindGate, kindProbability, kindRatchet, kindLate, kindHarmony, numKinds };

    // How an engine picks from the chord / held-note buffer on each trigger.
    enum ArpMode { arpCyclic, arpChordHold, arpLowHigh, arpRandom, arpRoot, numArpModes };

    enum ChordSource { chordInternal, chordLive };
    enum ChordTiming { timingOnBeat, timingAnticipate, timingOffBeat };

    // Where a generated note came from: 0 is the base chords, 1-4 the motif engines.
    inline constexpr int numSources = numEngines + 1;

    struct EngineSettings
    {
        int steps = 8;
        int rate = rates::defaultIndex;
        int num = 1, den = 1;      // ratio multiplier num:den on top of the rate
        int octave = 0;            // -2..2
        int arp = arpCyclic;
        int channel = 2;           // MIDI output channel 1-16
        bool mute = false, solo = false;
        std::array<std::array<int, maxSteps>, numKinds> step {};   // pitch, velocity, gate %, probability %, ratchet count, late %, harmony semitones

        // Every lane can run its own length and clock divide (a 3-step velocity lane against a 5-step pitch lane). Length 0 follows `steps`;
        // a divide of n makes the lane advance once every n steps of the engine's clock. Values below 1 count as 1.
        std::array<int, numKinds> laneLength {};
        std::array<int, numKinds> laneDivide {};

        // Which chord-buffer positions (bit 0 is the lowest note) receive the pitch lane's shift. The rest keep their own pitch.
        int pitchMask = 0xff;

        int lengthOf (int kind) const noexcept { return laneLength[(size_t) kind] > 0 ? laneLength[(size_t) kind] : steps; }
        int divideOf (int kind) const noexcept { return laneDivide[(size_t) kind] > 1 ? laneDivide[(size_t) kind] : 1; }
    };

    struct ChordSlot { int degree = 0; int extension = 0; };   // scale degree 0-6, index into chords::extensionNames()

    struct ChordSettings
    {
        int source = chordInternal;
        int length = 4;                         // slots used, 1-8
        std::array<ChordSlot, maxChordSlots> slot {};
        int barsIndex = 1;                      // index into chords::barNames() (length of each chord)
        int timing = timingOnBeat;
        bool out = true;                        // internal chords are sent as MIDI on `channel`
        int channel = 1;
    };

    // The built-in audition sampler: a sound per part (0 is off; the numbers index audition::soundNames()) and a level.
    struct AuditionSettings
    {
        bool on = false;
        float level = 0.7f;
        int chordsSound = 4;
        std::array<int, numEngines> engineSound { 2, 1, 3, 5 };   // bass, pluck, pad, bell
    };

    struct Settings
    {
        std::array<EngineSettings, numEngines> engine {};
        AuditionSettings audition;
        ChordSettings chords;

        // Per pitch class (C = 0), what happens to a generated note: 0 plays it, 1 blocks it, 2 plays a semitone higher, 3 a semitone lower.
        std::array<int, 12> noteRule {};

        int root = 0;              // 0-11
        int baseOctave = 3;        // MIDI note 60 is C3
        int scale = 1, borrowScale = 2;
        bool scaleLock = true;     // false = direct override: nothing is snapped
        float borrowAmount = 0.0f; // % of triggers that snap to the borrowed scale
        float jitterMs = 0.0f;
        float slipPercent = 0.0f;

        bool run = true;           // internal transport, only used when the host provides no position
        double internalBpm = 124.0;
    };
}
