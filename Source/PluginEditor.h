#pragma once

#include "ChordStrip.h"
#include "EditorWidgets.h"
#include "MidiClip.h"
#include "PatternEditor.h"
#include "PhoenixLogo.h"
#include "PluginProcessor.h"
#include "Preset.h"

// The editor lays everything out on a fixed 1240 x 896 canvas (the "content") and scales that canvas to whatever size the host gives the window.
class EmberlinePolyEditor final : public juce::AudioProcessorEditor,
                                  public juce::DragAndDropContainer,
                                  public juce::FileDragAndDropTarget,
                                  private juce::Timer
{
public:
    explicit EmberlinePolyEditor (EmberlinePolyProcessor&);
    ~EmberlinePolyEditor() override;

    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHint = true; content.repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHint = false; content.repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

    static constexpr int baseWidth = 1240, baseHeight = 896;

    // For the developer tool's snapshots.
    void selectEngine (int engine);
    void showSettings (bool show);
    ember::ui::PatternEditor& patternEditor() noexcept { return patterns; }

private:
    // The scaled canvas: it only forwards painting, so all the drawing code stays here with the state it needs.
    struct Content final : juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint, onPaintOver;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
        void paintOverChildren (juce::Graphics& g) override { if (onPaintOver) onPaintOver (g); }
    };

    // One slim row per engine: name, step count and rate, a miniature of the velocity lane with its own playhead, and the engine's voicing.
    struct EngineRow final : juce::Component
    {
        EngineRow (juce::AudioProcessorValueTreeState&, int engineIndex);
        void setPlayhead (int step);
        void setSelected (bool);
        void refresh();
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override { if (onSelect) onSelect(); }
        std::function<void()> onSelect;

    private:
        int index;
        juce::AudioProcessorValueTreeState& apvts;
        int playhead = -1;
        bool selected = false;
        std::uint32_t lastHash = 0;
    };

    // The controls of one engine that sit above the pattern lanes; only the selected engine's set is visible.
    struct EngineControls
    {
        EngineControls (juce::AudioProcessorValueTreeState&, int engine);
        ember::ui::BarField steps;
        ember::ui::ComboField rate, num, den, octave, arp, channel, sound;
        ember::ui::MaskToggles pitchMask;
        ember::ui::ToggleButton mute, solo;
        std::array<juce::TextButton, ember::patterns::numSlots> slots;
        juce::TextButton dice { "DICE" };
    };

    void timerCallback() override;
    void layout();
    void paintContent (juce::Graphics&);
    void paintOverContent (juce::Graphics&);
    int captureBarsSelected() const;
    int writeExport (unsigned sourceMask, const juce::File&);
    juce::File exportCapture (unsigned sourceMask, const juce::String& label);
    void saveCaptureAs (unsigned sourceMask, const juce::String& label);
    void suggestProgression();
    void applyImport (int engine, const ember::clip::Imported&);
    void showLinksMenu();
    void showPresetMenu();
    void cyclePreset (int direction);
    void loadPreset (const ember::presets::Preset&);
    void savePresetDialog();
    void rollEngine (int engine);
    void showSlotMenu (int engine, int slot);
    void toggleSettings() { showSettings (! drawer.isVisible()); }
    void updateHostDisplay();
    void updateChordSourceDisplay();
    void updateSlotButtons();
    void setStatus (const juce::String&);

    EmberlinePolyProcessor& processor;
    EmberLookAndFeel lnf;
    juce::Image phoenix;
    Content content;
    ember::params::PatternBank bank;

    // top bar
    juce::TextButton presetPrev { "<" }, presetNext { ">" }, presetName { "Presets" }, presetSave { "SAVE" }, settingsButton { "SETTINGS" };
    juce::Label hostTempo;

    // settings drawer (scale options, humanize, chord timing, note rules, internal clock)
    struct Drawer final : juce::Component
    {
        void paint (juce::Graphics&) override;
    } drawer;
    ember::ui::ComboField borrowScale, baseOctave, chordBars, chordTiming, chordChannel, chordsSound;
    ember::ui::Knob borrowAmount, jitter, slip;
    ember::ui::ToggleField chordOut, run, auditionOn;
    ember::ui::BarField auditionLevel;
    ember::ui::BarField tempo;
    ember::ui::NoteRules noteRules;
    juce::Label noteHint;
    bool lastHost = false, hostKnown = false;

    // chords
    ember::ui::ComboField root, scale, genre, captureBars;
    ember::ui::ToggleField scaleLock;
    juce::TextButton sourceInternal { "Internal" }, sourceLive { "Live MIDI" }, suggestButton { "SUGGEST" };
    ember::ui::ChordStrip chordStrip;
    juce::Label liveNote;
    int lastChordSource = -1;

    // motif engines
    std::deque<EngineRow> rows;
    std::deque<EngineControls> engines;
    ember::ui::PatternEditor patterns;
    int selected = 0;

    // export
    std::array<ember::ui::DragButton, ember::seq::numSources + 1> dragButtons;   // chords, engines 1-4, all tracks
    juce::TextButton clearButton { "CLEAR" }, linksButton { "LINKS" };
    juce::Label status;

    juce::TooltipWindow tooltips { this, 600 };
    juce::ComponentBoundsConstrainer constrainer;
    std::shared_ptr<juce::FileChooser> chooser;
    int statusTicks = 0, statusCountdown = 0;
    bool dropHint = false;
    juce::String lastCaptureText, lastPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmberlinePolyEditor)
};
