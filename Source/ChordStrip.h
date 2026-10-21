#pragma once

#include "Chords.h"
#include "EmberLookAndFeel.h"
#include "Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace ember::ui
{
    // The chord progression as a row of named blocks ("Am7", "Fmaj7"...). Click a block to change its degree or colour, duplicate or remove it;
    // the + at the end adds a chord. The block that is playing is lit. It edits the same c1_deg / c1_ext / chord_length parameters as everything else.
    class ChordStrip final : public juce::Component
    {
    public:
        explicit ChordStrip (juce::AudioProcessorValueTreeState&);

        void setPlaying (int slot);
        void refresh();   // repaints when the progression, key or scale changed

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        static constexpr int addWidth = 44, gap = 6;
        int length() const noexcept;
        int degreeAt (int slot) const noexcept;
        int colourAt (int slot) const noexcept;
        juce::Rectangle<int> blockBounds (int slot) const noexcept;
        void openMenu (int slot);
        void write (const std::array<int, seq::maxChordSlots>& degrees, const std::array<int, seq::maxChordSlots>& colours, int newLength);

        juce::AudioProcessorValueTreeState& apvts;
        std::atomic<float> *lengthValue, *barsValue, *scaleValue, *rootValue;
        std::array<std::atomic<float>*, seq::maxChordSlots> degree {}, colour {};
        int playing = -1;
        std::uint32_t lastHash = 0;
    };
}
