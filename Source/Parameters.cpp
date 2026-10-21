#include "Parameters.h"
#include "Chords.h"
#include "Scales.h"

namespace ember::params
{
    namespace
    {
        using juce::ParameterID;
        constexpr int version = 2;

        // One starting motif per engine, repeated to fill 32 steps. The lengths differ (8, 7, 3, 5) so the polymeter is audible at once.
        struct Motif { std::vector<int> pitch, velocity, gate, probability, ratchet; };

        const Motif& motif (int engine)
        {
            static const Motif motifs[seq::numEngines] {
                { { 0, 0, 0, 7, 0, 0, 5, 0 },  { 112, 0, 96, 100, 0, 90, 100, 80 },  { 80, 60, 60, 70, 60, 80, 60, 50 },  { 100 },                          { 1 } },
                { { 0, 12, 0, 0, 7, 0, 12 },   { 100, 70, 85, 70, 95, 65, 80 },      { 55, 40, 60, 45, 70, 40, 50 },      { 100, 90, 100, 80, 100, 70, 100 }, { 1, 1, 1, 2, 1, 1, 1 } },
                { { 0 },                       { 90, 70, 80 },                       { 180, 150, 190 },                   { 100 },                          { 1 } },
                { { 0, 0, 12, 0, 7 },          { 105, 0, 80, 0, 90 },                { 40, 40, 50, 40, 60 },              { 80, 100, 70, 100, 60 },         { 1, 1, 3, 1, 2 } },
            };
            return motifs[engine];
        }

        int defaultStep (int engine, int kind, int step)
        {
            const auto& m = motif (engine);
            const std::vector<int>* tables[] { &m.pitch, &m.velocity, &m.gate, &m.probability, &m.ratchet };
            const auto& v = *tables[kind];
            return v[(size_t) step % v.size()];
        }

        struct EngineDefaults { int steps, rate, octaveIndex, arp, channelIndex; };
        constexpr EngineDefaults engineDefaults[seq::numEngines] {
            { 8, 7,  0, seq::arpRoot,      1 },   // sub-bass: 1/8, two octaves down
            { 7, 10, 2, seq::arpCyclic,    2 },   // pluck lead: 1/16
            { 3, 1,  2, seq::arpChordHold, 3 },   // pad stabs: 1/2
            { 5, 9,  3, seq::arpLowHigh,   4 },   // accent motif: 1/8T, an octave up
        };

        // i - VI - III - VII, with a 7th on the tonic
        juce::StringArray progressionLengthNames()
        {
            juce::StringArray n;
            for (int i = 1; i <= seq::maxChordSlots; ++i)
                n.add (juce::String (i));
            return n;
        }

        constexpr int defaultDegree[seq::maxChordSlots] { 0, 5, 2, 6, 0, 3, 4, 6 };
        constexpr int defaultExtension[seq::maxChordSlots] { 1, 0, 0, 0, 1, 0, 0, 0 };
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        using Int = juce::AudioParameterInt;
        using Choice = juce::AudioParameterChoice;
        using Float = juce::AudioParameterFloat;
        using Bool = juce::AudioParameterBool;

