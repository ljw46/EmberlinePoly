#include "EmberLookAndFeel.h"

using namespace ember;

EmberLookAndFeel::EmberLookAndFeel()
{
    using namespace colours;
    setColour (juce::ResizableWindow::backgroundColourId, bgDeep);
    setColour (juce::Slider::rotarySliderFillColourId, cyan);
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff232b3b));
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.88f));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff080b10));
    setColour (juce::Slider::textBoxOutlineColourId, cyan.withAlpha (0.4f));
    setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.72f));
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0a0e15));
    setColour (juce::ComboBox::outlineColourId, cyan.withAlpha (0.4f));
    setColour (juce::ComboBox::textColourId, juce::Colours::white.withAlpha (0.9f));
    setColour (juce::ComboBox::arrowColourId, cyan);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff131a26));
    setColour (juce::TextButton::buttonOnColourId, cyan.withAlpha (0.14f));
    setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.58f));
    setColour (juce::TextButton::textColourOnId, cyan);
    setColour (juce::PopupMenu::backgroundColourId, bgPanel);
    setColour (juce::PopupMenu::textColourId, juce::Colours::white.withAlpha (0.9f));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cyan.withAlpha (0.25f));
    setColour (juce::Slider::thumbColourId, magenta);
    setColour (juce::ToggleButton::tickColourId, cyan);
}

void EmberLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    const auto geo = knobGeometry ({ x, y, w, h });
    const auto c = geo.centre;
    const float arcR = geo.radius - 2.0f;
    const float discR = geo.radius - 8.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);

    // flat dark disc with a faint rim light
    juce::ColourGradient disc (juce::Colour (0xff1c2331), c.x, c.y - discR, juce::Colour (0xff0a0d13), c.x, c.y + discR, false);
    g.setGradientFill (disc);
    g.fillEllipse (c.x - discR, c.y - discR, 2 * discR, 2 * discR);
    g.setColour (accent.withAlpha (0.16f));
    g.drawEllipse (c.x - discR, c.y - discR, 2 * discR, 2 * discR, 1.0f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (s.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (accent.withAlpha (0.2f));
    g.strokePath (value, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float tip = discR - 3.0f;
    g.setColour (accent.brighter (0.3f));
    g.drawLine (c.x + std::sin (angle) * tip * 0.3f, c.y - std::cos (angle) * tip * 0.3f,
                c.x + std::sin (angle) * tip,        c.y - std::cos (angle) * tip, 2.0f);
}

juce::Label* EmberLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::FontOptions (11.0f));
    l->setJustificationType (juce::Justification::centred);
    l->setMinimumHorizontalScale (0.7f);
    return l;
}

void EmberLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    const auto b = label.getLocalBounds().toFloat();
    const auto bg = label.findColour (juce::Label::backgroundColourId);
    const auto outline = label.findColour (juce::Label::outlineColourId);

    if (bg.getAlpha() > 0)
    {
        g.setColour (bg);
        g.fillRoundedRectangle (b.reduced (0.5f), 4.0f);
    }

    if (! label.isBeingEdited())
    {
        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (label.isEnabled() ? 1.0f : 0.5f));
        g.setFont (getLabelFont (label));
        g.drawText (label.getText(), label.getLocalBounds().reduced (2, 0), label.getJustificationType(), true);
    }

    if (outline.getAlpha() > 0)
    {
        g.setColour (outline);
        g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
    }
}

void EmberLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();

    g.setColour (on ? colours::cyan.withAlpha (0.13f) : juce::Colour (down ? 0xff1a2333 : over ? 0xff171f2d : 0xff131a26));
    g.fillRoundedRectangle (r, 6.0f);

    if (on)
    {
        g.setColour (colours::cyan.withAlpha (0.18f));
        g.drawRoundedRectangle (r.expanded (1.5f), 7.0f, 3.0f);
    }
    g.setColour (on ? colours::cyan : juce::Colour (0xff2b3548));
    g.drawRoundedRectangle (r, 6.0f, on ? 1.5f : 1.0f);
}

juce::Font EmberLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::Font (juce::FontOptions (12.5f, juce::Font::bold).withKerningFactor (0.05f));
}

void EmberLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

juce::Font EmberLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (12.5f));
}
