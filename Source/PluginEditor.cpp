#include "PluginEditor.h"
#include "Chords.h"
#include "FactoryPresets.h"
#include "Links.h"
#include "Randomise.h"
#include "Version.h"

using namespace ember;

namespace
{
    juce::Colour engineColour (int engine) { return colours::lane (engine); }

    const juce::Colour panelBorder { 0xff26324a }, textDim { 0xff8d9bb0 };

    void setParameter (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float plainValue)
    {
        params::setPlain (apvts, id, plainValue);
    }

    juce::File exportFolder() { return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Emberline Poly"); }

    juce::File userPresetFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Emberline").getChildFile ("Poly").getChildFile ("Presets");
    }

    // Preferences that belong to the session but are not automatable live on the state tree, so they are saved with the project.
    constexpr const char* presetNameKey = "presetName";
    constexpr const char* presetChordsKey = "presetLoadsChords";
    constexpr const char* selectedEngineKey = "uiEngine";

    constexpr int topY = 14, topH = 44;
    constexpr int chordY = 68, chordH = 84;
    constexpr int rowsY = 162, rowH = 38, rowGap = 5;
    constexpr int panelY = 339, panelH = 483;
    constexpr int exportY = 832, statusY = 870;
    constexpr int marginX = 16, contentW = 1208;
    const juce::String roles[] { "BASS", "LEAD", "PAD", "ACCENT" };
}

// ---------------------------------------------------------------- engine rows
EmberlinePolyEditor::EngineRow::EngineRow (juce::AudioProcessorValueTreeState& a, int engineIndex) : index (engineIndex), apvts (a)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void EmberlinePolyEditor::EngineRow::setPlayhead (int step)
{
    if (step != playhead)
    {
        playhead = step;
        repaint();
    }
}

void EmberlinePolyEditor::EngineRow::setSelected (bool on)
{
    if (on != selected)
    {
        selected = on;
        repaint();
    }
}

void EmberlinePolyEditor::EngineRow::refresh()
{
    std::uint32_t h = 2166136261u;
    auto mix = [&h] (float v) { h = (h ^ (std::uint32_t) ((int) v + 500)) * 16777619u; };
    for (const char* id : { "steps", "rate", "num", "den", "octave", "arp", "channel" })
        mix (apvts.getRawParameterValue (params::engineId (index, id))->load());
    for (int i = 0; i < seq::maxSteps; ++i)
        mix (apvts.getRawParameterValue (params::stepId (index, seq::kindVelocity, i))->load());
    mix (apvts.getRawParameterValue (params::laneLengthId (index, seq::kindVelocity))->load());
    if (h != lastHash)
    {
        lastHash = h;
        repaint();
    }
}

void EmberlinePolyEditor::EngineRow::paint (juce::Graphics& g)
{
    const auto colour = engineColour (index);
    auto value = [&] (const juce::String& id) { return (int) std::lround (apvts.getRawParameterValue (id)->load()); };

    const auto r = getLocalBounds().toFloat();
    g.setColour (selected ? juce::Colour (0xff17202e) : colours::panelFill);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (selected ? colour : panelBorder);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, selected ? 1.5f : 1.0f);
    if (selected)
    {
        g.setColour (colour);
        g.fillRoundedRectangle (r.getX(), r.getY() + 5.0f, 3.0f, r.getHeight() - 10.0f, 1.5f);
    }

    const int steps = value (params::engineId (index, "steps"));
    const int velocityLength = value (params::laneLengthId (index, seq::kindVelocity));
    const int shown = juce::jlimit (1, seq::maxSteps, velocityLength > 0 ? velocityLength : steps);
    const auto rateName = rates::names()[value (params::engineId (index, "rate"))];
    const int num = value (params::engineId (index, "num")) + 1, den = value (params::engineId (index, "den")) + 1;

    g.setColour (colour);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold).withKerningFactor (0.1f));
    g.drawText ("ENGINE " + juce::String (index + 1), 16, 5, 100, 15, juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (9.0f).withKerningFactor (0.08f));
    g.drawText (roles[index], 16, 20, 100, 12, juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.setFont (juce::FontOptions (11.5f));
    g.drawText (juce::String (steps) + " steps  -  " + rateName + (num != den ? "  " + juce::String (num) + ":" + juce::String (den) : juce::String()),
                118, 0, 170, getHeight(), juce::Justification::centredLeft);

    // a miniature of the velocity lane; the lit bar is where that lane's playhead is
    const float cell = 17.0f;
    for (int i = 0; i < shown; ++i)
    {
        const int v = value (params::stepId (index, seq::kindVelocity, i));
        const float h = v == 0 ? 2.0f : juce::jmax (3.0f, (float) v / 127.0f * 24.0f);
        const bool now = i == playhead;
        const auto bar = juce::Rectangle<float> (300.0f + (float) i * cell, 31.0f - h, 13.0f, h);
        g.setColour (colour.withAlpha (v == 0 ? 0.28f : now ? 1.0f : 0.5f));
        g.fillRoundedRectangle (bar, 2.0f);
        if (now)
        {
            g.setColour (colour.withAlpha (0.35f));
            g.drawRoundedRectangle (bar.expanded (1.5f), 3.0f, 1.5f);
        }
    }

    const int octave = value (params::engineId (index, "octave")) - 2;
    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText (params::arpNames[value (params::engineId (index, "arp"))] + "   OCT " + (octave > 0 ? "+" : "") + juce::String (octave) + "   CH "
                    + juce::String (value (params::engineId (index, "channel")) + 1),
                getWidth() - 400, 0, 380, getHeight(), juce::Justification::centredRight);
}

// ---------------------------------------------------------------- per-engine controls
EmberlinePolyEditor::EngineControls::EngineControls (juce::AudioProcessorValueTreeState& apvts, int e)
    : steps (apvts, params::engineId (e, "steps"), "STEPS", ""),
      rate (apvts, params::engineId (e, "rate"), "RATE"),
      num (apvts, params::engineId (e, "num"), " "),
      den (apvts, params::engineId (e, "den"), "RATIO"),
      octave (apvts, params::engineId (e, "octave"), "OCT"),
      arp (apvts, params::engineId (e, "arp"), "NOTE PICK"),
      channel (apvts, params::engineId (e, "channel"), "MIDI CH"),
      sound (apvts, params::engineId (e, "sound"), "SOUND"),
      pitchMask (apvts, params::engineId (e, "pmask")),
      mute (apvts, params::engineId (e, "mute"), "M", colours::coral),
      solo (apvts, params::engineId (e, "solo"), "S", colours::gold)
{
    pitchMask.setAccent (engineColour (e));
}

