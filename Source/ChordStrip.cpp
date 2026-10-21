#include "ChordStrip.h"

namespace ember::ui
{
    ChordStrip::ChordStrip (juce::AudioProcessorValueTreeState& a)
        : apvts (a),
          lengthValue (a.getRawParameterValue ("chord_length")), barsValue (a.getRawParameterValue ("chord_bars")),
          scaleValue (a.getRawParameterValue ("scale")), rootValue (a.getRawParameterValue ("root"))
    {
        for (int c = 0; c < seq::maxChordSlots; ++c)
        {
            degree[(size_t) c] = a.getRawParameterValue (params::chordId (c, "deg"));
            colour[(size_t) c] = a.getRawParameterValue (params::chordId (c, "ext"));
        }
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    int ChordStrip::length() const noexcept { return juce::jlimit (1, seq::maxChordSlots, (int) std::lround (lengthValue->load()) + 1); }
    int ChordStrip::degreeAt (int slot) const noexcept { return (int) std::lround (degree[(size_t) slot]->load()); }
    int ChordStrip::colourAt (int slot) const noexcept { return (int) std::lround (colour[(size_t) slot]->load()); }

    void ChordStrip::setPlaying (int slot)
    {
        if (slot != playing)
        {
            playing = slot;
            repaint();
        }
    }

    void ChordStrip::refresh()
    {
        std::uint32_t h = 2166136261u;
        auto mix = [&h] (int v) { h = (h ^ (std::uint32_t) (v + 7)) * 16777619u; };
        mix (length());
        mix ((int) std::lround (barsValue->load()));
        mix ((int) std::lround (scaleValue->load()));
        mix ((int) std::lround (rootValue->load()));
        for (int c = 0; c < seq::maxChordSlots; ++c)
        {
            mix (degreeAt (c));
            mix (colourAt (c));
        }
        if (h != lastHash)
        {
            lastHash = h;
            repaint();
        }
    }

    juce::Rectangle<int> ChordStrip::blockBounds (int slot) const noexcept
    {
        const int n = length();
        const int addW = n < seq::maxChordSlots ? addWidth + gap : 0;
        const int w = (getWidth() - addW - gap * (n - 1)) / n;
        return { slot * (w + gap), 0, w, getHeight() };
    }

    void ChordStrip::paint (juce::Graphics& g)
    {
        const int n = length();
        const int scale = (int) std::lround (scaleValue->load()), root = (int) std::lround (rootValue->load());
        const auto bars = chords::barNames()[juce::jlimit (0, 3, (int) std::lround (barsValue->load()))];

        for (int c = 0; c < n; ++c)
        {
            const auto r = blockBounds (c).toFloat();
            const auto label = chords::label (scale, root, degreeAt (c), colourAt (c));
            const bool now = c == playing;

            g.setColour (now ? colours::cyan.withAlpha (0.12f) : juce::Colour (0xff0a0e15));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (now ? colours::cyan : juce::Colour (0xff26324a));
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, now ? 1.5f : 1.0f);

            g.setColour (juce::Colours::white.withAlpha (0.95f));
            g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
            g.drawText (label.name, r.withTrimmedBottom (18.0f), juce::Justification::centred);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.setFont (juce::FontOptions (10.0f).withKerningFactor (0.05f));
            g.drawText (label.numeral + "  -  " + bars, r.withTrimmedTop (r.getHeight() - 22.0f), juce::Justification::centred);
        }

        if (n < seq::maxChordSlots)
        {
            const auto r = juce::Rectangle<float> ((float) (getWidth() - addWidth), 0.0f, (float) addWidth, (float) getHeight());
            g.setColour (juce::Colour (0xff26324a));
            const float dash[] { 4.0f, 3.0f };
            juce::Path p;
            p.addRoundedRectangle (r.reduced (0.5f), 8.0f);
            juce::Path dashed;
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, p, dash, 2);
            g.fillPath (dashed);
            g.setColour (juce::Colour (0xff8d9bb0));
            g.setFont (juce::FontOptions (22.0f));
            g.drawText ("+", r, juce::Justification::centred);
        }
    }

    void ChordStrip::write (const std::array<int, seq::maxChordSlots>& degrees, const std::array<int, seq::maxChordSlots>& colours, int newLength)
    {
        for (int c = 0; c < seq::maxChordSlots; ++c)
        {
            params::setPlain (apvts, params::chordId (c, "deg"), (float) degrees[(size_t) c]);
            params::setPlain (apvts, params::chordId (c, "ext"), (float) colours[(size_t) c]);
        }
        params::setPlain (apvts, "chord_length", (float) juce::jlimit (1, seq::maxChordSlots, newLength) - 1.0f);
        repaint();
    }

    void ChordStrip::mouseDown (const juce::MouseEvent& e)
    {
        const int n = length();
        if (n < seq::maxChordSlots && e.x >= getWidth() - addWidth)
        {
            std::array<int, seq::maxChordSlots> d {}, x {};
            for (int c = 0; c < seq::maxChordSlots; ++c)
            {
                d[(size_t) c] = degreeAt (c);
                x[(size_t) c] = colourAt (c);
            }
            d[(size_t) n] = d[(size_t) n - 1];   // the new chord starts as a copy of the last one
            x[(size_t) n] = x[(size_t) n - 1];
            write (d, x, n + 1);
            return;
        }

        for (int c = 0; c < n; ++c)
            if (blockBounds (c).contains (e.getPosition()))
            {
                openMenu (c);
                return;
            }
    }

    void ChordStrip::openMenu (int slot)
    {
        const int n = length();
        const int scale = (int) std::lround (scaleValue->load()), root = (int) std::lround (rootValue->load());
        const int currentDegree = degreeAt (slot), currentColour = colourAt (slot);

        juce::PopupMenu degrees, colours;
        for (int d = 0; d < 7; ++d)
        {
            const auto l = chords::label (scale, root, d, currentColour);
            degrees.addItem (100 + d, chords::degreeNames()[d] + "   " + l.name, true, d == currentDegree);
        }
        for (int x = 0; x < chords::numExtensions; ++x)
            colours.addItem (200 + x, chords::extensionNames()[x] + "   " + chords::label (scale, root, currentDegree, x).name, true, x == currentColour);

        juce::PopupMenu menu;
        menu.addSubMenu ("Chord", degrees);
        menu.addSubMenu ("Colour", colours);
        menu.addSeparator();
        menu.addItem (300, "Duplicate", n < seq::maxChordSlots);
        menu.addItem (301, "Remove", n > 1);

        juce::Component::SafePointer<ChordStrip> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (blockBounds (slot))).withMinimumWidth (blockBounds (slot).getWidth()),
                            [safe, slot, n] (int choice)
        {
            if (safe == nullptr || choice == 0)
                return;

            std::array<int, seq::maxChordSlots> d {}, x {};
            for (int c = 0; c < seq::maxChordSlots; ++c)
            {
                d[(size_t) c] = safe->degreeAt (c);
                x[(size_t) c] = safe->colourAt (c);
            }

            if (choice >= 100 && choice < 107)
                d[(size_t) slot] = choice - 100;
            else if (choice >= 200 && choice < 200 + chords::numExtensions)
                x[(size_t) slot] = choice - 200;
            else if (choice == 300)
            {
                for (int c = seq::maxChordSlots - 1; c > slot; --c)
                {
                    d[(size_t) c] = d[(size_t) c - 1];
                    x[(size_t) c] = x[(size_t) c - 1];
                }
                safe->write (d, x, n + 1);
                return;
            }
            else if (choice == 301)
            {
                for (int c = slot; c < seq::maxChordSlots - 1; ++c)
                {
                    d[(size_t) c] = d[(size_t) c + 1];
                    x[(size_t) c] = x[(size_t) c + 1];
                }
                safe->write (d, x, n - 1);
                return;
            }
            safe->write (d, x, n);
        });
    }
}
