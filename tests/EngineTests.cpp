// Command-line checks for the sequencer engines, chord generator, scale maths, humanization and MIDI export.
// Needs no display or audio device, so CI runs it on Linux:  EmberlinePolyTests
#include "Chords.h"
#include "FactoryPresets.h"
#include "Pattern.h"
#include "Randomise.h"
#include "MidiClip.h"
#include "SequencerEngine.h"
#include "Scales.h"
#include <juce_core/juce_core.h>
#include <cstdio>
#include <map>
#include <set>
#include <tuple>

#pragma GCC diagnostic ignored "-Wfloat-equal"   // window and rate maths here is exact on purpose

using namespace ember;

namespace
{
    int failures = 0, checks = 0;

    void check (bool ok, const char* what, int line)
    {
        ++checks;
        if (! ok)
        {
            ++failures;
            std::printf ("FAIL line %d: %s\n", line, what);
        }
    }
    #define CHECK(...) check ((__VA_ARGS__), #__VA_ARGS__, __LINE__)

    struct Ev { long long sample; bool on; int note, velocity, channel; };

    constexpr double sr = 44100.0;

    // C major, chord I (C E G, MIDI 60 64 67), chord output off. Engine 1 plays a sixteenth every step; the others rest.
    seq::Settings base()
    {
        seq::Settings s;
        s.scale = 0;
        s.root = 0;
        s.baseOctave = 3;
        s.scaleLock = true;
        s.chords.out = false;
        s.chords.length = 1;
        s.chords.slot[0] = { 0, chords::extTriad };

        for (int e = 0; e < seq::numEngines; ++e)
        {
            auto& en = s.engine[(size_t) e];
            en.steps = 4;
            en.channel = e + 2;
            en.arp = seq::arpCyclic;
            en.step[seq::kindPitch].fill (0);
            en.step[seq::kindVelocity].fill (e == 0 ? 100 : 0);
            en.step[seq::kindGate].fill (50);
            en.step[seq::kindProbability].fill (100);
            en.step[seq::kindRatchet].fill (1);
        }
        return s;
    }

    std::vector<Ev> run (const seq::Settings& s, double beats, int block, const juce::MidiBuffer& input = {}, double bpm = 120.0,
                         seq::CaptureRing* ring = nullptr)
    {
        seq::Engine e;
        e.prepare (sr);
        e.seed (12345);

        std::vector<Ev> out;
        const double bps = bpm / 60.0 / sr;
        long long pos = 0;
        const long long total = (long long) (beats / bps);
        while (pos < total)
        {
            seq::Transport t { true, true, (double) pos * bps, bpm };
            juce::MidiBuffer midi;
            e.process (s, t, pos == 0 ? input : juce::MidiBuffer(), midi, block, ring);
            for (const auto m : midi)
            {
                const auto msg = m.getMessage();
                if (pos + m.samplePosition < total)   // the last block overshoots the requested length
                    out.push_back ({ pos + m.samplePosition, msg.isNoteOn(), msg.getNoteNumber(), msg.getVelocity(), msg.getChannel() });
            }
            pos += block;
        }
        return out;
    }

    std::vector<Ev> ons (const std::vector<Ev>& v, int channel = 0)
    {
        std::vector<Ev> o;
        for (auto& e : v) if (e.on && (channel == 0 || e.channel == channel)) o.push_back (e);
        return o;
    }

    double beatOf (long long sample, double bpm = 120.0) { return (double) sample / sr * bpm / 60.0; }

    // Never two overlapping note-ons of one pitch on one channel, and every note-on eventually closed.
    bool balanced (const std::vector<Ev>& ev)
    {
        std::map<std::pair<int, int>, int> open;
        for (auto& e : ev)
        {
            auto& n = open[{ e.channel, e.note }];
            n += e.on ? 1 : -1;
            if (n < 0 || n > 1) return false;
        }
        return true;
    }

    // ------------------------------------------------------------------ maths
    void testRatesAndScales()
    {
        CHECK (rates::stepBeats (rates::defaultIndex, 1, 1) == 0.25);
        CHECK (std::abs (rates::stepBeats (rates::defaultIndex, 5, 4) - 0.2) < 1e-12);       // 5:4 is faster
        CHECK (std::abs (rates::stepBeats (rates::defaultIndex, 3, 4) - 1.0 / 3.0) < 1e-12);  // 3:4 is slower
        CHECK (rates::stepAt (0.0, 0.25, 7) == 0);
        CHECK (rates::stepAt (7 * 0.25, 0.25, 7) == 0);
        CHECK (rates::stepAt (-0.25, 0.25, 7) == 6);

        CHECK (scales::degreeToSemitones (0, 7) == 12);
        CHECK (scales::degreeToSemitones (0, -1) == -1);
        CHECK (scales::degreeToSemitones (1, 2) == 3);
        CHECK (scales::degreeToSemitones (6, 5) == 12);

        CHECK (scales::snap (0, 1) == 0);    // C# in C major: tie between C and D goes down
        CHECK (scales::snap (0, 6) == 5);
        CHECK (scales::snap (1, 4) == 3);    // E in C minor -> Eb
        CHECK (scales::snap (6, 2) == 3 && scales::snap (6, 1) == 0);   // minor pentatonic: D -> Eb, Db -> C

        // exact intervals of every supported scale, and snapping always lands inside the scale
        const int expectMajor[] { 0, 2, 4, 5, 7, 9, 11 }, expectDorian[] { 0, 2, 3, 5, 7, 9, 10 }, expectPhrygian[] { 0, 1, 3, 5, 7, 8, 10 },
                  expectHarmonic[] { 0, 2, 3, 5, 7, 8, 11 };
        for (int d = 0; d < 7; ++d)
        {
            CHECK (scales::degreeToSemitones (0, d) == expectMajor[d]);
            CHECK (scales::degreeToSemitones (2, d) == expectDorian[d]);
            CHECK (scales::degreeToSemitones (3, d) == expectPhrygian[d]);
            CHECK (scales::degreeToSemitones (7, d) == expectHarmonic[d]);
        }
        for (int sc = 0; sc < scales::count; ++sc)
            for (int n = -40; n <= 40; ++n)
            {
                const int snapped = scales::snap (sc, n);
                CHECK (scales::contains (sc, snapped));
                CHECK (std::abs (snapped - n) <= 3);   // never further than a minor third (pentatonic gaps)
            }
    }

    void testHumanizeMath()
    {
        humanize::Rng rng (7);
        double sum = 0.0;
        bool bounded = true;
        for (int i = 0; i < 20000; ++i)
        {
            const double j = humanize::jitterMs (rng, 10.0);
            sum += j;
            bounded = bounded && std::abs (j) <= 10.0;
        }
        CHECK (bounded);
        CHECK (std::abs (sum / 20000.0) < 0.2);
        CHECK (humanize::jitterMs (rng, 0.0) == 0.0);

        bool slipOk = true;
        for (int i = 0; i < 5000; ++i)
        {
            const double f = humanize::slipFactor (rng, 40.0);
            slipOk = slipOk && f >= 0.6 - 1e-9 && f <= 1.4 + 1e-9;
        }
        CHECK (slipOk);
    }

