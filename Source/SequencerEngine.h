#pragma once

#include "Capture.h"
#include "Humanize.h"
#include "Settings.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace ember::seq
{
    struct Transport
    {
        bool hasPosition = false;   // false when the host gives no timeline (e.g. Standalone): the internal clock runs instead
        bool playing = false;
        double ppq = 0.0;           // quarter notes
        double bpm = 120.0;
    };

    // Four independent motif engines and a chord memory.
    //
    // Every engine has its own step count and clock rate (polymeter and polyrhythm) and fires on its own grid. On each
    // trigger it picks notes from the chord buffer (held MIDI notes, or the internal progression) according to its arp
    // mode, adds the step's pitch shift and its octave, snaps the result to the scale, and plays it on its own MIDI
    // channel. Step positions come from the host's quarter-note timeline, so engines stay locked to it through loops,
    // jumps and tempo changes.
    //
    // Real-time safe: no allocation, no locks. Notes are scheduled slightly ahead of the block so that negative
    // humanization jitter can place a note before its grid position.
    class Engine
    {
    public:
        void prepare (double sampleRate);
        void reset();
        void seed (std::uint64_t s) noexcept { rng.reseed (s); }

        // `midiIn` supplies the held notes (live chord buffer) and the mod wheel; generated notes go to `midiOut`.
        void process (const Settings&, const Transport&, const juce::MidiBuffer& midiIn, juce::MidiBuffer& midiOut,
                      int numSamples, CaptureRing* capture = nullptr);

        // Step each engine was on at the start of the last block, or -1 while stopped. Read by the editor.
        int playheadStep (int engine) const noexcept { return playhead[(size_t) engine].load (std::memory_order_relaxed); }
        int chordSlot() const noexcept { return chordPlayhead.load (std::memory_order_relaxed); }
        double lastBpm() const noexcept { return bpmSeen.load (std::memory_order_relaxed); }
        bool followingHost() const noexcept { return hostSeen.load (std::memory_order_relaxed); }   // the last block had a host timeline

    private:
        struct Event { std::int64_t sample = 0; int note = 0, velocity = 0, channel = 1; bool on = false, used = false; };
        struct Buffer { std::array<int, maxBufferNotes> note {}; int count = 0; };

        void scanInput (const juce::MidiBuffer&);
        Buffer bufferAt (const Settings&, double beat) const noexcept;
        int guard (const Settings&, int note, bool borrow) noexcept;
        void scheduleEngineStep (const Settings&, int engine, double boundary, double ppq, double bps, CaptureRing*);
        void scheduleChord (const Settings&, long long index, double startBeat, double ppq, double bps, CaptureRing*);
        void addNote (double onsetBeat, double lengthBeats, int note, int velocity, int channel, int source,
                      double ppq, double bps, double jitterMs, double slipFactor, CaptureRing*);
        void push (std::int64_t sample, int note, int velocity, int channel, bool on);
        void flush (juce::MidiBuffer&, int position);
        void emit (juce::MidiBuffer&, int numSamples);

        static constexpr int queueSize = 2048;

        double sampleRate = 44100.0;
        std::int64_t sampleClock = 0;   // sample index of the start of the current block
        std::array<Event, queueSize> queue {};
        std::array<std::array<int, 128>, 16> sounding {};   // [channel - 1][note]: the same pitch can sound on several channels

        bool wasPlaying = false;
        double expectedPpq = 0.0, internalPpq = 0.0;
        std::array<double, numEngines> lastBoundary {};
        std::array<bool, numEngines> haveBoundary {};
        std::array<int, numEngines> arpCounter {};
        std::array<bool, numEngines> arpHigh {};
        long long nextChord = 0;
        bool haveChord = false;
        double chordLenSeen = 0.0, chordShiftSeen = 0.0;

        std::array<bool, 128> held {};
        int modWheel = 0;

        humanize::Rng rng;
        std::array<std::atomic<int>, numEngines> playhead {};
        std::atomic<int> chordPlayhead { -1 };
        std::atomic<double> bpmSeen { 120.0 };
        std::atomic<bool> hostSeen { false };
    };
}
