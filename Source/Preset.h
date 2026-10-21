#pragma once

#include "Chords.h"
#include "Rates.h"
#include "Scales.h"
#include "Settings.h"
#include <juce_core/juce_core.h>
#include <regex>

// Presets as JSON. A preset is a sound: the engines' patterns, the scale and the humanize amounts. It does not carry
// routing (MIDI channels, mute, solo), the host tempo or the export length. Engines a preset does not use are listed
// by their absence and get muted when it loads. The chord source is always stored; the chord progression is optional,
// so a preset can leave the player's own chords alone.
//
//   { "format": "emberline-poly-preset", "version": 1, "name": "...", "category": "...", "author": "...", "description": "...",
//     "scale": { "root": "A", "scale": "Natural Minor", "lock": true, "borrow_scale": "Dorian", "borrow_amount": 0, "base_octave": "C3" },
//     "humanize": { "jitter_ms": 0, "gate_slip": 0 },
//     "chord_source": "Internal",
//     "chords": { "length": 4, "each_lasts": "1 bar", "changes": "On beat", "output": true, "progression": [ { "degree": "I", "colour": "7th" }, ... ] },
//     "engines": [ { "engine": 1, "steps": 8, "rate": "1/8", "ratio": "1:1", "octave": -2, "note_pick": "Root",
//                    "pitch": [...], "velocity": [...], "gate": [...], "probability": [...], "ratchet": [...] }, ... ] }
//
// Each lane lists its first `steps` values and repeats to fill the 32 slots.
namespace ember::presets
{
    inline constexpr const char* formatTag = "emberline-poly-preset";
    inline constexpr int formatVersion = 1;

    inline const juce::StringArray& arpNames()
    {
        static const juce::StringArray n { "As Played", "Chord Hold", "Low & High", "Random", "Root" };
        return n;
    }

    inline const juce::StringArray& baseOctaveNames()
    {
        static const juce::StringArray n { "C0", "C1", "C2", "C3", "C4", "C5", "C6" };
        return n;
    }

    struct Preset
    {
        juce::String name, category, author, description;
        seq::Settings settings;
        std::array<bool, seq::numEngines> used {};   // engines the preset plays; the rest are muted on load
        bool hasChords = true;                       // the progression (length, slots, timing) is part of the preset
    };

    namespace detail
    {
        inline int indexOfName (const juce::StringArray& names, const juce::var& v, int fallback)
        {
            if (v.isString())
            {
                const int i = names.indexOf (v.toString(), true);
                return i >= 0 ? i : fallback;
            }
            if (v.isInt() || v.isInt64() || v.isDouble())
                return juce::jlimit (0, names.size() - 1, (int) v);
            return fallback;
        }

        inline int intOf (const juce::var& v, int fallback, int lo, int hi)
        {
            return v.isInt() || v.isInt64() || v.isDouble() ? juce::jlimit (lo, hi, juce::roundToInt ((double) v)) : fallback;
        }

        inline double numberOf (const juce::var& v, double fallback, double lo, double hi)
        {
            return v.isInt() || v.isInt64() || v.isDouble() ? juce::jlimit (lo, hi, (double) v) : fallback;
        }

        inline bool boolOf (const juce::var& v, bool fallback) { return v.isBool() || v.isInt() ? (bool) v : fallback; }

        inline juce::var lane (const seq::EngineSettings& e, int kind)
        {
            juce::Array<juce::var> a;
            for (int i = 0; i < e.steps; ++i)
                a.add (e.step[(size_t) kind][(size_t) i]);
            return a;
        }

        struct LaneInfo { const char* key; int min, max, fallback; };
        inline constexpr LaneInfo lanes[seq::numKinds] {
            { "pitch", -24, 24, 0 }, { "velocity", 0, 127, 100 }, { "gate", 10, 200, 60 }, { "probability", 0, 100, 100 }, { "ratchet", 1, 4, 1 },
        };

        inline void readLane (const juce::var& v, seq::EngineSettings& e, int kind)
        {
            const auto& info = lanes[kind];
            std::vector<int> values;
            if (auto* a = v.getArray())
                for (auto& x : *a)
                    values.push_back (intOf (x, info.fallback, info.min, info.max));
            if (values.empty())
                values.push_back (info.fallback);
            for (int i = 0; i < seq::maxSteps; ++i)
                e.step[(size_t) kind][(size_t) i] = values[(size_t) i % values.size()];
        }
    }

