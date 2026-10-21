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

    // One motif engine's steps as a row of pads. Each pad shows the step's velocity as its height, its gate as its width,
    // its probability as its brightness, its ratchet count as dots and its pitch shift as a number. Only the engine's
    // active steps are drawn, stretched across the width, so engines of different lengths look as different as they sound.
    // Dragging on the pads edits whichever value the edit mode selects.
    class MotifGrid final : public juce::Component
    {
    public:
        enum Mode { editVelocity, editGate, editPitch, editProbability, editRatchet, numModes };

        MotifGrid (juce::AudioProcessorValueTreeState& apvts, int engineIndex, juce::Colour accent)
            : colour (accent), stepsValue (apvts.getRawParameterValue (params::engineId (engineIndex, "steps")))
        {
            for (int k = 0; k < seq::numKinds; ++k)
                for (int i = 0; i < seq::maxSteps; ++i)
                    values[(size_t) k][(size_t) i] = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (params::stepId (engineIndex, k, i)));
        }

        void setMode (int m) { mode = juce::jlimit (0, numModes - 1, m); repaint(); }

        void setPlayhead (int step)
        {
            if (step != playhead)
            {
                playhead = step;
                repaint();
            }
        }

        int numSteps() const noexcept { return juce::jlimit (1, seq::maxSteps, (int) std::lround (stepsValue->load())); }

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            g.setColour (colours::panelFill);
            g.fillRoundedRectangle (b, 6.0f);
            g.setColour (colour.withAlpha (0.28f));
            g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);

            const int n = numSteps();
            const float cell = b.getWidth() / (float) n;
            const float top = b.getY() + textRow, bottom = b.getBottom() - dotRow - 2.0f;
            const float span = bottom - top;

            for (int i = 0; i < n; ++i)
            {
                const auto column = juce::Rectangle<float> (b.getX() + (float) i * cell, b.getY(), cell, b.getHeight());
                const bool now = i == playhead;
                if (now)
                {
                    g.setColour (colour.withAlpha (0.14f));
                    g.fillRect (column);
                }
                else if (i % 4 == 0)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.025f));
                    g.fillRect (column);
                }

                const int velocity = valueAt (seq::kindVelocity, i), gate = valueAt (seq::kindGate, i);
                const int probability = valueAt (seq::kindProbability, i), ratchet = valueAt (seq::kindRatchet, i), pitch = valueAt (seq::kindPitch, i);

                // velocity = height, gate = width, probability = brightness
                const float h = (float) velocity / 127.0f * span;
                const float w = juce::jmax (3.0f, (cell - 4.0f) * (float) gate / 200.0f);
                const juce::Rectangle<float> bar (column.getCentreX() - w * 0.5f, bottom - h, w, juce::jmax (2.0f, h));
                const float alpha = (0.3f + 0.65f * (float) probability / 100.0f) * (now ? 1.0f : 0.72f);
                g.setColour (colour.withAlpha (alpha));
                g.fillRoundedRectangle (bar, 2.0f);
                if (now)
                {
                    g.setColour (colour.withAlpha (0.25f));
                    g.drawRoundedRectangle (bar.expanded (1.5f), 3.0f, 2.0f);
                }

                // ratchet count as dots along the bottom
                for (int r = 0; r < ratchet; ++r)
                {
                    const float spacing = juce::jmin (6.0f, (cell - 6.0f) / 4.0f);
                    const float x = column.getCentreX() + ((float) r - (float) (ratchet - 1) * 0.5f) * spacing;
                    g.setColour (mode == editRatchet ? juce::Colours::white.withAlpha (0.9f) : colour.withAlpha (0.8f));
                    g.fillEllipse (x - 2.0f, b.getBottom() - dotRow + 1.0f, 4.0f, 4.0f);
                }

                // pitch shift overlay
                if (cell >= 22.0f)
                {
                    g.setColour (juce::Colours::white.withAlpha (mode == editPitch ? 0.95f : now ? 0.8f : 0.5f));
                    g.setFont (juce::FontOptions (mode == editPitch ? 11.5f : 10.5f, mode == editPitch ? juce::Font::bold : juce::Font::plain));
                    g.drawText (pitch > 0 ? "+" + juce::String (pitch) : juce::String (pitch), column.withHeight (textRow), juce::Justification::centred);
                }

                // marker for the value being edited
                if (mode == editGate || mode == editPitch || mode == editProbability)
                {
                    const float frac = mode == editGate ? (float) (gate - params::stepInfo[seq::kindGate].min) / 190.0f
                                     : mode == editPitch ? (float) (pitch + 24) / 48.0f
                                                         : (float) probability / 100.0f;
                    g.setColour (juce::Colours::white.withAlpha (0.9f));
                    g.fillRect (juce::Rectangle<float> (column.getX() + 2.0f, bottom - frac * span - 1.0f, cell - 4.0f, 2.0f));
                }
            }
        }

        void mouseDown (const juce::MouseEvent& e) override { edit (e); }
        void mouseDrag (const juce::MouseEvent& e) override { edit (e); }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (auto* p = parameterAt (kindForMode(), e.x))
            {
                auto* ranged = static_cast<juce::RangedAudioParameter*> (p);   // getDefaultValue is private on AudioParameterInt
                ranged->beginChangeGesture();
                ranged->setValueNotifyingHost (ranged->getDefaultValue());
                ranged->endChangeGesture();
            }
        }

    private:
        static constexpr float textRow = 14.0f, dotRow = 10.0f;

        int kindForMode() const noexcept
        {
            switch (mode)
            {
                case editGate:        return seq::kindGate;
                case editPitch:       return seq::kindPitch;
                case editProbability: return seq::kindProbability;
                case editRatchet:     return seq::kindRatchet;
                default:              return seq::kindVelocity;
            }
        }

        int valueAt (int kind, int i) const { auto* p = values[(size_t) kind][(size_t) i]; return p != nullptr ? p->get() : 0; }

        juce::AudioParameterInt* parameterAt (int kind, int x) const
        {
            const int n = numSteps();
            const int i = juce::jlimit (0, n - 1, (int) (((float) x / (float) juce::jmax (1, getWidth())) * (float) n));
            return values[(size_t) kind][(size_t) i];
        }

        void edit (const juce::MouseEvent& e)
        {
            const int kind = kindForMode();
            auto* p = parameterAt (kind, e.x);
            if (p == nullptr)
                return;

            const auto& info = params::stepInfo[kind];
            const float top = textRow, bottom = (float) getHeight() - dotRow - 2.0f;
            const float up = 1.0f - juce::jlimit (0.0f, 1.0f, ((float) e.y - top) / juce::jmax (1.0f, bottom - top));
            const int v = juce::jlimit (info.min, info.max, (int) std::lround ((double) info.min + (double) up * (info.max - info.min)));
            if (p->get() != v)
            {
                p->beginChangeGesture();
                *p = v;
                p->endChangeGesture();
                repaint();
            }
        }

        juce::Colour colour;
        std::atomic<float>* stepsValue;
        std::array<std::array<juce::AudioParameterInt*, seq::maxSteps>, seq::numKinds> values {};
        int mode = editVelocity, playhead = -1;
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
