#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Real-time-safe random numbers and the two humanization offsets (micro-timing jitter, gate slip).
namespace ember::humanize
{
    class Rng
    {
    public:
        explicit Rng (std::uint64_t seed = 0x9e3779b97f4a7c15ull) noexcept { reseed (seed); }

        void reseed (std::uint64_t seed) noexcept { state = seed != 0 ? seed : 0x9e3779b97f4a7c15ull; }

        std::uint64_t next() noexcept   // xorshift64*
        {
            state ^= state >> 12;
            state ^= state << 25;
            state ^= state >> 27;
            return state * 0x2545f4914f6cdd1dull;
        }

        double uniform() noexcept { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }   // [0, 1)

        double gaussian() noexcept   // standard normal, Box-Muller
        {
            const double u1 = std::max (uniform(), 1.0e-12), u2 = uniform();
            return std::sqrt (-2.0 * std::log (u1)) * std::cos (6.283185307179586 * u2);
        }

    private:
        std::uint64_t state = 1;
    };

    // Gaussian offset in the range +-limit: the spread is limit / 2.5, then clamped so a rare outlier can never
    // push a note far off the grid.
    inline double boundedGaussian (Rng& rng, double limit) noexcept
    {
        return limit <= 0.0 ? 0.0 : std::clamp (rng.gaussian() * limit / 2.5, -limit, limit);
    }

    inline double jitterMs (Rng& rng, double amountMs) noexcept { return boundedGaussian (rng, amountMs); }

    // Multiplier for a note's length: 1 +- (amountPercent / 100), never below 5% of the original.
    inline double slipFactor (Rng& rng, double amountPercent) noexcept
    {
        return std::max (0.05, 1.0 + boundedGaussian (rng, amountPercent * 0.01));
    }
}