    // ------------------------------------------------------------------ chords
    void testChordVoicing()
    {
        auto v = chords::voice (0, 0, chords::extTriad);
        CHECK (v.count == 3 && v.semis[0] == 0 && v.semis[1] == 4 && v.semis[2] == 7);

        v = chords::voice (0, 4, chords::extTriad);   // V in C major: G B D, dropped an octave to stay low
        CHECK (v.count == 3 && v.semis[0] == -5 && v.semis[1] == -1 && v.semis[2] == 2);

        v = chords::voice (1, 0, chords::ext7th);     // Cm7
        CHECK (v.count == 4 && v.semis[0] == 0 && v.semis[1] == 3 && v.semis[2] == 7 && v.semis[3] == 10);

        v = chords::voice (1, 0, chords::ext9th);     // Cm9
        CHECK (v.count == 5 && v.semis[4] == 14);

        v = chords::voice (1, 0, chords::extSus2);
        CHECK (v.count == 3 && v.semis[1] == 2 && v.semis[2] == 7);
        v = chords::voice (1, 0, chords::extSus4);
        CHECK (v.count == 3 && v.semis[1] == 5 && v.semis[2] == 7);
        v = chords::voice (1, 0, chords::ext7sus4);
        CHECK (v.count == 4 && v.semis[1] == 5 && v.semis[3] == 10);

        v = chords::voice (1, 5, chords::ext7th);     // VI maj7 in natural minor: Ab C Eb G
        CHECK (v.count == 4 && v.semis[0] == -4 && v.semis[1] == 0 && v.semis[2] == 3 && v.semis[3] == 7);

        // every chord of every scale is made of scale tones and sorted
        bool inScale = true, sorted = true;
        for (int sc = 0; sc < scales::count; ++sc)
            for (int d = 0; d < 7; ++d)
                for (int x = 0; x < chords::numExtensions; ++x)
                {
                    const auto c = chords::voice (sc, d, x);
                    for (int i = 0; i < c.count; ++i)
                    {
                        inScale = inScale && scales::contains (sc, c.semis[(size_t) i]);
                        sorted = sorted && (i == 0 || c.semis[(size_t) i] > c.semis[(size_t) i - 1]);
                    }
                }
        CHECK (inScale && sorted);
    }

    void testSuggestProgressions()
    {
        for (int genre = 0; genre < 3; ++genre)
        {
            humanize::Rng rng (99 + (std::uint64_t) genre);
            std::set<std::vector<int>> distinct;
            std::set<int> scalesUsed;
            bool valid = true, hasColour = false;

            for (int n = 0; n < 300; ++n)
            {
                const auto s = chords::suggest (rng, genre);
                valid = valid && s.progression.length >= 1 && s.progression.length <= seq::maxChordSlots;
                std::vector<int> key;
                for (int i = 0; i < s.progression.length; ++i)
                {
                    const auto& c = s.progression.slot[(size_t) i];
                    valid = valid && c.degree >= 0 && c.degree < 7 && c.extension >= 0 && c.extension < chords::numExtensions;
                    hasColour = hasColour || c.extension != chords::extTriad;
                    key.push_back (c.degree * 10 + c.extension);
                }
                key.push_back (s.scale);
                distinct.insert (key);
                scalesUsed.insert (s.scale);
                valid = valid && s.progression.slot[0].degree >= 0 && (s.scale == 1 || s.scale == 2 || s.scale == 3 || s.scale == 7);
            }
            CHECK (valid);
            CHECK (hasColour);
            CHECK (distinct.size() >= 6);
            if (genre == chords::genreDeepTechno)
                CHECK (scalesUsed.count (3) == 1 && scalesUsed.count (1) == 0);   // Phrygian / Dorian, never plain minor
        }

        humanize::Rng a (5), b (5);
        const auto x = chords::suggest (a, 0), y = chords::suggest (b, 0);
        CHECK (x.scale == y.scale && x.progression.length == y.progression.length && x.progression.slot[1].degree == y.progression.slot[1].degree);
    }

    // ------------------------------------------------------------------ arp buffer
    void testGridAndArpModes()
    {
        auto s = base();
        auto on = ons (run (s, 4.0, 512));
        CHECK (on.size() == 16);   // 4 beats of sixteenths
        const int cyc[] { 60, 64, 67 };
        for (size_t i = 0; i < on.size(); ++i)
        {
            CHECK (on[i].note == cyc[i % 3]);               // As Played / Cyclic walks the chord
            CHECK (on[i].channel == 2);
            CHECK (std::abs ((double) on[i].sample - (double) i * 0.25 * sr * 0.5) <= 1.0);   // 120 bpm: a beat is 0.5 s
        }

        s.engine[0].arp = seq::arpChordHold;
        auto ev = run (s, 2.0, 512);
        on = ons (ev);
        CHECK (on.size() == 24);   // 8 triggers x 3 notes
        for (size_t i = 0; i + 2 < on.size(); i += 3)
            CHECK (on[i].sample == on[i + 1].sample && on[i].sample == on[i + 2].sample && on[i].note == 60 && on[i + 1].note == 64 && on[i + 2].note == 67);
        CHECK (balanced (ev));

        s.engine[0].arp = seq::arpLowHigh;
        on = ons (run (s, 1.0, 512));
        CHECK (on.size() == 4 && on[0].note == 60 && on[1].note == 67 && on[2].note == 60 && on[3].note == 67);

        s.engine[0].arp = seq::arpRoot;
        on = ons (run (s, 1.0, 512));
        CHECK (on.size() == 4 && on[0].note == 60 && on[3].note == 60);

        s.engine[0].arp = seq::arpRandom;
        on = ons (run (s, 32.0, 512));
        std::set<int> seen;
        for (auto& e : on) seen.insert (e.note);
        CHECK (on.size() == 128 && seen == std::set<int> ({ 60, 64, 67 }));
    }

