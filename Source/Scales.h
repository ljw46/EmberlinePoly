#pragma once

#include <juce_core/juce_core.h>
#include <array>

namespace ember::scales
{
    struct Scale { const char* name; int count; std::array<int, 7> intervals; };

    inline constexpr Scale table[] {
        { "Major",            7, { 0, 2, 4, 5, 7, 9, 11 } },
        { "Natural Minor",    7, { 0, 2, 3, 5, 7, 8, 10 } },
        { "Dorian",           7, { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",         7, { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian",           7, { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",       7, { 0, 2, 4, 5, 7, 9, 10 } },
        { "Minor Pentatonic", 5, { 0, 3, 5, 7, 10, 0, 0 } },
        { "Harmonic Minor",   7, { 0, 2, 3, 5, 7, 8, 11 } },
    };

    inline constexpr int count = (int) std::size (table);

    inline const juce::StringArray& names()
    {
        static const juce::StringArray n = [] { juce::StringArray a; for (auto& s : table) a.add (s.name); return a; }();
        return n;
    }

    inline const juce::StringArray& rootNames()
    {
        static const juce::StringArray n { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return n;
    }

    inline int floorDiv (int a, int b) noexcept { return a / b - ((a % b != 0) && ((a < 0) != (b < 0)) ? 1 : 0); }

    // Scale degree (0 = root, 7 = root an octave up in a 7-note scale, negatives go down) to semitones from the root.
    inline int degreeToSemitones (int scaleIndex, int degree) noexcept
    {
        const auto& s = table[juce::jlimit (0, count - 1, scaleIndex)];
        const int oct = floorDiv (degree, s.count);
        return oct * 12 + s.intervals[(size_t) (degree - oct * s.count)];
    }

    // Nearest scale tone to a semitone offset from the root (ties go down).
    inline int snap (int scaleIndex, int semitones) noexcept
    {
        const auto& s = table[juce::jlimit (0, count - 1, scaleIndex)];
        const int oct = floorDiv (semitones, 12);
        const int pc = semitones - oct * 12;

        int best = s.intervals[0];
        for (int i = 0; i < s.count; ++i)
            if (std::abs (s.intervals[(size_t) i] - pc) < std::abs (best - pc))
                best = s.intervals[(size_t) i];

        if (std::abs (s.intervals[0] + 12 - pc) < std::abs (best - pc))
            best = s.intervals[0] + 12;

        return oct * 12 + best;
    }

    inline bool contains (int scaleIndex, int semitones) noexcept { return snap (scaleIndex, semitones) == semitones; }
}
