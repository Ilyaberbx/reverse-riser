#include "Theme.h"
#include "BinaryData.h"

namespace Theme
{
    const Palette& light()
    {
        static const Palette p { juce::Colour (0xffeef1f6), juce::Colour (0xfffbfcfe), juce::Colour (0xffe3e7ee),
            juce::Colour (0xffe2e6ed), juce::Colour (0x80a3b1c6), juce::Colour (0xffffffff),
            juce::Colour (0xff2b3040), juce::Colour (0xff626b80), juce::Colour (0xffc3cad6), false };
        return p;
    }

    const Palette& dark()
    {
        static const Palette p { juce::Colour (0xff161b26), juce::Colour (0xff232b3a), juce::Colour (0xff121722),
            juce::Colour (0xff0d111a), juce::Colour (0x8c000000), juce::Colour (0x16ffffff),
            juce::Colour (0xffe7e9ee), juce::Colour (0xff9198a8), juce::Colour (0xff363d4d), true };
        return p;
    }

    namespace
    {
        juce::PropertiesFile& settings()
        {
            static juce::PropertiesFile file ([] {
                juce::PropertiesFile::Options options;
                options.applicationName = JucePlugin_Name;
                options.folderName = JucePlugin_Manufacturer;
                options.filenameSuffix = "settings";
                options.osxLibrarySubFolder = "Application Support";
                return options;
            }());
            return file;
        }

        std::atomic<bool>& darkFlag()
        {
            static std::atomic<bool> flag { settings().getBoolValue ("darkTheme", false) };
            return flag;
        }
    }

    bool isDark() { return darkFlag().load(); }
    const Palette& palette() { return isDark() ? dark() : light(); }

    void setDark (bool shouldBeDark)
    {
        darkFlag() = shouldBeDark;
        settings().setValue ("darkTheme", shouldBeDark);
        settings().saveIfNeeded();
    }

    juce::ColourGradient glowGradient (juce::Point<float> from, juce::Point<float> to)
    {
        juce::ColourGradient gradient (glow.front(), from, glow.back(), to, false);
        for (size_t i = 1; i + 1 < glow.size(); ++i)
            gradient.addColour ((double) i / (double) (glow.size() - 1), glow[i]);
        return gradient;
    }

    juce::Colour glowAt (float proportion)
    {
        return glowGradient ({ 0.0f, 0.0f }, { 1.0f, 0.0f }).getColourAtPosition (juce::jlimit (0.0, 1.0, (double) proportion));
    }

    juce::Typeface::Ptr manrope (int weight)
    {
        static const std::array<juce::Typeface::Ptr, 5> faces {
            juce::Typeface::createSystemTypefaceFor (BinaryData::ManropeLight_ttf, BinaryData::ManropeLight_ttfSize),
            juce::Typeface::createSystemTypefaceFor (BinaryData::ManropeRegular_ttf, BinaryData::ManropeRegular_ttfSize),
            juce::Typeface::createSystemTypefaceFor (BinaryData::ManropeSemiBold_ttf, BinaryData::ManropeSemiBold_ttfSize),
            juce::Typeface::createSystemTypefaceFor (BinaryData::ManropeBold_ttf, BinaryData::ManropeBold_ttfSize),
            juce::Typeface::createSystemTypefaceFor (BinaryData::ManropeExtraBold_ttf, BinaryData::ManropeExtraBold_ttfSize),
        };
        if (weight <= 300)
            return faces[0];
        if (weight <= 500)
            return faces[1];
        if (weight <= 600)
            return faces[2];
        if (weight <= 700)
            return faces[3];
        return faces[4];
    }

