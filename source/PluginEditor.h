#pragma once

#include "PhyllonView.h"
#include "PluginProcessor.h"

//==============================================================================
// Preset bar, recessed Phýllon screen, hero Mix knob, tabbed secondary knobs, Output.
// Resizable; everything scales with one unit and the layout switches between portrait and wide.
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
    void showTab (int index);
    void stepPreset (int delta);
    void showMenu();
    void applyTheme();
    void updateResetValues();

    // round raised button with a drawn icon (‹ › ☰)
    class IconButton : public juce::Button
    {
    public:
        enum class Icon { previous, next, menu };
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
        void setJustification (juce::Justification j) { justification = j; }
        void paint (juce::Graphics&) override;

    private:
        void sliderValueChanged (juce::Slider*) override { repaint(); }

        juce::Slider& slider;
        juce::String name;
        Theme::Type nameRole, valueRole;
        float scale = 1.0f;
        juce::Justification justification = juce::Justification::centred;
    };

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    NeumorphicLookAndFeel theme;
    PluginProcessor& processorRef;
    juce::TooltipWindow tooltips { this, 700 };

    PhyllonAnalyser analyser;
    PhyllonView phyllon;
    std::vector<float> incoming = std::vector<float> (PhyllonAnalyser::fftSize * 4);
    double lastFrameMs = 0.0;
    int shownProgram = -1;

    float u = 1.0f;
    juce::Rectangle<float> screenArea, tabTrack, gripArea;
    melatonin::InnerShadow screenInset, trackInset;

    juce::ComboBox presetBox;
    IconButton previousButton { IconButton::Icon::previous, "Previous preset" };
    IconButton nextButton { IconButton::Icon::next, "Next preset" };
    IconButton menuButton { IconButton::Icon::menu, "Menu" };

    std::array<Knob, presetParamIds.size()> knobs;
    Caption mixCaption { knobs[Param::mix].slider, "Mix", Theme::Type::heroName, Theme::Type::heroValue };
    Caption outputCaption { knobs[Param::output].slider, "Output", Theme::Type::tab, Theme::Type::knobValue };

    std::array<juce::TextButton, 3> tabs;
    std::array<std::vector<Param::Index>, 3> tabKnobs;
    int currentTab = 0;

    juce::ToggleButton freezeButton { "Freeze" };
    juce::AudioProcessorValueTreeState::ButtonAttachment freezeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