void EmberlinePolyEditor::Drawer::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0e131d));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (colours::steel.withAlpha (0.45f));
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

    g.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.1f));
    struct Title { const char* text; int x; juce::Colour colour; };
    for (const auto& t : { Title { "SCALE OPTIONS", 14, colours::cyan }, Title { "HUMANIZE", 384, colours::amber }, Title { "CHORD TIMING", 584, colours::steel },
                           Title { "NOTE RULES", 14, colours::lime }, Title { "NO-HOST CLOCK", 880, colours::steel }, Title { "AUDITION", 880, colours::lime } })
    {
        g.setColour (t.colour);
        g.drawText (t.text, t.x, juce::String (t.text) == "NOTE RULES" || juce::String (t.text) == "AUDITION" ? 120 : 8, 200, 14, juce::Justification::centredLeft);
    }
}

// ---------------------------------------------------------------- construction
EmberlinePolyEditor::EmberlinePolyEditor (EmberlinePolyProcessor& p)
    : AudioProcessorEditor (&p), processor (p), phoenix (ember::loadPhoenix()), bank (p.apvts),
      borrowScale (p.apvts, "borrow_scale", "BORROW SCALE"),
      baseOctave (p.apvts, "base_octave", "CHORD OCTAVE"),
      chordBars (p.apvts, "chord_bars", "EACH LASTS"),
      chordTiming (p.apvts, "chord_timing", "CHANGES"),
      chordChannel (p.apvts, "chord_channel", "MIDI CH"),
      chordsSound (p.apvts, "sound_chords", "CHORDS SOUND"),
      borrowAmount (p.apvts, "borrow_amount", "BORROW", colours::magenta, "%", 60),
      jitter (p.apvts, "jitter", "JITTER", colours::amber, " ms", 60),
      slip (p.apvts, "slip", "GATE SLIP", colours::amber, "%", 60),
      chordOut (p.apvts, "chord_out", "OUTPUT", "ON"),
      run (p.apvts, "run", "NO HOST", "RUN"),
      auditionOn (p.apvts, "audition_on", "AUDITION", "ON"),
      auditionLevel (p.apvts, "audition_level", "LEVEL", "%"),
      tempo (p.apvts, "bpm", "NO-HOST TEMPO", " BPM"),
      noteRules (p.apvts),
      root (p.apvts, "root", "ROOT"),
      scale (p.apvts, "scale", "SCALE"),
      genre (p.apvts, "genre", "GENRE"),
      captureBars (p.apvts, "capture_bars", "DRAG LENGTH"),
      scaleLock (p.apvts, "scale_lock", "GUARD", "LOCK"),
      chordStrip (p.apvts),
      patterns (p.apvts)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.onPaint = [this] (juce::Graphics& g) { paintContent (g); };
    content.onPaintOver = [this] (juce::Graphics& g) { paintOverContent (g); };

    for (int e = 0; e < seq::numEngines; ++e)
    {
        rows.emplace_back (processor.apvts, e);
        engines.emplace_back (processor.apvts, e);
    }

    // top bar
    for (auto* c : std::initializer_list<juce::Component*> { &presetPrev, &presetName, &presetNext, &presetSave, &settingsButton, &hostTempo })
        content.addAndMakeVisible (*c);
    presetPrev.setBounds (410, topY + 5, 34, 34);
    presetName.setBounds (444, topY + 5, 300, 34);
    presetNext.setBounds (744, topY + 5, 34, 34);
    presetSave.setBounds (784, topY + 5, 64, 34);
    hostTempo.setBounds (968, topY + 7, 130, 30);
    settingsButton.setBounds (1108, topY + 7, 116, 30);
    presetPrev.setTooltip ("Previous preset");
    presetNext.setTooltip ("Next preset");
    presetName.setTooltip ("Load a factory or saved preset, load a file, or roll all engines");
    presetSave.setTooltip ("Save the current sound as a JSON preset");
    settingsButton.setTooltip ("Scale options, humanize, chord timing, note rules and the internal clock");
    presetPrev.onClick = [this] { cyclePreset (-1); };
    presetNext.onClick = [this] { cyclePreset (1); };
    presetName.onClick = [this] { showPresetMenu(); };
    presetSave.onClick = [this] { savePresetDialog(); };
    settingsButton.onClick = [this] { toggleSettings(); };
    hostTempo.setJustificationType (juce::Justification::centred);
    hostTempo.setColour (juce::Label::backgroundColourId, juce::Colour (0xff0a0e15));
    hostTempo.setColour (juce::Label::outlineColourId, panelBorder);
    hostTempo.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.9f));
    hostTempo.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    hostTempo.setInterceptsMouseClicks (false, false);

    // chord strip
    for (auto* c : std::initializer_list<juce::Component*> { &sourceInternal, &sourceLive, &chordStrip, &liveNote, &root, &scale, &scaleLock, &genre, &suggestButton })
        content.addAndMakeVisible (*c);
    sourceInternal.setBounds (30, chordY + 38, 84, 30);
    sourceLive.setBounds (114, chordY + 38, 84, 30);
    for (auto* b : { &sourceInternal, &sourceLive })
    {
        b->setClickingTogglesState (false);
        b->setConnectedEdges (b == &sourceInternal ? juce::Button::ConnectedOnRight : juce::Button::ConnectedOnLeft);
    }
    sourceInternal.onClick = [this] { setParameter (processor.apvts, "chord_source", (float) seq::chordInternal); };
    sourceLive.onClick = [this] { setParameter (processor.apvts, "chord_source", (float) seq::chordLive); };
    sourceInternal.setTooltip ("Chords come from the progression to the right");
    sourceLive.setTooltip ("Chords are the notes you hold (or a MIDI clip of chords) on the plugin's MIDI input; the progression is not used");
    chordStrip.setBounds (214, chordY + 10, 700, chordH - 20);
    liveNote.setBounds (214, chordY + 10, 700, chordH - 20);
    liveNote.setText ("Live MIDI: the chords are whatever you hold on the plugin's MIDI input (or a MIDI clip of chords). The progression, its timing and the chords export are not used.",
                      juce::dontSendNotification);
    liveNote.setJustificationType (juce::Justification::centredLeft);
    liveNote.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    liveNote.setFont (juce::FontOptions (13.0f));
    liveNote.setMinimumHorizontalScale (1.0f);
    root.setBounds (930, chordY + 6, 62, 40);
    scale.setBounds (998, chordY + 6, 150, 40);
    scaleLock.setBounds (1154, chordY + 6, 56, 40);
    genre.setBounds (930, chordY + 44, 150, 40);
    suggestButton.setBounds (1088, chordY + 57, 122, 26);
    suggestButton.onClick = [this] { suggestProgression(); };
    suggestButton.setTooltip ("Write a chord progression for the chosen genre (and its scale) into the strip");
    root.setTooltip ("Root note of the key");
    scale.setTooltip ("Scale used for the chords and, with Scale Lock, for every note");
    scaleLock.setTooltip ("On: every note an engine plays is snapped to the scale. Off: direct override, notes are exactly what the chord and pitch step say.");
    genre.setTooltip ("Style used by the SUGGEST button");

    // engine rows, header controls and the pattern lanes
    for (int e = 0; e < seq::numEngines; ++e)
    {
        auto& row = rows[(size_t) e];
        content.addAndMakeVisible (row);
        row.setBounds (marginX, rowsY + e * (rowH + rowGap), contentW - 88, rowH);
        row.onSelect = [this, e] { selectEngine (e); };

        auto& en = engines[(size_t) e];
        en.mute.setBounds (marginX + contentW - 80, rowsY + e * (rowH + rowGap) + 3, 36, rowH - 6);
        en.solo.setBounds (marginX + contentW - 38, rowsY + e * (rowH + rowGap) + 3, 36, rowH - 6);
        content.addAndMakeVisible (en.mute);
        content.addAndMakeVisible (en.solo);
        en.mute.setTooltip ("Mute this engine");
        en.solo.setTooltip ("Solo this engine (several can be soloed)");

        const int y = panelY + 8;
        en.steps.setBounds (150, y, 84, 40);
        en.rate.setBounds (242, y, 92, 40);
        en.num.setBounds (342, y, 52, 40);
        en.den.setBounds (402, y, 52, 40);
        en.octave.setBounds (462, y, 58, 40);
        en.arp.setBounds (528, y, 120, 40);
        en.channel.setBounds (656, y, 58, 40);
        en.pitchMask.setBounds (724, y, 150, 40);
        for (int s = 0; s < patterns::numSlots; ++s)
            en.slots[(size_t) s].setBounds (888 + s * 30, y + 14, 26, 26);
        en.dice.setBounds (1022, y + 14, 70, 26);
        en.sound.setBounds (1100, y, 108, 40);

        for (auto* c : std::initializer_list<juce::Component*> { &en.steps, &en.rate, &en.num, &en.den, &en.octave, &en.arp, &en.channel, &en.pitchMask, &en.dice, &en.sound })
            content.addAndMakeVisible (*c);
        for (auto& b : en.slots)
            content.addAndMakeVisible (b);

        en.steps.setTooltip ("Step count of the engine. Lanes follow it unless a lane has its own length (drag the len number on its label).");
        en.rate.setTooltip ("Base rate of the engine's clock");
        en.den.setTooltip ("Speed ratio n:d on top of the rate. 5:4 plays a quarter faster, 3:4 at three quarters speed.");
        en.num.setTooltip ("Speed ratio n:d on top of the rate. 5:4 plays a quarter faster, 3:4 at three quarters speed.");
        en.octave.setTooltip ("Octave shift for this whole engine");
        en.arp.setTooltip ("How this engine picks notes from the chord on each trigger. Root plays the lowest chord note only.");
        en.channel.setTooltip ("MIDI channel this engine sends on");
        en.sound.setTooltip ("The built-in audition sound for this engine (used only when AUDITION is on)");
        en.pitchMask.setTooltip ("Which chord notes (1 is the lowest) the pitch lane moves. The others keep their own pitch.");
        en.dice.setTooltip ("Roll a new pattern for this engine: step count, rate and the lanes. Octave, note pick and MIDI channel stay as they are.");
        en.dice.onClick = [this, e] { rollEngine (e); };

        for (int s = 0; s < patterns::numSlots; ++s)
        {
            auto& b = en.slots[(size_t) s];
            b.setButtonText (patterns::slotName (s));
            b.setClickingTogglesState (false);
            b.setTooltip ("Pattern " + juce::String (patterns::slotName (s)) + ". An unused slot starts as a copy of the current pattern, so you can make variations. Shift-click for copy / clear.");
            b.onClick = [this, e, s]
            {
                if (juce::ModifierKeys::currentModifiers.isShiftDown())
                    showSlotMenu (e, s);
                else
                {
                    bank.select (e, s);
                    updateSlotButtons();
                }
            };
        }
    }
    content.addAndMakeVisible (patterns);
    patterns.setBounds (marginX + 14, panelY + 56, contentW - 28, ui::PatternEditor::preferredHeight);

    // export bar
    int x = 124;
    const char* handleNames[] { "CHORDS", "ENGINE 1", "ENGINE 2", "ENGINE 3", "ENGINE 4", "ALL TRACKS" };
    for (int h = 0; h < (int) dragButtons.size(); ++h)
    {
        auto& b = dragButtons[(size_t) h];
        const bool all = h == (int) dragButtons.size() - 1;
        const unsigned mask = all ? clip::allSources : 1u << h;
        const juce::String label = juce::String (handleNames[h]).replaceCharacter (' ', '_');
        content.addAndMakeVisible (b);
        b.setBounds (x, exportY, all ? 124 : 100, 36);
        x += (all ? 124 : 100) + 6;
        b.setButtonText (handleNames[h]);
        b.setColour (juce::TextButton::textColourOffId, h == 0 ? colours::steel : all ? colours::cyan : engineColour (h - 1));
        b.setTooltip (all ? "Drag onto a DAW track to drop one MIDI file with a track per unmuted part, each on its own MIDI channel. Click to save it instead."
                          : "Drag onto a DAW track to drop this part as a MIDI clip. Click to save it as a .mid file instead.");
        b.prepareFile = [this, mask, label] { return exportCapture (mask, label); };
        b.onPlainClick = [this, mask, label] { saveCaptureAs (mask, label); };
        b.onNothingToDrag = [this] { setStatus ("Nothing to export for that part (a Live MIDI source needs the transport playing with notes held)."); };
        b.onDragStarted = [this] (const juce::File& f) { setStatus ("Dragging " + f.getFileName() + ". If the host does not accept the drop (e.g. FL Studio's Patcher), click the handle to save the file instead."); };
    }
    captureBars.setBounds (x + 8, exportY - 4, 100, 40);
    clearButton.setBounds (x + 114, exportY + 3, 56, 30);
    linksButton.setBounds (x + 176, exportY + 3, 56, 30);
    auditionOn.setBounds (1036, exportY - 4, 62, 40);
    auditionLevel.setBounds (1106, exportY - 4, 102, 40);
    auditionOn.setTooltip ("Play the generated notes through built-in sampled sounds so you can hear the patterns without another instrument. Turn it off when the MIDI goes to a real instrument.");
    auditionLevel.setTooltip ("Volume of the built-in audition sounds");
    for (auto* c : std::initializer_list<juce::Component*> { &captureBars, &clearButton, &linksButton, &status, &auditionOn, &auditionLevel })
        content.addAndMakeVisible (*c);
    captureBars.setTooltip ("How many bars a dragged clip contains, from the start of the progression");
    clearButton.setTooltip ("Forget the captured notes (Live MIDI exports)");
    clearButton.onClick = [this] { processor.clearCapture(); setStatus ("Capture cleared."); };
    linksButton.onClick = [this] { showLinksMenu(); };
    status.setBounds (marginX, statusY, contentW, 16);
    status.setFont (juce::FontOptions (11.5f));
    status.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    status.setJustificationType (juce::Justification::centredLeft);

    // settings drawer
    content.addChildComponent (drawer);
    drawer.setBounds (marginX, topY + topH + 6, contentW, 268);
    for (auto* c : std::initializer_list<juce::Component*> { &borrowScale, &baseOctave, &chordBars, &chordTiming, &chordChannel, &borrowAmount, &jitter, &slip, &chordOut, &run, &tempo, &noteRules, &noteHint, &chordsSound })
        drawer.addAndMakeVisible (*c);
    borrowScale.setBounds (14, 30, 150, 40);
    baseOctave.setBounds (174, 30, 90, 40);
    borrowAmount.setBounds (274, 26, 100, 84);
    jitter.setBounds (394, 26, 76, 84);
    slip.setBounds (478, 26, 76, 84);
    chordBars.setBounds (594, 30, 100, 40);
    chordTiming.setBounds (704, 30, 120, 40);
    chordOut.setBounds (594, 80, 80, 40);
    chordChannel.setBounds (684, 80, 64, 40);
    run.setBounds (880, 30, 70, 40);
    tempo.setBounds (960, 30, 160, 40);
    chordsSound.setBounds (880, 140, 150, 40);
    chordsSound.setTooltip ("The built-in audition sound for the chords (the engines choose theirs in their own header)");
    noteRules.setBounds (14, 140, 330, 64);
    noteHint.setBounds (356, 140, 820, 64);
    noteHint.setText ("Click a key to cycle: play, block (x), a semitone up (^) or a semitone down (v). Right-click clears it.\nRules apply to the notes the engines generate, after the scale guard, so blocked notes are simply silent.",
                      juce::dontSendNotification);
    noteHint.setJustificationType (juce::Justification::topLeft);
    noteHint.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    noteHint.setFont (juce::FontOptions (12.0f));
    borrowScale.setTooltip ("The scale used for borrowed notes (modal interchange)");
    borrowAmount.setTooltip ("How many triggers snap to the borrow scale. Hold the mod wheel above half to borrow on every note.");
    baseOctave.setTooltip ("Octave the chords are built in. Engines shift from there with their own octave setting.");
    jitter.setTooltip ("Gaussian micro-timing drift per note, up to this many milliseconds early or late");
    slip.setTooltip ("Random variation of each note's length");
    chordBars.setTooltip ("How long each chord lasts");
    chordTiming.setTooltip ("On beat, an eighth early (anticipated) or an eighth late (off-beat)");
    chordOut.setTooltip ("Send the internal chords as MIDI on their own channel (internal source only)");
    chordChannel.setTooltip ("MIDI channel of the chords");
    selectEngine (juce::jlimit (0, seq::numEngines - 1, (int) processor.apvts.state.getProperty (selectedEngineKey, 0)));

    constrainer.setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    constrainer.setSizeLimits (baseWidth / 2, baseHeight / 2, baseWidth * 2, baseHeight * 2);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (baseWidth, baseHeight);
    updateHostDisplay();
    timerCallback();
    startTimerHz (30);
}

