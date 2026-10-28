#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Presets.h"
#include "Riser.h"

// Reverse-reverb riser maker. The input passes through untouched; the first syllable after silence is
// captured once (until RESET), turned into a bar-synced riser on the message thread, previewed with
// LISTEN and dragged into the DAW as a WAV.
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

    //==============================================================================
    // message thread only

    // picks up a fresh capture and re-renders when it or a setting changed; true if the riser changed
    bool updateRiser();
    void resetCapture();
    bool hasRiser() const { return raw.getNumSamples() > 0; }
    // RAW or FX, whichever the switch selects
    const juce::AudioBuffer<float>& riser() const;
    // the captured audio and where the syllable sits in it (for the markers)
    const juce::AudioBuffer<float>& capturedAudio() const { return captured; }
    juce::Range<int> syllableRange() const { return syllable; }
    Riser::FxSettings fxSettings() const;
    // writes the riser as a WAV into the temp folder, named like Riser_FX_Chamber_1bars_100bpm.wav
    juce::File exportRiser() const;

    void setListening (bool shouldListen);
    bool isListening() const { return listening.load(); }
    void setLooping (bool shouldLoop) { looping = shouldLoop; }

    // user presets: riser settings only, as XML files
    static juce::File userPresetFolder();
    void saveUserPreset (const juce::File& file) const;
    void loadUserPreset (const juce::File& file);

    double bpm() const { return hostBpm.load(); }
    static constexpr double defaultBpm = 120.0;

private:
    float value (Param::Index i) const { return params[i]->load(); }
    std::array<float, riserParamIds.size()> settingsSnapshot() const;
    void setValue (const char* id, float v);

    std::array<std::atomic<float>*, riserParamIds.size()> params {};
    std::atomic<int> currentProgram { 0 };
    std::atomic<double> hostBpm { defaultBpm };
    double sr = 44100.0;

    // capture: the audio thread fills captureBuffer while waiting/recording; the message thread reads it once done
    enum CaptureState { waiting, recording, done };
    static constexpr double captureSeconds = 1.0, silenceBeforeOnsetMs = 100.0, gateReleaseMs = 50.0;
    std::atomic<int> captureState { waiting };
    std::atomic<bool> captureReady { false };
    juce::AudioBuffer<float> captureBuffer;
    int captureLength = 0, quietSamples = 0, quietNeeded = 0;
    float gateEnv = 0.0f, gateRelease = 0.0f;

    // preview: swapped in by the message thread only while it holds previewBusy; the audio thread skips a block if it can't
    juce::AudioBuffer<float> previewBuffer, pendingPreview;
    bool previewPending = false;
    std::atomic<bool> previewBusy { false }, listening { false }, looping { false }, restartPreview { false };
    int previewPos = 0;

    // message thread state
    juce::AudioBuffer<float> captured, syllableAudio, raw, fx;
    juce::Range<int> syllable;
    std::array<float, riserParamIds.size()> renderedSettings {};
    double renderedBpm = 0.0;
    std::atomic<bool> needsRender { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
