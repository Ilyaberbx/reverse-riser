#pragma once

#include "PluginProcessor.h"
#include "Theme.h"

//==============================================================================
// Preset bar, recessed riser display (waveform, RESET, LISTEN/LOOP, RAW|FX, drag to DAW),
// hero REVERSE knob, FX row (REVERB, CHORUS, TREMOLO, PITCH) with popovers.
// Resizable; everything scales with one unit.
class PluginEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int defaultWidth = 520, defaultHeight = 720;

private:
    void timerCallback() override;
    void stepPreset (int delta);
    void refreshPresetList();
    void savePreset();
    void showMenu();
    void applyTheme();
    void showFxPanel (juce::Component& anchor, std::vector<const char*> ids);

    // round raised button with a drawn icon (‹ › 💾 ☰)
    class IconButton : public juce::Button
    {
    public:
        enum class Icon { previous, next, save, menu };
        IconButton (Icon i, const juce::String& name);
        void setScale (float u);
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    private:
        Icon icon;
        float u = 1.0f;
        melatonin::DropShadow raisedShadow;
        melatonin::InnerShadow pressedShadow;
    };

    // "NAME value" in two weights, kept in sync with a slider
    class Caption : public juce::Component, private juce::Slider::Listener
    {
    public:
        Caption (juce::Slider& source, const juce::String& name, Theme::Type nameRole, Theme::Type valueRole);
        ~Caption() override { slider.removeListener (this); }
        void setScale (float newScale) { scale = newScale; repaint(); }
        void paint (juce::Graphics&) override;

    private:
        void sliderValueChanged (juce::Slider*) override { repaint(); }

        juce::Slider& slider;
        juce::String name;
        Theme::Type nameRole, valueRole;
        float scale = 1.0f;
    };

    // power toggle: the icon switches the effect, the name opens its settings
    class FxButton : public juce::ToggleButton
    {
    public:
        explicit FxButton (const juce::String& name) : juce::ToggleButton (name) {}
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        std::function<void()> onSettings;

    private:
        bool settingsPress = false;
    };

    // "RAW  FX": which render LISTEN plays and the drag exports
    class RenderSwitch : public juce::Button
    {
    public:
        RenderSwitch();
        void setScale (float newU) { u = newU; repaint(); }
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    private:
        float u = 1.0f;
    };

    // the recessed screen: riser waveform, syllable markers, pitch curve, drag source
    class RiserView : public juce::Component
    {
    public:
        explicit RiserView (PluginProcessor&);
        void setScale (float newU);
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override { dragging = false; }

        juce::TextButton resetButton { "Reset" }, listenButton { "Listen" }, loopButton { "Loop" };
        RenderSwitch renderSwitch;

    private:
        juce::Rectangle<float> waveArea() const;
        void paintSyllable (juce::Graphics&, juce::Rectangle<float>) const;

        PluginProcessor& processor;
        juce::AudioProcessorValueTreeState::ButtonAttachment renderAttachment;
        melatonin::InnerShadow inset;
        float u = 1.0f;
        bool dragging = false;
    };

    class FxPanel;

    NeumorphicLookAndFeel theme;
    PluginProcessor& processorRef;
    juce::TooltipWindow tooltips { this, 700 };

    float u = 1.0f;
    juce::Rectangle<float> gripArea;
    int shownProgram = -1;
    juce::Array<juce::File> userPresets;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::ComboBox presetBox;
    IconButton previousButton { IconButton::Icon::previous, "Previous preset" };
    IconButton nextButton { IconButton::Icon::next, "Next preset" };
    IconButton saveButton { IconButton::Icon::save, "Save preset" };
    IconButton menuButton { IconButton::Icon::menu, "Menu" };

    RiserView riserView { processorRef };

    juce::Slider lengthKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::AudioProcessorValueTreeState::SliderAttachment lengthAttachment;
    Caption lengthCaption { lengthKnob, "Reverse", Theme::Type::heroName, Theme::Type::heroValue };

    std::array<FxButton, 4> fxButtons { FxButton ("Reverb"), FxButton ("Chorus"), FxButton ("Tremolo"), FxButton ("Pitch") };
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> fxAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