EmberlinePolyEditor::~EmberlinePolyEditor() { setLookAndFeel (nullptr); }

void EmberlinePolyEditor::resized()
{
    content.setBounds (0, 0, baseWidth, baseHeight);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseWidth));
}

// ---------------------------------------------------------------- selection and small actions
void EmberlinePolyEditor::selectEngine (int engine)
{
    selected = juce::jlimit (0, seq::numEngines - 1, engine);
    processor.apvts.state.setProperty (selectedEngineKey, selected, nullptr);
    patterns.setEngine (selected, engineColour (selected));
    for (int e = 0; e < seq::numEngines; ++e)
    {
        auto& en = engines[(size_t) e];
        const bool on = e == selected;
        for (auto* c : std::initializer_list<juce::Component*> { &en.steps, &en.rate, &en.num, &en.den, &en.octave, &en.arp, &en.channel, &en.pitchMask, &en.dice, &en.sound })
            c->setVisible (on);
        for (auto& b : en.slots)
            b.setVisible (on);
        rows[(size_t) e].setSelected (on);
    }
    updateSlotButtons();
    content.repaint();
}

void EmberlinePolyEditor::updateSlotButtons()
{
    auto& en = engines[(size_t) selected];
    const int active = bank.active (selected);
    for (int s = 0; s < patterns::numSlots; ++s)
    {
        auto& b = en.slots[(size_t) s];
        b.setToggleState (s == active, juce::dontSendNotification);
        b.setAlpha (bank.filled (selected, s) ? 1.0f : 0.5f);
    }
}

