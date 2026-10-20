#include "PluginEditor.h"
#include "Chords.h"
#include "Links.h"
#include "Version.h"

using namespace ember;

namespace
{
    constexpr int editorW = 1040, editorH = 852, headerH = 54;
    constexpr int rowAY = 58, rowAH = 104, rowBY = 168, rowBH = 104, rowCY = 278, rowCH = 68;
    constexpr int enginesY = 354, engineH = 112, engineGap = 6;
    constexpr int headerX = 14, headerW = 330, gridX = 352;

    juce::Colour engineColour (int engine) { return colours::lane (engine); }

    void setParameter (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float plainValue)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
            p->endChangeGesture();
        }
    }

    juce::File exportFolder() { return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Emberline Poly"); }
}

EmberlinePolyEditor::ChordSlotControls::ChordSlotControls (juce::AudioProcessorValueTreeState& apvts, int slot)
    : degree (apvts, params::chordId (slot, "deg"), juce::String (slot + 1)),
      extension (apvts, params::chordId (slot, "ext"), " ")
{
}

EmberlinePolyEditor::EngineControls::EngineControls (juce::AudioProcessorValueTreeState& apvts, int e)
    : steps (apvts, params::engineId (e, "steps"), "STEPS", engineColour (e)),
      rate (apvts, params::engineId (e, "rate"), "RATE"),
      num (apvts, params::engineId (e, "num"), "RATIO"),
      den (apvts, params::engineId (e, "den"), " "),
      octave (apvts, params::engineId (e, "octave"), "OCT"),
      arp (apvts, params::engineId (e, "arp"), "NOTE PICK"),
      channel (apvts, params::engineId (e, "channel"), "MIDI CH"),
      mute (apvts, params::engineId (e, "mute"), "M", colours::coral),
      solo (apvts, params::engineId (e, "solo"), "S", colours::gold),
      grid (apvts, e, engineColour (e))
{
}

