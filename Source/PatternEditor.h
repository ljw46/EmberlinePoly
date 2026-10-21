#pragma once

#include "EmberLookAndFeel.h"
#include "Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace ember::ui
{
    // The selected engine's pattern as a stack of lanes that share one column grid: a pitch roll, then velocity, gate, probability, ratchet,
    // late and harmony, a row of on/off pills and the length bar. Every lane has its own length and clock divide (set on its label), so the
    // highlighted column moves through each lane at its own pace, and columns past a lane's length show the repeating pattern dimmed.
    // Dragging across a lane paints its values; double-click (or right-click) puts a step back to its default.
    class PatternEditor final : public juce::Component
    {
    public:
        explicit PatternEditor (juce::AudioProcessorValueTreeState&);

        void setEngine (int engine, juce::Colour accent);
        int engine() const noexcept { return engineIndex; }

        // Step each lane is on (or -1 while stopped). Repaints only when something moved.
        void setPlayheads (const std::array<int, seq::numKinds>&);

        // Repaints if any displayed parameter changed (presets, automation, undo).
        void refresh();

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        static constexpr int columns = 16;
        static constexpr int labelWidth = 104;
        static constexpr int preferredHeight = 416;

    private:
        enum Row { rowNumbers = -1, rowPitch, rowVelocity, rowGate, rowProbability, rowRatchet, rowLate, rowHarmony, rowOnOff = seq::numKinds, rowLength, numRows };
        struct Geometry { int y, h; };
        static constexpr Geometry rows[numRows] {
            { 20, 120 }, { 144, 72 }, { 220, 28 }, { 252, 28 }, { 284, 24 }, { 312, 28 }, { 344, 28 }, { 376, 24 }, { 404, 12 },
        };
        static constexpr int numbersHeight = 16;

        int columnWidth() const noexcept { return juce::jmax (1, (getWidth() - labelWidth) / columns); }
        int columnAt (int x) const noexcept;
        int stepIndexOf (int column) const noexcept { return page * columns + column; }
        int rowAt (int y) const noexcept;
        int laneLength (int kind) const noexcept;
        int laneDivide (int kind) const noexcept;
        int valueAt (int kind, int step) const noexcept;
        int stepsValue() const noexcept;

        enum Field { fieldNone, fieldLength, fieldDivide };
        Field fieldAt (juce::Point<int> p, int& kind) const noexcept;
        juce::Rectangle<int> lengthField (int kind) const noexcept;
        juce::Rectangle<int> divideField (int kind) const noexcept;

        int valueFromY (int kind, int y) const noexcept;
        void paintLane (juce::Graphics&, int kind);
        void paintPitch (juce::Graphics&);
        void edit (const juce::MouseEvent&, bool pressed);
        void resetStep (int kind, int step);
        void setInt (juce::AudioParameterInt*, int value);

        juce::AudioProcessorValueTreeState& apvts;
        int engineIndex = 0;
        juce::Colour colour = colours::cyan;
        int page = 0;
        std::array<int, seq::numKinds> playheads {};

        struct EngineParams
        {
            juce::AudioParameterInt* steps = nullptr;
            std::array<juce::AudioParameterInt*, seq::numKinds> length {}, divide {};
            std::array<std::array<juce::AudioParameterInt*, seq::maxSteps>, seq::numKinds> value {};
        };
        std::array<EngineParams, seq::numEngines> params;
        std::array<std::array<int, seq::maxSteps>, seq::numEngines> lastVelocity {};

        int dragRow = -2, dragKind = -1, dragStartValue = 0, dragStartY = 0;
        Field dragField = fieldNone;
        bool pillTarget = false;
        std::uint32_t lastHash = 0;
    };
}