    juce::Font font (Type role, float u)
    {
        struct Spec
        {
            int weight;
            float size, tracking;
            bool tabular;
        };
        const auto spec = [role]() -> Spec {
            switch (role)
            {
                case Type::wordmarkLight: return { 300, 15.0f, 0.22f, false };
                case Type::wordmarkBold: return { 800, 15.0f, 0.22f, false };
                case Type::heroName: return { 800, 15.0f, 0.16f, false };
                case Type::heroValue: return { 400, 15.0f, 0.06f, true };
                case Type::knobLabel: return { 700, 10.5f, 0.12f, false };
                case Type::knobValue: return { 600, 12.5f, 0.02f, true };
                case Type::tab: return { 700, 11.5f, 0.10f, false };
                case Type::preset: return { 600, 13.0f, 0.01f, false };
                case Type::menu: return { 500, 13.0f, 0.0f, false };
            }
            return { 400, 13.0f, 0.0f, false };
        }();

        auto options = juce::FontOptions().withTypeface (manrope (spec.weight)).withHeight (spec.size * u).withKerningFactor (spec.tracking);
        if (spec.tabular)
            options = options.withFeatureEnabled ("tnum");
        return options;
    }

    melatonin::DropShadow raised (const Palette& p, float distance, float blur)
    {
        const auto d = juce::roundToInt (distance), b = juce::jmax (1, juce::roundToInt (blur));
        return { { p.shadowDark, b, { d, d } }, { p.shadowLight, b, { -d, -d } } };
    }

    melatonin::InnerShadow pressed (const Palette& p, float distance, float blur)
    {
        const auto d = juce::jmax (1, juce::roundToInt (distance)), b = juce::jmax (1, juce::roundToInt (blur));
        return { { p.shadowDark, b, { d, d } }, { p.shadowLight, b, { -d, -d } } };
    }

    const Images& images()
    {
        static const Images i {
            juce::ImageCache::getFromMemory (BinaryData::hero_vortex_png, BinaryData::hero_vortex_pngSize),
            juce::ImageCache::getFromMemory (BinaryData::knob_cap_png, BinaryData::knob_cap_pngSize),
            juce::ImageCache::getFromMemory (BinaryData::logo_mark_png, BinaryData::logo_mark_pngSize),
            juce::ImageCache::getFromMemory (BinaryData::glass_light_png, BinaryData::glass_light_pngSize),
            juce::ImageCache::getFromMemory (BinaryData::glass_dark_png, BinaryData::glass_dark_pngSize),
        };
        return i;
    }

    void fillFace (juce::Graphics& g, const juce::Path& shape, juce::Rectangle<float> bounds, const Palette& p)
    {
        g.setGradientFill ({ p.faceTop, bounds.getTopLeft(), p.faceBottom, bounds.getBottomRight(), false });
        g.fillPath (shape);
    }

    void drawTinted (juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> area, juce::Colour colour)
    {
        if (! image.isValid())
            return;
        const auto fitted = juce::RectanglePlacement (juce::RectanglePlacement::centred)
                                .getTransformToFit (image.getBounds().toFloat(), area);
        juce::Graphics::ScopedSaveState state (g);
        g.setColour (colour);
        g.drawImageTransformed (image, fitted, true);
    }

    void drawCornerGrips (juce::Graphics& g, juce::Rectangle<float> area, float u)
    {
        // three pressed dots per corner, in an L that opens towards the centre
        const auto& p = palette();
        const auto step = 7.0f * u, size = 2.6f * u;
        const auto dot = [&] (juce::Point<float> c) {
            const auto r = juce::Rectangle<float> (size, size).withCentre (c);
            g.setColour (p.shadowLight);
            g.fillEllipse (r.translated (0.6f * u, 0.6f * u));
            g.setColour (p.shadowDark);
            g.fillEllipse (r);
        };
        const std::array<std::pair<juce::Point<float>, juce::Point<float>>, 4> corners { {
            { area.getTopLeft(), { 1.0f, 1.0f } },
            { area.getTopRight(), { -1.0f, 1.0f } },
            { area.getBottomLeft(), { 1.0f, -1.0f } },
            { area.getBottomRight(), { -1.0f, -1.0f } },
        } };
        for (const auto& [corner, dir] : corners)
        {
            dot (corner);
            dot (corner + juce::Point<float> (dir.x * step, 0.0f));
            dot (corner + juce::Point<float> (0.0f, dir.y * step));
        }
    }

