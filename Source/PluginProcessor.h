#pragma once

#include "Capture.h"
#include "Parameters.h"
#include "SequencerEngine.h"
#include <juce_audio_processors/juce_audio_processors.h>

class EmberlinePolyProcessor final : public juce::AudioProcessor
{
public:
    EmberlinePolyProcessor();
    ~EmberlinePolyProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // For the editor: where each engine's playhead is, which chord is playing, and the notes captured for Drag-to-DAW.
    int playheadStep (int engineIndex) const noexcept { return engine.playheadStep (engineIndex); }
    int chordSlot() const noexcept { return engine.chordSlot(); }
    double currentBpm() const noexcept { return engine.lastBpm(); }
    const ember::seq::CaptureRing& capture() const noexcept { return captureRing; }
    void clearCapture() noexcept { captureRing.clear(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    ember::params::Reader reader;
    ember::seq::Settings settings;
    ember::seq::Engine engine;
    ember::seq::CaptureRing captureRing;
    juce::MidiBuffer incoming;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmberlinePolyProcessor)
};