    inline juce::var toVar (const Preset& p)
    {
        using namespace detail;
        const auto& s = p.settings;
        auto* root = new juce::DynamicObject();
        juce::var result (root);
        root->setProperty ("format", formatTag);
        root->setProperty ("version", formatVersion);
        root->setProperty ("name", p.name);
        root->setProperty ("category", p.category);
        root->setProperty ("author", p.author);
        root->setProperty ("description", p.description);

        auto* scale = new juce::DynamicObject();
        scale->setProperty ("root", scales::rootNames()[s.root]);
        scale->setProperty ("scale", scales::names()[s.scale]);
        scale->setProperty ("lock", s.scaleLock);
        scale->setProperty ("borrow_scale", scales::names()[s.borrowScale]);
        scale->setProperty ("borrow_amount", juce::roundToInt (s.borrowAmount));
        scale->setProperty ("base_octave", baseOctaveNames()[s.baseOctave]);
        root->setProperty ("scale", juce::var (scale));

        auto* human = new juce::DynamicObject();
        human->setProperty ("jitter_ms", juce::roundToInt (s.jitterMs * 10.0f) / 10.0);
        human->setProperty ("gate_slip", juce::roundToInt (s.slipPercent));
        root->setProperty ("humanize", juce::var (human));

        root->setProperty ("chord_source", chords::sourceNames()[s.chords.source]);
        if (p.hasChords)
        {
            auto* c = new juce::DynamicObject();
            c->setProperty ("length", s.chords.length);
            c->setProperty ("each_lasts", chords::barNames()[s.chords.barsIndex]);
            c->setProperty ("changes", chords::timingNames()[s.chords.timing]);
            c->setProperty ("output", s.chords.out);
            juce::Array<juce::var> prog;
            for (int i = 0; i < s.chords.length; ++i)
            {
                auto* slot = new juce::DynamicObject();
                slot->setProperty ("degree", chords::degreeNames()[s.chords.slot[(size_t) i].degree]);
                slot->setProperty ("colour", chords::extensionNames()[s.chords.slot[(size_t) i].extension]);
                prog.add (juce::var (slot));
            }
            c->setProperty ("progression", prog);
            root->setProperty ("chords", juce::var (c));
        }

        juce::Array<juce::var> engines;
        for (int n = 0; n < seq::numEngines; ++n)
        {
            if (! p.used[(size_t) n])
                continue;
            const auto& e = s.engine[(size_t) n];
            auto* o = new juce::DynamicObject();
            o->setProperty ("engine", n + 1);
            o->setProperty ("steps", e.steps);
            o->setProperty ("rate", rates::names()[e.rate]);
            o->setProperty ("ratio", juce::String (e.num) + ":" + juce::String (e.den));
            o->setProperty ("octave", e.octave);
            o->setProperty ("note_pick", arpNames()[e.arp]);
            for (int k = 0; k < seq::numKinds; ++k)
                o->setProperty (lanes[k].key, lane (e, k));
            engines.add (juce::var (o));
        }
        root->setProperty ("engines", engines);
        return result;
    }

    // JUCE prints every array element on its own line; the step lanes read far better as one line each.
    inline juce::String toJson (const Preset& p)
    {
        static const std::regex numberArray (R"(\[\s*(-?\d+(?:\s*,\s*-?\d+)*)\s*\])");
        const auto pretty = juce::JSON::toString (toVar (p), false).toStdString();

        std::string out;
        auto last = pretty.cbegin();
        for (std::sregex_iterator it (pretty.begin(), pretty.end(), numberArray), end; it != end; ++it)
        {
            out.append (last, pretty.cbegin() + it->position());
            std::string body = (*it)[1].str(), compact;
            for (char c : body)
                if (c != ' ' && c != '\n' && c != '\r' && c != '\t')
                    compact += c == ',' ? std::string (", ") : std::string (1, c);
            out += "[" + compact + "]";
            last = pretty.cbegin() + it->position() + it->length();
        }
        out.append (last, pretty.cend());
        return juce::String (out);
    }