EmberlinePolyEditor::EmberlinePolyEditor (EmberlinePolyProcessor& p)
    : AudioProcessorEditor (&p), processor (p), phoenix (ember::loadPhoenix()),
      root (p.apvts, "root", "ROOT"),
      scale (p.apvts, "scale", "SCALE"),
      borrowScale (p.apvts, "borrow_scale", "BORROW SCALE"),
      scaleLock (p.apvts, "scale_lock", "GUARD", "SCALE LOCK"),
      borrowAmount (p.apvts, "borrow_amount", "BORROW", colours::magenta, "%", 60),
      jitter (p.apvts, "jitter", "JITTER", colours::amber, " ms", 60),
      slip (p.apvts, "slip", "GATE SLIP", colours::amber, "%", 60),
      baseOctave (p.apvts, "base_octave", "CHORD OCT"),
      captureBars (p.apvts, "capture_bars", "DRAG LENGTH"),
      run (p.apvts, "run", "NO HOST", "RUN"),
      tempo (p.apvts, "bpm", "NO-HOST TEMPO", " BPM"),
      chordSource (p.apvts, "chord_source", "CHORD SOURCE"),
      genre (p.apvts, "genre", "GENRE"),
      chordLength (p.apvts, "chord_length", "CHORDS"),
      chordBars (p.apvts, "chord_bars", "EACH LASTS"),
      chordTiming (p.apvts, "chord_timing", "CHANGES"),
      chordChannel (p.apvts, "chord_channel", "MIDI CH"),
      chordOut (p.apvts, "chord_out", "OUTPUT", "ON")
{
    setLookAndFeel (&lnf);

    for (int c = 0; c < seq::maxChordSlots; ++c)
        chordSlots.emplace_back (processor.apvts, c);
    for (int e = 0; e < seq::numEngines; ++e)
        engines.emplace_back (processor.apvts, e);

    for (auto* c : std::initializer_list<juce::Component*> { &root, &scale, &borrowScale, &scaleLock, &borrowAmount, &jitter, &slip, &baseOctave,
                                                            &captureBars, &run, &tempo, &chordSource, &genre, &chordLength, &chordBars, &chordTiming,
                                                            &chordChannel, &chordOut, &suggestButton, &clearButton, &linksButton, &status })
        addAndMakeVisible (*c);
    for (auto& b : dragButtons)
        addAndMakeVisible (b);
    for (auto& cs : chordSlots)
    {
        addAndMakeVisible (cs.degree);
        addAndMakeVisible (cs.extension);
    }

    for (int e = 0; e < seq::numEngines; ++e)
    {
        auto& en = engines[(size_t) e];
        for (auto* c : std::initializer_list<juce::Component*> { &en.steps, &en.rate, &en.num, &en.den, &en.octave, &en.arp, &en.channel, &en.mute, &en.solo, &en.grid })
            addAndMakeVisible (*c);

        const char* modeNames[] { "VEL", "GATE", "PITCH", "PROB", "RATCH" };
        const char* modeTips[] { "Edit velocity (bar height)", "Edit gate length (bar width)", "Edit pitch shift in semitones (number on the pad)",
                                 "Edit trigger probability (bar brightness)", "Edit ratchet count (dots under the pad)" };
        for (int m = 0; m < ui::MotifGrid::numModes; ++m)
        {
            auto& b = en.modes[(size_t) m];
            b.setButtonText (modeNames[m]);
            b.setTooltip (modeTips[m]);
            b.setClickingTogglesState (true);
            b.setRadioGroupId (100 + e);
            b.setColour (juce::TextButton::textColourOnId, engineColour (e));
            b.onClick = [this, e, m] { engines[(size_t) e].grid.setMode (m); };
            addAndMakeVisible (b);
        }
        en.modes[0].setToggleState (true, juce::dontSendNotification);

        en.mute.setTooltip ("Mute this engine");
        en.solo.setTooltip ("Solo this engine (several can be soloed)");
        en.arp.setTooltip ("How this engine picks notes from the chord on each trigger. Root plays the lowest chord note only.");
        en.octave.setTooltip ("Octave shift for this whole engine");
        en.channel.setTooltip ("MIDI channel this engine sends on");
        en.num.setTooltip ("Speed ratio n:d on top of the rate. 5:4 plays a quarter faster, 3:4 at three quarters speed.");
    }

    const char* handleNames[] { "CHORDS", "ENGINE 1", "ENGINE 2", "ENGINE 3", "ENGINE 4", "ALL TRACKS" };
    for (int h = 0; h < (int) dragButtons.size(); ++h)
    {
        auto& b = dragButtons[(size_t) h];
        const bool all = h == (int) dragButtons.size() - 1;
        const unsigned mask = all ? clip::allSources : 1u << h;
        const juce::String label = juce::String (handleNames[h]).replaceCharacter (' ', '_');
        b.setButtonText (handleNames[h]);
        b.setColour (juce::TextButton::textColourOffId, h == 0 ? colours::steel : all ? colours::cyan : engineColour (h - 1));
        b.setTooltip (all ? "Drag onto a DAW track to drop one MIDI file with a track per source, each on its own MIDI channel. Click to save it instead."
                          : "Drag onto a DAW track to drop this part as a MIDI clip. Click to save it as a .mid file instead.");
        b.prepareFile = [this, mask, label] { return exportCapture (mask, label); };
        b.onPlainClick = [this, mask, label] { saveCaptureAs (mask, label); };
        b.onNothingToDrag = [this] { setStatus ("Nothing captured yet. Press play and let it run for a bar or two."); };
    }

    suggestButton.setTooltip ("Write a chord progression for the chosen genre (and its scale) into the slots");
    suggestButton.onClick = [this] { suggestProgression(); };
    clearButton.setTooltip ("Forget the captured notes");
    clearButton.onClick = [this] { processor.clearCapture(); setStatus ("Capture cleared."); };
    linksButton.onClick = [this] { showLinksMenu(); };

    status.setFont (juce::FontOptions (11.5f));
    status.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    status.setJustificationType (juce::Justification::centredLeft);

    scaleLock.setTooltip ("On: every note an engine plays is snapped to the scale. Off: direct override, notes are exactly what the chord and pitch step say.");
    borrowScale.setTooltip ("The scale used for borrowed notes (modal interchange)");
    borrowAmount.setTooltip ("How many triggers snap to the borrow scale. Hold the mod wheel above half to borrow on every note.");
    jitter.setTooltip ("Gaussian micro-timing drift per note, up to this many milliseconds early or late");
    slip.setTooltip ("Random variation of each note's length");
    baseOctave.setTooltip ("Octave the chords are built in. Engines shift from there with their own octave setting.");
    run.setTooltip ("Only used when the host provides no transport, e.g. in the Standalone app");
    chordSource.setTooltip ("Internal: the progression below. Live MIDI: notes you hold (or a MIDI clip of chords) fill the buffer.");
    chordTiming.setTooltip ("On beat, an eighth early (anticipated) or an eighth late (off-beat)");
    chordOut.setTooltip ("Send the internal chords as MIDI on their own channel (internal source only)");
    genre.setTooltip ("Style used by the SUGGEST button");

    setSize (editorW, editorH);
    timerCallback();
    startTimerHz (30);
}

