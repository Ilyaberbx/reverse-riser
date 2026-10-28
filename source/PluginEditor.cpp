#include "PluginEditor.h"

namespace
{
    // round edges, not rectangles, so neighbours never overlap by a pixel
    juce::Rectangle<int> snap (juce::Rectangle<float> r)
    {
        return juce::Rectangle<int>::leftTopRightBottom (juce::roundToInt (r.getX()), juce::roundToInt (r.getY()),
            juce::roundToInt (r.getRight()), juce::roundToInt (r.getBottom()));
    }
    constexpr float baseWidth = (float) PluginEditor::defaultWidth, baseHeight = (float) PluginEditor::defaultHeight;
    constexpr int firstUserPresetId = 101;
    const juce::Colour syllableMarker { 0xffe8685a };

    // the settings behind each FX toggle, in fxButtons order
    const std::array<std::vector<const char*>, 4> fxSettingIds { {
        { "reverbType" }, { "chorusMix", "chorusRate" }, { "tremoloMix", "tremoloTiming" }, { "pitchStart", "pitchEnd", "pitchShape" },
    } };
    constexpr std::array<const char*, 4> fxToggleIds { "reverbOn", "chorusOn", "tremoloOn", "pitchOn" };
    constexpr std::array<const char*, 4> fxTips {
        "Reverb: the space the riser is made from. Click the name to pick one; off uses the default hall.",
        "Chorus: a wide, moving double of the riser. Click the name for Mix and Rate.",
        "Tremolo: chops the riser in time with the song. Click the name for Mix and Timing.",
        "Pitch: bends the riser's pitch from Start to End. Click the name to shape the curve.",
    };
}

//==============================================================================
PluginEditor::IconButton::IconButton (Icon i, const juce::String& name) : juce::Button (name), icon (i)
{
    setTooltip (name);
}

void PluginEditor::IconButton::setScale (float newU)
{
    u = newU;
    raisedShadow = Theme::raised (Theme::palette(), 2.5f * u, 7.0f * u);
    pressedShadow = Theme::pressed (Theme::palette(), 2.0f * u, 5.0f * u);
    repaint();
}

