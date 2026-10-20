// Command-line checks for the sequencer engines, chord generator, scale maths, humanization and MIDI export.
// Needs no display or audio device, so CI runs it on Linux:  EmberlinePolyTests
#include "Chords.h"
#include "MidiClip.h"
#include "SequencerEngine.h"
#include "Scales.h"
#include <juce_core/juce_core.h>
#include <cstdio>
#include <map>
#include <set>

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
        const auto a = run (s, 8.0, 512), b = run (s, 8.0, 37), c = run (s, 8.0, 4096);
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

    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
