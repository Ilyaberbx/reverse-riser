#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <melatonin_blur/melatonin_blur.h>

// Design system: soft neumorphism in a light and a dark palette, Manrope type, colour only from the
// glow and the accent. Everything scales with one unit `u` (1 at the 520×720 base size). See DESIGN.md.
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

    // glow hues for the hero's lit ticks, low -> high: ice cyan into electric blue; with the accent, the only
    // saturated colours in the UI
    inline const std::array<juce::Colour, 8> glow {
        juce::Colour (0xff7df3ff), juce::Colour (0xff5fdcff), juce::Colour (0xff46c2ff), juce::Colour (0xff35a7ff),
        juce::Colour (0xff2f8cfb), juce::Colour (0xff2f7cf6), juce::Colour (0xff4f8ff8), juce::Colour (0xff8ab8ff)
    };
    // single accent for "on" states
    inline const juce::Colour accent { 0xff2f7cf6 };

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

    // generated surfaces (assets/images, embedded as BinaryData): the vortex in the hero, the metal cap of small
    // knobs, the logo mark, and the frosted texture under the panel. Everything else stays vector. See DESIGN.md.
    struct Images
    {
        juce::Image heroVortex, knobCap, logoMark, glassLight, glassDark;
    };
    const Images& images();

    // shared drawing helpers
    void fillFace (juce::Graphics& g, const juce::Path& shape, juce::Rectangle<float> bounds, const Palette& p);
    // draws an image's alpha mask in one colour, e.g. the logo in the text colour
    void drawTinted (juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> area, juce::Colour colour);
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