void PluginEditor::IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto& p = Theme::palette();
    const auto bounds = getLocalBounds().toFloat().reduced (3.0f * u);
    juce::Path shape;
    shape.addEllipse (bounds);
    if (down)
    {
        g.setColour (p.screen);
        g.fillPath (shape);
        pressedShadow.render (g, shape);
    }
    else
    {
        raisedShadow.render (g, shape);
        Theme::fillFace (g, shape, bounds, p);
    }

    const auto c = bounds.getCentre();
    const auto s = bounds.getWidth() * 0.16f;
    juce::Path glyph;
    switch (icon)
    {
        case Icon::previous:
            glyph.startNewSubPath (c.translated (s * 0.4f, -s));
            glyph.lineTo (c.translated (-s * 0.6f, 0.0f));
            glyph.lineTo (c.translated (s * 0.4f, s));
            break;
        case Icon::next:
            glyph.startNewSubPath (c.translated (-s * 0.4f, -s));
            glyph.lineTo (c.translated (s * 0.6f, 0.0f));
            glyph.lineTo (c.translated (-s * 0.4f, s));
            break;
        case Icon::save:
            // arrow down into a tray
            glyph.startNewSubPath (c.translated (0.0f, -s * 1.1f));
            glyph.lineTo (c.translated (0.0f, s * 0.4f));
            glyph.startNewSubPath (c.translated (-s * 0.55f, -s * 0.15f));
            glyph.lineTo (c.translated (0.0f, s * 0.4f));
            glyph.lineTo (c.translated (s * 0.55f, -s * 0.15f));
            glyph.startNewSubPath (c.translated (-s * 1.1f, s * 0.5f));
            glyph.lineTo (c.translated (-s * 1.1f, s * 1.1f));
            glyph.lineTo (c.translated (s * 1.1f, s * 1.1f));
            glyph.lineTo (c.translated (s * 1.1f, s * 0.5f));
            break;
        case Icon::menu:
            for (auto dy : { -0.8f, 0.0f, 0.8f })
            {
                glyph.startNewSubPath (c.translated (-s * 1.1f, dy * s));
                glyph.lineTo (c.translated (s * 1.1f, dy * s));
            }
            break;
    }
    g.setColour (highlighted || down ? p.textPrimary : p.textMuted);
    g.strokePath (glyph, juce::PathStrokeType (1.6f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
PluginEditor::Caption::Caption (juce::Slider& source, const juce::String& n, Theme::Type nr, Theme::Type vr)
    : slider (source), name (n.toUpperCase()), nameRole (nr), valueRole (vr)
{
    slider.addListener (this);
    setInterceptsMouseClicks (false, false);
}

void PluginEditor::Caption::paint (juce::Graphics& g)
{
    const auto& p = Theme::palette();
    juce::AttributedString text;
    text.append (name + "  ", Theme::font (nameRole, scale), p.textPrimary);
    text.append (slider.getTextFromValue (slider.getValue()).toUpperCase(), Theme::font (valueRole, scale), p.textMuted);
    text.setJustification (juce::Justification::centred);
    text.draw (g, getLocalBounds().toFloat());
}

//==============================================================================
// the icon is the left square of the button (see NeumorphicLookAndFeel::drawToggleButton); the name opens the settings
void PluginEditor::FxButton::mouseDown (const juce::MouseEvent& e)
{
    settingsPress = onSettings != nullptr && (float) e.x >= (float) getHeight() * 1.1f;
    if (settingsPress)
        onSettings();
    else
        juce::ToggleButton::mouseDown (e);
}

void PluginEditor::FxButton::mouseDrag (const juce::MouseEvent& e)
{
    if (! settingsPress)
        juce::ToggleButton::mouseDrag (e);
}

void PluginEditor::FxButton::mouseUp (const juce::MouseEvent& e)
{
    if (! settingsPress)
        juce::ToggleButton::mouseUp (e);
}

//==============================================================================
PluginEditor::RenderSwitch::RenderSwitch() : juce::Button ("Render")
{
    setClickingTogglesState (true);
    setTooltip ("RAW: the plain reverse reverb. FX: with Chorus, Tremolo and Pitch. LISTEN and the drag use this one.");
}

void PluginEditor::RenderSwitch::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto& p = Theme::palette();
    const auto font = Theme::font (Theme::Type::tab, u);
    auto area = getLocalBounds().toFloat();
    const auto fx = getToggleState();
    g.setFont (font);
    g.setColour (fx ? Theme::accent : (highlighted ? p.textPrimary : p.textMuted));
    g.drawText ("FX", area.removeFromRight (area.getWidth() * 0.36f), juce::Justification::centredLeft);
    area.removeFromRight (8.0f * u);
    g.setColour (fx ? (highlighted ? p.textPrimary : p.textMuted) : Theme::accent);
    g.drawText ("RAW", area, juce::Justification::centredRight);
}

//==============================================================================
PluginEditor::RiserView::RiserView (PluginProcessor& p)
    : processor (p), renderAttachment (p.apvts, "fx", renderSwitch)
{
    resetButton.setTooltip ("Clear the riser and wait for the next phrase.");
    listenButton.setTooltip ("Play the riser through the plugin's output.");
    loopButton.setTooltip ("Keep LISTEN playing the riser over and over.");
    listenButton.setClickingTogglesState (true);
    loopButton.setClickingTogglesState (true);
    for (auto* c : std::initializer_list<juce::Component*> { &resetButton, &listenButton, &loopButton, &renderSwitch })
        addAndMakeVisible (c);
}

void PluginEditor::RiserView::setScale (float newU)
{
    u = newU;
    inset = Theme::pressed (Theme::palette(), 3.0f * u, 9.0f * u);
    renderSwitch.setScale (u);
    resized();
    repaint();
}

void PluginEditor::RiserView::resized()
{
    auto area = getLocalBounds().toFloat().reduced (12.0f * u, 8.0f * u);
    const auto row = 26.0f * u;
    auto top = area.removeFromTop (row);
    resetButton.setBounds (snap (top.removeFromRight (74.0f * u)));
    auto bottom = area.removeFromBottom (row);
    listenButton.setBounds (snap (bottom.removeFromLeft (78.0f * u)));
    bottom.removeFromLeft (4.0f * u);
    loopButton.setBounds (snap (bottom.removeFromLeft (70.0f * u)));
    renderSwitch.setBounds (snap (bottom.removeFromRight (72.0f * u)));
}

juce::Rectangle<float> PluginEditor::RiserView::waveArea() const
{
    return getLocalBounds().toFloat().reduced (20.0f * u, 0.0f).withTrimmedTop (40.0f * u).withTrimmedBottom (40.0f * u);
}

void PluginEditor::RiserView::paint (juce::Graphics& g)
{
    const auto& p = Theme::palette();
    const auto bounds = getLocalBounds().toFloat();
    juce::Path screen;
    screen.addRoundedRectangle (bounds, 18.0f * u);
    g.setColour (p.screen);
    g.fillPath (screen);
    inset.render (g, screen);

    // wordmark: light + extra bold, widely tracked
    const auto light = Theme::font (Theme::Type::wordmarkLight, u), bold = Theme::font (Theme::Type::wordmarkBold, u);
    const auto origin = bounds.getTopLeft().translated (16.0f * u, 12.0f * u);
    const auto lightWidth = juce::GlyphArrangement::getStringWidth (light, "REVERSE ");
    g.setColour (p.textMuted);
    g.setFont (light);
    g.drawText ("REVERSE ", juce::Rectangle<float> (lightWidth + 2.0f, 20.0f * u).withPosition (origin), juce::Justification::centredLeft);
    g.setColour (p.textPrimary);
    g.setFont (bold);
    g.drawText ("RISER", juce::Rectangle<float> (120.0f * u, 20.0f * u).withPosition (origin.translated (lightWidth, 0.0f)), juce::Justification::centredLeft);

    const auto wave = waveArea();
    if (! processor.hasRiser())
    {
        g.setColour (p.textMuted);
        g.setFont (Theme::font (Theme::Type::menu, u));
        g.drawFittedText ("Play your vocal.\nIts first syllable becomes the riser.", wave.toNearestInt(), juce::Justification::centred, 2);
        return;
    }

    // the riser: bars that swell into the vocal, fading in from the left
    const auto& riser = processor.riser();
    const auto barStep = 5.0f * u;
    const auto bars = juce::jmax (1, (int) (wave.getWidth() / barStep));
    const auto perBar = juce::jmax (1, riser.getNumSamples() / bars);
    for (int b = 0; b < bars; ++b)
    {
        const auto start = b * perBar;
        const auto level = riser.getMagnitude (start, juce::jmin (perBar, riser.getNumSamples() - start));
        const auto t = (float) b / (float) bars;
        // dB scale (-48..0), so the whole swell reads, not just its last beat
        const auto db = juce::jlimit (0.0f, 1.0f, 1.0f + juce::Decibels::gainToDecibels (level, -48.0f) / 48.0f);
        const auto h = juce::jmax (1.5f * u, db * wave.getHeight() * 0.9f);
        g.setColour (Theme::accent.withAlpha (0.2f + 0.8f * t));
        g.fillRoundedRectangle (juce::Rectangle<float> (wave.getX() + (float) b * barStep, wave.getCentreY() - h * 0.5f, 2.0f * u, h), 1.0f * u);
    }

    // pitch curve over the FX render: +24 st at the top, -24 st at the bottom
    const auto fx = processor.fxSettings();
    if (fx.pitch && processor.apvts.getRawParameterValue ("fx")->load() > 0.5f)
    {
        juce::Path curve;
        for (int i = 0; i <= 64; ++i)
        {
            const auto t = (float) i / 64.0f;
            const juce::Point<float> pt { wave.getX() + t * wave.getWidth(), wave.getCentreY() - Riser::pitchAt (fx, t) / 24.0f * wave.getHeight() * 0.5f };
            i == 0 ? curve.startNewSubPath (pt) : curve.lineTo (pt);
        }
        g.setColour (p.textPrimary.withAlpha (0.7f));
        g.strokePath (curve, juce::PathStrokeType (1.5f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    paintSyllable (g, { bounds.getRight() - 196.0f * u, bounds.getY() + 10.0f * u, 104.0f * u, 26.0f * u });

    if (isMouseOver (true))
    {
        const auto zone = wave.reduced (0.0f, 4.0f * u);
        juce::Path outline, dashed;
        outline.addRoundedRectangle (zone, 10.0f * u);
        const float dashes[] = { 4.0f * u, 4.0f * u };
        juce::PathStrokeType (1.2f * u).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (Theme::accent);
        g.fillPath (dashed);
        g.setColour (p.textPrimary);
        g.setFont (Theme::font (Theme::Type::preset, u));
        g.drawText ("Drag & Drop into DAW", zone, juce::Justification::centred);
    }
}

// the captured audio, with red markers around the syllable the riser is made from
void PluginEditor::RiserView::paintSyllable (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const auto& audio = processor.capturedAudio();
    const auto n = audio.getNumSamples();
    if (n == 0)
        return;

    const auto& p = Theme::palette();
    const auto columns = juce::jmax (1, (int) (area.getWidth() / (2.0f * u)));
    for (int col = 0; col < columns; ++col)
    {
        const auto start = col * n / columns;
        const auto level = audio.getMagnitude (start, juce::jmax (1, n / columns));
        const auto h = juce::jmax (1.0f, level * area.getHeight());
        g.setColour (p.textMuted.withAlpha (0.6f));
        g.fillRect (area.getX() + (float) col * 2.0f * u, area.getCentreY() - h * 0.5f, 1.0f * u, h);
    }
    g.setColour (syllableMarker);
    const auto range = processor.syllableRange();
    for (auto s : { range.getStart(), range.getEnd() })
    {
        const auto x = area.getX() + area.getWidth() * (float) s / (float) n;
        g.drawLine (x, area.getY(), x, area.getBottom(), 1.5f * u);
    }
}

void PluginEditor::RiserView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || ! processor.hasRiser() || e.getDistanceFromDragStart() < 6)
        return;
    dragging = true;
    if (const auto file = processor.exportRiser(); file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

//==============================================================================
// sliders (and the reverb dropdown) for one effect, shown in a call-out
class PluginEditor::FxPanel : public juce::Component
{
public:
    FxPanel (juce::AudioProcessorValueTreeState& apvts, const std::vector<const char*>& ids, float scale) : u (scale)
    {
        const auto& p = Theme::palette();
        for (const auto* id : ids)
        {
            auto* param = apvts.getParameter (id);
            auto& row = rows.emplace_back();
            if (std::string_view (id) == "reverbType")
            {
                row.combo = std::make_unique<juce::ComboBox>();
                row.combo->addItemList (dynamic_cast<juce::AudioParameterChoice*> (param)->choices, 1);
                row.comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, *row.combo);
                row.combo->setTitle (param->getName (32));
                addAndMakeVisible (*row.combo);
                continue;
            }
            row.slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
            row.slider->setColour (juce::Slider::trackColourId, Theme::accent);
            row.slider->setColour (juce::Slider::backgroundColourId, p.base);
            row.slider->setColour (juce::Slider::thumbColourId, p.faceTop);
            row.slider->setTitle (param->getName (32));
            row.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *row.slider);
            // "Chorus Mix" -> "MIX"
            row.caption = std::make_unique<Caption> (*row.slider, param->getName (32).fromFirstOccurrenceOf (" ", false, false),
                Theme::Type::knobLabel, Theme::Type::knobValue);
            row.caption->setScale (u);
            addAndMakeVisible (*row.slider);
            addAndMakeVisible (*row.caption);
        }
        setSize (juce::roundToInt (220.0f * u), juce::roundToInt ((float) rows.size() * 52.0f * u + 12.0f * u));
    }

    void resized() override
    {
        auto area = getLocalBounds().toFloat().reduced (10.0f * u, 6.0f * u);
        for (auto& row : rows)
        {
            auto cell = area.removeFromTop (52.0f * u);
            if (row.combo != nullptr)
            {
                row.combo->setBounds (snap (cell.withSizeKeepingCentre (cell.getWidth(), 34.0f * u)));
                continue;
            }
            row.caption->setBounds (snap (cell.removeFromTop (22.0f * u)));
            row.slider->setBounds (snap (cell.reduced (0.0f, 2.0f * u)));
        }
    }

private:
    struct Row
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<Caption> caption;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
    };
    std::vector<Row> rows;
    float u;
};

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      lengthAttachment (p.apvts, "length", lengthKnob)
{
    setLookAndFeel (&theme);
    setOpaque (true);

    refreshPresetList();
    presetBox.onChange = [this] {
        const auto id = presetBox.getSelectedId();
        if (id >= firstUserPresetId)
            processorRef.loadUserPreset (userPresets[id - firstUserPresetId]);
        else if (id > 0)
            processorRef.setCurrentProgram (id - 1);
    };
    presetBox.setTooltip ("Choose a preset");
    previousButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { savePreset(); };
    menuButton.onClick = [this] { showMenu(); };
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &previousButton, &nextButton, &saveButton, &menuButton, &riserView })
        addAndMakeVisible (c);

    riserView.resetButton.onClick = [this] { processorRef.resetCapture(); riserView.repaint(); };
    riserView.listenButton.onClick = [this] { processorRef.setListening (riserView.listenButton.getToggleState()); };
    riserView.loopButton.onClick = [this] { processorRef.setLooping (riserView.loopButton.getToggleState()); };

    lengthKnob.getProperties().set ("hero", true);
    lengthKnob.setTitle ("Reverse");
    lengthKnob.setTooltip ("Reverse: how long the riser swells before the vocal, in bars of the song's tempo.");
    addAndMakeVisible (lengthKnob);
    addAndMakeVisible (lengthCaption);

    for (size_t i = 0; i < fxButtons.size(); ++i)
    {
        auto& button = fxButtons[i];
        fxAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.apvts, fxToggleIds[i], button));
        button.setTooltip (fxTips[i]);
        button.onSettings = [this, &button, i] { showFxPanel (button, fxSettingIds[i]); };
        addAndMakeVisible (button);
    }

    setResizable (true, true);
    setResizeLimits (380, 520, 1800, 1800);
    const auto& state = processorRef.apvts.state;
    setSize ((int) state.getProperty ("uiWidth", defaultWidth), (int) state.getProperty ("uiHeight", defaultHeight));

    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < processorRef.getNumPrograms(); ++i)
        presetBox.addItem (processorRef.getProgramName (i), i + 1);

    userPresets = PluginProcessor::userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.xml");
    userPresets.sort();
    if (! userPresets.isEmpty())
        presetBox.addSeparator();
    for (int i = 0; i < userPresets.size(); ++i)
        presetBox.addItem (userPresets[i].getFileNameWithoutExtension(), firstUserPresetId + i);
    presetBox.setSelectedId (processorRef.getCurrentProgram() + 1, juce::dontSendNotification);
}