EmberlinePolyEditor::~EmberlinePolyEditor() { setLookAndFeel (nullptr); }

// ---------------------------------------------------------------- layout
void EmberlinePolyEditor::resized()
{
    panels = { juce::Rectangle<int> (14, rowAY, 440, rowAH), juce::Rectangle<int> (464, rowAY, 170, rowAH), juce::Rectangle<int> (644, rowAY, 382, rowAH),
               juce::Rectangle<int> (14, rowBY, 1012, rowBH), juce::Rectangle<int> (14, rowCY, 1012, rowCH) };

    // row A
    root.setBounds (26, 80, 64, 40);
    scale.setBounds (98, 80, 150, 40);
    scaleLock.setBounds (256, 80, 84, 40);
    borrowScale.setBounds (26, 120, 150, 40);
    borrowAmount.setBounds (350, 76, 100, 84);
    jitter.setBounds (474, 76, 76, 84);
    slip.setBounds (554, 76, 76, 84);
    baseOctave.setBounds (656, 80, 66, 40);
    captureBars.setBounds (730, 80, 82, 40);
    run.setBounds (820, 80, 56, 40);
    tempo.setBounds (884, 80, 130, 40);

    // row B: chords
    chordSource.setBounds (26, 190, 96, 40);
    genre.setBounds (130, 190, 128, 40);
    suggestButton.setBounds (266, 203, 96, 26);
    chordLength.setBounds (26, 230, 56, 40);
    chordBars.setBounds (90, 230, 86, 40);
    chordTiming.setBounds (184, 230, 110, 40);
    chordOut.setBounds (302, 230, 64, 40);
    chordChannel.setBounds (374, 230, 56, 40);
    for (int c = 0; c < seq::maxChordSlots; ++c)
    {
        chordSlots[(size_t) c].degree.setBounds (446 + c * 72, 186, 66, 40);
        chordSlots[(size_t) c].extension.setBounds (446 + c * 72, 228, 66, 40);
    }

    // row C: export
    int x = 26;
    for (size_t h = 0; h < dragButtons.size(); ++h)
    {
        const int w = h == dragButtons.size() - 1 ? 150 : 108;
        dragButtons[h].setBounds (x, 300, w, 32);
        x += w + 8;
    }
    clearButton.setBounds (x + 8, 300, 64, 32);
    linksButton.setBounds (x + 80, 300, 70, 32);
    status.setBounds (26, 336, 980, 18);

    // motif engines
    for (int e = 0; e < seq::numEngines; ++e)
    {
        const int y = enginesY + e * (engineH + engineGap);
        engineBounds[(size_t) e] = { headerX, y, headerW, engineH };
        auto& en = engines[(size_t) e];

        en.mute.setBounds (headerX + 248, y + 5, 34, 20);
        en.solo.setBounds (headerX + 288, y + 5, 34, 20);
        en.steps.setBounds (headerX + 8, y + 22, 84, 86);
        en.rate.setBounds (headerX + 98, y + 24, 110, 40);
        en.num.setBounds (headerX + 98, y + 68, 48, 40);
        en.den.setBounds (headerX + 160, y + 68, 48, 40);
        ratioColon[e] = { headerX + 146, y + 82, 14, 20 };
        en.arp.setBounds (headerX + 216, y + 24, 106, 40);
        en.octave.setBounds (headerX + 216, y + 68, 50, 40);
        en.channel.setBounds (headerX + 272, y + 68, 50, 40);

        for (int m = 0; m < ui::MotifGrid::numModes; ++m)
            en.modes[(size_t) m].setBounds (gridX + m * 58, y, 54, 20);
        en.grid.setBounds (gridX, y + 24, editorW - 14 - gridX, engineH - 24);
    }
}