    void drawPowerIcon (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, float thickness)
    {
        const auto r = area.reduced (thickness);
        const auto c = r.getCentre();
        const auto radius = r.getWidth() * 0.5f;
        juce::Path icon;
        icon.addCentredArc (c.x, c.y, radius, radius, 0.0f, juce::degreesToRadians (35.0f), juce::degreesToRadians (325.0f), true);
        icon.startNewSubPath (c.x, r.getY() - thickness * 0.5f);
        icon.lineTo (c.x, c.y);
        g.setColour (colour);
        g.strokePath (icon, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

//==============================================================================
NeumorphicLookAndFeel::NeumorphicLookAndFeel()
{
    setScale (1.0f);
}

void NeumorphicLookAndFeel::setScale (float newU)
{
    u = newU;
    const auto& p = Theme::palette();

    heroDisc = Theme::raised (p, 9.0f * u, 24.0f * u);
    heroWell = Theme::pressed (p, 4.0f * u, 10.0f * u);
    knobDisc = Theme::raised (p, 4.0f * u, 10.0f * u);
    knobGroove = Theme::pressed (p, 1.5f * u, 3.0f * u);
    fieldInset = Theme::pressed (p, 2.5f * u, 6.0f * u);
    buttonRaised = Theme::raised (p, 3.0f * u, 8.0f * u);

    setColour (juce::ResizableWindow::backgroundColourId, p.base);
    setColour (juce::Label::textColourId, p.textMuted);
    setColour (juce::Slider::textBoxTextColourId, p.textPrimary);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Theme::accent.withAlpha (0.3f));
    setColour (juce::TextEditor::textColourId, p.textPrimary);
    setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::highlightColourId, Theme::accent.withAlpha (0.3f));
    setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::CaretComponent::caretColourId, Theme::accent);
    setColour (juce::ComboBox::textColourId, p.textPrimary);
    setColour (juce::PopupMenu::textColourId, p.textPrimary);
    setColour (juce::PopupMenu::highlightedTextColourId, p.textPrimary);
    setColour (juce::ToggleButton::textColourId, p.textPrimary);
    setColour (juce::TextButton::textColourOffId, p.textMuted);
    setColour (juce::TextButton::textColourOnId, p.textPrimary);
}

//==============================================================================
void NeumorphicLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
    float startAngle, float endAngle, juce::Slider& slider)
{
    const auto size = (float) juce::jmin (width, height);
    const auto bounds = juce::Rectangle<float> (size, size).withCentre (juce::Rectangle<int> (x, y, width, height).toFloat().getCentre());
    const auto hot = slider.isMouseOverOrDragging() && slider.isEnabled();

    if (slider.getProperties()["hero"])
        drawHeroKnob (g, bounds, sliderPos, startAngle, endAngle, hot);
    else
        drawSmallKnob (g, bounds, sliderPos, startAngle, endAngle, hot);
}