void PluginEditor::stepPreset (int delta)
{
    const auto count = presetBox.getNumItems();
    presetBox.setSelectedItemIndex ((presetBox.getSelectedItemIndex() + delta + count) % count, juce::sendNotification);
}

void PluginEditor::savePreset()
{
    const auto folder = PluginProcessor::userPresetFolder();
    folder.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save preset", folder.getChildFile ("My Riser.xml"), "*.xml");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc) {
        const auto file = fc.getResult();
        if (file == juce::File())
            return;
        processorRef.saveUserPreset (file.withFileExtension ("xml"));
        refreshPresetList();
        if (const auto index = userPresets.indexOf (file.withFileExtension ("xml")); index >= 0)
            presetBox.setSelectedId (firstUserPresetId + index, juce::dontSendNotification);
    });
}

void PluginEditor::showMenu()
{
    juce::PopupMenu menu;
    menu.addItem ("Light theme", true, ! Theme::isDark(), [this] { Theme::setDark (false); applyTheme(); });
    menu.addItem ("Dark theme", true, Theme::isDark(), [this] { Theme::setDark (true); applyTheme(); });
    menu.addSeparator();
    menu.addItem ("Show user presets", [] { PluginProcessor::userPresetFolder().startAsProcess(); });
    menu.addItem ("Reset window size", [this] { setSize (defaultWidth, defaultHeight); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton));
}