void EmberlinePolyEditor::showSlotMenu (int engine, int slot)
{
    juce::PopupMenu menu;
    const int active = bank.active (engine);
    menu.addSectionHeader (juce::String ("Pattern ") + patterns::slotName (slot));
    for (int to = 0; to < patterns::numSlots; ++to)
        if (to != slot)
            menu.addItem (10 + to, juce::String ("Copy to ") + patterns::slotName (to));
    if (slot != active && bank.filled (engine, slot))
    {
        menu.addSeparator();
        menu.addItem (20, juce::String ("Clear ") + patterns::slotName (slot) + " (it starts as a copy again)");
    }

    juce::Component::SafePointer<EmberlinePolyEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (engines[(size_t) engine].slots[(size_t) slot]), [safe, engine, slot] (int choice)
    {
        if (safe == nullptr || choice == 0)
            return;
        if (choice == 20)
            safe->bank.clear (engine, slot);
        else
            safe->bank.copy (engine, slot, choice - 10);
        safe->updateSlotButtons();
    });
}

void EmberlinePolyEditor::showSettings (bool show)
{
    drawer.setVisible (show);
    drawer.toFront (false);
    settingsButton.setToggleState (show, juce::dontSendNotification);
    settingsButton.setButtonText (show ? "CLOSE" : "SETTINGS");
}

