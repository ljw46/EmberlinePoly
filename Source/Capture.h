#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace ember::seq
{
    struct CapturedNote
    {
        double startBeats = 0.0;    // host position (quarter notes) of the note-on, including humanization
        double lengthBeats = 0.0;
        int note = 60, velocity = 100, channel = 1;
        int source = 0;             // 0 base chords, 1-4 motif engines
    };

    // Rolling record of the notes the engine generated, for Drag-to-DAW. The audio thread appends; the message
    // thread copies a snapshot. Nothing blocks: the oldest notes are simply overwritten, and the reader leaves a
    // safety margin so it never copies a slot the writer is about to reuse.
    class CaptureRing
    {
    public:
        static constexpr std::uint64_t capacity = 16384;

        void push (const CapturedNote& n) noexcept   // audio thread only
        {
            const auto w = written.load (std::memory_order_relaxed);
            slots[(size_t) (w % capacity)] = n;
            written.store (w + 1, std::memory_order_release);
        }

        void clear() noexcept { clearedAt.store (written.load (std::memory_order_acquire), std::memory_order_release); }

        std::vector<CapturedNote> snapshot() const
        {
            constexpr std::uint64_t margin = 512;
            const auto end = written.load (std::memory_order_acquire);
            auto begin = clearedAt.load (std::memory_order_acquire);
            if (end - begin > capacity - margin)
                begin = end - (capacity - margin);

            std::vector<CapturedNote> out;
            out.reserve ((size_t) (end - begin));
            for (auto i = begin; i < end; ++i)
                out.push_back (slots[(size_t) (i % capacity)]);
            return out;
        }

    private:
        std::array<CapturedNote, (size_t) capacity> slots {};
        std::atomic<std::uint64_t> written { 0 }, clearedAt { 0 };
    };
}
