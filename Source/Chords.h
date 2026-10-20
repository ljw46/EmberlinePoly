#pragma once

#include "Humanize.h"
#include "Settings.h"
#include "Scales.h"
#include <algorithm>

// Chord memory: scale-degree chords built by stacking scale tones, and a progression suggester for melodic house,
// progressive and deep techno.
namespace ember::chords
{
    inline constexpr int numExtensions = 6;
    enum Extension { extTriad, ext7th, ext9th, extSus2, extSus4, ext7sus4 };

    inline const juce::StringArray& extensionNames()
    {
        static const juce::StringArray n { "Triad", "7th", "9th", "Sus2", "Sus4", "7sus4" };
        return n;
    }

    inline const juce::StringArray& degreeNames()
    {
        static const juce::StringArray n { "I", "II", "III", "IV", "V", "VI", "VII" };
        return n;
    }

    inline const juce::StringArray& barNames()
    {
        static const juce::StringArray n { "1/2 bar", "1 bar", "2 bars", "4 bars" };
        return n;
    }

    inline const juce::StringArray& timingNames()
    {
        static const juce::StringArray n { "On beat", "Anticipate", "Off beat" };
        return n;
    }

    inline const juce::StringArray& sourceNames()
    {
        static const juce::StringArray n { "Internal", "Live MIDI" };
        return n;
    }

    inline const juce::StringArray& genreNames()
    {
        static const juce::StringArray n { "Melodic House", "Progressive", "Deep Techno" };
        return n;
    }

    inline constexpr double barBeats[] { 2.0, 4.0, 8.0, 16.0 };

    inline double chordBeats (int barsIndex) noexcept { return barBeats[juce::jlimit (0, 3, barsIndex)]; }

    // Chord changes can land on the beat, an eighth early (anticipated) or an eighth late (off-beat).
    inline double timingShift (int timing) noexcept { return timing == 1 ? -0.5 : timing == 2 ? 0.5 : 0.0; }

    inline long long indexAt (double beat, double chordLen, double shift) noexcept
    {
        return (long long) std::floor ((beat - shift) / chordLen + 1.0e-6);
    }

    inline int slotOf (long long index, int length) noexcept
    {
        const long long n = juce::jmax (1, length);
        return (int) (((index % n) + n) % n);
    }

    struct Voicing
    {
        std::array<int, 6> semis {};   // ascending, in semitones from the scale root
        int count = 0;
    };

    // Stacks scale tones above `degree` (degree 0 is the scale root). Sevenths and ninths come from the same scale, so
    // the chord always agrees with it. Chords whose root sits high in the scale drop an octave to keep a tight register.
    inline Voicing voice (int scaleIndex, int degree, int extension) noexcept
    {
        static constexpr int offsets[numExtensions][5] {
            { 0, 2, 4, -1, -1 },   // triad
            { 0, 2, 4, 6, -1 },    // 7th
            { 0, 2, 4, 6, 8 },     // 9th
            { 0, 1, 4, -1, -1 },   // sus2
            { 0, 3, 4, -1, -1 },   // sus4
            { 0, 3, 4, 6, -1 },    // 7sus4
        };

        Voicing v;
        const int drop = scales::degreeToSemitones (scaleIndex, degree) > 6 ? 12 : 0;
        for (int o : offsets[juce::jlimit (0, numExtensions - 1, extension)])
            if (o >= 0)
                v.semis[(size_t) v.count++] = scales::degreeToSemitones (scaleIndex, degree + o) - drop;
        return v;
    }

    struct Progression
    {
        int length = 4;
        std::array<seq::ChordSlot, seq::maxChordSlots> slot {};
    };

    enum Genre { genreMelodicHouse, genreProgressive, genreDeepTechno };

    struct Suggestion
    {
        Progression progression;
        int scale = 1;
    };

    namespace detail
    {
        struct Template { int length; std::array<int, 4> degrees; };

        template <size_t N>
        const Template& pick (humanize::Rng& rng, const Template (&t)[N]) { return t[(size_t) (rng.uniform() * (double) N) % N]; }

        // weights: triad, 7th, 9th, sus2, sus4, 7sus4
        inline int pickExtension (humanize::Rng& rng, const std::array<int, numExtensions>& w)
        {
            int total = 0;
            for (int x : w) total += x;
            int r = (int) (rng.uniform() * total);
            for (int i = 0; i < numExtensions; ++i)
            {
                if (r < w[(size_t) i]) return i;
                r -= w[(size_t) i];
            }
            return extTriad;
        }
    }

    // A progression in a minor-leaning mode with extensions typical of the genre. Scale indices follow scales::table:
    // 1 Natural Minor, 2 Dorian, 3 Phrygian, 7 Harmonic Minor.
    inline Suggestion suggest (humanize::Rng& rng, int genre)
    {
        using detail::Template;
        static const Template house[] { { 4, { 0, 5, 2, 6 } }, { 4, { 0, 6, 5, 6 } }, { 4, { 0, 3, 5, 4 } },
                                        { 4, { 5, 6, 0, 2 } }, { 4, { 0, 2, 6, 3 } }, { 4, { 3, 0, 6, 5 } } };
        static const Template prog[]  { { 4, { 0, 5, 6, 0 } }, { 4, { 0, 6, 5, 4 } }, { 4, { 5, 6, 0, 0 } },
                                        { 4, { 0, 3, 6, 2 } }, { 4, { 0, 5, 3, 6 } } };
        static const Template techno[] { { 4, { 0, 0, 6, 0 } }, { 4, { 0, 1, 0, 6 } }, { 4, { 0, 0, 3, 0 } },
                                         { 4, { 0, 6, 0, 3 } }, { 2, { 0, 1, 0, 0 } }, { 2, { 0, 6, 0, 0 } } };

        Suggestion s;
        const detail::Template* t = nullptr;
        std::array<int, numExtensions> tonicWeights {}, otherWeights {};

        switch (genre)
        {
            case genreProgressive:
            {
                const double r = rng.uniform();
                s.scale = r < 0.5 ? 1 : r < 0.85 ? 2 : 7;
                t = &detail::pick (rng, prog);
                tonicWeights = { 30, 20, 5, 35, 10, 0 };
                otherWeights = { 40, 30, 5, 10, 15, 0 };
                break;
            }
            case genreDeepTechno:
            {
                s.scale = rng.uniform() < 0.5 ? 3 : 2;   // Phrygian or Dorian
                t = &detail::pick (rng, techno);
                tonicWeights = { 5, 30, 5, 30, 10, 20 };
                otherWeights = { 5, 35, 10, 15, 15, 20 };
                break;
            }
            default:
            {
                s.scale = rng.uniform() < 0.45 ? 1 : 2;
                t = &detail::pick (rng, house);
                tonicWeights = { 20, 40, 40, 0, 0, 0 };
                otherWeights = { 30, 35, 20, 5, 10, 0 };
                break;
            }
        }

        s.progression.length = t->length;
        for (int i = 0; i < t->length; ++i)
        {
            const int degree = t->degrees[(size_t) i];
            s.progression.slot[(size_t) i] = { degree, detail::pickExtension (rng, degree == 0 ? tonicWeights : otherWeights) };
        }
        return s;
    }
}