// ---------------------------------------------------------------- painting
void EmberlinePolyEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (colours::bgPanel, 0.0f, 0.0f, colours::bgDeep, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // faint horizon grid in the lower corners, echoing the cover art
    g.setColour (colours::cyan.withAlpha (0.035f));
    for (int i = 0; i < 12; ++i)
        g.drawHorizontalLine (getHeight() - 8 - i * i * 2, 0.0f, (float) getWidth());

    // header: gradient-glow title with the phoenix mark on both sides
    const juce::String title = "EMBERLINE POLY";
    juce::GlyphArrangement ga;
    const juce::Font font (juce::FontOptions (31.0f, juce::Font::bold).withKerningFactor (0.14f));
    ga.addLineOfText (font, title, 0.0f, 0.0f);
    const auto head = getLocalBounds().removeFromTop (headerH).toFloat();

    juce::Path text;
    ga.createPath (text);
    const auto raw = text.getBounds();
    text.applyTransform (juce::AffineTransform::translation (head.getCentreX() - raw.getCentreX(), head.getCentreY() + 2.0f - raw.getCentreY()));
    const auto tb = text.getBounds();
    juce::ColourGradient grad (colours::cyan, tb.getX(), 0.0f, colours::coral, tb.getRight(), 0.0f, false);
    grad.addColour (0.5, colours::cyan.interpolatedWith (colours::magenta, 0.55f));   // avoids a grey midpoint

    for (float width : { 11.0f, 7.0f, 3.5f })
    {
        auto glow = grad;
        glow.multiplyOpacity (width > 10.0f ? 0.07f : width > 5.0f ? 0.11f : 0.18f);
        g.setGradientFill (glow);
        g.strokePath (text, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setGradientFill (grad);
    g.fillPath (text);

    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.setFont (juce::FontOptions (10.5f));
    g.drawText (ember::version::full(), juce::Rectangle<int> (66, 40, 222, 13), juce::Justification::centredLeft);

    ember::drawPhoenix (g, phoenix, juce::Rectangle<float> (16.0f, 6.0f, 42.0f, 42.0f), 0.55f);
    ember::drawPhoenix (g, phoenix, juce::Rectangle<float> ((float) getWidth() - 58.0f, 6.0f, 42.0f, 42.0f), 0.55f);

    // panels
    const char* panelTitles[] { "SCALE & CHORD GUARD", "HUMANIZE", "GLOBAL", "CHORD MEMORY & PROGRESSION", "EXPORT TO DAW" };
    const juce::Colour panelColours[] { colours::cyan, colours::amber, colours::steel, colours::steel, colours::cyan };
    for (size_t i = 0; i < panels.size(); ++i)
    {
        const auto b = panels[i].toFloat();
        g.setColour (colours::panelFill);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (panelColours[i].withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (panelColours[i]);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.1f));
        g.drawText (panelTitles[i], panels[i].reduced (12, 6).removeFromTop (14), juce::Justification::centredLeft);
    }

    // engine headers
    for (int e = 0; e < seq::numEngines; ++e)
    {
        const auto b = engineBounds[(size_t) e].toFloat();
        const auto c = engineColour (e);
        g.setColour (colours::panelFill);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (c.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

        const char* roles[] { "BASS", "LEAD", "PAD", "ACCENT" };
        g.setColour (c);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold).withKerningFactor (0.1f));
        g.drawText ("ENGINE " + juce::String (e + 1), engineBounds[(size_t) e].reduced (10, 6).removeFromTop (14).removeFromLeft (90), juce::Justification::centredLeft);
        g.setColour (c.withAlpha (0.5f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (roles[e], engineBounds[(size_t) e].reduced (10, 6).removeFromTop (14).withTrimmedLeft (86).withWidth (60), juce::Justification::centredLeft);

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText (":", ratioColon[e], juce::Justification::centred);

        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("drag to edit  /  double-click resets", juce::Rectangle<int> (gridX + 5 * 58 + 6, engineBounds[(size_t) e].getY(), 300, 20), juce::Justification::centredLeft);
    }

    // footer
    g.setColour (juce::Colours::white.withAlpha (0.36f));
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Pad height is velocity, width is gate, brightness is probability, dots are ratchets, the number is the pitch shift. Drop a .mid file on an engine to load it.",
                juce::Rectangle<int> (14, getHeight() - 24, 1010, 16), juce::Justification::centredLeft);
}

void EmberlinePolyEditor::paintOverChildren (juce::Graphics& g)
{
    if (! dropHint)
        return;

    const auto b = getLocalBounds().toFloat().reduced (6.0f);
    g.setColour (colours::bgDeep.withAlpha (0.78f));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (colours::cyan);
    g.drawRoundedRectangle (b, 10.0f, 2.0f);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold).withKerningFactor (0.08f));
    g.drawText ("DROP A MIDI FILE ON AN ENGINE TO LOAD IT", getLocalBounds(), juce::Justification::centred);
}