void PluginEditor::applyTheme()
{
    resized(); // rebuilds every shadow recipe for the new palette
    sendLookAndFeelChange();
    repaint();
}

void PluginEditor::showFxPanel (juce::Component& anchor, std::vector<const char*> ids)
{
    juce::CallOutBox::launchAsynchronously (std::make_unique<FxPanel> (processorRef.apvts, ids, u), getLocalArea (&anchor, anchor.getLocalBounds()), this);
}

// picks up captures and setting changes, keeps LISTEN and the preset in sync
void PluginEditor::timerCallback()
{
    if (processorRef.updateRiser())
        riserView.repaint();

    if (riserView.listenButton.getToggleState() != processorRef.isListening())
        riserView.listenButton.setToggleState (processorRef.isListening(), juce::dontSendNotification);

    if (const auto program = processorRef.getCurrentProgram(); program != shownProgram)
    {
        shownProgram = program;
        if (presetBox.getSelectedId() < firstUserPresetId)
            presetBox.setSelectedId (program + 1, juce::dontSendNotification);
    }
}

//==============================================================================
void PluginEditor::paint (juce::Graphics& g)
{
    const auto& p = Theme::palette();
    const auto bounds = getLocalBounds().toFloat();

    // base, lit softly from the top
    g.fillAll (p.base);
    g.setGradientFill ({ p.faceTop.withAlpha (p.dark ? 0.25f : 0.6f), { bounds.getCentreX(), 0.0f },
        p.faceTop.withAlpha (0.0f), { bounds.getCentreX(), bounds.getHeight() * 0.7f }, true });
    g.fillRect (bounds);
    Theme::drawCornerGrips (g, gripArea, u);
}

