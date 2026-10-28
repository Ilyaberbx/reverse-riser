#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Presets.h"
#include "ReverbEngine.h"
#include "VocalChain.h"

#if (MSVC)
#include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // message thread: takes up to maxSamples of the latest mono output for the visualizer (lock-free, single reader)
    int readVisualizerSamples (float* dest, int maxSamples);

private:
    ReverbParams readParams() const;
    double hostBpm() const;

    // cached in presetParamIds order, so the audio thread never looks parameters up by name
    std::array<std::atomic<float>*, presetParamIds.size()> params {};
    std::atomic<float>* freeze = nullptr;

    ReverbEngine engine;
    juce::AudioBuffer<float> wetBuffer;
    juce::SmoothedValue<float> mix, outputGain;
    std::atomic<int> currentProgram { 0 }; // read on the audio thread to switch the vocal chain

    VocalChain vocalChain;
    juce::AudioBuffer<float> voxBuffer;
    juce::SmoothedValue<float> vocalChainAmount;
    bool vocalChainRunning = false;

    // audio -> UI: mono output samples; drops samples instead of blocking when the UI isn't reading
    void pushVisualizerSamples (const float* left, const float* right, int numSamples);
    static constexpr int visualizerCapacity = 8192;
    juce::AbstractFifo visualizerFifo { visualizerCapacity };
    std::array<float, visualizerCapacity> visualizerSamples {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
