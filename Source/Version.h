#pragma once

#include <juce_core/juce_core.h>

// The version comes from project(... VERSION x.y.z) in CMakeLists.txt, the single source of truth.
// EMBER_VERSION_LABEL ("pre-beta", "beta", "") is set there too.
#ifndef EMBER_VERSION
 #define EMBER_VERSION "0.0.0"
#endif
#ifndef EMBER_VERSION_LABEL
 #define EMBER_VERSION_LABEL ""
#endif

namespace ember::version
{
    inline juce::String number() { return EMBER_VERSION; }
    inline juce::String label()  { return EMBER_VERSION_LABEL; }
    inline juce::String full()   { return "v" + number() + (label().isEmpty() ? juce::String() : " " + label()); }
}
