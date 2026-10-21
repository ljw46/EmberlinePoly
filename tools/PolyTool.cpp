// Developer helper:  EmberlinePolyTool snapshot out.png   renders the editor (after a few beats of playback) to a PNG
//                    EmberlinePolyTool audition out.wav  bounces the current patterns through the built-in audition sounds
//                    EmberlinePolyTool selftest x        checks the pattern slots, preset apply and state save against a real processor
//                    EmberlinePolyTool presets dir       writes the factory presets as JSON files into dir
//                    EmberlinePolyTool render out.mid     plays four bars through the processor and writes the multi-track MIDI it produced
#include "FactoryPresets.h"
#include "MidiClip.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace
{
    // Runs the processor against a fake host transport for `beats` quarter notes.
    // Returns the loudest audio sample the plugin produced (it is silent unless AUDITION is on).
    float play (EmberlinePolyProcessor& p, double beats, double bpm = 124.0, int block = 512,
                const std::function<void (const juce::AudioBuffer<float>&)>& onBlock = {})
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
        float peak = 0.0f;
        for (long long pos = 0; head.ppq < beats; pos += block)
        {
            head.ppq = (double) pos * bpm / 60.0 / sr;
            midi.clear();
            p.processBlock (audio, midi);
            peak = std::max ({ peak, audio.getMagnitude (0, block), audio.getMagnitude (1, 0, block) });
            if (onBlock)
                onBlock (audio);
        }
        p.setPlayHead (nullptr);
        return peak;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 3)
    {
        std::printf ("usage: EmberlinePolyTool snapshot|render|presets|selftest <output>\n");
        return 2;
    }

    const juce::String command (argv[1]);
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    EmberlinePolyProcessor processor;

    if (command == "selftest")
    {
        using namespace ember;
        int failed = 0;
        auto check = [&failed] (bool ok, const char* what) { if (! ok) { ++failed; std::printf ("FAIL %s\n", what); } };
        auto value = [&] (const char* id) { return (int) std::lround (processor.apvts.getRawParameterValue (id)->load()); };

        params::PatternBank bank (processor.apvts);
        check (bank.active (0) == 0 && bank.filled (0, 0) && ! bank.filled (0, 1), "starts on A");
        const int stepsA = value ("e1_steps");

        bank.select (0, 1);   // a new slot starts as a copy
        check (bank.active (0) == 1 && bank.filled (0, 1) && value ("e1_steps") == stepsA, "B starts as a copy of A");
        params::setPlain (processor.apvts, "e1_steps", 5.0f);
        params::setPlain (processor.apvts, params::stepId (0, seq::kindPitch, 1), 7.0f);
        bank.select (0, 0);
        check (value ("e1_steps") == stepsA && value ("e1_pitch_02") != 7, "A comes back untouched");
        bank.select (0, 1);
        check (value ("e1_steps") == 5 && value ("e1_pitch_02") == 7, "B keeps its edits");
        check (bank.read (0, 0).steps == stepsA && bank.read (0, 1).steps == 5, "read() sees both slots");

        bank.copy (0, 1, 2);   // C gets B
        check (bank.filled (0, 2) && bank.read (0, 2).steps == 5, "copy to C");
        bank.copy (0, 0, 1);   // onto the active slot: the live pattern changes
        check (value ("e1_steps") == stepsA, "copy onto the active slot");
        bank.clear (0, 2);
        check (! bank.filled (0, 2), "clear");
        bank.select (0, 3);
        params::setPlain (processor.apvts, "e1_steps", 3.0f);

        // other engines are independent, and a saved project keeps every slot and which one is active
        check (bank.active (1) == 0 && ! bank.filled (1, 1), "engines are independent");
        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        EmberlinePolyProcessor restored;
        restored.setStateInformation (saved.getData(), (int) saved.getSize());
        params::PatternBank back (restored.apvts);
        check (back.active (0) == 3 && back.filled (0, 0) && back.filled (0, 1) && ! back.filled (0, 2), "state round trip");
        check (back.read (0, 0).steps == stepsA && (int) std::lround (restored.apvts.getRawParameterValue ("e1_steps")->load()) == 3, "slots restored");

        // presets: loading one writes the live pattern and mutes the engines it does not use
        const auto library = presets::factory();
        params::applyPreset (processor.apvts, library.front(), true);
        check (value ("e1_mute") == 0 && value ("e2_mute") == 1 && value ("e3_mute") == 1, "preset mutes unused engines");
        check (bank.read (0, 0).steps == stepsA && value ("e1_steps") == library.front().settings.engine[0].steps, "preset leaves other slots alone");
        const auto captured = params::capturePreset (processor.apvts, true);
        check (captured.used[0] && ! captured.used[1] && captured.settings.engine[0].steps == library.front().settings.engine[0].steps, "capture matches what was applied");

        // the audition sampler: silent by default, audible when switched on, and gone again when switched off
        {
            EmberlinePolyProcessor audible;
            check (audible.auditionLoaded(), "the embedded audition samples decode");
            check (play (audible, 8.0) < 1.0e-6f, "audio is silent while AUDITION is off");
            params::setPlain (audible.apvts, "audition_on", 1.0f);
            const float on = play (audible, 8.0);
            check (on > 0.02f && on < 1.5f, "AUDITION makes sound at a sane level");
            for (int e = 0; e < seq::numEngines; ++e)
                params::setPlain (audible.apvts, params::engineId (e, "sound"), 0.0f);
            params::setPlain (audible.apvts, "sound_chords", 0.0f);
            check (play (audible, 8.0) < 1.0e-6f, "every part set to Off is silent");
            params::setPlain (audible.apvts, params::engineId (0, "sound"), 2.0f);   // just the bass
            params::setPlain (audible.apvts, params::engineId (1, "mute"), 1.0f);
            params::setPlain (audible.apvts, params::engineId (2, "mute"), 1.0f);
            params::setPlain (audible.apvts, params::engineId (3, "mute"), 1.0f);
            params::setPlain (audible.apvts, "chord_out", 0.0f);
            check (play (audible, 8.0) > 0.02f, "one part with a sound is audible");
        }

        // the pattern editor: click and drag edits the parameters of the selected engine
        {
            EmberlinePolyEditor editor (processor);
            editor.setVisible (true);
            editor.selectEngine (1);
            auto& pe = editor.patternEditor();
            const int w = pe.getWidth();
            const int col = (w - ui::PatternEditor::labelWidth) / ui::PatternEditor::columns;
            auto event = [&] (juce::Point<int> p, bool dragged)
            {
                const auto now = juce::Time::getCurrentTime();
                return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), p.toFloat(), juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &pe, &pe, now, p.toFloat(), now, 1, dragged);
            };
            auto columnX = [&] (int c) { return ui::PatternEditor::labelWidth + c * col + col / 2; };

            params::setPlain (processor.apvts, params::engineId (1, "steps"), 7.0f);
            // velocity lane (y 144..216): the top of the lane is 127, the bottom 0
            pe.mouseDown (event ({ columnX (2), 146 }, false));
            check (value ("e2_vel_03") >= 120, "click at the top of the velocity lane");
            pe.mouseDrag (event ({ columnX (3), 214 }, true));
            check (value ("e2_vel_04") <= 4, "drag paints the next column");
            pe.mouseUp (event ({ columnX (3), 214 }, true));
            // pitch roll: the middle row is 0, rows above are positive
            pe.mouseDown (event ({ columnX (1), 20 + 2 }, false));
            check (value ("e2_pitch_02") == 12, "top of the roll is +12");
            pe.mouseUp (event ({ columnX (1), 22 }, false));
            // late lane: a dead zone in the middle makes 0 easy; the top is +50
            pe.mouseDown (event ({ columnX (0), 312 + 1 }, false));
            check (value ("e2_late_01") >= 45, "late lane top");
            pe.mouseUp (event ({ columnX (0), 313 }, false));
            // on/off pill: toggles velocity to 0 and back
            pe.mouseDown (event ({ columnX (0), 376 + 10 }, false));
            check (value ("e2_vel_01") == 0, "pill turns a step off");
            pe.mouseUp (event ({ columnX (0), 386 }, false));
            pe.mouseDown (event ({ columnX (0), 376 + 10 }, false));
            check (value ("e2_vel_01") > 0, "pill turns it back on");
            pe.mouseUp (event ({ columnX (0), 386 }, false));
            // length bar: click on column 5 sets 5 steps
            pe.mouseDown (event ({ columnX (4), 404 + 5 }, false));
            check (value ("e2_steps") == 5, "length bar sets the step count");
            pe.mouseUp (event ({ columnX (4), 409 }, false));
            // per-lane length field: drag up two notches (12px)
            pe.mouseDown (event ({ 20, 241 }, false));   // the gate lane's length field (lane y 220..248)
            pe.mouseDrag (event ({ 20, 241 - 12 }, true));
            pe.mouseUp (event ({ 20, 229 }, true));
            check (value ("e2_len_gate") == 2, "dragging the len field sets a lane length");
            pe.mouseDoubleClick (event ({ 20, 241 }, false));
            check (value ("e2_len_gate") == 0, "double-click puts the lane back on the engine's length");
            // out-of-lane clicks do nothing: step 8 is past the 5-step velocity lane
            const int before = value ("e2_vel_08");
            pe.mouseDown (event ({ columnX (7), 150 }, false));
            pe.mouseUp (event ({ columnX (7), 150 }, false));
            check (value ("e2_vel_08") == before, "columns past the lane length are not editable");
        }

        std::printf (failed == 0 ? "selftest passed\n" : "selftest FAILED\n");
        return failed == 0 ? 0 : 1;
    }

    if (command == "audition")   // bounces 16 beats of the current sound through the audition sampler to a WAV file, to hear it without a host
    {
        ember::params::setPlain (processor.apvts, "audition_on", 1.0f);
        out.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (new juce::FileOutputStream (out), 44100.0, 2, 16, {}, 0));
        if (writer == nullptr)
            return 1;
        const float peak = play (processor, 32.0, 124.0, 512, [&writer] (const juce::AudioBuffer<float>& b) { writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); });
        writer.reset();
        std::printf ("wrote %s (peak %.2f)\n", out.getFullPathName().toRawUTF8(), (double) peak);
        return peak > 0.0f ? 0 : 1;
    }

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
        // optional: snapshot <out.png> <engine 1-4> [settings]
        if (argc > 3)
            editor.selectEngine (juce::jlimit (1, 4, juce::String (argv[3]).getIntValue()) - 1);
        if (argc > 4)
            editor.showSettings (true);
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