        for (int e = 0; e < seq::numEngines; ++e)
        {
            const auto prefix = "Engine " + juce::String (e + 1) + " ";
            const auto& d = engineDefaults[e];
            layout.add (std::make_unique<Int> (ParameterID { engineId (e, "steps"), version }, prefix + "Steps", 1, seq::maxSteps, d.steps));
            layout.add (std::make_unique<Choice> (ParameterID { engineId (e, "rate"), version }, prefix + "Rate", rates::names(), d.rate));
            layout.add (std::make_unique<Choice> (ParameterID { engineId (e, "num"), version }, prefix + "Ratio Numerator", rates::ratioNames(), 0));
            layout.add (std::make_unique<Choice> (ParameterID { engineId (e, "den"), version }, prefix + "Ratio Denominator", rates::ratioNames(), 0));
            layout.add (std::make_unique<Choice> (ParameterID { engineId (e, "octave"), version }, prefix + "Octave", octaveShiftNames, d.octaveIndex));
            layout.add (std::make_unique<Bool> (ParameterID { engineId (e, "mute"), version }, prefix + "Mute", false));
            layout.add (std::make_unique<Bool> (ParameterID { engineId (e, "solo"), version }, prefix + "Solo", false));
            layout.add (std::make_unique<Choice> (ParameterID { engineId (e, "arp"), version }, prefix + "Arp Mode", arpNames, d.arp));
            layout.add (std::make_unique<Choice> (ParameterID { engineId (e, "channel"), version }, prefix + "MIDI Channel", rates::ratioNames(), d.channelIndex));

            for (int k = 0; k < seq::numKinds; ++k)
                for (int i = 0; i < seq::maxSteps; ++i)
                    layout.add (std::make_unique<Int> (ParameterID { stepId (e, k, i), version },
                                                       prefix + stepInfo[k].name + " Step " + juce::String (i + 1),
                                                       stepInfo[k].min, stepInfo[k].max, defaultStep (e, k, i)));
        }

        layout.add (std::make_unique<Choice> (ParameterID { "chord_source", version }, "Chord Source", chords::sourceNames(), 0));
        layout.add (std::make_unique<Choice> (ParameterID { "chord_length", version }, "Progression Length", progressionLengthNames(), 3));
        layout.add (std::make_unique<Choice> (ParameterID { "chord_bars", version }, "Chord Length", chords::barNames(), 1));
        layout.add (std::make_unique<Choice> (ParameterID { "chord_timing", version }, "Chord Timing", chords::timingNames(), 0));
        layout.add (std::make_unique<Bool> (ParameterID { "chord_out", version }, "Chords Out", true));
        layout.add (std::make_unique<Choice> (ParameterID { "chord_channel", version }, "Chord MIDI Channel", rates::ratioNames(), 0));
        layout.add (std::make_unique<Choice> (ParameterID { "genre", version }, "Suggest Genre", chords::genreNames(), 0));
        for (int c = 0; c < seq::maxChordSlots; ++c)
        {
            layout.add (std::make_unique<Choice> (ParameterID { chordId (c, "deg"), version }, "Chord " + juce::String (c + 1) + " Degree", chords::degreeNames(), defaultDegree[c]));
            layout.add (std::make_unique<Choice> (ParameterID { chordId (c, "ext"), version }, "Chord " + juce::String (c + 1) + " Colour", chords::extensionNames(), defaultExtension[c]));
        }

        layout.add (std::make_unique<Choice> (ParameterID { "root", version }, "Root", scales::rootNames(), 0));
        layout.add (std::make_unique<Choice> (ParameterID { "scale", version }, "Scale", scales::names(), 1));
        layout.add (std::make_unique<Bool> (ParameterID { "scale_lock", version }, "Scale Lock", true));
        layout.add (std::make_unique<Choice> (ParameterID { "borrow_scale", version }, "Borrow Scale", scales::names(), 2));
        layout.add (std::make_unique<Float> (ParameterID { "borrow_amount", version }, "Borrow Amount",
                                             juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f, juce::AudioParameterFloatAttributes().withLabel ("%")));
        {
            juce::StringArray names;
            for (int i = 0; i <= 6; ++i)
                names.add ("C" + juce::String (i));   // MIDI note 60 is C3
            layout.add (std::make_unique<Choice> (ParameterID { "base_octave", version }, "Base Octave", names, 3));
        }

