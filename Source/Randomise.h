#pragma once

#include "Humanize.h"
#include "Settings.h"
#include <algorithm>
#include <vector>

// Dice for one motif engine. It only draws the pattern (step count, rate, ratio and the five main step lanes, and resets lane lengths, divides, late and harmony); the part's
// voicing and routing (octave, note pick, MIDI channel, mute, solo) stay as the player set them. The draws are shaped per
// role so a roll is usable rather than noise: the first step always sounds, rests and probability dips are limited, the
// bass never ratchets past two, and step counts avoid values that line up with the other engines.
namespace ember::dice
{
    enum Role { roleBass, roleLead, rolePad, roleAccent, numRoles };

    namespace detail
    {
        struct Profile
        {
            std::vector<int> steps, rates, pitches;
            double density;                  // chance that a step sounds
            int velLo, velHi, gateLo, gateHi;
            double ratchetChance;            // chance that a sounding step ratchets
            int maxRatchet;
        };

        inline const Profile& profile (int role)
        {
            static const Profile p[numRoles] {
                { { 3, 4, 5, 6, 7, 8, 9, 12, 16 }, { 4, 7, 7, 8, 10 },        { 0, 0, 0, 0, 0, 7, 5, 3, 12 },                0.55, 85, 120, 40, 90,   0.06, 2 },
                { { 5, 6, 7, 9, 10, 12, 16 },      { 7, 10, 10, 9, 12, 11 },  { 0, 0, 2, 4, 7, 7, 12, 5, -5, 9, 12, 0 },     0.65, 60, 118, 30, 90,   0.14, 4 },
                { { 2, 3, 4, 5, 6 },               { 0, 1, 2, 4, 5 },         { 0, 0, 0, 0, 7, 4, 12, 0 },                    0.70, 70, 105, 120, 190, 0.00, 1 },
                { { 3, 5, 7, 9, 11, 13 },          { 7, 9, 10, 12 },          { 0, 7, 12, 12, 5, 10, -5, 0 },                 0.42, 80, 120, 20, 60,   0.20, 3 },
            };
            return p[std::clamp (role, 0, numRoles - 1)];
        }

        template <typename T>
        const T& pick (humanize::Rng& rng, const std::vector<T>& v) { return v[(size_t) (rng.uniform() * (double) v.size()) % v.size()]; }

        inline int between (humanize::Rng& rng, int lo, int hi) { return lo + (int) (rng.uniform() * (double) (hi - lo + 1)) % (hi - lo + 1); }
    }

    // `avoidSteps` lists the step counts the other engines already use; a roll prefers one that is not in it.
    inline void randomiseEngine (humanize::Rng& rng, seq::EngineSettings& e, int role, const std::vector<int>& avoidSteps = {})
    {
        const auto& p = detail::profile (role);

        int steps = detail::pick (rng, p.steps);
        for (int tries = 0; tries < 8 && std::find (avoidSteps.begin(), avoidSteps.end(), steps) != avoidSteps.end(); ++tries)
            steps = detail::pick (rng, p.steps);
        e.steps = steps;
        e.rate = detail::pick (rng, p.rates);

        // a ratio other than 1:1 is the exception, and only a gentle one
        e.num = e.den = 1;
        if (rng.uniform() < 0.12)
        {
            static const int ratios[][2] { { 3, 2 }, { 4, 3 }, { 5, 4 }, { 2, 3 } };
            const auto& r = ratios[(size_t) (rng.uniform() * 4.0) % 4];
            e.num = r[0];
            e.den = r[1];
        }

        std::array<int, seq::maxSteps> pitch {}, vel {}, gate {}, prob {}, ratch {};
        int active = 0;
        for (int i = 0; i < steps; ++i)
        {
            const bool on = i == 0 || rng.uniform() < p.density;
            vel[(size_t) i] = on ? detail::between (rng, p.velLo, p.velHi) : 0;
            gate[(size_t) i] = detail::between (rng, p.gateLo, p.gateHi);
            pitch[(size_t) i] = on && i > 0 ? detail::pick (rng, p.pitches) : 0;
            prob[(size_t) i] = on && i > 0 && rng.uniform() < 0.15 ? detail::between (rng, 55, 90) : 100;

            ratch[(size_t) i] = 1;
            if (on && i > 0 && rng.uniform() < p.ratchetChance)
                ratch[(size_t) i] = detail::between (rng, 2, p.maxRatchet);
            active += on ? 1 : 0;
        }

        // a lane that is nearly empty is not a pattern: wake a few more steps
        const int wanted = std::max (2, (steps + 3) / 4);
        for (int i = 1; active < wanted && i < steps; i += 2)
            if (vel[(size_t) i] == 0)
            {
                vel[(size_t) i] = detail::between (rng, p.velLo, p.velHi);
                ++active;
            }

        // lanes repeat across all 32 slots, so raising the step count later keeps the same pattern
        for (int i = 0; i < seq::maxSteps; ++i)
        {
            const auto s = (size_t) (i % steps);
            e.step[seq::kindPitch][(size_t) i] = pitch[s];
            e.step[seq::kindVelocity][(size_t) i] = vel[s];
            e.step[seq::kindGate][(size_t) i] = gate[s];
            e.step[seq::kindProbability][(size_t) i] = prob[s];
            e.step[seq::kindRatchet][(size_t) i] = ratch[s];
        }

        // a roll starts the lanes over together: linked lengths, no divides, no late or harmony
        e.laneLength.fill (0);
        e.laneDivide.fill (1);
        e.step[seq::kindLate].fill (0);
        e.step[seq::kindHarmony].fill (0);
    }

    // Rolls the engines in `which` (bit per engine). Step counts are chosen to differ from the engines left alone and from each other.
    inline void randomiseEngines (humanize::Rng& rng, seq::Settings& s, unsigned which)
    {
        std::vector<int> taken;
        for (int e = 0; e < seq::numEngines; ++e)
            if ((which & (1u << e)) == 0)
                taken.push_back (s.engine[(size_t) e].steps);

        for (int e = 0; e < seq::numEngines; ++e)
            if ((which & (1u << e)) != 0)
            {
                randomiseEngine (rng, s.engine[(size_t) e], e, taken);
                taken.push_back (s.engine[(size_t) e].steps);
            }
    }
}