    void testLiveBuffer()
    {
        auto s = base();
        s.chords.source = seq::chordLive;

        CHECK (ons (run (s, 2.0, 512)).empty());   // nothing held: nothing to play

        juce::MidiBuffer held;
        held.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);   // A, F, D: held out of order
        held.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100), 0);
        held.addEvent (juce::MidiMessage::noteOn (1, 65, (juce::uint8) 100), 0);
        auto on = ons (run (s, 1.0, 512, held));
        CHECK (on.size() == 4 && on[0].note == 62 && on[1].note == 65 && on[2].note == 69 && on[3].note == 62);   // sorted by pitch

        s.engine[0].arp = seq::arpChordHold;
        CHECK (ons (run (s, 1.0, 512, held)).size() == 12);

        // the guard applies to live notes: C# is pulled into C major, and passed through with Scale Lock off
        juce::MidiBuffer chromatic;
        chromatic.addEvent (juce::MidiMessage::noteOn (1, 61, (juce::uint8) 100), 0);
        s.engine[0].arp = seq::arpRoot;
        CHECK (ons (run (s, 0.25, 512, chromatic))[0].note == 60);
        s.scaleLock = false;
        CHECK (ons (run (s, 0.25, 512, chromatic))[0].note == 61);

        // mod wheel above half switches the guard to the borrow scale (natural minor): E becomes Eb
        auto b = base();
        b.chords.source = seq::chordLive;
        b.borrowScale = 1;
        b.engine[0].arp = seq::arpRoot;
        juce::MidiBuffer wheel;
        wheel.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
        wheel.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0);
        CHECK (ons (run (b, 0.25, 512, wheel))[0].note == 63);
    }

    void testPitchOctaveAndGuard()
    {
        auto s = base();
        s.engine[0].arp = seq::arpRoot;
        s.engine[0].step[seq::kindPitch] = { 0, 4, 7, -12 };
        auto on = ons (run (s, 1.0, 512));
        CHECK (on[0].note == 60 && on[1].note == 64 && on[2].note == 67 && on[3].note == 48);

        s.engine[0].octave = 1;
        on = ons (run (s, 1.0, 512));
        CHECK (on[0].note == 72 && on[3].note == 60);
        s.engine[0].octave = -2;
        CHECK (ons (run (s, 1.0, 512))[0].note == 36);
        s.engine[0].octave = 0;

        s.engine[0].step[seq::kindPitch] = { 1, 6, 11, 3 };   // C#, F#, B, Eb: snapped into C major
        on = ons (run (s, 1.0, 512));
        CHECK (on[0].note == 60 && on[1].note == 65 && on[2].note == 71 && on[3].note == 62);
        s.scaleLock = false;
        on = ons (run (s, 1.0, 512));
        CHECK (on[0].note == 61 && on[1].note == 66 && on[2].note == 71 && on[3].note == 63);   // direct override

        s.scaleLock = true;
        s.root = 7;   // G major: the chord rebuilds on G and the guard follows
        s.engine[0].step[seq::kindPitch] = { 0, 4, 6, 1 };
        on = ons (run (s, 1.0, 512));
        CHECK (on[0].note == 67 && on[1].note == 71 && on[2].note == 72 && on[3].note == 67);   // +6 is C#: tie, snaps down to C; +1 snaps back to G

        // every note from every scale and a spread of pitch steps lands inside the active scale
        for (int sc = 0; sc < scales::count; ++sc)
        {
            auto t = base();
            t.scale = sc;
            t.root = 5;
            t.engine[0].arp = seq::arpChordHold;
            for (int i = 0; i < 4; ++i) t.engine[0].step[seq::kindPitch][(size_t) i] = i * 5 - 7;
            bool all = true;
            for (auto& e : ons (run (t, 2.0, 512)))
                all = all && scales::contains (sc, e.note - 5);
            CHECK (all);
        }
    }

    // ------------------------------------------------------------------ polymeter / polyrhythm
    void testPolyrhythmAcrossEngines()
    {
        auto s = base();
        s.engine[1].step[seq::kindVelocity].fill (90);
        s.engine[1].rate = 9;                      // 1/8T: three per beat
        s.engine[2].step[seq::kindVelocity].fill (80);
        s.engine[2].num = 5; s.engine[2].den = 4;  // 1/16 at 5:4: five per beat
        s.engine[3].step[seq::kindVelocity].fill (70);
        s.engine[3].rate = 4;                      // 1/4: one per beat

        const auto ev = run (s, 4.0, 512);
        CHECK (ons (ev, 2).size() == 16);
        CHECK (ons (ev, 3).size() == 12);
        CHECK (ons (ev, 4).size() == 20);
        CHECK (ons (ev, 5).size() == 4);

        const auto triplets = ons (ev, 3);
        bool spacing = true;
        for (size_t i = 1; i < triplets.size(); ++i)
            spacing = spacing && std::abs (beatOf (triplets[i].sample - triplets[i - 1].sample) - 1.0 / 3.0) < 1e-3;
        CHECK (spacing);

        // the engines share the transport, so they line up on the downbeat
        CHECK (ons (ev, 2)[0].sample == ons (ev, 3)[0].sample && ons (ev, 3)[0].sample == ons (ev, 5)[0].sample);
    }

    void testPolymeterAcrossEngines()
    {
        auto s = base();
        s.engine[0].steps = 3;
        s.engine[0].step[seq::kindVelocity] = { 10, 20, 30, 0 };
        s.engine[1].steps = 4;
        s.engine[1].step[seq::kindVelocity] = { 1, 2, 3, 4 };

        const auto ev = run (s, 6.0, 512);   // 24 triggers each
        const auto a = ons (ev, 2), b = ons (ev, 3);
        CHECK (a.size() == 24 && b.size() == 24);

        std::set<std::pair<int, int>> pairs;
        for (size_t i = 0; i < a.size(); ++i)
        {
            CHECK (a[i].velocity == 10 + 10 * (int) (i % 3));
            CHECK (b[i].velocity == 1 + (int) (i % 4));
            pairs.insert ({ a[i].velocity, b[i].velocity });
        }
        CHECK (pairs.size() == 12);   // the combined pattern repeats every lcm(3, 4) = 12 triggers
    }

    void testMuteSoloAndChannels()
    {
        auto s = base();
        for (int e = 1; e < 4; ++e)
            s.engine[(size_t) e].step[seq::kindVelocity].fill (90);
        s.engine[0].channel = 7;
        s.engine[3].channel = 16;

        auto ev = run (s, 1.0, 512);
        std::set<int> channels;
        for (auto& e : ons (ev)) channels.insert (e.channel);
        CHECK (channels == std::set<int> ({ 7, 3, 4, 16 }));

        s.engine[1].mute = true;
        channels.clear();
        for (auto& e : ons (run (s, 1.0, 512))) channels.insert (e.channel);
        CHECK (channels == std::set<int> ({ 7, 4, 16 }));

        s.engine[2].solo = true;
        channels.clear();
        for (auto& e : ons (run (s, 1.0, 512))) channels.insert (e.channel);
        CHECK (channels == std::set<int> ({ 4 }));

        s.engine[0].solo = true;   // two soloed engines play, the rest stay silent
        channels.clear();
        for (auto& e : ons (run (s, 1.0, 512))) channels.insert (e.channel);
        CHECK (channels == std::set<int> ({ 7, 4 }));
    }

    void testRatchetProbabilityAndRests()
    {
        auto s = base();
        s.engine[0].rate = 4;                                      // quarter notes
        s.engine[0].step[seq::kindRatchet] = { 3, 1, 4, 2 };
        s.engine[0].step[seq::kindGate].fill (100);
        auto ev = run (s, 4.0, 512);
        auto on = ons (ev);
        CHECK (on.size() == 10);   // 3 + 1 + 4 + 2
        CHECK (std::abs (beatOf (on[1].sample - on[0].sample) - 1.0 / 3.0) < 1e-3);        // a third of the beat apart
        CHECK (on[0].velocity > on[1].velocity && on[1].velocity > on[2].velocity);        // each roll hit a little softer
        CHECK (std::abs (beatOf (on[5].sample - on[4].sample) - 0.25) < 1e-3);   // the four-way ratchet
        CHECK (balanced (ev));

        s = base();
        s.engine[0].step[seq::kindProbability].fill (0);
        CHECK (ons (run (s, 4.0, 512)).empty());
        s.engine[0].step[seq::kindProbability].fill (50);
        const auto n = (int) ons (run (s, 128.0, 512)).size();    // 512 chances
        CHECK (n > 200 && n < 312);

        s = base();
        s.engine[0].step[seq::kindVelocity] = { 100, 0, 100, 0 };
        CHECK (ons (run (s, 4.0, 512)).size() == 8);   // velocity 0 is a rest
    }

    void testBlockSizeIndependence()
    {
        auto s = base();
        s.engine[0].steps = 7;
        s.engine[1].step[seq::kindVelocity].fill (90);
        s.engine[1].steps = 3;
        s.engine[1].arp = seq::arpChordHold;
        s.engine[1].step[seq::kindRatchet].fill (2);
        s.chords.out = true;
        auto a = run (s, 8.0, 512), b = run (s, 8.0, 37), c = run (s, 8.0, 4096);
        // events at the very same sample may come out in a different order for different block sizes; the stream must otherwise match
        for (auto* v : { &a, &b, &c })
            std::stable_sort (v->begin(), v->end(), [] (const Ev& x, const Ev& y)
            {
                return std::tie (x.sample, x.on, x.channel, x.note) < std::tie (y.sample, y.on, y.channel, y.note);
            });
        CHECK (a.size() == b.size() && a.size() == c.size());
        bool same = a.size() == b.size() && a.size() == c.size();
        for (size_t i = 0; same && i < a.size(); ++i)
            same = std::abs (a[i].sample - b[i].sample) <= 1 && std::abs (a[i].sample - c[i].sample) <= 1   // host position maths may differ by a sample
                   && a[i].note == b[i].note && a[i].note == c[i].note && a[i].channel == c[i].channel && a[i].on == b[i].on;
        CHECK (same);
    }

    // ------------------------------------------------------------------ chord output
    void testChordOutputAndTiming()
    {
        auto s = base();
        s.chords.out = true;
        s.chords.channel = 1;
        s.chords.length = 2;
        s.chords.slot[0] = { 0, chords::extTriad };   // C
        s.chords.slot[1] = { 3, chords::extTriad };   // F
        s.engine[0].arp = seq::arpRoot;
        s.engine[0].rate = 4;   // quarter notes: the engine follows the chord changes

        auto ev = run (s, 8.0, 512);
        auto chord = ons (ev, 1);
        CHECK (chord.size() == 6);
        CHECK (chord[0].note == 60 && chord[1].note == 64 && chord[2].note == 67);
        CHECK (chord[3].note == 65 && chord[4].note == 69 && chord[5].note == 72);
        CHECK (std::abs (beatOf (chord[3].sample) - 4.0) < 1e-3);
        CHECK (balanced (ev));

        const auto bass = ons (ev, 2);
        CHECK (bass.size() == 8 && bass[0].note == 60 && bass[3].note == 60 && bass[4].note == 65 && bass[7].note == 65);

        // chord changes an eighth early (anticipated); the engine reads the same chord grid
        s.chords.timing = seq::timingAnticipate;
        ev = run (s, 8.0, 512);
        chord = ons (ev, 1);
        CHECK (chord.size() >= 6 && std::abs (beatOf (chord[3].sample) - 3.5) < 1e-3);
        CHECK (std::abs (beatOf (chord[0].sample)) < 1e-3);   // starts on the first beat, mid-way into the first chord
        CHECK (balanced (ev));

        // off-beat: the previous chord wraps in until the delayed change lands an eighth after the downbeat
        s.chords.timing = seq::timingOffBeat;
        chord = ons (run (s, 8.0, 512), 1);
        CHECK (chord.size() >= 6 && chord[0].note == 65 && std::abs (beatOf (chord[3].sample) - 0.5) < 1e-3 && chord[3].note == 60);

        // chord length: two bars per chord
        s.chords.timing = seq::timingOnBeat;
        s.chords.barsIndex = 2;
        chord = ons (run (s, 16.0, 512), 1);
        CHECK (chord.size() == 6 && std::abs (beatOf (chord[3].sample) - 8.0) < 1e-3);

        // chords out off silences the chord channel but not the engines
        s.chords.out = false;
        CHECK (ons (run (s, 4.0, 512), 1).empty() && ! ons (run (s, 4.0, 512), 2).empty());

        // live mode: no internal chord output
        s.chords.out = true;
        s.chords.source = seq::chordLive;
        CHECK (ons (run (s, 4.0, 512), 1).empty());
    }

    void testHumanizedTimingStaysOnClock()
    {
        auto s = base();
        s.engine[0].arp = seq::arpChordHold;
        s.jitterMs = 25.0f;
        s.slipPercent = 30.0f;
        const auto ev = run (s, 32.0, 256);
        const auto on = ons (ev);
        CHECK (on.size() >= 380 && on.size() <= 388);   // 128 triggers x 3 notes; one near the cut-off may jitter across it

        const double limit = 0.025 * sr + 1.0;
        bool inRange = true, together = true;
        int spread = 0;
        for (size_t t = 0; t + 2 < on.size() && t / 3 < 127; t += 3)
        {
            const double grid = (double) (t / 3) * 0.25 * sr * 0.5;
            const double d = (double) on[t].sample - grid;
            inRange = inRange && std::abs (d) <= limit;
            spread += std::abs (d) > 20.0;
            together = together && on[t].sample == on[t + 1].sample && on[t].sample == on[t + 2].sample;   // a chord keeps its notes together
        }
        CHECK (inRange && together);
        CHECK (spread > 100);   // jitter is actually applied
        CHECK (balanced (ev));
    }

    void testOverlapAndRetrigger()
    {
        auto s = base();
        s.engine[0].arp = seq::arpRoot;
        s.engine[0].step[seq::kindGate].fill (200);   // each note lasts two steps, the same pitch every step
        CHECK (balanced (run (s, 4.0, 512)));
    }

    void testTransportStopAndJump()
    {
        seq::Engine e;
        e.prepare (sr);
        auto s = base();
        s.chords.out = true;
        s.engine[0].step[seq::kindGate].fill (200);

        juce::MidiBuffer midi;
        e.process (s, { true, true, 0.0, 120.0 }, {}, midi, 512);
        int on = 0, off = 0;
        for (const auto m : midi) { on += m.getMessage().isNoteOn(); off += m.getMessage().isNoteOff(); }
        CHECK (on == 4 && off == 0);   // three chord notes and the first engine note

        juce::MidiBuffer stopped;
        e.process (s, { true, false, 0.0, 120.0 }, {}, stopped, 512);
        int stopOffs = 0;
        for (const auto m : stopped) stopOffs += m.getMessage().isNoteOff();
        CHECK (stopOffs == 4);
        CHECK (e.playheadStep (0) == -1 && e.chordSlot() == -1);

        juce::MidiBuffer a, b;
        e.process (s, { true, true, 0.0, 120.0 }, {}, a, 512);
        e.process (s, { true, true, 0.0, 120.0 }, {}, b, 512);   // the host jumped back: sounding notes close, the grids restart
        int jumpOffs = 0, jumpOns = 0;
        for (const auto m : b) { jumpOffs += m.getMessage().isNoteOff(); jumpOns += m.getMessage().isNoteOn(); }
        CHECK (jumpOffs == 4 && jumpOns == 4);
    }

    void testInternalClockAndPlayhead()
    {
        seq::Engine e;
        e.prepare (sr);
        auto s = base();
        s.engine[0].steps = 7;
        s.engine[1].steps = 3;
        s.internalBpm = 120.0;

        int notes = 0;
        for (int i = 0; i < 86; ++i)   // about one second
        {
            juce::MidiBuffer midi;
            e.process (s, { false, false, 0.0, 120.0 }, {}, midi, 512);
            for (const auto m : midi) notes += m.getMessage().isNoteOn();
        }
        CHECK (notes == 8);   // 2 beats of sixteenths
        CHECK (e.playheadStep (0) >= 0 && e.playheadStep (0) < 7);
        CHECK (e.playheadStep (1) >= 0 && e.playheadStep (1) < 3);

        s.run = false;
        juce::MidiBuffer midi;
        e.process (s, { false, false, 0.0, 120.0 }, {}, midi, 512);
        CHECK (e.playheadStep (0) == -1);
    }

    // ------------------------------------------------------------------ capture and MIDI files
    juce::MidiFile readBack (const juce::File& file)
    {
        juce::MidiFile midi;
        juce::FileInputStream in (file);
        midi.readFrom (in);
        return midi;
    }

    std::string trackName (const juce::MidiMessageSequence& track)
    {
        for (int i = 0; i < track.getNumEvents(); ++i)
            if (track.getEventPointer (i)->message.isTrackNameEvent())
                return track.getEventPointer (i)->message.getTextFromTextMetaEvent().toStdString();
        return {};
    }

    void testCaptureAndMultiTrackExport()
    {
        auto s = base();
        s.chords.out = true;
        s.chords.channel = 1;
        s.engine[0].channel = 2;
        s.engine[1].step[seq::kindVelocity].fill (90);
        s.engine[1].channel = 6;
        s.engine[1].rate = 7;

        seq::CaptureRing ring;
        run (s, 8.0, 512, {}, 120.0, &ring);
        std::vector<seq::CapturedNote> notes;
        for (auto& n : ring.snapshot())
            if (n.startBeats < 8.0)   // the engine schedules a little ahead of the block it is in
                notes.push_back (n);

        std::map<int, int> perSource;
        for (auto& n : notes) ++perSource[n.source];
        CHECK (perSource.size() == 3 && perSource[0] == 6 && perSource[1] == 32 && perSource[2] == 16);   // chords, engine 1, engine 2

        const auto w = clip::chooseWindow (notes, 8);
        CHECK (w.startBeats == 0.0 && w.endBeats == 8.0);
        CHECK (clip::countInWindow (notes, clip::allSources, 8) == 54);
        CHECK (clip::countInWindow (notes, clip::engineOnly (1), 8) == 16);
        CHECK (clip::countInWindow (notes, clip::engineOnly (3), 8) == 0);

        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("EmberlinePolyTests");
        const auto master = folder.getChildFile ("master.mid");
        CHECK (clip::write (notes, clip::allSources, 8, 120.0, master) == 54);

        const auto m = readBack (master);
        CHECK (m.getNumTracks() == 4);   // tempo track + chords + engine 1 + engine 2
        CHECK (m.getTimeFormat() == clip::ticksPerQuarter);
        if (m.getNumTracks() == 4)
        {
            CHECK (trackName (*m.getTrack (1)) == "Chords" && trackName (*m.getTrack (2)) == "Engine 1" && trackName (*m.getTrack (3)) == "Engine 2");

            const int expectChannel[] { 1, 2, 6 };
            for (int t = 1; t <= 3; ++t)
            {
                bool channelsMatch = true;
                int noteOns = 0;
                for (int i = 0; i < m.getTrack (t)->getNumEvents(); ++i)
                {
                    const auto& msg = m.getTrack (t)->getEventPointer (i)->message;
                    if (msg.isNoteOn())
                    {
                        ++noteOns;
                        channelsMatch = channelsMatch && msg.getChannel() == expectChannel[t - 1];
                    }
                }
                CHECK (channelsMatch && noteOns > 0);
            }
        }

        // a single source: tempo track + one track, aligned to the same window
        const auto single = folder.getChildFile ("engine2.mid");
        CHECK (clip::write (notes, clip::engineOnly (1), 8, 120.0, single) == 16);
        const auto one = readBack (single);
        CHECK (one.getNumTracks() == 2 && trackName (*one.getTrack (1)) == "Engine 2");

        const auto chordFile = folder.getChildFile ("chords.mid");
        CHECK (clip::write (notes, clip::chordsOnly, 8, 120.0, chordFile) == 6);
        const auto chordMidi = readBack (chordFile);
        CHECK (chordMidi.getNumTracks() == 2 && chordMidi.getTrack (1)->getNumEvents() >= 13);   // 6 on + 6 off + name

        // an empty selection writes nothing
        CHECK (clip::write (notes, clip::engineOnly (3), 8, 120.0, folder.getChildFile ("none.mid")) == 0);
        CHECK (clip::write ({}, clip::allSources, 8, 120.0, folder.getChildFile ("none.mid")) == 0);
        folder.deleteRecursively();

        ring.clear();
        CHECK (ring.snapshot().empty());
    }

    void testWindowAndImport()
    {
        std::vector<seq::CapturedNote> notes;
        for (int i = 0; i < 16; ++i)
            notes.push_back ({ 8.0 + i * 0.25, 0.125, 60 + i, 90 + i, 1, 1 });   // two bars in, a bar of sixteenths

        const auto w = clip::chooseWindow (notes, 8);
        CHECK (w.startBeats == 8.0 && w.endBeats == 12.0);

        std::vector<seq::CapturedNote> longRun;
        for (int i = 0; i < 40; ++i)
            longRun.push_back ({ i * 1.0, 0.5, 60, 100, 1, 1 });   // 10 bars
        const auto lw = clip::chooseWindow (longRun, 4);
        CHECK (lw.endBeats == 40.0 && lw.startBeats == 24.0);   // only the last 4 bars

        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("EmberlinePolyImport.mid");
        CHECK (clip::write (notes, clip::allSources, 8, 124.0, file) == 16);

        clip::Imported imported;
        CHECK (clip::importFile (file, imported));
        CHECK (imported.count == 16);
        CHECK (imported.pitch[0] == 0 && imported.pitch[1] == 1 && imported.pitch[15] == 15);
        CHECK (imported.velocity[0] == 90);
        CHECK (imported.gate[0] == 50);   // an eighth-beat note on a quarter-beat gap
        file.deleteFile();
    }

    bool sameEngine (const seq::EngineSettings& a, const seq::EngineSettings& b)
    {
        return a.steps == b.steps && a.rate == b.rate && a.num == b.num && a.den == b.den && a.octave == b.octave && a.arp == b.arp && a.step == b.step;
    }

    void testDice()
    {
        humanize::Rng rng (777);
        int nearEmpty = 0;
        for (int round = 0; round < 400; ++round)
            for (int role = 0; role < dice::numRoles; ++role)
            {
                seq::EngineSettings e;
                dice::randomiseEngine (rng, e, role);
                CHECK (e.steps >= 1 && e.steps <= seq::maxSteps);
                CHECK (e.rate >= 0 && e.rate < rates::count);
                CHECK (e.num >= 1 && e.num <= rates::maxRatio && e.den >= 1 && e.den <= rates::maxRatio);
                CHECK (e.step[seq::kindVelocity][0] >= 60);                    // the first step always sounds
                CHECK (e.step[seq::kindProbability][0] == 100);
                CHECK (e.step[seq::kindPitch][0] == 0);

                int active = 0;
                bool inRange = true, repeats = true;
                for (int i = 0; i < seq::maxSteps; ++i)
                {
                    const int v = e.step[seq::kindVelocity][(size_t) i], g = e.step[seq::kindGate][(size_t) i], r = e.step[seq::kindRatchet][(size_t) i];
                    inRange = inRange && v >= 0 && v <= 127 && g >= 10 && g <= 200 && r >= 1 && r <= 4
                              && std::abs (e.step[seq::kindPitch][(size_t) i]) <= 24 && e.step[seq::kindProbability][(size_t) i] >= 50;
                    if (i < e.steps)
                        active += v > 0 ? 1 : 0;
                    for (int k = 0; k < seq::numKinds; ++k)
                        repeats = repeats && e.step[(size_t) k][(size_t) i] == e.step[(size_t) k][(size_t) (i % e.steps)];
                    if (role == dice::roleBass || role == dice::rolePad)
                        inRange = inRange && r <= (role == dice::roleBass ? 2 : 1);
                }
                CHECK (inRange);
                CHECK (repeats);                                                // lanes tile across all 32 slots
                nearEmpty += active < (e.steps + 3) / 4 && active < 2 ? 1 : 0;
            }
        CHECK (nearEmpty == 0);

        // step counts avoid the engines that are left alone
        for (int round = 0; round < 200; ++round)
        {
            seq::Settings s;
            for (int e = 0; e < seq::numEngines; ++e)
                s.engine[(size_t) e].steps = 4 + e;
            dice::randomiseEngines (rng, s, 0b0101);   // roll engines 1 and 3, keep 2 and 4 (5 and 7 steps)
            CHECK (s.engine[1].steps == 5 && s.engine[3].steps == 7);
        }

        // the same seed gives the same roll, and the voicing and routing are left alone
        seq::EngineSettings a, b;
        a.channel = b.channel = 5;
        a.octave = b.octave = -1;
        a.arp = b.arp = seq::arpRoot;
        humanize::Rng r1 (99), r2 (99);
        dice::randomiseEngine (r1, a, dice::roleLead);
        dice::randomiseEngine (r2, b, dice::roleLead);
        CHECK (sameEngine (a, b));
        CHECK (a.channel == 5 && a.octave == -1 && a.arp == seq::arpRoot && ! a.mute);
    }

    void testPresets()
    {
        const auto library = presets::factory();
        CHECK (library.size() >= 10);

        std::set<juce::String> names;
        for (auto& p : library)
        {
            CHECK (names.insert (p.name).second);   // unique names
            int engines = 0;
            for (bool u : p.used)
                engines += u ? 1 : 0;
            CHECK (engines >= 1);
            CHECK (p.settings.chords.length >= 1 && p.settings.chords.length <= seq::maxChordSlots);

            // JSON round trip keeps everything the file stores
            juce::String error;
            presets::Preset back;
            CHECK (presets::fromJson (presets::toJson (p), back, error));
            CHECK (back.name == p.name && back.category == p.category && back.hasChords == p.hasChords);
            CHECK (back.used == p.used);
            CHECK (back.settings.root == p.settings.root && back.settings.scale == p.settings.scale && back.settings.borrowScale == p.settings.borrowScale);
            CHECK (back.settings.chords.source == p.settings.chords.source && back.settings.chords.length == p.settings.chords.length);
            CHECK (back.settings.chords.barsIndex == p.settings.chords.barsIndex && back.settings.chords.timing == p.settings.chords.timing);
            for (int c = 0; c < p.settings.chords.length; ++c)
                CHECK (back.settings.chords.slot[(size_t) c].degree == p.settings.chords.slot[(size_t) c].degree
                       && back.settings.chords.slot[(size_t) c].extension == p.settings.chords.slot[(size_t) c].extension);
            for (int e = 0; e < seq::numEngines; ++e)
                if (p.used[(size_t) e])
                    CHECK (sameEngine (back.settings.engine[(size_t) e], p.settings.engine[(size_t) e]));

            // and it makes sound: a used engine produces notes, a muted one never would
            auto s = p.settings;
            for (int e = 0; e < seq::numEngines; ++e)
                s.engine[(size_t) e].mute = ! p.used[(size_t) e], s.engine[(size_t) e].channel = e + 2;
            s.chords.out = false;
            const auto events = run (s, 16.0, 512);
            std::set<int> channels;
            for (auto& ev : events)
                if (ev.on)
                    channels.insert (ev.channel);
            CHECK (! channels.empty());
            for (int e = 0; e < seq::numEngines; ++e)
                CHECK ((channels.count (e + 2) != 0) == p.used[(size_t) e]);
        }

        // a preset without chords stores none, and loading it reports that
        presets::Preset noChords = library.front();
        noChords.hasChords = false;
        const auto json = presets::toJson (noChords);
        CHECK (! json.contains ("progression"));
        juce::String error;
        presets::Preset parsed;
        CHECK (presets::fromJson (json, parsed, error) && ! parsed.hasChords);

        // lanes repeat to fill all 32 slots, and values are clamped
        CHECK (presets::fromJson ("{\"format\":\"emberline-poly-preset\",\"engines\":[{\"engine\":2,\"steps\":3,\"pitch\":[0,7,99],\"velocity\":[100,0],\"rate\":\"1/8T\",\"ratio\":\"3:2\"}]}", parsed, error));
        CHECK (parsed.used[1] && ! parsed.used[0] && ! parsed.used[2] && ! parsed.used[3]);
        const auto& e = parsed.settings.engine[1];
        CHECK (e.steps == 3 && e.num == 3 && e.den == 2 && rates::names()[e.rate] == "1/8T");
        CHECK (e.step[seq::kindPitch][2] == 24 && e.step[seq::kindPitch][3] == 0 && e.step[seq::kindPitch][31] == 7);   // 99 clamps to 24; the 3-value lane wraps
        CHECK (e.step[seq::kindVelocity][1] == 0 && e.step[seq::kindVelocity][2] == 100);
        CHECK (e.step[seq::kindGate][5] == 60 && e.step[seq::kindRatchet][5] == 1);

        // things that are not presets are refused with a message
        CHECK (! presets::fromJson ("not json", parsed, error) && error.isNotEmpty());
        CHECK (! presets::fromJson ("{\"format\":\"something-else\"}", parsed, error));
        CHECK (! presets::fromJson ("{\"format\":\"emberline-poly-preset\",\"engines\":[]}", parsed, error));
    }

    void testOfflineRender()
    {
        auto s = base();
        s.chords.out = false;   // render forces chord output on
        s.chords.length = 4;
        s.chords.barsIndex = 1;   // one bar each
        for (int c = 0; c < 4; ++c)
            s.chords.slot[(size_t) c] = { c, chords::extTriad };
        s.engine[0].mute = true;   // mute and solo do not stop a part being rendered
        s.jitterMs = 20.0f;

        const auto notes = clip::render (s, 8, 4242);
        CHECK (! notes.empty());

        int chordNotes = 0, engine1 = 0;
        double lastChordStart = -1.0;
        for (auto& n : notes)
        {
            CHECK (n.startBeats >= 0.0 && n.startBeats < 32.0);   // inside the 8 bars, jitter clamped at zero
            if (n.source == 0)
            {
                ++chordNotes;
                lastChordStart = std::max (lastChordStart, n.startBeats);
                CHECK (std::fmod (n.startBeats, 4.0) == 0.0 && n.lengthBeats > 3.9);   // whole-bar chords, unhumanized
            }
            engine1 += n.source == 1 ? 1 : 0;
        }
        CHECK (chordNotes == 8 * 3);        // 8 bars, a triad each
        CHECK (lastChordStart == 28.0);     // the progression runs the full length
        CHECK (engine1 > 0);

        // the file is the whole window from beat 0, whether or not anything was played
        juce::MidiFile file;
        CHECK (clip::buildWindow (notes, clip::chordsOnly, { 0.0, 32.0 }, 120.0, file) == 24);
        CHECK (file.getNumTracks() == 2);   // tempo track + chords
        double end = 0.0;
        for (int t = 0; t < file.getNumTracks(); ++t)
            end = std::max (end, file.getTrack (t)->getEndTime());
        CHECK (end >= 31.0 * clip::ticksPerQuarter);

        // two renders with the same seed match; the chord handle never leaks engine notes
        const auto again = clip::render (s, 8, 4242);
        CHECK (again.size() == notes.size());
        CHECK (clip::buildWindow (notes, clip::engineOnly (3), { 0.0, 32.0 }, 120.0, file) == 0);   // engine 4 rests in this setup
    }

    // ------------------------------------------------------------------ lanes, late, harmony, pitch mask, note rules
    std::vector<int> notesOf (const std::vector<Ev>& ev, int channel = 2)
    {
        std::vector<int> n;
        for (auto& e : ons (ev, channel)) n.push_back (e.note);
        return n;
    }

    void testLanesAndRules()
    {
        // lane step maths: advance every `divide` grid steps, wrap at `length`, negative positions wrap the same way
        const int expectDiv2[] { 0, 0, 1, 1, 2, 2, 0, 0, 1, 1, 2, 2 };
        for (int n = 0; n < 12; ++n)
            CHECK (rates::laneStepAt (n, 2, 3) == expectDiv2[n]);
        CHECK (rates::laneStepAt (-1, 1, 3) == 2 && rates::laneStepAt (-1, 2, 3) == 2 && rates::laneStepAt (-3, 2, 3) == 1);
        CHECK (rates::laneStepAt (5, 0, 0) == 0);   // nonsense lengths and divides are treated as 1

        // lanes of different lengths drift against each other: velocity 2 (every other step rests), pitch 3
        auto s = base();
        s.chords.length = 1;
        s.engine[0].arp = seq::arpRoot;
        s.engine[0].steps = 8;
        s.engine[0].laneLength[seq::kindVelocity] = 2;
        s.engine[0].laneLength[seq::kindPitch] = 3;
        s.engine[0].step[seq::kindVelocity].fill (0);
        for (int i = 0; i < seq::maxSteps; i += 2) s.engine[0].step[seq::kindVelocity][(size_t) i] = 100;
        for (int i = 0; i < seq::maxSteps; ++i) s.engine[0].step[seq::kindPitch][(size_t) i] = (i % 3) * 2;   // 0, 2, 4 in C major: C D E
        auto notes = notesOf (run (s, 3.0, 512));
        // grid steps 0, 2, 4, 6, 8, 10 sound; pitch index is the grid step mod 3: 0, 2, 1, 0, 2, 1
        CHECK (notes == std::vector<int> ({ 60, 64, 62, 60, 64, 62 }));

        // the clock divide slows one lane: pitch advances every 2 steps while every step sounds
        s = base();
        s.engine[0].arp = seq::arpRoot;
        s.engine[0].steps = 4;
        s.engine[0].laneDivide[seq::kindPitch] = 2;
        s.engine[0].laneLength[seq::kindPitch] = 2;
        s.engine[0].step[seq::kindPitch][0] = 0;
        s.engine[0].step[seq::kindPitch][1] = 2;
        notes = notesOf (run (s, 2.0, 512));
        CHECK (notes == std::vector<int> ({ 60, 60, 62, 62, 60, 60, 62, 62 }));

        // playheads are per lane
        {
            seq::Engine e;
            e.prepare (sr);
            auto ps = base();
            ps.engine[0].steps = 4;
            ps.engine[0].laneLength[seq::kindPitch] = 3;
            ps.engine[0].laneDivide[seq::kindGate] = 2;
            juce::MidiBuffer midi;
            e.process (ps, { true, true, 0.6, 120.0 }, {}, midi, 64);   // grid step 2 (0.5 to 0.75 beats)
            CHECK (e.playheadStep (0, seq::kindVelocity) == 2);
            CHECK (e.playheadStep (0, seq::kindPitch) == 2);
            CHECK (e.playheadStep (0, seq::kindGate) == 1 % 4);   // 2 / 2 = 1
            CHECK (e.playheadStep (0, seq::kindLate) == 2);
        }

        // late moves the onset: +20 % of a 0.25 beat step is 0.05 beat, 1102.5 samples at 120 bpm
        s = base();
        s.engine[0].arp = seq::arpRoot;
        const auto flat = ons (run (s, 2.0, 512), 2);
        s.engine[0].step[seq::kindLate].fill (20);
        const auto late = ons (run (s, 2.0, 512), 2);
        s.engine[0].step[seq::kindLate].fill (-20);
        const auto early = ons (run (s, 2.0, 512), 2);
        CHECK (flat.size() >= 7 && std::abs ((int) flat.size() - (int) late.size()) <= 1 && std::abs ((int) flat.size() - (int) early.size()) <= 1);   // the run's end cuts the last note
        for (size_t i = 1; i < std::min ({ flat.size(), late.size(), early.size() }); ++i)   // step 0 cannot go before the start
        {
            CHECK (std::abs (late[i].sample - flat[i].sample - 1102) <= 2);
            CHECK (std::abs (flat[i].sample - early[i].sample - 1102) <= 2);
        }

        // harmony adds a second note on the step, at the same time, snapped to the scale
        s = base();
        s.engine[0].arp = seq::arpRoot;
        s.engine[0].step[seq::kindHarmony].fill (7);
        const auto harm = ons (run (s, 1.0, 512), 2);
        CHECK (harm.size() == 8 && harm[0].sample == harm[1].sample);
        CHECK (harm[0].note == 60 && harm[1].note == 67);

        // pitch applies only to the chord notes in the mask: +12 on the lowest note of the triad
        s = base();
        s.engine[0].arp = seq::arpChordHold;
        s.engine[0].step[seq::kindPitch].fill (12);
        auto chord = notesOf (run (s, 0.2, 512));
        std::sort (chord.begin(), chord.end());
        CHECK (chord == std::vector<int> ({ 72, 76, 79 }));
        s.engine[0].pitchMask = 0b001;
        chord = notesOf (run (s, 0.2, 512));
        std::sort (chord.begin(), chord.end());
        CHECK (chord == std::vector<int> ({ 64, 67, 72 }));

        // note rules by pitch class: block E, nudge it up (F is in the scale), or nudge G down (the guard has already run, so F# stays)
        s = base();
        s.engine[0].arp = seq::arpChordHold;
        s.noteRule[4] = 1;
        chord = notesOf (run (s, 0.2, 512));
        std::sort (chord.begin(), chord.end());
        CHECK (chord == std::vector<int> ({ 60, 67 }));
        s.noteRule[4] = 2;
        chord = notesOf (run (s, 0.2, 512));
        std::sort (chord.begin(), chord.end());
        CHECK (chord == std::vector<int> ({ 60, 65, 67 }));
        s.noteRule[4] = 0;
        s.noteRule[7] = 3;
        chord = notesOf (run (s, 0.2, 512));
        std::sort (chord.begin(), chord.end());
        CHECK (chord == std::vector<int> ({ 60, 64, 66 }));
        for (auto& r : s.noteRule) r = 1;   // everything blocked: silence, and no crash
        CHECK (ons (run (s, 1.0, 512)).empty());

        // presets keep the new fields
        presets::Preset p;
        p.name = "lanes";
        p.settings = base();
        p.settings.engine[0].steps = 6;
        p.settings.engine[0].laneLength[seq::kindPitch] = 3;
        p.settings.engine[0].laneDivide[seq::kindGate] = 2;
        p.settings.engine[0].pitchMask = 0b101;
        for (int i = 0; i < seq::maxSteps; ++i)
        {
            p.settings.engine[0].step[seq::kindPitch][(size_t) i] = (i % 3) * 3;
            p.settings.engine[0].step[seq::kindLate][(size_t) i] = i % 2 == 0 ? 10 : -10;
            p.settings.engine[0].step[seq::kindHarmony][(size_t) i] = i % 3 == 0 ? 7 : 0;
        }
        p.settings.noteRule[1] = 1;
        p.settings.noteRule[6] = 2;
        p.used = { true, false, false, false };
        juce::String error;
        presets::Preset back;
        CHECK (presets::fromJson (presets::toJson (p), back, error));
        const auto& b = back.settings.engine[0];
        CHECK (b.steps == 6 && b.laneLength[seq::kindPitch] == 3 && b.divideOf (seq::kindGate) == 2 && b.divideOf (seq::kindPitch) == 1 && b.pitchMask == 0b101);
        CHECK (b.step == p.settings.engine[0].step);
        CHECK (back.settings.noteRule[1] == 1 && back.settings.noteRule[6] == 2 && back.settings.noteRule[0] == 0);
        const auto json = presets::toJson (presets::factory().front());
        CHECK (! json.contains ("late") && ! json.contains ("note_rules") && ! json.contains ("lane_length"));   // defaults stay out of the file
    }

    void testPatternText()
    {
        seq::EngineSettings e;
        e.steps = 11;
        e.rate = 9;
        e.num = 3;
        e.den = 2;
        e.pitchMask = 0b1011;
        e.laneLength[seq::kindPitch] = 5;
        e.laneDivide[seq::kindGate] = 4;
        for (int k = 0; k < seq::numKinds; ++k)
            for (int i = 0; i < seq::maxSteps; ++i)
                e.step[(size_t) k][(size_t) i] = (k * 7 + i * 3) % 20 - 10;
        e.octave = 2;      // voicing and routing are not part of a slot
        e.channel = 9;

        seq::EngineSettings back;
        back.octave = -1;
        back.channel = 3;
        CHECK (patterns::decode (patterns::encode (e), back));
        CHECK (back.steps == 11 && back.rate == 9 && back.num == 3 && back.den == 2 && back.pitchMask == 0b1011);
        CHECK (back.laneLength == e.laneLength && back.divideOf (seq::kindGate) == 4 && back.divideOf (seq::kindPitch) == 1);
        CHECK (back.step == e.step);
        CHECK (back.octave == -1 && back.channel == 3);
        CHECK (! patterns::decode ("1 2 3", back) && ! patterns::decode ("", back));

        CHECK (patterns::decode (juce::String ("99 99 99 99 999 ") + patterns::encode (e).fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false)
                                     .fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false), back));
        CHECK (back.steps == seq::maxSteps && back.rate == rates::count - 1 && back.num == rates::maxRatio && back.pitchMask == 255);   // out-of-range values clamp
    }

    void testChordLabels()
    {
        // A natural minor (scale 1, root A = 9): i, VI, III, VII
        CHECK (chords::label (1, 9, 0, chords::extTriad).name == "Am");
        CHECK (chords::label (1, 9, 0, chords::ext7th).name == "Am7");
        CHECK (chords::label (1, 9, 5, chords::extTriad).name == "F");
        CHECK (chords::label (1, 9, 5, chords::ext7th).name == "Fmaj7");
        CHECK (chords::label (1, 9, 2, chords::ext7th).name == "Cmaj7");
        CHECK (chords::label (1, 9, 6, chords::ext7th).name == "G7");
        CHECK (chords::label (1, 9, 6, chords::extTriad).name == "G");
        CHECK (chords::label (1, 9, 1, chords::extTriad).name == "Bdim");
        CHECK (chords::label (1, 9, 0, chords::ext9th).name == "Am9");
        CHECK (chords::label (1, 9, 5, chords::ext9th).name == "Fmaj9");
        CHECK (chords::label (1, 9, 0, chords::extSus4).name == "Asus4");
        CHECK (chords::label (1, 9, 0, chords::extSus2).name == "Asus2");
        CHECK (chords::label (1, 9, 0, chords::ext7sus4).name == "A7sus4");
        CHECK (chords::label (0, 0, 0, chords::extTriad).name == "C" && chords::label (0, 0, 4, chords::ext7th).name == "G7");   // C major: I, V7

        // flat keys use flats: C minor (Eb major's relative) and F major; sharp keys keep sharps
        CHECK (chords::label (1, 0, 5, chords::extTriad).name == "Ab" && chords::label (1, 0, 6, chords::extTriad).name == "Bb" && chords::label (1, 0, 2, chords::ext7th).name == "Ebmaj7");
        CHECK (chords::label (0, 5, 3, chords::extTriad).name == "Bb");   // F major: IV is Bb
        CHECK (chords::label (1, 4, 5, chords::extTriad).name == "C" && chords::label (0, 7, 3, chords::extTriad).name == "C");   // E minor and G major: no accidentals
        CHECK (chords::label (0, 2, 2, chords::extTriad).name == "F#m");   // D major: iii is F#m, a sharp key

        CHECK (chords::label (1, 9, 0, chords::extTriad).numeral == "i");
        CHECK (chords::label (1, 9, 0, chords::ext7th).numeral == "i7");
        CHECK (chords::label (1, 9, 5, chords::ext7th).numeral == "VIM7");
        CHECK (chords::label (1, 9, 6, chords::extTriad).numeral == "VII");
        CHECK (chords::label (1, 9, 1, chords::extTriad).numeral == juce::String ("ii") + juce::String::charToString ((juce::juce_wchar) 0x00b0));
        CHECK (chords::label (1, 9, 0, chords::extSus4).numeral == "Isus4");
    }
}

int main()
{
    testRatesAndScales();
    testHumanizeMath();
    testChordVoicing();
    testSuggestProgressions();
    testGridAndArpModes();
    testLiveBuffer();
    testPitchOctaveAndGuard();
    testPolyrhythmAcrossEngines();
    testPolymeterAcrossEngines();
    testMuteSoloAndChannels();
    testRatchetProbabilityAndRests();
    testBlockSizeIndependence();
    testChordOutputAndTiming();
    testHumanizedTimingStaysOnClock();
    testOverlapAndRetrigger();
    testTransportStopAndJump();
    testInternalClockAndPlayhead();
    testCaptureAndMultiTrackExport();
    testWindowAndImport();
    testDice();
    testPresets();
    testOfflineRender();
    testLanesAndRules();
    testPatternText();
    testChordLabels();

    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
