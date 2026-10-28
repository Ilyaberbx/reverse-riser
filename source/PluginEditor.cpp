#include "PluginEditor.h"

namespace
{
    // what each control does, shown on hover
    constexpr std::array<const char*, presetParamIds.size()> tips {
        "Size: how big the space is. Bigger rooms give longer, smoother reflections.",
        "Decay: how long the reverb tail rings out.",
        "Pre-delay: a gap before the reverb starts, so the dry sound stays clear.",
        "Mix: balance between your dry sound and the reverb.",
        "Damping: darkens the tail. Higher sounds warmer and softer.",
        "Diffusion: low gives distinct echoes, high gives a smooth wash.",
        "Mod rate: speed of the slow, chorus-like movement in the tail.",
        "Mod depth: how much the tail moves.",
        "Width: from mono to wide stereo.",
        "Shimmer: an octave-up sparkle that climbs in the tail.",
        "Spring: the boingy drip of a spring tank.",
        "Lo-fi: a gritty, band-limited, wobbly tape-style tail.",
        "Output: overall level.",
    };

    constexpr float minKnob = 56.0f, minTarget = 44.0f;

    // round edges, not rectangles, so neighbours never overlap by a pixel
    juce::Rectangle<int> snap (juce::Rectangle<float> r)
    {
        return juce::Rectangle<int>::leftTopRightBottom (juce::roundToInt (r.getX()), juce::roundToInt (r.getY()),
            juce::roundToInt (r.getRight()), juce::roundToInt (r.getBottom()));
    }
    constexpr float baseWidth = (float) PluginEditor::defaultWidth, baseHeight = (float) PluginEditor::defaultHeight;
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
    text.append (slider.getTextFromValue (slider.getValue()), Theme::font (valueRole, scale), p.textMuted);
    text.setJustification (justification);
    text.draw (g, getLocalBounds().toFloat());
}

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      freezeAttachment (p.apvts, "freeze", freezeButton)
{
    setLookAndFeel (&theme);
    setOpaque (true);

    for (int i = 0; i < processorRef.getNumPrograms(); ++i)
        presetBox.addItem (processorRef.getProgramName (i), i + 1);
    presetBox.onChange = [this] { processorRef.setCurrentProgram (presetBox.getSelectedId() - 1); };
    presetBox.setTooltip ("Choose a preset");
    previousButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    menuButton.onClick = [this] { showMenu(); };
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &previousButton, &nextButton, &menuButton })
        addAndMakeVisible (c);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& knob = knobs[i];
        auto* param = processorRef.apvts.getParameter (presetParamIds[i]);
        knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, presetParamIds[i], knob.slider);
        knob.slider.setTextValueSuffix (" " + param->getLabel());
        knob.slider.setTitle (param->getName (32));
        knob.slider.setTooltip (tips[i]);
        // Shift-drag switches to velocity mode for fine adjustments
        knob.slider.setVelocityModeParameters (0.5, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        knob.label.setText (param->getName (32).toUpperCase(), juce::dontSendNotification);
        knob.label.setJustificationType (juce::Justification::centred);
        knob.label.setInterceptsMouseClicks (false, false);
        addChildComponent (knob.slider);
        addChildComponent (knob.label);
    }

    auto& mix = knobs[Param::mix].slider;
    mix.getProperties().set ("hero", true);
    mix.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    mix.setVisible (true);
    knobs[Param::output].slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knobs[Param::output].slider.setVisible (true);
    outputCaption.setJustification (juce::Justification::centredLeft);
    addAndMakeVisible (mixCaption);
    addAndMakeVisible (outputCaption);

    using namespace Param;
    tabKnobs = { { { size, decay, predelay, diffusion }, { damping, modrate, moddepth, width }, { shimmer, spring, lofi } } };
    const std::array<const char*, 3> tabNames { "Space", "Tone", "Character" };
    for (size_t t = 0; t < tabs.size(); ++t)
    {
        tabs[t].setButtonText (tabNames[t]);
        tabs[t].setRadioGroupId (1);
        tabs[t].onClick = [this, t] { showTab ((int) t); };
        addAndMakeVisible (tabs[t]);
    }

    freezeButton.setTooltip ("Freeze: hold the current tail forever. New sound passes through dry.");
    addAndMakeVisible (freezeButton);

    setResizable (true, true);
    setResizeLimits (380, 520, 1800, 1800);
    const auto& state = processorRef.apvts.state;
    showTab (0);
    setSize ((int) state.getProperty ("uiWidth", defaultWidth), (int) state.getProperty ("uiHeight", defaultHeight));

    lastFrameMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::showTab (int index)
{
    currentTab = index;
    tabs[(size_t) index].setToggleState (true, juce::dontSendNotification);
    for (size_t t = 0; t < tabKnobs.size(); ++t)
        for (auto k : tabKnobs[t])
        {
            knobs[(size_t) k].slider.setVisible ((int) t == index);
            knobs[(size_t) k].label.setVisible ((int) t == index);
        }
}

