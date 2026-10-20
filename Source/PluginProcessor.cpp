#include "PluginProcessor.h"
#include "PluginEditor.h"

EmberlinePolyProcessor::EmberlinePolyProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", ember::params::createLayout()),
      reader (apvts)
{
}

bool EmberlinePolyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet().isDisabled();
}

void EmberlinePolyProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    incoming.ensureSize (2048);
}

void EmberlinePolyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();   // this plugin makes MIDI, not audio

    ember::seq::Transport transport;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
        {
            transport.bpm = pos->getBpm().orFallback (120.0);
            transport.playing = pos->getIsPlaying();

            if (auto ppq = pos->getPpqPosition())
            {
                transport.hasPosition = true;
                transport.ppq = *ppq;
            }
            else if (auto seconds = pos->getTimeInSeconds())
            {
                transport.hasPosition = true;
                transport.ppq = *seconds * transport.bpm / 60.0;
            }
        }

    reader.read (settings);

    incoming.clear();
    incoming.swapWith (midi);   // the host's MIDI is read for transpose / mod wheel; `midi` is now the empty output
    engine.process (settings, transport, incoming, midi, buffer.getNumSamples(), &captureRing);
}

juce::AudioProcessorEditor* EmberlinePolyProcessor::createEditor() { return new EmberlinePolyEditor (*this); }

void EmberlinePolyProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void EmberlinePolyProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EmberlinePolyProcessor(); }