void NeumorphicLookAndFeel::drawHeroKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float sliderPos,
    float startAngle, float endAngle, bool hot)
{
    const auto& p = Theme::palette();
    const auto centre = bounds.getCentre();
    const auto radius = bounds.getWidth() * 0.5f;
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);

    // tick-dot ring: dots up to the value light up in the glow gradient
    constexpr int ticks = 41;
    const auto tickRadius = radius - 5.0f * u;
    for (int i = 0; i < ticks; ++i)
    {
        const auto t = (float) i / (float) (ticks - 1);
        const auto at = centre.getPointOnCircumference (tickRadius, startAngle + t * (endAngle - startAngle));
        if (sliderPos > 0.0f && t <= sliderPos + 1.0e-4f)
        {
            const auto colour = Theme::glowAt (t);
            g.setGradientFill ({ colour.withAlpha (0.45f), at, colour.withAlpha (0.0f), at.translated (6.0f * u, 0.0f), true });
            g.fillEllipse (juce::Rectangle<float> (12.0f * u, 12.0f * u).withCentre (at));
            g.setColour (colour);
            g.fillEllipse (juce::Rectangle<float> (3.4f * u, 3.4f * u).withCentre (at));
        }
        else
        {
            g.setColour (p.textFaint);
            g.fillEllipse (juce::Rectangle<float> (2.4f * u, 2.4f * u).withCentre (at));
        }
    }

    // recessed well the disc sits in
    const auto wellRadius = radius - 16.0f * u;
    juce::Path well;
    well.addEllipse (juce::Rectangle<float> (wellRadius * 2.0f, wellRadius * 2.0f).withCentre (centre));
    g.setColour (p.base);
    g.fillPath (well);
    heroWell.render (g, well);

    // raised disc
    const auto discRadius = wellRadius - 9.0f * u;
    const auto discBounds = juce::Rectangle<float> (discRadius * 2.0f, discRadius * 2.0f).withCentre (centre);
    juce::Path disc;
    disc.addEllipse (discBounds);
    heroDisc.render (g, disc);
    Theme::fillFace (g, disc, discBounds, p);
    // the face: the generated vortex, clipped to the disc, turning with the value
    if (const auto& vortex = Theme::images().heroVortex; vortex.isValid())
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.reduceClipRegion (disc);
        const auto face = discBounds.reduced (1.5f * u);
        const auto scale = face.getWidth() / (float) vortex.getWidth();
        g.drawImageTransformed (vortex, juce::AffineTransform::scale (scale).translated (face.getX(), face.getY()).rotated (angle, centre.x, centre.y));
    }
    g.setColour (p.shadowLight.withMultipliedAlpha (p.dark ? 1.6f : 0.7f));
    g.drawEllipse (discBounds.reduced (0.75f * u), 1.0f * u);
    if (hot)
    {
        g.setColour (Theme::accent.withAlpha (0.35f));
        g.drawEllipse (discBounds.expanded (3.0f * u), 1.2f * u);
    }

    // pointer line
    const auto from = centre.getPointOnCircumference (discRadius * 0.5f, angle);
    const auto to = centre.getPointOnCircumference (discRadius * 0.82f, angle);
    g.setColour (Theme::glowAt (sliderPos).withAlpha (0.45f));
    g.drawLine ({ from, to }, 7.0f * u);
    g.setColour (juce::Colour (0xfff4f7ff));
    g.drawLine ({ from, to }, 3.0f * u);
}

void NeumorphicLookAndFeel::drawSmallKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float sliderPos,
    float startAngle, float endAngle, bool hot)
{
    const auto& p = Theme::palette();
    const auto centre = bounds.getCentre();
    const auto grooveRadius = bounds.getWidth() * 0.5f - 4.0f * u;
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const juce::PathStrokeType stroke (4.0f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    // pressed groove + value arc
    juce::Path groove;
    groove.addCentredArc (centre.x, centre.y, grooveRadius, grooveRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (p.faceBottom);
    g.strokePath (groove, stroke);
    knobGroove.render (g, groove, stroke);

    if (sliderPos > 0.0f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, grooveRadius, grooveRadius, 0.0f, startAngle, angle, true);
        g.setGradientFill (Theme::glowGradient (centre.getPointOnCircumference (grooveRadius, startAngle),
            centre.getPointOnCircumference (grooveRadius, endAngle)));
        g.strokePath (value, stroke);
    }

    // raised disc
    const auto discRadius = grooveRadius - 8.0f * u;
    const auto discBounds = juce::Rectangle<float> (discRadius * 2.0f, discRadius * 2.0f).withCentre (centre);
    juce::Path disc;
    disc.addEllipse (discBounds);
    knobDisc.render (g, disc);
    Theme::fillFace (g, disc, discBounds, p);
    // the cap: the generated brushed metal, clipped to the disc
    if (const auto& cap = Theme::images().knobCap; cap.isValid())
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.reduceClipRegion (disc);
        g.drawImage (cap, discBounds, juce::RectanglePlacement::stretchToFit);
    }
    if (hot)
    {
        g.setColour (Theme::accent.withAlpha (0.35f));
        g.drawEllipse (discBounds.expanded (2.0f * u), 1.0f * u);
    }

    g.setColour (p.textPrimary);
    g.drawLine ({ centre.getPointOnCircumference (discRadius * 0.4f, angle), centre.getPointOnCircumference (discRadius * 0.8f, angle) }, 2.2f * u);
}