    // Reads a preset leniently: missing fields keep their defaults and out-of-range values are clamped. Fails only when the
    // text is not JSON, is not an Emberline Poly preset, or lists no engine.
    inline bool fromVar (const juce::var& v, Preset& out, juce::String& error)
    {
        using namespace detail;
        if (! v.isObject() || v.getProperty ("format", {}).toString() != formatTag)
        {
            error = "This is not an Emberline Poly preset.";
            return false;
        }

        Preset p;
        p.name = v.getProperty ("name", "Untitled").toString();
        p.category = v.getProperty ("category", {}).toString();
        p.author = v.getProperty ("author", {}).toString();
        p.description = v.getProperty ("description", {}).toString();
        auto& s = p.settings;

        const auto scale = v.getProperty ("scale", {});
        s.root = indexOfName (scales::rootNames(), scale.getProperty ("root", {}), s.root);
        s.scale = indexOfName (scales::names(), scale.getProperty ("scale", {}), s.scale);
        s.scaleLock = boolOf (scale.getProperty ("lock", {}), s.scaleLock);
        s.borrowScale = indexOfName (scales::names(), scale.getProperty ("borrow_scale", {}), s.borrowScale);
        s.borrowAmount = (float) numberOf (scale.getProperty ("borrow_amount", {}), s.borrowAmount, 0.0, 100.0);
        s.baseOctave = indexOfName (baseOctaveNames(), scale.getProperty ("base_octave", {}), s.baseOctave);

        const auto human = v.getProperty ("humanize", {});
        s.jitterMs = (float) numberOf (human.getProperty ("jitter_ms", {}), 0.0, 0.0, 25.0);
        s.slipPercent = (float) numberOf (human.getProperty ("gate_slip", {}), 0.0, 0.0, 100.0);

        s.chords.source = indexOfName (chords::sourceNames(), v.getProperty ("chord_source", {}), seq::chordInternal);

        const auto c = v.getProperty ("chords", {});
        p.hasChords = c.isObject();
        if (p.hasChords)
        {
            s.chords.barsIndex = indexOfName (chords::barNames(), c.getProperty ("each_lasts", {}), s.chords.barsIndex);
            s.chords.timing = indexOfName (chords::timingNames(), c.getProperty ("changes", {}), s.chords.timing);
            s.chords.out = boolOf (c.getProperty ("output", {}), s.chords.out);
            if (auto* prog = c.getProperty ("progression", {}).getArray())
            {
                int n = 0;
                for (auto& slot : *prog)
                {
                    if (n >= seq::maxChordSlots)
                        break;
                    s.chords.slot[(size_t) n++] = { indexOfName (chords::degreeNames(), slot.getProperty ("degree", {}), 0),
                                                    indexOfName (chords::extensionNames(), slot.getProperty ("colour", {}), 0) };
                }
                s.chords.length = juce::jmax (1, n);
            }
            else
                s.chords.length = intOf (c.getProperty ("length", {}), s.chords.length, 1, seq::maxChordSlots);
        }

        int listed = 0;
        if (auto* engines = v.getProperty ("engines", {}).getArray())
        {
            int next = 0;
            for (auto& o : *engines)
            {
                const int slot = intOf (o.getProperty ("engine", {}), next + 1, 1, seq::numEngines) - 1;
                next = slot + 1;
                auto& e = s.engine[(size_t) slot];
                e.steps = intOf (o.getProperty ("steps", {}), e.steps, 1, seq::maxSteps);
                e.rate = indexOfName (rates::names(), o.getProperty ("rate", {}), e.rate);
                const auto ratio = juce::StringArray::fromTokens (o.getProperty ("ratio", "1:1").toString(), ":", "");
                e.num = ratio.size() == 2 ? juce::jlimit (1, rates::maxRatio, ratio[0].getIntValue()) : 1;
                e.den = ratio.size() == 2 ? juce::jlimit (1, rates::maxRatio, ratio[1].getIntValue()) : 1;
                e.octave = intOf (o.getProperty ("octave", {}), e.octave, -2, 2);
                e.arp = indexOfName (arpNames(), o.getProperty ("note_pick", {}), e.arp);
                for (int k = 0; k < seq::numKinds; ++k)
                    readLane (o.getProperty (lanes[k].key, {}), e, k);
                e.mute = e.solo = false;
                p.used[(size_t) slot] = true;
                ++listed;
            }
        }

        if (listed == 0)
        {
            error = "The preset has no engines.";
            return false;
        }

        out = std::move (p);
        return true;
    }

    inline bool fromJson (const juce::String& text, Preset& out, juce::String& error)
    {
        juce::var parsed;
        if (juce::JSON::parse (text, parsed).failed())
        {
            error = "The file is not valid JSON.";
            return false;
        }
        return fromVar (parsed, out, error);
    }
}
