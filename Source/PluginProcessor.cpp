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
    player.prepare (sampleRate);
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

    // the optional audition sampler plays what was just generated, each MIDI channel with the sound of the part that sends on it
    if (settings.audition.on)
    {
        std::array<ember::audition::Route, 16> routes {};
        auto assign = [&routes] (int channel, int sound, float pan)
        {
            auto& r = routes[(size_t) juce::jlimit (1, 16, channel) - 1];
            if (r.sound == ember::audition::soundOff)   // when parts share a channel, the first one named here (chords, then engine 1 ...) decides
                r = { sound, pan };
        };
        assign (settings.chords.channel, settings.audition.chordsSound, 0.0f);
        const float pans[] { 0.0f, -0.3f, 0.3f, 0.15f };
        for (size_t e = 0; e < (size_t) ember::seq::numEngines; ++e)
            assign (settings.engine[e].channel, settings.audition.engineSound[e], pans[e]);
        player.render (buffer, midi, routes, settings.audition.level);
    }
    else if (auditionWasOn)
        player.reset();
    auditionWasOn = settings.audition.on;
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
