#pragma once

#include "Settings.h"
#include <juce_core/juce_core.h>

// Pattern slots (A to D) per engine. A slot holds one engine's pattern: step count, rate, ratio, the lanes with their lengths and clock
// divides, and which chord notes the pitch lane moves. Voicing and routing (octave, note pick, channel, mute, solo) belong to the engine,
// not to the slot. The active pattern always lives in the parameters; the other slots are text in the plugin state, which this file reads and writes.
namespace ember::patterns
{
    inline constexpr int numSlots = 4;
    inline const char* slotName (int slot) noexcept { return slot == 0 ? "A" : slot == 1 ? "B" : slot == 2 ? "C" : "D"; }

    // "steps rate num den pitchMask  (length divide) x kinds  (values x 32) x kinds"
    inline juce::String encode (const seq::EngineSettings& e)
    {
        juce::StringArray t;
        t.add (juce::String (e.steps));
        t.add (juce::String (e.rate));
        t.add (juce::String (e.num));
        t.add (juce::String (e.den));
        t.add (juce::String (e.pitchMask));
        for (int k = 0; k < seq::numKinds; ++k)
        {
            t.add (juce::String (e.laneLength[(size_t) k]));
            t.add (juce::String (e.divideOf (k)));
        }
        for (int k = 0; k < seq::numKinds; ++k)
            for (int i = 0; i < seq::maxSteps; ++i)
                t.add (juce::String (e.step[(size_t) k][(size_t) i]));
        return t.joinIntoString (" ");
    }

    // Reads a pattern back, clamping every value. Fails on text of the wrong size. Only the pattern fields of `e` change.
    inline bool decode (const juce::String& text, seq::EngineSettings& e)
    {
        const auto t = juce::StringArray::fromTokens (text, " ", "");
        if (t.size() != 5 + 2 * seq::numKinds + seq::numKinds * seq::maxSteps)
            return false;

        int i = 0;
        auto next = [&] { return t[i++].getIntValue(); };
        e.steps = juce::jlimit (1, seq::maxSteps, next());
        e.rate = juce::jlimit (0, rates::count - 1, next());
        e.num = juce::jlimit (1, rates::maxRatio, next());
        e.den = juce::jlimit (1, rates::maxRatio, next());
        e.pitchMask = juce::jlimit (0, 255, next());
        for (int k = 0; k < seq::numKinds; ++k)
        {
            e.laneLength[(size_t) k] = juce::jlimit (0, seq::maxSteps, next());
            e.laneDivide[(size_t) k] = juce::jlimit (1, 8, next());
        }
        for (int k = 0; k < seq::numKinds; ++k)
            for (int n = 0; n < seq::maxSteps; ++n)
                e.step[(size_t) k][(size_t) n] = next();   // the parameters clamp these to their own ranges
        return true;
    }
}