juce::Label* NeumorphicLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setColour (juce::Label::textColourId, Theme::palette().textPrimary);
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineWhenEditingColourId, Theme::accent.withAlpha (0.5f));
    label->setColour (juce::TextEditor::textColourId, Theme::palette().textPrimary);
    label->setColour (juce::TextEditor::highlightColourId, Theme::accent.withAlpha (0.3f));
    return label;
}

juce::Font NeumorphicLookAndFeel::getLabelFont (juce::Label& label)
{
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr)
        return Theme::font (Theme::Type::knobValue, u);
    return label.getFont();
}

//==============================================================================
// Freeze: a quiet power toggle, no container; the icon and text light up when on
void NeumorphicLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const auto& p = Theme::palette();
    const auto on = button.getToggleState();
    auto area = button.getLocalBounds().toFloat();
    const auto iconSize = 13.0f * u;
    const auto icon = area.removeFromLeft (iconSize + 4.0f * u).withSizeKeepingCentre (iconSize, iconSize);

    if (on)
    {
        g.setGradientFill ({ Theme::accent.withAlpha (0.4f), icon.getCentre(), Theme::accent.withAlpha (0.0f),
            icon.getCentre().translated (iconSize, 0.0f), true });
        g.fillEllipse (icon.expanded (iconSize * 0.6f));
    }
    const auto colour = on ? Theme::accent : (highlighted ? p.textPrimary : p.textMuted);
    Theme::drawPowerIcon (g, icon, colour, 1.6f * u);

    g.setColour (on ? p.textPrimary : colour);
    g.setFont (Theme::font (Theme::Type::tab, u));
    g.drawText (button.getButtonText().toUpperCase(), area.withTrimmedLeft (4.0f * u), juce::Justification::centredLeft);
}

// tabs: a selected tab is a raised pill on the pressed track (the editor draws the track)
void NeumorphicLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool, bool)
{
    if (! button.getToggleState())
        return;

    const auto& p = Theme::palette();
    const auto bounds = button.getLocalBounds().toFloat().reduced (3.0f * u);
    juce::Path pill;
    pill.addRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
    buttonRaised.render (g, pill);
    Theme::fillFace (g, pill, bounds, p);
}

void NeumorphicLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool highlighted, bool)
{
    const auto& p = Theme::palette();
    g.setColour (button.getToggleState() ? p.textPrimary : (highlighted ? p.textPrimary.withAlpha (0.8f) : p.textMuted));
    g.setFont (Theme::font (Theme::Type::tab, u));
    g.drawText (button.getButtonText().toUpperCase(), button.getLocalBounds(), juce::Justification::centred);
}

void NeumorphicLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto& p = Theme::palette();
    const auto bounds = juce::Rectangle<float> ((float) width, (float) height);
    juce::Path field;
    field.addRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
    g.setColour (p.screen);
    g.fillPath (field);
    fieldInset.render (g, field);

    // small chevron says "this opens a list"
    const auto c = juce::Point<float> (bounds.getRight() - 16.0f * u, bounds.getCentreY());
    juce::Path chevron;
    chevron.startNewSubPath (c.translated (-3.5f * u, -1.5f * u));
    chevron.lineTo (c.translated (0.0f, 2.0f * u));
    chevron.lineTo (c.translated (3.5f * u, -1.5f * u));
    g.setColour (box.isMouseOver (true) ? p.textPrimary : p.textMuted);
    g.strokePath (chevron, juce::PathStrokeType (1.4f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void NeumorphicLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (box.getLocalBounds().reduced (juce::roundToInt (28.0f * u), 0));
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
}

//==============================================================================
void NeumorphicLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto& p = Theme::palette();
    g.fillAll (p.faceTop);
    g.setColour (p.shadowDark);
    g.drawRect (juce::Rectangle<int> (width, height), 1);
}

void NeumorphicLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
    bool isHighlighted, bool isTicked, bool, const juce::String& text, const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    const auto& p = Theme::palette();
    if (isSeparator)
    {
        g.setColour (p.shadowDark.withMultipliedAlpha (0.5f));
        g.fillRect (area.toFloat().withSizeKeepingCentre ((float) area.getWidth() - 20.0f * u, 1.0f));
        return;
    }

    const auto row = area.toFloat().reduced (5.0f * u, 1.5f * u);
    if (isHighlighted && isActive)
    {
        g.setColour (Theme::accent.withAlpha (p.dark ? 0.22f : 0.12f));
        g.fillRoundedRectangle (row, 7.0f * u);
    }

    auto content = row.reduced (10.0f * u, 0.0f);
    const auto dot = content.removeFromLeft (7.0f * u).withSizeKeepingCentre (6.0f * u, 6.0f * u);
    if (isTicked)
    {
        g.setColour (Theme::accent);
        g.fillEllipse (dot);
    }

    g.setColour (isActive ? p.textPrimary : p.textMuted);
    g.setFont (getPopupMenuFont());
    g.drawText (text, content.withTrimmedLeft (8.0f * u), juce::Justification::centredLeft);
}

void NeumorphicLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto& p = Theme::palette();
    const auto bounds = juce::Rectangle<float> ((float) width, (float) height);
    g.setColour (p.faceTop);
    g.fillRoundedRectangle (bounds, 6.0f * u);
    g.setColour (p.shadowDark);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f * u, 1.0f);
    g.setColour (p.textPrimary);
    g.setFont (Theme::font (Theme::Type::menu, u * 0.92f));
    g.drawFittedText (text, bounds.reduced (10.0f * u, 6.0f * u).toNearestInt(), juce::Justification::centredLeft, 3);
}

juce::Rectangle<int> NeumorphicLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const auto f = Theme::font (Theme::Type::menu, u * 0.92f);
    const auto fullWidth = juce::GlyphArrangement::getStringWidth (f, tipText);
    const auto textWidth = juce::jmin (320.0f * u, fullWidth);
    const auto lines = (int) std::ceil (fullWidth / juce::jmax (1.0f, textWidth));
    const auto w = juce::roundToInt (textWidth + 22.0f * u), h = juce::roundToInt ((float) lines * f.getHeight() + 14.0f * u);
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
        screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

// anything that asks for the default sans (popups, text editors) gets Manrope too
juce::Typeface::Ptr NeumorphicLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return Theme::manrope (f.isBold() ? 700 : 500);
    return LookAndFeel_V4::getTypefaceForFont (f);
}

// resize grip: a diagonal of three dots, matching the corner grips
void NeumorphicLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
{
    const auto& p = Theme::palette();
    const auto size = 2.6f * u, step = 5.0f * u;
    const auto corner = juce::Point<float> ((float) w, (float) h).translated (-4.0f * u, -4.0f * u);
    g.setColour (isMouseOver || isMouseDragging ? Theme::accent : p.textMuted.withAlpha (0.7f));
    for (const auto offset : { juce::Point<float> (0.0f, 0.0f), { -step, 0.0f }, { 0.0f, -step }, { -2.0f * step, 0.0f }, { -step, -step }, { 0.0f, -2.0f * step } })
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (corner + offset));
}
