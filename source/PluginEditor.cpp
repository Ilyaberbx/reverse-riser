#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      freezeAttachment (p.apvts, "freeze", freezeButton)
{
    presetLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (presetLabel);
    for (int i = 0; i < processorRef.getNumPrograms(); ++i)
        presetBox.addItem (processorRef.getProgramName (i), i + 1);
    presetBox.setSelectedId (processorRef.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { processorRef.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& knob = knobs[i];
        auto* param = processorRef.apvts.getParameter (presetParamIds[i]);
        knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, presetParamIds[i], knob.slider);
        knob.slider.setTextValueSuffix (" " + param->getLabel());
        knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        knob.label.setText (param->getName (32), juce::dontSendNotification);
        knob.label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (knob.slider);
        addAndMakeVisible (knob.label);
    }

    using namespace Param;
    const std::pair<const char*, std::vector<Index>> layout[] = {
        { "Space", { size, decay, predelay, diffusion } },
        { "Tone / Motion", { damping, modrate, moddepth, width } },
        { "Character", { shimmer, spring, lofi } },
        { "Output", { mix, output } },
    };
    for (size_t g = 0; g < groups.size(); ++g)
    {
        groups[g].frame.setText (layout[g].first);
        groups[g].knobs = layout[g].second;
        addAndMakeVisible (groups[g].frame, 0);
    }
    addAndMakeVisible (freezeButton);

    setSize (780, 440);
    startTimerHz (10);
}

PluginEditor::~PluginEditor()
{
}

// keeps the dropdown in sync when the host changes program
void PluginEditor::timerCallback()
{
    presetBox.setSelectedId (processorRef.getCurrentProgram() + 1, juce::dontSendNotification);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto top = area.removeFromTop (30);
    presetLabel.setBounds (top.removeFromLeft (60));
    presetBox.setBounds (top.removeFromLeft (240).reduced (4, 0));
    area.removeFromTop (6);

    // two rows: Space | Tone/Motion, then Character (+ Freeze) | Output
    const auto knobWidth = area.getWidth() / 8;
    auto rows = std::array { area.removeFromTop (area.getHeight() / 2), area };
    const std::array<int, 4> slots = { 4, 4, 4, 2 }; // Character gets a 4th slot for Freeze

    for (size_t g = 0; g < groups.size(); ++g)
    {
        auto& row = rows[g / 2];
        auto box = row.removeFromLeft (knobWidth * slots[g]);
        groups[g].frame.setBounds (box);
        auto inner = box.reduced (6).withTrimmedTop (14);

        for (auto index : groups[g].knobs)
        {
            auto cell = inner.removeFromLeft (knobWidth - 2);
            knobs[(size_t) index].label.setBounds (cell.removeFromBottom (18));
            knobs[(size_t) index].slider.setBounds (cell);
        }
        if (g == 2)
            freezeButton.setBounds (inner.withSizeKeepingCentre (80, 30));
    }
}