int EmberlinePolyEditor::captureBarsSelected() const
{
    return params::captureBars[(size_t) juce::jlimit (0, 5, (int) processor.apvts.getRawParameterValue ("capture_bars")->load())];
}

void EmberlinePolyEditor::setStatus (const juce::String& text)
{
    status.setText (text, juce::dontSendNotification);
    lastCaptureText = {};
    statusCountdown = 90;   // keep the message for about three seconds
}

// With a host timeline the plugin follows the host's tempo and transport; the no-host controls only matter in the Standalone app.
void EmberlinePolyEditor::updateHostDisplay()
{
    const bool host = processor.followingHost();
    hostTempo.setText (host ? "HOST  " + juce::String (processor.currentBpm(), 1) + " BPM" : "NO HOST CLOCK", juce::dontSendNotification);

    if (hostKnown && host == lastHost)
        return;

    hostKnown = true;
    lastHost = host;
    run.setEnabled (! host);
    tempo.setEnabled (! host);
    run.setAlpha (host ? 0.4f : 1.0f);
    tempo.setAlpha (host ? 0.4f : 1.0f);
    run.setCaption (host ? "HOST" : "NO HOST");
    tempo.setCaption (host ? "HOST TEMPO" : "NO-HOST TEMPO");
    run.setTooltip (host ? "The host's transport is running this plugin" : "Only used when the host provides no transport, e.g. in the Standalone app");
    tempo.setTooltip (host ? "The plugin follows the host's tempo" : "Tempo of the internal clock, used when the host provides no transport");
}

// The progression, its timing and the chord MIDI output only apply to the internal source; in Live MIDI mode they are greyed out.
void EmberlinePolyEditor::updateChordSourceDisplay()
{
    const int source = (int) processor.apvts.getRawParameterValue ("chord_source")->load();
    if (source == lastChordSource)
        return;

    lastChordSource = source;
    const bool internal = source == seq::chordInternal;
    sourceInternal.setToggleState (internal, juce::dontSendNotification);
    sourceLive.setToggleState (! internal, juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &genre, &suggestButton, &chordBars, &chordTiming, &chordChannel, &chordOut, &dragButtons[0] })
    {
        c->setEnabled (internal);
        c->setAlpha (internal ? 1.0f : 0.35f);
    }
    chordStrip.setVisible (internal);
    liveNote.setVisible (! internal);
}

void EmberlinePolyEditor::cyclePreset (int direction)
{
    static const auto factoryPresets = presets::factory();
    struct Entry { juce::String name; int factory; juce::File file; };
    std::vector<Entry> list;
    for (size_t i = 0; i < factoryPresets.size(); ++i)
        list.push_back ({ factoryPresets[i].name, (int) i, {} });
    auto files = userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.json");
    files.sort();
    for (auto& f : files)
        list.push_back ({ f.getFileNameWithoutExtension(), -1, f });
    if (list.empty())
        return;

    const auto current = processor.apvts.state.getProperty (presetNameKey).toString();
    int index = -1;
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].name == current)
            index = (int) i;
    const int next = index < 0 ? (direction > 0 ? 0 : (int) list.size() - 1) : ((index + direction) % (int) list.size() + (int) list.size()) % (int) list.size();

    const auto& entry = list[(size_t) next];
    if (entry.factory >= 0)
        loadPreset (factoryPresets[(size_t) entry.factory]);
    else
    {
        juce::String error;
        presets::Preset p;
        if (presets::fromJson (entry.file.loadFileAsString(), p, error))
            loadPreset (p);
        else
            setStatus (error);
    }
}