void PluginEditor::stepPreset (int delta)
{
    const auto count = processorRef.getNumPrograms();
    processorRef.setCurrentProgram ((processorRef.getCurrentProgram() + delta + count) % count);
}

void PluginEditor::showMenu()
{
    juce::PopupMenu menu;
    menu.addItem ("Light theme", true, ! Theme::isDark(), [this] { Theme::setDark (false); applyTheme(); });
    menu.addItem ("Dark theme", true, Theme::isDark(), [this] { Theme::setDark (true); applyTheme(); });
    menu.addSeparator();
    menu.addItem ("Reset window size", [this] { setSize (defaultWidth, defaultHeight); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton));
}

void PluginEditor::applyTheme()
{
    resized(); // rebuilds every shadow recipe for the new palette
    sendLookAndFeelChange();
    repaint();
}

// double-click a knob to go back to the current preset's value
void PluginEditor::updateResetValues()
{
    const auto& preset = factoryPresets[(size_t) processorRef.getCurrentProgram()];
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i].slider.setDoubleClickReturnValue (true, preset.values[i]);
}

// one frame: pull output audio -> analyse -> move the trails -> redraw the screen; keep the preset in sync
void PluginEditor::timerCallback()
{
    const auto count = processorRef.readVisualizerSamples (incoming.data(), (int) incoming.size());
    analyser.pushSamples (incoming.data(), count);
    analyser.analyse();

    const auto now = juce::Time::getMillisecondCounterHiRes();
    phyllon.advance (analyser, (float) juce::jlimit (0.0, 0.1, (now - lastFrameMs) * 0.001));
    lastFrameMs = now;
    phyllon.render (juce::roundToInt (screenArea.getWidth()), juce::roundToInt (screenArea.getHeight()));

    if (const auto program = processorRef.getCurrentProgram(); program != shownProgram)
    {
        shownProgram = program;
        presetBox.setSelectedId (program + 1, juce::dontSendNotification);
        updateResetValues();
    }

    repaint (screenArea.expanded (80.0f * u).getIntersection (getLocalBounds().toFloat()).toNearestInt());
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

    // screen: glow spills around it, the visualizer sits recessed inside
    phyllon.paintHalo (g, screenArea, 60.0f * u);
    juce::Path screen;
    screen.addRoundedRectangle (screenArea, 18.0f * u);
    g.setColour (p.screen);
    g.fillPath (screen);
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (screen);
        phyllon.paint (g, screenArea);
    }
    screenInset.render (g, screen);

    // wordmark: light + extra bold, widely tracked
    const auto light = Theme::font (Theme::Type::wordmarkLight, u), bold = Theme::font (Theme::Type::wordmarkBold, u);
    const auto origin = screenArea.getTopLeft().translated (16.0f * u, 12.0f * u);
    const auto lightWidth = juce::GlyphArrangement::getStringWidth (light, "RE");
    g.setColour (p.textMuted);
    g.setFont (light);
    g.drawText ("RE", juce::Rectangle<float> (lightWidth + 2.0f, 20.0f * u).withPosition (origin), juce::Justification::centredLeft);
    g.setColour (p.textPrimary);
    g.setFont (bold);
    g.drawText ("VERB", juce::Rectangle<float> (120.0f * u, 20.0f * u).withPosition (origin.translated (lightWidth, 0.0f)), juce::Justification::centredLeft);

    // tab track
    juce::Path track;
    track.addRoundedRectangle (tabTrack, tabTrack.getHeight() * 0.5f);
    g.setColour (p.screen);
    g.fillPath (track);
    trackInset.render (g, track);
}