void PluginEditor::resized()
{
    const auto w = (float) getWidth(), h = (float) getHeight();
    u = juce::jlimit (0.75f, 2.2f, juce::jmin (w / baseWidth, h / baseHeight));

    theme.setScale (u);
    for (auto* b : { &previousButton, &nextButton, &saveButton, &menuButton })
        b->setScale (u);
    lengthCaption.setScale (u);
    riserView.setScale (u);

    const auto pad = 22.0f * u, gap = 16.0f * u, barHeight = 34.0f * u, captionHeight = 26.0f * u, fxHeight = 30.0f * u;
    gripArea = getLocalBounds().toFloat().reduced (10.0f * u);
    auto area = getLocalBounds().toFloat().reduced (pad);
    // one column; wide windows centre it rather than stretch it
    area = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), h * 0.9f), area.getHeight());

    auto bar = area.removeFromTop (barHeight);
    menuButton.setBounds (snap (bar.removeFromRight (barHeight)));
    bar.removeFromLeft (barHeight); // keeps the preset group optically centred
    auto group = bar.reduced (8.0f * u, 0.0f).withSizeKeepingCentre (juce::jmin (bar.getWidth() - 16.0f * u, 340.0f * u), barHeight);
    saveButton.setBounds (snap (group.removeFromRight (barHeight)));
    group.removeFromRight (6.0f * u);
    previousButton.setBounds (snap (group.removeFromLeft (barHeight)));
    nextButton.setBounds (snap (group.removeFromRight (barHeight)));
    presetBox.setBounds (snap (group.reduced (6.0f * u, 2.0f * u)));
    area.removeFromTop (gap);

    // FX row at the bottom, four toggles spread evenly
    auto fxRow = area.removeFromBottom (fxHeight);
    const auto fxWidth = fxRow.getWidth() / (float) fxButtons.size();
    for (auto& button : fxButtons)
        button.setBounds (snap (fxRow.removeFromLeft (fxWidth).withSizeKeepingCentre (juce::jmin (fxWidth, 104.0f * u), fxHeight)));
    area.removeFromBottom (gap);

    // the hero takes the lion's share; the screen gets the rest
    const auto rest = area.getHeight() - gap - captionHeight;
    const auto heroSize = juce::jmin (area.getWidth() * 0.7f, rest * 0.6f);
    const auto screenHeight = juce::jlimit (150.0f * u, 320.0f * u, rest - heroSize);
    riserView.setBounds (snap (area.removeFromTop (screenHeight)));
    area.removeFromTop (gap);
    lengthCaption.setBounds (snap (area.removeFromBottom (captionHeight)));
    const auto size = juce::jmin (area.getWidth(), area.getHeight());
    lengthKnob.setBounds (snap (area.withSizeKeepingCentre (size, size)));

    processorRef.apvts.state.setProperty ("uiWidth", getWidth(), nullptr);
    processorRef.apvts.state.setProperty ("uiHeight", getHeight(), nullptr);
}
