#pragma once

#include <juce_core/juce_core.h>

// Lane clock rates. Every lane steps at its own rate (polyrhythm) over its own step count (polymeter).
namespace ember::rates
{
    struct Entry { const char* name; double beats; };   // length of one step in quarter-note beats

    inline constexpr Entry table[] {
        { "1/1", 4.0 },       { "1/2", 2.0 },        { "1/2.", 3.0 },       { "1/2T", 4.0 / 3.0 },
        { "1/4", 1.0 },       { "1/4.", 1.5 },       { "1/4T", 2.0 / 3.0 },
        { "1/8", 0.5 },       { "1/8.", 0.75 },      { "1/8T", 1.0 / 3.0 },
        { "1/16", 0.25 },     { "1/16.", 0.375 },    { "1/16T", 1.0 / 6.0 },
        { "1/32", 0.125 },    { "1/32.", 0.1875 },   { "1/32T", 1.0 / 12.0 },
    };

    inline constexpr int count = (int) std::size (table);
    inline constexpr int defaultIndex = 10;   // 1/16
    inline constexpr int maxRatio = 16;

    inline juce::StringArray names()
    {
        juce::StringArray n;
        for (auto& e : table)
            n.add (e.name);
        return n;
    }

    inline juce::StringArray ratioNames()
    {
        juce::StringArray n;
        for (int i = 1; i <= maxRatio; ++i)
            n.add (juce::String (i));
        return n;
    }

    // Length of one step in beats. The ratio num:den is a speed multiplier on top of the base rate, so
    // 5:4 plays 1.25x faster (steps are 4/5 as long) and 3:4 plays at 3/4 speed (steps are 4/3 as long).
    inline double stepBeats (int rateIndex, int num, int den) noexcept
    {
        const auto base = table[juce::jlimit (0, count - 1, rateIndex)].beats;
        return base * (double) juce::jlimit (1, maxRatio, den) / (double) juce::jlimit (1, maxRatio, num);
    }

    // Which step of an n-step lane is playing at a position in beats. Stateless, so lanes stay locked to the
    // host timeline through loops, jumps and rate changes.
    inline int stepAt (double ppq, double stepLenBeats, int numSteps) noexcept
    {
        const auto index = (long long) std::floor (ppq / stepLenBeats + 1.0e-6);
        const auto n = (long long) juce::jmax (1, numSteps);
        return (int) (((index % n) + n) % n);
    }
}
