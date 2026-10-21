#pragma once

#include "EmberLookAndFeel.h"
#include "Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace ember::ui
{
    inline void drawCaption (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
    {
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.06f));
        g.drawText (text, area, juce::Justification::centredLeft);
    }

    // A rotary knob with a caption above it, bound to a parameter.
    class Knob final : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& captionText,
              juce::Colour accent, const juce::String& suffix = {}, int textBoxWidth = 56)
            : caption (captionText), attachment (apvts, paramId, slider)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxWidth, 16);
            slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
            slider.setTextValueSuffix (suffix);
            slider.setDoubleClickReturnValue (true, apvts.getParameter (paramId)->convertFrom0to1 (apvts.getParameter (paramId)->getDefaultValue()));
            addAndMakeVisible (slider);
        }

        void paint (juce::Graphics& g) override { drawCaption (g, getLocalBounds().removeFromTop (13), caption); }
        void resized() override { slider.setBounds (getLocalBounds().withTrimmedTop (12)); }
        void setTooltip (const juce::String& text) { slider.setTooltip (text); }

        juce::Slider slider;

    private:
        juce::String caption;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };

    // A drop-down with a caption above it, bound to a choice parameter.
    class ComboField final : public juce::Component
    {
    public:
        ComboField (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& captionText)
            : caption (captionText)
        {
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramId)))
                box.addItemList (choice->choices, 1);
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramId, box);
            addAndMakeVisible (box);
        }

        void paint (juce::Graphics& g) override
        {
            drawCaption (g, getLocalBounds().removeFromTop (12), caption);
            if (highlighted)
            {
                g.setColour (colours::cyan.withAlpha (0.9f));
                g.fillEllipse ((float) getWidth() - 9.0f, 2.5f, 6.0f, 6.0f);
            }
        }

        void resized() override { box.setBounds (getLocalBounds().withTrimmedTop (13).withHeight (26)); }
        void setTooltip (const juce::String& text) { box.setTooltip (text); }

        // A lit dot beside the caption, used to show which chord is playing.
        void setHighlight (bool on)
        {
            if (on != highlighted)
            {
                highlighted = on;
                repaint();
            }
        }

        juce::ComboBox box;

    private:
        juce::String caption;
        bool highlighted = false;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    // An on/off button bound to a bool parameter, lined up with a ComboField beside it.
    class ToggleField final : public juce::Component
    {
    public:
        ToggleField (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& captionText,
                     const juce::String& buttonText)
            : caption (captionText), attachment (apvts, paramId, button)
        {
            button.setButtonText (buttonText);
            button.setClickingTogglesState (true);
            addAndMakeVisible (button);
        }

        void paint (juce::Graphics& g) override { drawCaption (g, getLocalBounds().removeFromTop (12), caption); }
        void resized() override { button.setBounds (getLocalBounds().withTrimmedTop (13).withHeight (26)); }
        void setTooltip (const juce::String& text) { button.setTooltip (text); }
        void setCaption (const juce::String& text) { if (text != caption) { caption = text; repaint(); } }

        juce::TextButton button;

    private:
        juce::String caption;
        juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
    };

    // A horizontal value bar with a caption above it, bound to a float parameter.
    class BarField final : public juce::Component
    {
    public:
        BarField (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& captionText,
                  const juce::String& suffix)
            : caption (captionText), attachment (apvts, paramId, slider)
        {
            slider.setSliderStyle (juce::Slider::LinearBar);
            slider.setTextBoxStyle (juce::Slider::TextBoxLeft, true, 60, 20);
            slider.setTextValueSuffix (suffix);
            slider.setColour (juce::Slider::trackColourId, colours::steel.withAlpha (0.35f));
            slider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff0a0e15));
            slider.setColour (juce::Slider::textBoxOutlineColourId, colours::steel.withAlpha (0.4f));
            slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.9f));
            addAndMakeVisible (slider);
        }

        void paint (juce::Graphics& g) override { drawCaption (g, getLocalBounds().removeFromTop (12), caption); }
        void resized() override { slider.setBounds (getLocalBounds().withTrimmedTop (13).withHeight (26)); }
        void setTooltip (const juce::String& text) { slider.setTooltip (text); }
        void setCaption (const juce::String& text) { if (text != caption) { caption = text; repaint(); } }

        juce::Slider slider;

    private:
        juce::String caption;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };

    // A small on/off button (mute, solo) bound to a bool parameter.
    class ToggleButton final : public juce::Component
    {
    public:
        ToggleButton (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& text, juce::Colour onColour)
            : attachment (apvts, paramId, button)
        {
            button.setButtonText (text);
            button.setClickingTogglesState (true);
            button.setColour (juce::TextButton::textColourOnId, onColour);
            button.setColour (juce::TextButton::buttonOnColourId, onColour.withAlpha (0.2f));
            addAndMakeVisible (button);
        }

        void resized() override { button.setBounds (getLocalBounds()); }
        void setTooltip (const juce::String& text) { button.setTooltip (text); }

        juce::TextButton button;

    private:
        juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
    };

    // One octave of piano keys that decide what happens to a generated note of that pitch class: play it, block it, or nudge it a semitone
    // up or down (the arp's note prevention). Click cycles the rule, right-click clears it. Bound to the twelve rule_NN parameters.
    class NoteRules final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit NoteRules (juce::AudioProcessorValueTreeState& a) : apvts (a)
        {
            for (int pc = 0; pc < 12; ++pc)
                value[(size_t) pc] = a.getRawParameterValue (params::ruleId (pc));
            setTooltip ("Click a key to cycle: play, block, up a semitone, down a semitone. Right-click clears it.");
        }

        void paint (juce::Graphics& g) override
        {
            static const bool black[12] { false, true, false, true, false, false, true, false, true, false, true, false };
            static const int whiteIndex[12] { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };   // a white key's own position, or the white key to the left of a black one
            const float whiteW = (float) getWidth() / 7.0f;
            for (int pass = 0; pass < 2; ++pass)
                for (int pc = 0; pc < 12; ++pc)
                    if (black[pc] == (pass == 1))
                        paintKey (g, pc, keyBounds (pc, whiteW, black[pc], whiteIndex[pc]));
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int pc = keyAt (e.position);
            if (pc < 0)
                return;
            const int rule = (int) std::lround (value[(size_t) pc]->load());
            params::setPlain (apvts, params::ruleId (pc), e.mods.isPopupMenu() ? 0.0f : (float) ((rule + 1) % 4));
            repaint();
        }

        void refresh()
        {
            std::uint32_t h = 17;
            for (auto* v : value)
                h = h * 31u + (std::uint32_t) std::lround (v->load());
            if (h != lastHash)
            {
                lastHash = h;
                repaint();
            }
        }

    private:
        juce::Rectangle<float> keyBounds (int, float whiteW, bool isBlack, int white) const
        {
            const float h = (float) getHeight();
            if (isBlack)
                return { ((float) white + 1.0f) * whiteW - whiteW * 0.3f, 0.0f, whiteW * 0.6f, h * 0.6f };
            return { (float) white * whiteW + 1.0f, 0.0f, whiteW - 2.0f, h };
        }

        int keyAt (juce::Point<float> p) const
        {
            static const bool black[12] { false, true, false, true, false, false, true, false, true, false, true, false };
            static const int whiteIndex[12] { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };   // a white key's own position, or the white key to the left of a black one
            const float whiteW = (float) getWidth() / 7.0f;
            for (int pc = 0; pc < 12; ++pc)   // black keys sit on top, so test them first
                if (black[pc] && keyBounds (pc, whiteW, true, whiteIndex[pc]).contains (p))
                    return pc;
            for (int pc = 0; pc < 12; ++pc)
                if (! black[pc] && keyBounds (pc, whiteW, false, whiteIndex[pc]).contains (p))
                    return pc;
            return -1;
        }

        void paintKey (juce::Graphics& g, int pc, juce::Rectangle<float> r) const
        {
            static const bool black[12] { false, true, false, true, false, false, true, false, true, false, true, false };
            const int rule = (int) std::lround (value[(size_t) pc]->load());
            const bool isBlack = black[pc];
            juce::Colour fill = rule == 1 ? colours::coral : rule == 0 ? (isBlack ? juce::Colour (0xff1b2433) : juce::Colour (0xffd8e0ec))
                                                                      : colours::cyan.withMultipliedBrightness (isBlack ? 0.7f : 1.0f);
            g.setColour (fill);
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawRoundedRectangle (r, 3.0f, 1.0f);
            const char* mark[] { "", "x", "^", "v" };
            if (rule > 0)
            {
                g.setColour (juce::Colour (0xff070a10));
                g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
                g.drawText (mark[rule], r.removeFromBottom (isBlack ? r.getHeight() : 22.0f), juce::Justification::centred);
            }
        }

        juce::AudioProcessorValueTreeState& apvts;
        std::array<std::atomic<float>*, 12> value {};
        std::uint32_t lastHash = 0;
    };

    // Eight small switches, one per chord note (1 is the lowest), choosing which chord notes the pitch lane moves. Bound to an int bit mask.
    class MaskToggles final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        MaskToggles (juce::AudioProcessorValueTreeState& a, const juce::String& paramId)
            : apvts (a), id (paramId), raw (a.getRawParameterValue (paramId)) {}

        void setAccent (juce::Colour c) { accent = c; repaint(); }

        void paint (juce::Graphics& g) override
        {
            drawCaption (g, getLocalBounds().removeFromTop (12), "PITCH MOVES NOTES");
            const int mask = (int) std::lround (raw->load());
            const float w = (float) getWidth() / 8.0f;
            for (int b = 0; b < 8; ++b)
            {
                const auto r = juce::Rectangle<float> ((float) b * w + 1.5f, 15.0f, w - 3.0f, (float) getHeight() - 17.0f);
                const bool on = ((mask >> b) & 1) != 0;
                g.setColour (on ? accent : juce::Colour (0xff0a0e15));
                g.fillRoundedRectangle (r, 4.0f);
                g.setColour (on ? accent : juce::Colour (0xff26324a));
                g.drawRoundedRectangle (r, 4.0f, 1.0f);
                g.setColour (on ? juce::Colour (0xff070a10) : juce::Colour (0xff8d9bb0));
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
                g.drawText (juce::String (b + 1), r, juce::Justification::centred);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int b = juce::jlimit (0, 7, (int) ((float) e.x / ((float) getWidth() / 8.0f)));
            const int mask = (int) std::lround (raw->load()) ^ (1 << b);
            params::setPlain (apvts, id, (float) mask);
            repaint();
        }

        void refresh() { const int m = (int) std::lround (raw->load()); if (m != last) { last = m; repaint(); } }

    private:
        juce::AudioProcessorValueTreeState& apvts;
        juce::String id;
        std::atomic<float>* raw;
        juce::Colour accent = colours::cyan;
        int last = -1;
    };

    // A button you can click to save the clip, or drag straight out of the plugin into a DAW track.
    class DragButton final : public juce::TextButton
    {
    public:
        std::function<juce::File()> prepareFile;   // builds the .mid and returns it (or a non-existent File)
        std::function<void()> onPlainClick;        // click without a drag: "save as"
        std::function<void()> onNothingToDrag;
        std::function<void (const juce::File&)> onDragStarted;

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragged = false;
            TextButton::mouseDown (e);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            TextButton::mouseDrag (e);
            if (dragged || e.getDistanceFromDragStart() < 8 || ! prepareFile)
                return;

            dragged = true;
            const auto file = prepareFile();
            if (file.existsAsFile())
                {
                    if (onDragStarted)
                        onDragStarted (file);
                    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
                }
            else if (onNothingToDrag)
                onNothingToDrag();
        }

        using juce::TextButton::clicked;

        void clicked() override
        {
            if (! dragged && onPlainClick)
                onPlainClick();
        }

    private:
        bool dragged = false;
    };
}
