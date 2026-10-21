#include "PatternEditor.h"

namespace ember::ui
{
    namespace
    {
        const juce::Colour laneBg { 0xff0a0e15 }, laneEdge { 0xff1b2536 }, textDim { 0xff8d9bb0 };

        const char* laneNames[seq::numKinds] { "PITCH", "VELOCITY", "GATE", "PROBABILITY", "RATCHET", "LATE", "HARMONY" };

        juce::String signedText (int v) { return v > 0 ? "+" + juce::String (v) : juce::String (v); }
    }

    PatternEditor::PatternEditor (juce::AudioProcessorValueTreeState& a) : apvts (a)
    {
        playheads.fill (-1);
        for (int e = 0; e < seq::numEngines; ++e)
        {
            auto& p = params[(size_t) e];
            p.steps = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (params::engineId (e, "steps")));
            for (int k = 0; k < seq::numKinds; ++k)
            {
                p.length[(size_t) k] = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (params::laneLengthId (e, k)));
                p.divide[(size_t) k] = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (params::laneDivideId (e, k)));
                for (int i = 0; i < seq::maxSteps; ++i)
                    p.value[(size_t) k][(size_t) i] = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (params::stepId (e, k, i)));
            }
            lastVelocity[(size_t) e].fill (100);
        }
    }

    void PatternEditor::setEngine (int engine, juce::Colour accent)
    {
        engineIndex = juce::jlimit (0, seq::numEngines - 1, engine);
        colour = accent;
        page = 0;
        playheads.fill (-1);
        lastHash = 0;
        repaint();
    }

    void PatternEditor::setPlayheads (const std::array<int, seq::numKinds>& heads)
    {
        if (heads != playheads)
        {
            playheads = heads;
            repaint();
        }
    }

    // ---------------------------------------------------------------- values
    int PatternEditor::stepsValue() const noexcept
    {
        auto* p = params[(size_t) engineIndex].steps;
        return p != nullptr ? p->get() : 8;
    }

    int PatternEditor::laneLength (int kind) const noexcept
    {
        auto* p = params[(size_t) engineIndex].length[(size_t) kind];
        const int own = p != nullptr ? p->get() : 0;
        return own > 0 ? own : stepsValue();
    }

    int PatternEditor::laneDivide (int kind) const noexcept
    {
        auto* p = params[(size_t) engineIndex].divide[(size_t) kind];
        return p != nullptr ? juce::jmax (1, p->get()) : 1;
    }

    int PatternEditor::valueAt (int kind, int step) const noexcept
    {
        auto* p = params[(size_t) engineIndex].value[(size_t) kind][(size_t) juce::jlimit (0, seq::maxSteps - 1, step)];
        return p != nullptr ? p->get() : 0;
    }

    void PatternEditor::refresh()
    {
        std::uint32_t h = 2166136261u;
        auto mix = [&h] (int v) { h = (h ^ (std::uint32_t) (v + 1000)) * 16777619u; };
        mix (stepsValue());
        mix (page);
        for (int k = 0; k < seq::numKinds; ++k)
        {
            mix (laneLength (k));
            mix (laneDivide (k));
            for (int i = 0; i < seq::maxSteps; ++i)
                mix (valueAt (k, i));
        }
        if (h != lastHash)
        {
            lastHash = h;
            repaint();
        }
    }

    // ---------------------------------------------------------------- geometry
    int PatternEditor::columnAt (int x) const noexcept
    {
        return juce::jlimit (0, columns - 1, (x - labelWidth) / columnWidth());
    }

    int PatternEditor::rowAt (int y) const noexcept
    {
        if (y < rows[0].y)
            return rowNumbers;
        for (int r = 0; r < numRows; ++r)
            if (y < rows[r].y + rows[r].h + 2)
                return r;
        return numRows - 1;
    }

    juce::Rectangle<int> PatternEditor::lengthField (int kind) const noexcept
    {
        return { 6, rows[kind].y + rows[kind].h - 13, 44, 12 };
    }

    juce::Rectangle<int> PatternEditor::divideField (int kind) const noexcept
    {
        return { 54, rows[kind].y + rows[kind].h - 13, 44, 12 };
    }

    PatternEditor::Field PatternEditor::fieldAt (juce::Point<int> p, int& kind) const noexcept
    {
        for (int k = 0; k < seq::numKinds; ++k)
        {
            if (rows[k].h < 24)
                continue;
            if (lengthField (k).expanded (0, 2).contains (p)) { kind = k; return fieldLength; }
            if (divideField (k).expanded (0, 2).contains (p)) { kind = k; return fieldDivide; }
        }
        return fieldNone;
    }

    // ---------------------------------------------------------------- painting
    void PatternEditor::paint (juce::Graphics& g)
    {
        const int cw = columnWidth();
        const int steps = stepsValue();

        // step numbers and the page switch
        g.setFont (juce::FontOptions (10.0f));
        for (int c = 0; c < columns; ++c)
        {
            const int i = stepIndexOf (c);
            const bool on = i < steps;
            const bool now = playheads[(size_t) seq::kindVelocity] == i && on;
            g.setColour (now ? colour : textDim.withAlpha (on ? 1.0f : 0.4f));
            g.drawText (juce::String (i + 1), labelWidth + c * cw, 0, cw, numbersHeight, juce::Justification::centred);
        }
        g.setColour (steps > columns ? colour : textDim.withAlpha (0.6f));
        g.drawText (page == 0 ? "1-16  (17-32 >)" : "17-32  (< 1-16)", 0, 0, labelWidth - 8, numbersHeight, juce::Justification::centredRight);

        for (int k = 0; k < seq::numKinds; ++k)
            paintLane (g, k);

        // on/off pills
        {
            const auto& r = rows[rowOnOff];
            g.setColour (textDim);
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.08f));
            g.drawText ("ON / OFF", 0, r.y, labelWidth - 8, r.h, juce::Justification::centredRight);
            for (int c = 0; c < columns; ++c)
            {
                const int i = stepIndexOf (c);
                const bool active = i < laneLength (seq::kindVelocity);
                const bool on = valueAt (seq::kindVelocity, i) > 0;
                const auto pill = juce::Rectangle<float> ((float) (labelWidth + c * cw) + 5.0f, (float) r.y + 6.0f, (float) cw - 10.0f, (float) r.h - 12.0f);
                g.setColour (on ? colour.withAlpha (active ? 1.0f : 0.25f) : juce::Colour (0xff3b4a63).withAlpha (active ? 1.0f : 0.3f));
                if (on)
                    g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
                else
                    g.drawRoundedRectangle (pill, pill.getHeight() * 0.5f, 1.0f);
            }
        }

        // length bar: the engine's step count
        {
            const auto& r = rows[rowLength];
            g.setColour (textDim);
            g.setFont (juce::FontOptions (9.0f).withKerningFactor (0.08f));
            g.drawText ("STEPS", 0, r.y, labelWidth - 8, r.h, juce::Justification::centredRight);
            const int visible = juce::jlimit (0, columns, steps - page * columns);
            g.setColour (juce::Colour (0xff43d68a));
            g.fillRoundedRectangle ((float) labelWidth, (float) r.y + 4.0f, (float) (visible * cw), 3.0f, 1.5f);
            if (visible > 0)
                g.fillEllipse ((float) (labelWidth + visible * cw) - 4.0f, (float) r.y + 2.0f, 8.0f, 8.0f);
        }
    }

    void PatternEditor::paintLane (juce::Graphics& g, int kind)
    {
        const auto& r = rows[kind];
        const int cw = columnWidth();
        const auto grid = juce::Rectangle<int> (labelWidth, r.y, cw * columns, r.h);

        g.setColour (laneBg);
        g.fillRoundedRectangle (grid.toFloat(), 5.0f);

        const int length = laneLength (kind), divide = laneDivide (kind);
        const int head = playheads[(size_t) kind];
        const int velLength = laneLength (seq::kindVelocity);

        // label and the lane's own length / divide
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.08f));
        g.drawText (laneNames[kind], 0, r.y + 2, labelWidth - 8, 13, juce::Justification::centredRight);
        if (r.h >= 24)
        {
            const bool own = params[(size_t) engineIndex].length[(size_t) kind] != nullptr && params[(size_t) engineIndex].length[(size_t) kind]->get() > 0;
            g.setFont (juce::FontOptions (9.5f));
            g.setColour (own ? colour : textDim);
            g.drawText (own ? "len " + juce::String (length) : "len =", lengthField (kind), juce::Justification::centredLeft);
            g.setColour (divide > 1 ? colour : textDim);
            g.drawText (juce::String::charToString ((juce::juce_wchar) 0x00f7) + juce::String (divide), divideField (kind), juce::Justification::centredLeft);
        }

        if (kind == seq::kindPitch)
            paintPitch (g);

        for (int c = 0; c < columns; ++c)
        {
            const int i = stepIndexOf (c);
            const bool active = i < length;
            const bool now = active && head == i;
            const float alpha = active ? 1.0f : 0.2f;
            const int shown = active ? i : i % juce::jmax (1, length);
            const int v = valueAt (kind, shown);
            const bool rest = valueAt (seq::kindVelocity, i < velLength ? i : i % juce::jmax (1, velLength)) == 0 && kind != seq::kindVelocity;
            const auto col = juce::Rectangle<float> ((float) (labelWidth + c * cw), (float) r.y, (float) cw, (float) r.h);

            if (now)
            {
                g.setColour (colour.withAlpha (0.16f));
                g.fillRect (col);
            }
            if (c > 0)
            {
                g.setColour (laneEdge);
                g.fillRect (col.getX(), col.getY(), 1.0f, col.getHeight());
            }

            const auto inner = col.reduced (cw >= 30 ? 6.0f : 3.0f, 0.0f);
            const float top = col.getY() + 2.0f, bottom = col.getBottom() - 2.0f, span = bottom - top;
            const float restFade = rest ? 0.45f : 1.0f;

            switch (kind)
            {
                case seq::kindPitch:
                {
                    const int clamped = juce::jlimit (-12, 12, v);
                    const float rowH = (float) r.h / 25.0f;
                    const float y = (float) r.y + (12 - clamped) * rowH + rowH * 0.5f;
                    const auto note = juce::Rectangle<float> (inner.getX(), y - 5.0f, inner.getWidth(), 10.0f);
                    if (valueAt (seq::kindVelocity, shown) == 0)
                    {
                        g.setColour (colour.withAlpha (0.5f * alpha));
                        g.drawRoundedRectangle (note, 3.0f, 1.0f);
                    }
                    else
                    {
                        g.setColour (colour.withAlpha (alpha));
                        g.fillRoundedRectangle (note, 3.0f);
                        if (now)
                        {
                            g.setColour (colour.withAlpha (0.35f));
                            g.drawRoundedRectangle (note.expanded (1.5f), 4.0f, 2.0f);
                        }
                    }
                    g.setColour (juce::Colours::white.withAlpha (0.85f * alpha));
                    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
                    const float labelY = clamped > 4 ? note.getBottom() + 1.0f : note.getY() - 12.0f;
                    g.drawText ((v != clamped ? (v > 0 ? "^" : "v") : juce::String()) + signedText (v), juce::Rectangle<float> (col.getX(), labelY, col.getWidth(), 11.0f), juce::Justification::centred);
                    break;
                }
                case seq::kindVelocity:
                case seq::kindProbability:
                case seq::kindGate:
                {
                    const int lo = params::stepInfo[kind].min, hi = params::stepInfo[kind].max;
                    const float frac = (float) (v - lo) / (float) (hi - lo);
                    const float h = kind == seq::kindVelocity ? frac * span : juce::jmax (2.0f, frac * span);
                    const float a = kind == seq::kindProbability ? (0.3f + 0.7f * frac) : kind == seq::kindVelocity ? (now ? 1.0f : 0.85f) : 0.8f;
                    g.setColour (colour.withAlpha (a * alpha * (kind == seq::kindVelocity ? 1.0f : restFade)));
                    g.fillRoundedRectangle (juce::Rectangle<float> (inner.getX(), bottom - h, inner.getWidth(), juce::jmax (2.0f, h)), 3.0f);
                    break;
                }
                case seq::kindRatchet:
                {
                    g.setColour (colour.withAlpha (alpha * restFade));
                    const float spacing = juce::jmin (7.0f, ((float) cw - 8.0f) / 4.0f);
                    for (int n = 0; n < v; ++n)
                        g.fillEllipse (col.getCentreX() + ((float) n - (float) (v - 1) * 0.5f) * spacing - 2.5f, col.getCentreY() - 2.5f, 5.0f, 5.0f);
                    break;
                }
                case seq::kindLate:
                case seq::kindHarmony:
                {
                    const int range = params::stepInfo[kind].max;
                    const float mid = col.getCentreY();
                    const float h = (float) v / (float) range * (span * 0.5f);
                    g.setColour (juce::Colours::white.withAlpha (0.08f));
                    g.fillRect (inner.getX(), mid, inner.getWidth(), 1.0f);
                    if (v != 0)
                    {
                        g.setColour (colour.withAlpha (0.85f * alpha * restFade));
                        g.fillRoundedRectangle (inner.getX(), h > 0 ? mid - h : mid, inner.getWidth(), std::abs (h), 2.0f);
                        if (cw >= 26)
                        {
                            g.setColour (juce::Colours::white.withAlpha (0.8f * alpha));
                            g.setFont (juce::FontOptions (9.0f));
                            g.drawText (signedText (v), col.withHeight (11.0f).translated (0.0f, h > 0 ? span * 0.5f + 2.0f : 0.0f), juce::Justification::centred);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }

        g.setColour (laneEdge);
        g.drawRoundedRectangle (grid.toFloat().reduced (0.5f), 5.0f, 1.0f);
    }

    void PatternEditor::paintPitch (juce::Graphics& g)
    {
        const auto& r = rows[seq::kindPitch];
        const float rowH = (float) r.h / 25.0f;
        const int cw = columnWidth();
        for (int n = 0; n <= 24; ++n)
        {
            const int semis = 12 - n;
            const float y = (float) r.y + (float) n * rowH;
            if (n % 2 == 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.018f));
                g.fillRect ((float) labelWidth, y, (float) (cw * columns), rowH);
            }
            if (semis == 0 || semis == 12 || semis == -12)
            {
                g.setColour (juce::Colours::white.withAlpha (semis == 0 ? 0.16f : 0.07f));
                g.fillRect ((float) labelWidth, y + rowH * 0.5f, (float) (cw * columns), 1.0f);
            }
        }
        g.setColour (textDim);
        g.setFont (juce::FontOptions (9.0f));
        g.drawText ("+12", 8, r.y, 40, 11, juce::Justification::centredLeft);
        g.drawText ("0", 8, r.y + (int) (12.0f * rowH), 40, 11, juce::Justification::centredLeft);
        g.drawText ("-12", 8, r.y + r.h - 28, 40, 11, juce::Justification::centredLeft);
    }

    // ---------------------------------------------------------------- editing
    int PatternEditor::valueFromY (int kind, int y) const noexcept
    {
        const auto& r = rows[kind];
        const auto& info = params::stepInfo[kind];
        const float rel = juce::jlimit (0.0f, 1.0f, ((float) y - (float) r.y) / (float) r.h);
        switch (kind)
        {
            case seq::kindPitch:
                return 12 - juce::jlimit (0, 24, (int) std::floor (rel * 25.0f));
            case seq::kindRatchet:
                return 4 - juce::jlimit (0, 3, (int) std::floor (rel * 4.0f));
            case seq::kindLate:
            case seq::kindHarmony:
            {
                const float v = (0.5f - rel) * 2.0f * (float) info.max;
                return juce::jlimit (info.min, info.max, (int) std::lround (std::abs (v) < (float) info.max * 0.06f ? 0.0f : v));   // a dead zone around the middle makes zero easy to hit
            }
            default:
                return juce::jlimit (info.min, info.max, (int) std::lround ((double) info.min + (1.0 - (double) rel) * (info.max - info.min)));
        }
    }

    void PatternEditor::setInt (juce::AudioParameterInt* p, int value)
    {
        if (p != nullptr && p->get() != value)
        {
            p->beginChangeGesture();
            *p = value;
            p->endChangeGesture();
            repaint();
        }
    }

    void PatternEditor::resetStep (int kind, int step)
    {
        if (auto* p = params[(size_t) engineIndex].value[(size_t) kind][(size_t) step])
        {
            auto* ranged = static_cast<juce::RangedAudioParameter*> (p);   // getDefaultValue is private on AudioParameterInt
            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost (ranged->getDefaultValue());
            ranged->endChangeGesture();
            repaint();
        }
    }

    void PatternEditor::mouseDown (const juce::MouseEvent& e)
    {
        dragRow = -2;
        dragField = fieldNone;
        const int row = rowAt (e.y);

        if (row == rowNumbers && e.x < labelWidth)
        {
            page = 1 - page;
            repaint();
            return;
        }

        int kind = -1;
        if (const auto field = fieldAt (e.getPosition(), kind); field != fieldNone)
        {
            dragField = field;
            dragKind = kind;
            dragStartY = e.y;
            auto* p = field == fieldLength ? params[(size_t) engineIndex].length[(size_t) kind] : params[(size_t) engineIndex].divide[(size_t) kind];
            dragStartValue = p != nullptr ? p->get() : 0;
            return;
        }

        if (e.x < labelWidth || row < 0)
            return;

        dragRow = row;
        edit (e, true);
    }

    void PatternEditor::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragField != fieldNone)
        {
            const int delta = (dragStartY - e.y) / 6;   // six pixels per notch
            if (dragField == fieldLength)
                setInt (params[(size_t) engineIndex].length[(size_t) dragKind], juce::jlimit (0, seq::maxSteps, dragStartValue + delta));
            else
                setInt (params[(size_t) engineIndex].divide[(size_t) dragKind], juce::jlimit (1, params::maxLaneDivide, dragStartValue + delta));
            return;
        }

        if (dragRow >= 0)
            edit (e, false);
    }

    void PatternEditor::mouseUp (const juce::MouseEvent&)
    {
        dragRow = -2;
        dragField = fieldNone;
    }

    void PatternEditor::mouseDoubleClick (const juce::MouseEvent& e)
    {
        int kind = -1;
        if (const auto field = fieldAt (e.getPosition(), kind); field != fieldNone)
        {
            auto* p = field == fieldLength ? params[(size_t) engineIndex].length[(size_t) kind] : params[(size_t) engineIndex].divide[(size_t) kind];
            setInt (p, field == fieldLength ? 0 : 1);
            return;
        }

        const int row = rowAt (e.y);
        if (e.x >= labelWidth && row >= 0 && row < seq::numKinds)
        {
            const int i = stepIndexOf (columnAt (e.x));
            if (i < laneLength (row))
                resetStep (row, i);
        }
    }

    void PatternEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
    {
        int kind = -1;
        const auto field = fieldAt (e.getPosition(), kind);
        if (field == fieldNone)
            return;
        const int notch = w.deltaY > 0 ? 1 : -1;
        if (field == fieldLength)
        {
            auto* p = params[(size_t) engineIndex].length[(size_t) kind];
            setInt (p, juce::jlimit (0, seq::maxSteps, (p->get() == 0 ? stepsValue() : p->get()) + notch));
        }
        else
            setInt (params[(size_t) engineIndex].divide[(size_t) kind], juce::jlimit (1, params::maxLaneDivide, laneDivide (kind) + notch));
    }

    void PatternEditor::edit (const juce::MouseEvent& e, bool pressed)
    {
        const int row = dragRow;
        const int c = columnAt (e.x);
        const int i = stepIndexOf (c);
        auto& ep = params[(size_t) engineIndex];

        if (row == rowLength)
        {
            setInt (ep.steps, juce::jlimit (1, seq::maxSteps, i + 1));
            return;
        }

        if (row == rowOnOff)
        {
            if (i >= laneLength (seq::kindVelocity))
                return;
            const int v = valueAt (seq::kindVelocity, i);
            if (pressed)
                pillTarget = v == 0;   // dragging across the pills sets them all to what the first one turned into
            if ((v > 0) == pillTarget)
                return;
            if (v > 0)
                lastVelocity[(size_t) engineIndex][(size_t) i] = v;
            setInt (ep.value[seq::kindVelocity][(size_t) i], pillTarget ? juce::jmax (1, lastVelocity[(size_t) engineIndex][(size_t) i]) : 0);
            return;
        }

        if (row < 0 || row >= seq::numKinds || i >= laneLength (row))
            return;

        if (pressed && (e.mods.isRightButtonDown() || e.mods.isAltDown()))
        {
            resetStep (row, i);
            dragRow = -2;
            return;
        }

        const int v = valueFromY (row, e.y);
        if (row == seq::kindVelocity && v > 0)
            lastVelocity[(size_t) engineIndex][(size_t) i] = v;
        setInt (ep.value[(size_t) row][(size_t) i], v);
    }
}