// ---------------------------------------------------------------- live updates
void EmberlinePolyEditor::timerCallback()
{
    updateHostDisplay();
    updateChordSourceDisplay();

    {
        const auto name = processor.apvts.state.getProperty (presetNameKey).toString();
        if (name != lastPresetName)
        {
            lastPresetName = name;
            presetName.setButtonText (name.isEmpty() ? juce::String ("Presets") : name);
        }
    }

    for (int e = 0; e < seq::numEngines; ++e)
    {
        rows[(size_t) e].setPlayhead (processor.playheadStep (e, seq::kindVelocity));
        rows[(size_t) e].refresh();
    }

    std::array<int, seq::numKinds> heads {};
    for (int k = 0; k < seq::numKinds; ++k)
        heads[(size_t) k] = processor.playheadStep (selected, k);
    patterns.setPlayheads (heads);
    patterns.refresh();
    engines[(size_t) selected].pitchMask.refresh();
    noteRules.refresh();
    chordStrip.setPlaying (processor.chordSlot());
    chordStrip.refresh();
    updateSlotButtons();

    if (statusCountdown > 0)
    {
        --statusCountdown;
        return;
    }

    if (++statusTicks % 10 == 0)
    {
        if (lastChordSource == seq::chordInternal)
        {
            const juce::String text = "Drag a handle to export " + juce::String (captureBarsSelected()) + " bars, rendered from the start of the progression. "
                                      "You do not need to play first. Click a handle to save the file instead.";
            if (text != lastCaptureText)
            {
                lastCaptureText = text;
                status.setText (text, juce::dontSendNotification);
            }
            return;
        }

        const auto notes = processor.capture().snapshot();
        const int bars = captureBarsSelected();
        const int inWindow = clip::countInWindow (notes, clip::allSources, bars);
        const auto w = clip::chooseWindow (notes, bars);

        const juce::String text = inWindow == 0 ? "Live MIDI: nothing captured yet. Play the transport and hold some chords to record the patterns."
                                                : juce::String (inWindow) + " notes over " + juce::String (juce::roundToInt ((w.endBeats - w.startBeats) / 4.0))
                                                    + " bars ready to drag (chords " + juce::String (clip::countInWindow (notes, clip::chordsOnly, bars)) + ", engines "
                                                    + juce::String (inWindow - clip::countInWindow (notes, clip::chordsOnly, bars)) + ").";
        if (text != lastCaptureText)
        {
            lastCaptureText = text;
            status.setText (text, juce::dontSendNotification);
        }
    }
}

// ---------------------------------------------------------------- painting
void EmberlinePolyEditor::paintContent (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (colours::bgPanel, 0.0f, 0.0f, colours::bgDeep, 0.0f, (float) baseHeight, false));
    g.fillAll();

    g.setColour (colours::cyan.withAlpha (0.03f));
    for (int i = 0; i < 12; ++i)
        g.drawHorizontalLine (baseHeight - 6 - i * i * 2, 0.0f, (float) baseWidth);

    // title: gradient-glow lettering with the phoenix mark
    const juce::String title = "EMBERLINE POLY";
    juce::GlyphArrangement ga;
    const juce::Font font (juce::FontOptions (24.0f, juce::Font::bold).withKerningFactor (0.14f));
    ga.addLineOfText (font, title, 0.0f, 0.0f);
    juce::Path text;
    ga.createPath (text);
    const auto raw = text.getBounds();
    text.applyTransform (juce::AffineTransform::translation (66.0f - raw.getX(), (float) topY + 20.0f - raw.getCentreY()));
    const auto tb = text.getBounds();
    juce::ColourGradient grad (colours::cyan, tb.getX(), 0.0f, colours::coral, tb.getRight(), 0.0f, false);
    grad.addColour (0.5, colours::cyan.interpolatedWith (colours::magenta, 0.55f));
    for (float width : { 9.0f, 5.5f, 3.0f })
    {
        auto glow = grad;
        glow.multiplyOpacity (width > 8.0f ? 0.07f : width > 5.0f ? 0.11f : 0.18f);
        g.setGradientFill (glow);
        g.strokePath (text, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setGradientFill (grad);
    g.fillPath (text);
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText (ember::version::full(), juce::Rectangle<int> (68, topY + 30, 200, 12), juce::Justification::centredLeft);
    ember::drawPhoenix (g, phoenix, juce::Rectangle<float> (16.0f, (float) topY, 42.0f, 42.0f), 0.6f);

    // chord strip panel
    {
        const auto b = juce::Rectangle<float> ((float) marginX, (float) chordY, (float) contentW, (float) chordH);
        g.setColour (colours::panelFill);
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (panelBorder);
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (textDim);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.12f));
        g.drawText ("CHORDS", 30, chordY + 12, 120, 14, juce::Justification::centredLeft);
        g.drawText ("SOURCE", 30, chordY + 26, 120, 12, juce::Justification::centredLeft);
    }

    // editor panel in the selected engine's colour
    {
        const auto c = engineColour (selected);
        const auto b = juce::Rectangle<float> ((float) marginX, (float) panelY, (float) contentW, (float) panelH);
        g.setColour (colours::panelFill);
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (c.withAlpha (0.45f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

        g.setColour (c);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold).withKerningFactor (0.1f));
        g.drawText ("ENGINE " + juce::String (selected + 1), 30, panelY + 10, 110, 18, juce::Justification::centredLeft);
        g.setColour (c.withAlpha (0.6f));
        g.setFont (juce::FontOptions (10.0f).withKerningFactor (0.08f));
        g.drawText (roles[selected], 30, panelY + 28, 110, 12, juce::Justification::centredLeft);

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText (":", 394, panelY + 28, 8, 22, juce::Justification::centred);
        ui::drawCaption (g, { 888, panelY + 8, 100, 12 }, "PATTERN");
    }

    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.12f));
    g.drawText ("DRAG TO DAW", marginX, exportY, 104, 36, juce::Justification::centredLeft);
}

void EmberlinePolyEditor::paintOverContent (juce::Graphics& g)
{
    if (! dropHint)
        return;

    const auto b = content.getLocalBounds().toFloat().reduced (6.0f);
    g.setColour (colours::bgDeep.withAlpha (0.78f));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (colours::cyan);
    g.drawRoundedRectangle (b, 10.0f, 2.0f);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold).withKerningFactor (0.08f));
    g.drawText ("DROP A MIDI FILE TO LOAD IT INTO THE SELECTED ENGINE", content.getLocalBounds(), juce::Justification::centred);
}