        layout.add (std::make_unique<Float> (ParameterID { "jitter", version }, "Jitter",
                                             juce::NormalisableRange<float> (0.0f, 25.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel ("ms")));
        layout.add (std::make_unique<Float> (ParameterID { "slip", version }, "Gate Slip",
                                             juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f, juce::AudioParameterFloatAttributes().withLabel ("%")));

        layout.add (std::make_unique<Bool> (ParameterID { "run", version }, "Run (no host)", true));
        layout.add (std::make_unique<Float> (ParameterID { "bpm", version }, "Tempo (no host)",
                                             juce::NormalisableRange<float> (40.0f, 240.0f, 0.1f), 124.0f, juce::AudioParameterFloatAttributes().withLabel ("BPM")));
        layout.add (std::make_unique<Choice> (ParameterID { "capture_bars", version }, "Drag Length", captureBarNames, 3));
        return layout;
    }

    Reader::Reader (juce::AudioProcessorValueTreeState& apvts)
    {
        auto get = [&] (const juce::String& id)
        {
            auto* p = apvts.getRawParameterValue (id);
            jassert (p != nullptr);
            return p;
        };

        for (int e = 0; e < seq::numEngines; ++e)
        {
            auto& en = engine[(size_t) e];
            en.steps = get (engineId (e, "steps"));   en.rate = get (engineId (e, "rate"));
            en.num = get (engineId (e, "num"));       en.den = get (engineId (e, "den"));
            en.octave = get (engineId (e, "octave")); en.mute = get (engineId (e, "mute"));
            en.solo = get (engineId (e, "solo"));     en.arp = get (engineId (e, "arp"));
            en.channel = get (engineId (e, "channel"));
            for (int k = 0; k < seq::numKinds; ++k)
                for (int i = 0; i < seq::maxSteps; ++i)
                    en.step[(size_t) k][(size_t) i] = get (stepId (e, k, i));
        }

        for (int c = 0; c < seq::maxChordSlots; ++c)
            slot[(size_t) c] = { get (chordId (c, "deg")), get (chordId (c, "ext")) };

        chordSource = get ("chord_source");  chordLength = get ("chord_length");  chordBars = get ("chord_bars");
        chordTiming = get ("chord_timing");  chordOut = get ("chord_out");        chordChannel = get ("chord_channel");
        root = get ("root");  baseOctave = get ("base_octave");  scale = get ("scale");  borrowScale = get ("borrow_scale");
        scaleLock = get ("scale_lock");  borrowAmount = get ("borrow_amount");  jitter = get ("jitter");  slip = get ("slip");
        run = get ("run");  bpm = get ("bpm");
    }

    void Reader::read (seq::Settings& s) const noexcept
    {
        auto i = [] (std::atomic<float>* p) { return (int) std::lround (p->load (std::memory_order_relaxed)); };
        auto f = [] (std::atomic<float>* p) { return p->load (std::memory_order_relaxed); };

        for (size_t e = 0; e < (size_t) seq::numEngines; ++e)
        {
            auto& out = s.engine[e];
            const auto& in = engine[e];
            out.steps = i (in.steps);    out.rate = i (in.rate);
            out.num = i (in.num) + 1;    out.den = i (in.den) + 1;
            out.octave = i (in.octave) - 2;
            out.mute = f (in.mute) >= 0.5f;   out.solo = f (in.solo) >= 0.5f;
            out.arp = i (in.arp);        out.channel = i (in.channel) + 1;
            for (size_t k = 0; k < (size_t) seq::numKinds; ++k)
                for (size_t n = 0; n < (size_t) seq::maxSteps; ++n)
                    out.step[k][n] = i (in.step[k][n]);
        }

        for (size_t c = 0; c < (size_t) seq::maxChordSlots; ++c)
            s.chords.slot[c] = { i (slot[c].degree), i (slot[c].extension) };

        s.chords.source = i (chordSource);   s.chords.length = i (chordLength) + 1;   s.chords.barsIndex = i (chordBars);
        s.chords.timing = i (chordTiming);   s.chords.out = f (chordOut) >= 0.5f;     s.chords.channel = i (chordChannel) + 1;

        s.root = i (root);   s.baseOctave = i (baseOctave);   s.scale = i (scale);   s.borrowScale = i (borrowScale);
        s.scaleLock = f (scaleLock) >= 0.5f;   s.borrowAmount = f (borrowAmount);
        s.jitterMs = f (jitter);   s.slipPercent = f (slip);
        s.run = f (run) >= 0.5f;   s.internalBpm = (double) f (bpm);
    }
}

namespace ember::params
{
    void setPlain (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float plainValue)
    {
        if (auto* p = apvts.getParameter (id))
        {
            const float target = p->convertTo0to1 (plainValue);
            if (std::abs (p->getValue() - target) < 1.0e-6f)
                return;
            p->beginChangeGesture();
            p->setValueNotifyingHost (target);
            p->endChangeGesture();
        }
    }

