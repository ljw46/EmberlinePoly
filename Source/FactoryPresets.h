#pragma once

#include "Preset.h"
#include <vector>

// The factory library. Most presets use one or two engines: a bass or pad on engine 1 with a lead on engine 2 already
// sounds finished with a single synth, and the unused engines are muted. Patterns list the first `steps` values of each
// lane; rests are velocity 0. Degrees are 0 = I ... 6 = VII, colours index chords::extensionNames()
// (0 triad, 1 7th, 2 9th, 3 sus2, 4 sus4, 5 7sus4).
namespace ember::presets
{
    namespace detail
    {
        struct Part
        {
            int engine;                       // 1-4
            int rate;                         // index into rates::table (7 = 1/8, 10 = 1/16, 9 = 1/8T)
            int octave, arp;                  // octave shift -2..2, seq::ArpMode
            std::vector<int> pitch, vel, gate, prob, ratch;
            int num = 1, den = 1;
        };

        struct Def
        {
            const char* name;
            const char* category;
            const char* description;
            int root, scale;                  // scales::table index (0 Major, 1 Natural Minor, 2 Dorian, 3 Phrygian, 4 Lydian, 5 Mixolydian, 6 Minor Pent, 7 Harmonic Minor)
            std::vector<std::pair<int, int>> progression;   // degree, colour
            int barsIndex;                    // 0 half bar, 1 one bar, 2 two bars, 3 four bars
            int timing;                       // 0 on beat, 1 anticipate, 2 off beat
            float jitter, slip;
            std::vector<Part> parts;
        };

        inline Preset build (const Def& d)
        {
            Preset p;
            p.name = d.name;
            p.category = d.category;
            p.author = "Emberline";
            p.description = d.description;
            auto& s = p.settings;
            s.root = d.root;
            s.scale = d.scale;
            s.scaleLock = true;
            s.borrowScale = d.scale == 1 ? 2 : 1;
            s.borrowAmount = 0.0f;
            s.jitterMs = d.jitter;
            s.slipPercent = d.slip;
            s.chords.source = seq::chordInternal;
            s.chords.length = (int) d.progression.size();
            s.chords.barsIndex = d.barsIndex;
            s.chords.timing = d.timing;
            for (size_t i = 0; i < d.progression.size(); ++i)
                s.chords.slot[i] = { d.progression[i].first, d.progression[i].second };

            for (auto& part : d.parts)
            {
                auto& e = s.engine[(size_t) part.engine - 1];
                p.used[(size_t) part.engine - 1] = true;
                e.steps = (int) part.vel.size();
                e.rate = part.rate;
                e.num = part.num;
                e.den = part.den;
                e.octave = part.octave;
                e.arp = part.arp;
                const std::vector<int>* laneData[] { &part.pitch, &part.vel, &part.gate, &part.prob, &part.ratch };
                for (int k = 0; k < seq::numKinds; ++k)
                    for (int i = 0; i < seq::maxSteps; ++i)
                        e.step[(size_t) k][(size_t) i] = (*laneData[k])[(size_t) (i % e.steps) % laneData[k]->size()];
            }
            return p;
        }
    }

