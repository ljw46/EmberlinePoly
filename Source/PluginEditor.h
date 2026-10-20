#pragma once

#include "EditorWidgets.h"
#include "MidiClip.h"
#include "PhoenixLogo.h"
#include "PluginProcessor.h"

class EmberlinePolyEditor final : public juce::AudioProcessorEditor,
                                  public juce::DragAndDropContainer,
                                  public juce::FileDragAndDropTarget,
                                  private juce::Timer
{
public:
    explicit EmberlinePolyEditor (EmberlinePolyProcessor&);
    ~EmberlinePolyEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHint = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHint = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    void timerCallback() override;
    int captureBarsSelected() const;
    juce::File exportCapture (unsigned sourceMask, const juce::String& label);
    void saveCaptureAs (unsigned sourceMask, const juce::String& label);
    void suggestProgression();
    void applyImport (int engine, const ember::clip::Imported&);
    void showLinksMenu();
    void setStatus (const juce::String&);

    EmberlinePolyProcessor& processor;
    EmberLookAndFeel lnf;
    juce::Image phoenix;

    // scale & chord guard, humanize, global
    ember::ui::ComboField root, scale, borrowScale;
    ember::ui::ToggleField scaleLock;
    ember::ui::Knob borrowAmount, jitter, slip;
    ember::ui::ComboField baseOctave, captureBars;
    ember::ui::ToggleField run;
    ember::ui::BarField tempo;

    // chords
    ember::ui::ComboField chordSource, genre, chordLength, chordBars, chordTiming, chordChannel;
    ember::ui::ToggleField chordOut;
    juce::TextButton suggestButton { "SUGGEST" };
    struct ChordSlotControls
    {
        ChordSlotControls (juce::AudioProcessorValueTreeState&, int slot);
        ember::ui::ComboField degree, extension;
    };
    std::deque<ChordSlotControls> chordSlots;

    // export
    std::array<ember::ui::DragButton, ember::seq::numSources + 1> dragButtons;   // chords, engines 1-4, all tracks
    juce::TextButton clearButton { "CLEAR" }, linksButton { "LINKS" };
    juce::Label status;

    // motif engines
    struct EngineControls
    {
        EngineControls (juce::AudioProcessorValueTreeState&, int engine);
        ember::ui::Knob steps;
        ember::ui::ComboField rate, num, den, octave, arp, channel;
        ember::ui::ToggleButton mute, solo;
        std::array<juce::TextButton, ember::ui::MotifGrid::numModes> modes;
        ember::ui::MotifGrid grid;
    };
    std::deque<EngineControls> engines;
    std::array<juce::Rectangle<int>, ember::seq::numEngines> engineBounds;
    std::array<juce::Rectangle<int>, 5> panels;
    juce::Rectangle<int> ratioColon[ember::seq::numEngines];

    juce::TooltipWindow tooltips { this, 600 };
    std::shared_ptr<juce::FileChooser> chooser;
    int statusTicks = 0, statusCountdown = 0;
    bool dropHint = false;
    juce::String lastCaptureText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmberlinePolyEditor)
};