    presets::Preset capturePreset (juce::AudioProcessorValueTreeState& apvts, bool includeChords)
    {
        presets::Preset p;
        Reader (apvts).read (p.settings);
        for (size_t e = 0; e < (size_t) seq::numEngines; ++e)
            p.used[e] = ! p.settings.engine[e].mute;
        p.hasChords = includeChords;
        return p;
    }

    void applyPattern (juce::AudioProcessorValueTreeState& apvts, int e, const seq::EngineSettings& en)
    {
        setPlain (apvts, engineId (e, "steps"), (float) en.steps);
        setPlain (apvts, engineId (e, "rate"), (float) en.rate);
        setPlain (apvts, engineId (e, "num"), (float) en.num - 1.0f);
        setPlain (apvts, engineId (e, "den"), (float) en.den - 1.0f);
        for (int k = 0; k < seq::numKinds; ++k)
            for (int i = 0; i < seq::maxSteps; ++i)
                setPlain (apvts, stepId (e, k, i), (float) en.step[(size_t) k][(size_t) i]);
    }

    void applyPreset (juce::AudioProcessorValueTreeState& apvts, const presets::Preset& p, bool includeChords)
    {
        const auto& s = p.settings;
        for (int e = 0; e < seq::numEngines; ++e)
        {
            const auto& en = s.engine[(size_t) e];
            setPlain (apvts, engineId (e, "solo"), 0.0f);   // a leftover solo would silence the preset's engines
            setPlain (apvts, engineId (e, "mute"), p.used[(size_t) e] ? 0.0f : 1.0f);
            if (! p.used[(size_t) e])
                continue;
            applyPattern (apvts, e, en);
            setPlain (apvts, engineId (e, "octave"), (float) en.octave + 2.0f);
            setPlain (apvts, engineId (e, "arp"), (float) en.arp);
        }

        setPlain (apvts, "root", (float) s.root);
        setPlain (apvts, "scale", (float) s.scale);
        setPlain (apvts, "scale_lock", s.scaleLock ? 1.0f : 0.0f);
        setPlain (apvts, "borrow_scale", (float) s.borrowScale);
        setPlain (apvts, "borrow_amount", s.borrowAmount);
        setPlain (apvts, "base_octave", (float) s.baseOctave);
        setPlain (apvts, "jitter", s.jitterMs);
        setPlain (apvts, "slip", s.slipPercent);
        setPlain (apvts, "chord_source", (float) s.chords.source);

        if (includeChords && p.hasChords)
        {
            setPlain (apvts, "chord_length", (float) s.chords.length - 1.0f);
            setPlain (apvts, "chord_bars", (float) s.chords.barsIndex);
            setPlain (apvts, "chord_timing", (float) s.chords.timing);
            setPlain (apvts, "chord_out", s.chords.out ? 1.0f : 0.0f);
            for (int c = 0; c < seq::maxChordSlots; ++c)
            {
                setPlain (apvts, chordId (c, "deg"), (float) s.chords.slot[(size_t) c].degree);
                setPlain (apvts, chordId (c, "ext"), (float) s.chords.slot[(size_t) c].extension);
            }
        }
    }
}