    inline std::vector<Preset> factory()
    {
        using detail::Def;
        using detail::Part;
        using seq::arpChordHold;
        using seq::arpCyclic;
        using seq::arpLowHigh;
        using seq::arpRoot;
        constexpr int A = 9, D = 2, F = 5, G = 7, C = 0;

        const std::vector<Def> defs {
            // ---- Deep Techno
            { "Rolling Sub", "Deep Techno", "One sub bass engine on a rolling sixteenth pattern.",
              A, 1, { { 0, 0 }, { 0, 0 }, { 5, 0 }, { 6, 0 } }, 2, 0, 0.0f, 0.0f,
              { Part { 1, 10, -2, arpRoot, { 0 }, { 110, 0, 0, 90, 0, 100, 0, 0, 105, 0, 90, 0, 0, 100, 0, 85 }, { 55 }, { 100 }, { 1 } } } },

            { "Dub Stabs", "Deep Techno", "Offbeat chord stabs over a pulsing root bass. The two cycles drift against each other.",
              D, 2, { { 0, 1 }, { 0, 1 }, { 3, 1 }, { 6, 0 } }, 2, 0, 3.0f, 10.0f,
              { Part { 1, 7, -2, arpRoot, { 0 }, { 105, 0, 0, 90, 0, 0, 95, 0 }, { 50 }, { 100 }, { 1 } },
                Part { 2, 4, 0, arpChordHold, { 0 }, { 0, 100, 0 }, { 40, 45, 40 }, { 100 }, { 1 } } } },

            { "Hypnotic Pulse", "Deep Techno", "A sixteenth root pulse against a five step triplet accent. Polyrhythm that lines up every five beats.",
              G, 3, { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 3, 0 } }, 3, 0, 4.0f, 15.0f,
              { Part { 1, 10, -2, arpRoot, { 0 }, { 100, 0, 80, 0, 90, 0, 0, 85 }, { 45 }, { 100 }, { 1 } },
                Part { 2, 9, 1, arpLowHigh, { 0, 7, 12, 0, 5 }, { 85, 0, 75, 90, 0 }, { 30 }, { 100, 70, 100, 85, 100 }, { 1, 1, 2, 1, 1 } } } },

            // ---- Progressive
            { "Slow Burn", "Progressive", "Held chords on a long cycle and a seven step lead that never repeats against them.",
              A, 1, { { 0, 1 }, { 5, 0 }, { 2, 0 }, { 6, 1 } }, 3, 0, 2.0f, 12.0f,
              { Part { 1, 1, 0, arpChordHold, { 0 }, { 80, 0, 70, 0 }, { 190, 150, 190, 150 }, { 100 }, { 1 } },
                Part { 2, 10, 1, arpCyclic, { 0, 2, 4, 7, 4, 2, 0 }, { 95, 70, 80, 90, 65, 85, 75 }, { 55, 40, 55, 50, 40, 60, 45 }, { 100, 85, 100, 100, 80, 100, 90 }, { 1 } } } },

            { "Night Drive", "Progressive", "Eighth note bass and a twelve step arpeggio over a four chord loop.",
              F, 2, { { 0, 0 }, { 5, 0 }, { 6, 0 }, { 0, 1 } }, 2, 0, 3.0f, 10.0f,
              { Part { 1, 7, -2, arpRoot, { 0 }, { 105, 0, 85, 0, 95, 0, 80, 90 }, { 60 }, { 100 }, { 1 } },
                Part { 2, 10, 1, arpCyclic, { 0 }, { 90, 0, 70, 80, 0, 75, 85, 0, 65, 0, 80, 70 }, { 45 }, { 100, 100, 85, 100, 100, 90, 100, 100, 80, 100, 100, 90 }, { 1 } } } },

            { "Drifting Triplets", "Progressive", "Three parts on three cycle lengths: bass, a triplet lead and slow pad hits.",
              D, 2, { { 0, 2 }, { 3, 0 }, { 0, 4 }, { 6, 0 } }, 3, 0, 4.0f, 14.0f,
              { Part { 1, 7, -2, arpRoot, { 0 }, { 100, 0, 0, 85, 0, 90 }, { 55 }, { 100 }, { 1 } },
                Part { 2, 9, 1, arpCyclic, { 0, 4, 7, 12, 7, 4, 0, 2, 4 }, { 85, 65, 75, 90, 60, 70, 80, 60, 75 }, { 40 }, { 100, 90, 100, 100, 85, 100, 100, 80, 100 }, { 1 } },
                Part { 3, 1, 1, arpChordHold, { 0 }, { 75, 0, 65 }, { 170, 140, 180 }, { 100 }, { 1 } } } },

            // ---- Melodic House
            { "Sunrise Pluck", "Melodic House", "A bright sixteenth pluck over an offbeat bass.",
              A, 1, { { 0, 1 }, { 5, 0 }, { 2, 0 }, { 6, 0 } }, 2, 1, 3.0f, 12.0f,
              { Part { 1, 7, -2, arpRoot, { 0 }, { 0, 100, 0, 100 }, { 55 }, { 100 }, { 1 } },
                Part { 2, 10, 1, arpCyclic, { 0 }, { 105, 70, 85, 0, 95, 65, 0, 80, 100, 70, 0, 85, 90, 0, 75, 65 }, { 40, 35, 45, 40, 50, 35, 40, 45, 40, 35, 45, 40, 55, 40, 35, 45 }, { 100 }, { 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1 } } } },

            { "Golden Hour", "Melodic House", "A dotted sixteenth arpeggio drifting over warm sustained chords.",
              C, 4, { { 0, 1 }, { 4, 0 }, { 5, 1 }, { 3, 0 } }, 2, 0, 2.0f, 10.0f,
              { Part { 1, 1, 0, arpChordHold, { 0 }, { 80, 0, 70, 0 }, { 190, 160, 190, 150 }, { 100 }, { 1 } },
                Part { 2, 11, 1, arpLowHigh, { 0 }, { 95, 70, 85, 65, 90, 60, 80 }, { 50 }, { 100, 90, 100, 85, 100, 80, 100 }, { 1 } } } },

            { "Offbeat Bounce", "Melodic House", "Classic offbeat bass with a sparse chord pluck.",
              G, 2, { { 0, 0 }, { 6, 0 }, { 5, 0 }, { 6, 0 } }, 1, 1, 3.0f, 12.0f,
              { Part { 1, 7, -2, arpRoot, { 0 }, { 0, 110, 0, 105 }, { 50 }, { 100 }, { 1 } },
                Part { 2, 10, 0, arpCyclic, { 0, 7, 12 }, { 90, 0, 0, 80, 0, 85, 0, 0 }, { 40 }, { 100, 100, 100, 90, 100, 100, 100, 100 }, { 1 } } } },

            { "Heartbeat", "Melodic House", "A single lead engine: five steps on eighth notes, a pattern that turns over every five beats.",
              A, 2, { { 0, 0 }, { 5, 0 }, { 2, 0 }, { 6, 0 } }, 2, 0, 3.0f, 10.0f,
              { Part { 1, 7, 1, arpCyclic, { 0, 7, 5, 12, 7 }, { 100, 75, 90, 70, 85 }, { 55 }, { 100 }, { 1 } } } },

            // ---- Starter
            { "Single Arp", "Starter", "One engine arpeggiating the chords. A good place to start building from.",
              A, 1, { { 0, 0 }, { 5, 0 }, { 2, 0 }, { 6, 0 } }, 1, 0, 0.0f, 0.0f,
              { Part { 1, 10, 1, arpCyclic, { 0 }, { 100, 70, 85, 70, 95, 65, 80, 65 }, { 55 }, { 100 }, { 1 } } } },

            { "Four Part Polymeter", "Starter", "All four engines on different cycle lengths (8, 7, 3 and 5 steps). The default sketch.",
              A, 1, { { 0, 1 }, { 5, 0 }, { 2, 0 }, { 6, 0 } }, 1, 0, 0.0f, 0.0f,
              { Part { 1, 7, -2, arpRoot, { 0 }, { 112, 0, 96, 100, 0, 90, 100, 80 }, { 80, 60, 60, 70, 60, 80, 60, 50 }, { 100 }, { 1 } },
                Part { 2, 10, 0, arpCyclic, { 0, 12, 0, 0, 7, 0, 12 }, { 100, 70, 85, 70, 95, 65, 80 }, { 55, 40, 60, 45, 70, 40, 50 }, { 100, 90, 100, 80, 100, 70, 100 }, { 1, 1, 1, 2, 1, 1, 1 } },
                Part { 3, 1, 0, arpChordHold, { 0 }, { 90, 70, 80 }, { 180, 150, 190 }, { 100 }, { 1 } },
                Part { 4, 9, 1, arpLowHigh, { 0, 0, 12, 0, 7 }, { 105, 0, 80, 0, 90 }, { 40, 40, 50, 40, 60 }, { 80, 100, 70, 100, 60 }, { 1, 1, 3, 1, 2 } } } },
        };

        std::vector<Preset> out;
        for (auto& d : defs)
            out.push_back (detail::build (d));
        return out;
    }
}