// ---------------------------------------------------------------- live updates
int EmberlinePolyEditor::captureBarsSelected() const
{
    return params::captureBars[(size_t) juce::jlimit (0, 5, (int) processor.apvts.getRawParameterValue ("capture_bars")->load())];
}

void EmberlinePolyEditor::timerCallback()
{
    for (int e = 0; e < seq::numEngines; ++e)
        engines[(size_t) e].grid.setPlayhead (processor.playheadStep (e));

    const int playing = processor.chordSlot();
    const int length = (int) processor.apvts.getRawParameterValue ("chord_length")->load() + 1;
    for (int c = 0; c < seq::maxChordSlots; ++c)
    {
        chordSlots[(size_t) c].degree.setHighlight (c == playing);
        const float alpha = c < length ? 1.0f : 0.35f;
        chordSlots[(size_t) c].degree.setAlpha (alpha);
        chordSlots[(size_t) c].extension.setAlpha (alpha);
    }

    if (statusCountdown > 0)
    {
        --statusCountdown;
        return;
    }

    if (++statusTicks % 10 == 0)
    {
        const auto notes = processor.capture().snapshot();
        const int bars = captureBarsSelected();
        const int inWindow = clip::countInWindow (notes, clip::allSources, bars);
        const auto w = clip::chooseWindow (notes, bars);

        const juce::String text = inWindow == 0 ? "Nothing captured yet. Play the transport to record the patterns."
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

void EmberlinePolyEditor::setStatus (const juce::String& text)
{
    status.setText (text, juce::dontSendNotification);
    lastCaptureText = {};
    statusCountdown = 90;   // keep the message for about three seconds
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

// ---------------------------------------------------------------- MIDI export / import
juce::File EmberlinePolyEditor::exportCapture (unsigned sourceMask, const juce::String& label)
{
    const double bpm = processor.currentBpm();
    const auto folder = exportFolder();
    clip::deleteStaleExports (folder);
    const auto file = folder.getChildFile ("EmberlinePoly_" + label + "_" + juce::String (juce::roundToInt (bpm)) + "bpm_"
                                           + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".mid");
    return clip::write (processor.capture().snapshot(), sourceMask, captureBarsSelected(), bpm, file) > 0 ? file : juce::File();
}

void EmberlinePolyEditor::saveCaptureAs (unsigned sourceMask, const juce::String& label)
{
    chooser = std::make_shared<juce::FileChooser> ("Save the captured pattern as a MIDI file",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("EmberlinePoly_" + label + ".mid"),
                                                   "*.mid");
    juce::Component::SafePointer<EmberlinePolyEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe, sourceMask] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;

        const auto target = fc.getResult().withFileExtension ("mid");
        const int count = clip::write (safe->processor.capture().snapshot(), sourceMask, safe->captureBarsSelected(), safe->processor.currentBpm(), target);
        safe->setStatus (count > 0 ? "Saved " + target.getFileName() : "Nothing captured for that part yet. Press play first.");
    });
}

bool EmberlinePolyEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("mid;midi"))
            return true;
    return false;
}

void EmberlinePolyEditor::filesDropped (const juce::StringArray& files, int, int y)
{
    dropHint = false;
    repaint();

    int engine = 0;   // the engine under the drop, or the first one
    for (int e = 0; e < seq::numEngines; ++e)
        if (y >= engineBounds[(size_t) e].getY())
            engine = e;

    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("mid;midi"))
        {
            clip::Imported imported;
            if (clip::importFile (juce::File (f), imported))
            {
                applyImport (engine, imported);
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