// ---------------------------------------------------------------- chords
void EmberlinePolyEditor::suggestProgression()
{
    humanize::Rng rng ((std::uint64_t) juce::Time::currentTimeMillis() * 0x9e3779b97f4a7c15ull + (std::uint64_t) juce::Time::getHighResolutionTicks());
    const int style = (int) processor.apvts.getRawParameterValue ("genre")->load();
    const auto suggestion = chords::suggest (rng, style);

    auto& apvts = processor.apvts;
    setParameter (apvts, "chord_source", (float) seq::chordInternal);
    setParameter (apvts, "chord_length", (float) suggestion.progression.length - 1.0f);
    for (int i = 0; i < suggestion.progression.length; ++i)
    {
        setParameter (apvts, params::chordId (i, "deg"), (float) suggestion.progression.slot[(size_t) i].degree);
        setParameter (apvts, params::chordId (i, "ext"), (float) suggestion.progression.slot[(size_t) i].extension);
    }
    setParameter (apvts, "scale", (float) suggestion.scale);
    setStatus ("Suggested a " + chords::genreNames()[style] + " progression in " + scales::names()[suggestion.scale] + ".");
}

// ---------------------------------------------------------------- dice & presets
void EmberlinePolyEditor::rollEngine (int e)
{
    humanize::Rng rng ((std::uint64_t) juce::Time::currentTimeMillis() * 0x9e3779b97f4a7c15ull + (std::uint64_t) juce::Time::getHighResolutionTicks() + 7919ull * (unsigned long long) e);
    auto settings = params::capturePreset (processor.apvts, false).settings;
    dice::randomiseEngines (rng, settings, 1u << e);
    params::applyPattern (processor.apvts, e, settings.engine[(size_t) e]);

    const auto& en = settings.engine[(size_t) e];
    setStatus ("Engine " + juce::String (e + 1) + " rolled: " + juce::String (en.steps) + " steps at " + rates::names()[en.rate]
               + (en.num != en.den ? ", ratio " + juce::String (en.num) + ":" + juce::String (en.den) : juce::String()) + ".");
}

void EmberlinePolyEditor::loadPreset (const presets::Preset& p)
{
    const bool chordsToo = (bool) processor.apvts.state.getProperty (presetChordsKey, true);
    params::applyPreset (processor.apvts, p, chordsToo);
    processor.apvts.state.setProperty (presetNameKey, p.name, nullptr);

    int used = 0;
    for (bool u : p.used)
        used += u ? 1 : 0;
    setStatus ("Loaded \"" + p.name + "\": " + juce::String (used) + (used == 1 ? " engine" : " engines")
               + (p.hasChords && ! chordsToo ? ", chords kept." : "."));
}

void EmberlinePolyEditor::showPresetMenu()
{
    static const auto factoryPresets = presets::factory();
    const bool chordsToo = (bool) processor.apvts.state.getProperty (presetChordsKey, true);
    const auto userFiles = userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.json");

    juce::PopupMenu menu;
    juce::StringArray categories;
    for (auto& p : factoryPresets)
        categories.addIfNotAlreadyThere (p.category);

    enum { loadFile = 1, saveAs, openFolder, toggleChords, rollAll, firstFactory = 100, firstUser = 400 };
    for (auto& category : categories)
    {
        juce::PopupMenu sub;
        for (size_t i = 0; i < factoryPresets.size(); ++i)
            if (factoryPresets[i].category == category)
                sub.addItem (firstFactory + (int) i, factoryPresets[i].name);
        menu.addSubMenu (category, sub);
    }

    juce::PopupMenu mine;
    for (int i = 0; i < userFiles.size() && i < 200; ++i)
        mine.addItem (firstUser + i, userFiles[i].getFileNameWithoutExtension());
    if (userFiles.isEmpty())
        mine.addItem (-1, "(none saved yet)", false);
    menu.addSubMenu ("My presets", mine);

    menu.addSeparator();
    menu.addItem (rollAll, "Roll all engines");
    menu.addSeparator();
    menu.addItem (saveAs, "Save preset...");
    menu.addItem (loadFile, "Load from file...");
    menu.addItem (openFolder, "Show presets folder");
    menu.addSeparator();
    menu.addItem (toggleChords, "Presets also set the chord progression", true, chordsToo);

    juce::Component::SafePointer<EmberlinePolyEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetName).withMinimumWidth (220), [safe, userFiles] (int choice)
    {
        if (safe == nullptr || choice == 0)
            return;

        if (choice >= firstUser)
        {
            juce::String error;
            presets::Preset p;
            if (presets::fromJson (userFiles[choice - firstUser].loadFileAsString(), p, error))
                safe->loadPreset (p);
            else
                safe->setStatus (error);
        }
        else if (choice >= firstFactory)
            safe->loadPreset (factoryPresets[(size_t) (choice - firstFactory)]);
        else if (choice == rollAll)
        {
            for (int e = 0; e < seq::numEngines; ++e)
                safe->rollEngine (e);
            safe->setStatus ("Rolled all four engines.");
        }
        else if (choice == toggleChords)
            safe->processor.apvts.state.setProperty (presetChordsKey, ! (bool) safe->processor.apvts.state.getProperty (presetChordsKey, true), nullptr);
        else if (choice == openFolder)
        {
            userPresetFolder().createDirectory();
            userPresetFolder().revealToUser();
        }
        else if (choice == saveAs)
            safe->savePresetDialog();
        else if (choice == loadFile)
        {
            safe->chooser = std::make_shared<juce::FileChooser> ("Load a preset", userPresetFolder(), "*.json");
            safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
            {
                if (safe == nullptr || fc.getResult() == juce::File())
                    return;
                juce::String error;
                presets::Preset p;
                if (presets::fromJson (fc.getResult().loadFileAsString(), p, error))
                    safe->loadPreset (p);
                else
                    safe->setStatus (error);
            });
        }
    });
}

