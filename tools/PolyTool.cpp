// Developer helper:  EmberlinePolyTool snapshot out.png   renders the editor (after a few beats of playback) to a PNG
//                    EmberlinePolyTool presets dir       writes the factory presets as JSON files into dir
//                    EmberlinePolyTool render out.mid     plays four bars through the processor and writes the multi-track MIDI it produced
#include "FactoryPresets.h"
#include "MidiClip.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    // Runs the processor against a fake host transport for `beats` quarter notes.
    void play (EmberlinePolyProcessor& p, double beats, double bpm = 124.0, int block = 512)
    {
        const double sr = 44100.0;
        p.setPlayConfigDetails (0, 2, sr, block);
        p.prepareToPlay (sr, block);

        struct Head final : juce::AudioPlayHead
        {
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo info;
                info.setBpm (bpm);
                info.setPpqPosition (ppq);
                info.setIsPlaying (true);
                return info;
            }
            double ppq = 0.0, bpm = 124.0;
        } head;
        head.bpm = bpm;
        p.setPlayHead (&head);

        juce::AudioBuffer<float> audio (2, block);
        juce::MidiBuffer midi;
        for (long long pos = 0; head.ppq < beats; pos += block)
        {
            head.ppq = (double) pos * bpm / 60.0 / sr;
            midi.clear();
            p.processBlock (audio, midi);
        }
        p.setPlayHead (nullptr);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 3)
    {
        std::printf ("usage: EmberlinePolyTool snapshot|render|presets <output>\n");
        return 2;
    }

    const juce::String command (argv[1]);
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    EmberlinePolyProcessor processor;

    if (command == "presets")
    {
        out.createDirectory();
        int n = 0;
        for (auto& p : ember::presets::factory())
            n += out.getChildFile (juce::File::createLegalFileName (p.name)).withFileExtension ("json").replaceWithText (ember::presets::toJson (p)) ? 1 : 0;
        std::printf ("wrote %d presets to %s\n", n, out.getFullPathName().toRawUTF8());
        return n > 0 ? 0 : 1;
    }

    if (command == "snapshot")
    {
        play (processor, 6.3);
        EmberlinePolyEditor editor (processor);
        editor.setVisible (true);
        const auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f);
        out.deleteFile();
        juce::FileOutputStream stream (out);
        juce::PNGImageFormat().writeImageToStream (image, stream);
        std::printf ("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
        return 0;
    }

    if (command == "render")
    {
        play (processor, 16.0);
        const int n = ember::clip::write (processor.capture().snapshot(), ember::clip::allSources, 8, 124.0, out);
        std::printf ("wrote %d notes to %s\n", n, out.getFullPathName().toRawUTF8());
        return n > 0 ? 0 : 1;
    }

    return 2;
}