void PluginEditor::resized()
{
    const auto w = (float) getWidth(), h = (float) getHeight();
    u = juce::jlimit (0.75f, 2.2f, juce::jmin (w / baseWidth, h / baseHeight));
    const auto& p = Theme::palette();

    theme.setScale (u);
    screenInset = Theme::pressed (p, 3.0f * u, 9.0f * u);
    trackInset = Theme::pressed (p, 2.0f * u, 5.0f * u);
    for (auto* b : { &previousButton, &nextButton, &menuButton })
        b->setScale (u);
    mixCaption.setScale (u);
    outputCaption.setScale (u);

    const auto pad = 22.0f * u, gap = 16.0f * u, barHeight = 34.0f * u, captionHeight = 26.0f * u;
    const auto valueHeight = 18.0f * u, labelHeight = 16.0f * u, outputHeight = juce::jmax (minTarget, 44.0f * u);
    auto area = getLocalBounds().toFloat().reduced (pad);
    gripArea = getLocalBounds().toFloat().reduced (10.0f * u);
    const auto wide = w > 1.15f * h;

    const auto layoutTopBar = [&] (juce::Rectangle<float> bar) {
        const auto b = bar.getHeight();
        menuButton.setBounds (snap (bar.removeFromRight (b)));
        bar.removeFromLeft (b); // keeps the preset group optically centred
        auto group = bar.reduced (8.0f * u, 0.0f).withSizeKeepingCentre (juce::jmin (bar.getWidth() - 16.0f * u, 300.0f * u), b);
        previousButton.setBounds (snap (group.removeFromLeft (b)));
        nextButton.setBounds (snap (group.removeFromRight (b)));
        presetBox.setBounds (snap (group.reduced (6.0f * u, 2.0f * u)));
    };

    // knob grid: 4 across when there's room, otherwise 2×2; all tabs share the same cells
    const auto columnsFor = [&] (float width) { return width >= 4.0f * 84.0f * u ? 4 : 2; };
    const auto knobSizeFor = [&] (float width) {
        return juce::jlimit (juce::jmax (minKnob, 0.0f), juce::jmax (minKnob, 84.0f * u), width / (float) columnsFor (width) - 18.0f * u);
    };
    const auto gridHeightFor = [&] (float width) {
        const auto rows = 4 / columnsFor (width);
        return (float) rows * (knobSizeFor (width) + valueHeight + labelHeight + 6.0f * u);
    };
    const auto layoutGrid = [&] (juce::Rectangle<float> grid) {
        const auto columns = columnsFor (grid.getWidth());
        const auto rows = 4 / columns;
        const auto cellWidth = grid.getWidth() / (float) columns, cellHeight = grid.getHeight() / (float) rows;
        const auto knobSize = knobSizeFor (grid.getWidth());
        for (const auto& tab : tabKnobs)
            for (size_t i = 0; i < tab.size(); ++i)
            {
                const auto column = (int) i % columns, row = (int) i / columns;
                const auto inRow = juce::jmin (columns, (int) tab.size() - row * columns);
                const auto x = grid.getX() + (float) column * cellWidth + (float) (columns - inRow) * cellWidth * 0.5f;
                auto cell = juce::Rectangle<float> (x, grid.getY() + (float) row * cellHeight, cellWidth, cellHeight);
                auto& knob = knobs[(size_t) tab[i]];
                knob.label.setFont (Theme::font (Theme::Type::knobLabel, u));
                knob.label.setBounds (snap (cell.removeFromBottom (labelHeight)));
                knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::roundToInt (cellWidth), juce::roundToInt (valueHeight));
                knob.slider.setBounds (snap (cell.withSizeKeepingCentre (juce::jmin (cellWidth, knobSize + 24.0f * u), knobSize + valueHeight)));
            }
    };
    const auto layoutOutput = [&] (juce::Rectangle<float> row) {
        const auto knobSize = row.getHeight();
        auto group = row.withSizeKeepingCentre (juce::jmin (row.getWidth(), knobSize + 130.0f * u), knobSize);
        knobs[Param::output].slider.setBounds (snap (group.removeFromLeft (knobSize)));
        outputCaption.setBounds (snap (group.withTrimmedLeft (8.0f * u)));
    };
    const auto layoutTabs = [&] (juce::Rectangle<float> row) {
        tabTrack = row.withSizeKeepingCentre (juce::jmin (row.getWidth(), 330.0f * u), row.getHeight());
        auto cells = tabTrack;
        const auto tabWidth = cells.getWidth() / (float) tabs.size();
        for (auto& tab : tabs)
            tab.setBounds (snap (cells.removeFromLeft (tabWidth)));
    };
    const auto layoutHero = [&] (juce::Rectangle<float> space) {
        auto caption = space.removeFromBottom (captionHeight);
        const auto heroSize = juce::jmin (space.getWidth(), space.getHeight());
        const auto hero = space.withSizeKeepingCentre (heroSize, heroSize);
        knobs[Param::mix].slider.setBounds (snap (hero));
        mixCaption.setBounds (snap (caption));
    };

    if (wide)
    {
        // controls on the left, the hero gets the whole right column
        auto left = area.removeFromLeft (area.getWidth() * 0.52f);
        area.removeFromLeft (pad);
        layoutTopBar (left.removeFromTop (barHeight));
        left.removeFromTop (gap);
        layoutGrid (left.removeFromBottom (gridHeightFor (left.getWidth())));
        left.removeFromBottom (10.0f * u);
        layoutTabs (left.removeFromBottom (barHeight));
        left.removeFromBottom (gap);
        screenArea = left;

        layoutOutput (area.removeFromBottom (outputHeight));
        area.removeFromBottom (12.0f * u);
        layoutHero (area);
    }
    else
    {
        layoutTopBar (area.removeFromTop (barHeight));
        area.removeFromTop (gap);
        layoutOutput (area.removeFromBottom (outputHeight));
        area.removeFromBottom (14.0f * u);
        layoutGrid (area.removeFromBottom (gridHeightFor (area.getWidth())));
        area.removeFromBottom (10.0f * u);
        layoutTabs (area.removeFromBottom (barHeight));
        area.removeFromBottom (18.0f * u);

        // the hero takes the lion's share; the screen gets the rest, so tall windows don't leave dead space
        const auto rest = area.getHeight() - 14.0f * u - captionHeight;
        const auto heroSize = juce::jmin (area.getWidth() * 0.8f, rest * 0.68f);
        const auto screenHeight = juce::jlimit (64.0f * u, 340.0f * u, rest - heroSize);
        screenArea = area.removeFromTop (screenHeight);
        area.removeFromTop (14.0f * u);
        layoutHero (area.withSizeKeepingCentre (juce::jmin (area.getWidth() * 0.8f, area.getHeight()), area.getHeight()));
    }

    freezeButton.setBounds (snap (juce::Rectangle<float> (100.0f * u, 24.0f * u)
                                      .withPosition (screenArea.getX() + 14.0f * u, screenArea.getBottom() - 34.0f * u)));

    processorRef.apvts.state.setProperty ("uiWidth", getWidth(), nullptr);
    processorRef.apvts.state.setProperty ("uiHeight", getHeight(), nullptr);
}
