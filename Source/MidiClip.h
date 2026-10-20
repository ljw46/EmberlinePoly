#pragma once

#include "Capture.h"
#include "Settings.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace ember::clip
{
    inline constexpr int ticksPerQuarter = 960;

    // Which sources go into an export: bit 0 is the base chords, bits 1-4 the motif engines.
    inline constexpr unsigned chordsOnly = 1u;
    inline constexpr unsigned allSources = (1u << seq::numSources) - 1u;
    inline constexpr unsigned engineOnly (int engine) noexcept { return 1u << (engine + 1); }

    const char* sourceName (int source) noexcept;   // "Chords", "Engine 1", ...

    // The part of the capture that gets exported: the last `maxBars` bars, starting on a bar line. It is worked out from
    // every captured note, whatever the source, so clips dragged out one by one still line up with each other.
    struct Window { double startBeats = 0.0, endBeats = 0.0; };

    Window chooseWindow (const std::vector<seq::CapturedNote>&, int maxBars, int beatsPerBar = 4);

    // Notes of the selected sources that fall inside the window.
    int countInWindow (const std::vector<seq::CapturedNote>&, unsigned sourceMask, int maxBars);

    // Builds a type-1 MIDI file: a tempo track plus one named track per selected source that has notes. Notes keep the
    // MIDI channel they were generated on and are shifted so the window starts at beat 0. Returns the number of notes
    // written, or 0 if there was nothing to export.
    int build (const std::vector<seq::CapturedNote>&, unsigned sourceMask, int maxBars, double bpm, juce::MidiFile& result);

    // Writes `build()`'s result to a file, replacing it. Returns the number of notes, or 0 on failure.
    int write (const std::vector<seq::CapturedNote>&, unsigned sourceMask, int maxBars, double bpm, const juce::File&);

    // A pattern read from a dropped MIDI file, ready to load into one engine.
    struct Imported
    {
        int count = 0;
        std::array<int, seq::maxSteps> pitch {}, velocity {}, gate {};
    };

    // Reads up to 32 notes (the top note of any chord). Pitch is relative to the first note, gate is the note length
    // as a percentage of the gap to the next note.
    bool importFile (const juce::File&, Imported& result);

    // Removes exported files older than a day from `folder`.
    void deleteStaleExports (const juce::File& folder);
}
