#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <melatonin_blur/melatonin_blur.h>

// Design system: soft neumorphism in a light and a dark palette, Manrope type, colour only from the
// Phýllon glow. Everything scales with one unit `u` (1 at the 520×720 base size). See DESIGN.md.
namespace Theme
{
    // literal ARGB only: juce::Colours::* may not be initialised yet when statics are built
    struct Palette
    {
        juce::Colour base, faceTop, faceBottom, screen, shadowDark, shadowLight;
        juce::Colour textPrimary, textMuted, textFaint;
        bool dark;
    };

    const Palette& light();
    const Palette& dark();
    // the active palette (a per-user preference shared by every instance)
    const Palette& palette();
    bool isDark();
    void setDark (bool shouldBeDark);

    // one glow hue per Phýllon band, low -> high; the only saturated colours in the UI
    inline const std::array<juce::Colour, 8> glow {
        juce::Colour (0xff5ee7df), juce::Colour (0xff7fb8ff), juce::Colour (0xff8c7cff), juce::Colour (0xffc77dff),
        juce::Colour (0xffff7eb3), juce::Colour (0xffff9a8b), juce::Colour (0xffffc371), juce::Colour (0xffffe29a)
    };
    // single accent for "on" states
    inline const juce::Colour accent { 0xff8c7cff };

    juce::ColourGradient glowGradient (juce::Point<float> from, juce::Point<float> to);
    juce::Colour glowAt (float proportion);

    // type: Manrope, sized by role and scaled by u
    enum class Type
    {
        wordmarkLight,
        wordmarkBold,
        heroName,
        heroValue,
        knobLabel,
        knobValue,
        tab,
        preset,
        menu
    };
    juce::Font font (Type role, float u);
    juce::Typeface::Ptr manrope (int weight);

    // depth recipes: raised = light top-left + dark bottom-right; pressed = the same pair inside
    melatonin::DropShadow raised (const Palette& p, float distance, float blur);
    melatonin::InnerShadow pressed (const Palette& p, float distance, float blur);

    // shared drawing helpers
    void fillFace (juce::Graphics& g, const juce::Path& shape, juce::Rectangle<float> bounds, const Palette& p);
    void drawCornerGrips (juce::Graphics& g, juce::Rectangle<float> area, float u);
    void drawPowerIcon (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, float thickness);
}

// Neumorphic controls. Set the scale with setScale(u) whenever the editor's unit changes.
class NeumorphicLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NeumorphicLookAndFeel();

    // rebuilds colours and shadow recipes for the active palette and unit
    void setScale (float newU);
    float getScale() const { return u; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
        float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
        int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return Theme::font (Theme::Type::preset, u); }
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getPopupMenuFont() override { return Theme::font (Theme::Type::menu, u); }
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
        bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
        const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;

private:
    void drawHeroKnob (juce::Graphics&, juce::Rectangle<float> bounds, float sliderPos, float startAngle, float endAngle, bool hot);
    void drawSmallKnob (juce::Graphics&, juce::Rectangle<float> bounds, float sliderPos, float startAngle, float endAngle, bool hot);

    float u = 1.0f;
    // melatonin caches per instance and path, so one recipe per element kind
    melatonin::DropShadow heroDisc, knobDisc, buttonRaised;
    melatonin::InnerShadow heroWell, knobGroove, fieldInset;
};