void EmberlinePolyEditor::savePresetDialog()
{
    auto* alert = new juce::AlertWindow ("Save preset", "Saves the engines, scale and humanize settings as a JSON file in the presets folder.", juce::MessageBoxIconType::NoIcon, this);
    alert->addTextEditor ("name", processor.apvts.state.getProperty (presetNameKey).toString(), "Name");
    alert->addComboBox ("chords", { "With the chord progression", "Without the chord progression" }, "Chords");
    alert->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<EmberlinePolyEditor> safe (this);
    alert->enterModalState (true, juce::ModalCallbackFunction::create ([safe, alert] (int result)
    {
        if (result != 1 || safe == nullptr)
            return;

        auto name = alert->getTextEditorContents ("name").trim();
        if (name.isEmpty())
            name = "Untitled";

        auto preset = params::capturePreset (safe->processor.apvts, alert->getComboBoxComponent ("chords")->getSelectedItemIndex() == 0);
        preset.name = name;
        preset.category = "User";

        const auto folder = userPresetFolder();
        folder.createDirectory();
        const auto file = folder.getChildFile (juce::File::createLegalFileName (name)).withFileExtension ("json");
        if (file.replaceWithText (presets::toJson (preset)))
        {
            safe->processor.apvts.state.setProperty (presetNameKey, name, nullptr);
            safe->setStatus ("Saved " + file.getFileName() + " to the presets folder.");
        }
        else
            safe->setStatus ("Could not write " + file.getFullPathName());
    }), true);
}

// ---------------------------------------------------------------- MIDI export / import
// With the internal chord source the clip is rendered offline (full length, from the first chord, no playback needed). With Live MIDI the
// result depends on the notes being played, so the capture of the running transport is used instead.
int EmberlinePolyEditor::writeExport (unsigned sourceMask, const juce::File& file)
{
    const double bpm = processor.currentBpm();
    const int bars = captureBarsSelected();
    auto settings = params::capturePreset (processor.apvts, false).settings;

    if (settings.chords.source != seq::chordInternal)
        return clip::write (processor.capture().snapshot(), sourceMask, bars, bpm, file);

    if (sourceMask == clip::allSources)   // ALL TRACKS follows mute and solo; a part's own handle always exports it
    {
        bool anySolo = false;
        for (auto& e : settings.engine)
            anySolo = anySolo || e.solo;
        for (int e = 0; e < seq::numEngines; ++e)
        {
            const auto& en = settings.engine[(size_t) e];
            if (en.mute || (anySolo && ! en.solo))
                sourceMask &= ~clip::engineOnly (e);
        }
    }

    const auto seed = (std::uint64_t) juce::Time::currentTimeMillis() * 0x9e3779b97f4a7c15ull + (std::uint64_t) juce::Time::getHighResolutionTicks();
    return clip::writeWindow (clip::render (settings, bars, seed), sourceMask, { 0.0, (double) bars * 4.0 }, bpm, file);
}

juce::File EmberlinePolyEditor::exportCapture (unsigned sourceMask, const juce::String& label)
{
    const double bpm = processor.currentBpm();
    const auto folder = exportFolder();
    clip::deleteStaleExports (folder);
    const auto file = folder.getChildFile ("EmberlinePoly_" + label + "_" + juce::String (juce::roundToInt (bpm)) + "bpm_"
                                           + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".mid");
    return writeExport (sourceMask, file) > 0 ? file : juce::File();
}

void EmberlinePolyEditor::saveCaptureAs (unsigned sourceMask, const juce::String& label)
{
    chooser = std::make_shared<juce::FileChooser> ("Save the pattern as a MIDI file",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("EmberlinePoly_" + label + ".mid"),
                                                   "*.mid");
    juce::Component::SafePointer<EmberlinePolyEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe, sourceMask] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;

        const auto target = fc.getResult().withFileExtension ("mid");
        const int count = safe->writeExport (sourceMask, target);
        safe->setStatus (count > 0 ? "Saved " + target.getFileName() : "Nothing to export for that part.");
    });
}

bool EmberlinePolyEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("mid;midi"))
            return true;
    return false;
}

void EmberlinePolyEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    dropHint = false;
    content.repaint();

    int engine = selected;   // a drop on an engine row loads that engine, anywhere else the selected one
    const auto p = content.getLocalPoint (this, juce::Point<int> (x, y));
    for (int e = 0; e < seq::numEngines; ++e)
        if (rows[(size_t) e].getBounds().contains (p))
            engine = e;

    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("mid;midi"))
        {
            clip::Imported imported;
            if (clip::importFile (juce::File (f), imported))
            {
                applyImport (engine, imported);
                selectEngine (engine);
                setStatus ("Loaded " + juce::String (imported.count) + " notes into engine " + juce::String (engine + 1) + " (note pick set to Root).");
            }
            else
                setStatus ("That MIDI file has no notes.");
            return;
        }
}

void EmberlinePolyEditor::applyImport (int engine, const clip::Imported& in)
{
    auto& apvts = processor.apvts;
    for (int i = 0; i < in.count; ++i)
    {
        setParameter (apvts, params::stepId (engine, seq::kindPitch, i), (float) in.pitch[(size_t) i]);
        setParameter (apvts, params::stepId (engine, seq::kindVelocity, i), (float) in.velocity[(size_t) i]);
        setParameter (apvts, params::stepId (engine, seq::kindGate, i), (float) in.gate[(size_t) i]);
        setParameter (apvts, params::stepId (engine, seq::kindProbability, i), 100.0f);
        setParameter (apvts, params::stepId (engine, seq::kindRatchet, i), 1.0f);
    }

    for (int i = 0; i < seq::maxSteps; ++i)   // a loaded clip is a plain pattern: no late or harmony
    {
        setParameter (apvts, params::stepId (engine, seq::kindLate, i), 0.0f);
        setParameter (apvts, params::stepId (engine, seq::kindHarmony, i), 0.0f);
    }
    for (int k = 0; k < seq::numKinds; ++k)
    {
        setParameter (apvts, params::laneLengthId (engine, k), 0.0f);
        setParameter (apvts, params::laneDivideId (engine, k), 1.0f);
    }
    setParameter (apvts, params::engineId (engine, "steps"), (float) in.count);
    setParameter (apvts, params::engineId (engine, "arp"), (float) seq::arpRoot);   // imported pitches are semitones from the first note, played from the chord root
}

void EmberlinePolyEditor::showLinksMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Support EmberlineSky - buy me a coffee");
    menu.addItem (2, "Report an issue");
    menu.addItem (3, "Source code");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&linksButton), [] (int id)
    {
        if (id == 1)      juce::URL (ember::links::support).launchInDefaultBrowser();
        else if (id == 2) juce::URL (ember::links::issues).launchInDefaultBrowser();
        else if (id == 3) juce::URL (ember::links::source).launchInDefaultBrowser();
    });
}
