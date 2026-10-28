#pragma once

#include "PluginProcessor.h"

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Group
    {
        juce::GroupComponent frame;
        std::vector<Param::Index> knobs;
    };

    PluginProcessor& processorRef;
    juce::Label presetLabel { {}, "Preset" };
    juce::ComboBox presetBox;
    std::array<Knob, presetParamIds.size()> knobs;
    std::array<Group, 4> groups;
    juce::ToggleButton freezeButton { "Freeze" };
    juce::AudioProcessorValueTreeState::ButtonAttachment freezeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
