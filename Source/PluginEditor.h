#pragma once

#include "PluginProcessor.h"
#include "UiSettings.h"

/**
    Hosts the Glacier Riser web UI (ui/ -> bundled into a single index.html) inside a WebView2
    browser and bridges it to the processor.

    Bridge protocol (see ui/src/bridge/juceBackend.ts for the other side)
      Native functions (JS -> C++, each returns a promise):
        ready, setParam, beginGesture, endGesture, setSelection, play, stop, setLoop, reset,
        loadPreset, savePreset, openFile, exportWav, openExportsFolder,
        startDragOut, loadAudioData, setTheme
      Events (C++ -> JS):
        glacier:state, glacier:params, glacier:waveforms, glacier:playback, glacier:toast, glacier:theme
      Initialisation data: theme (the saved UI theme, applied before the page draws)
*/
class GlacierRiserEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit GlacierRiserEditor (GlacierRiserProcessor&);
    ~GlacierRiserEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;
    using Args = juce::Array<juce::var>;

    juce::WebBrowserComponent::Options createBrowserOptions();
    static std::optional<juce::WebBrowserComponent::Resource> serveResource (const juce::String& url);

    void timerCallback() override;
    void emit (const juce::String& eventId, const juce::var& payload);
    void toast (const juce::String& text, bool isError = false);
    juce::var makeResult (bool ok, const juce::String& message = {}) const;
    juce::var makeInitialPayload();

    void openFileChooser();
    void exportWithChooser();

    GlacierRiserProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;
    juce::Label fallbackLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    bool pageReady = false;
    int lastStateVersion = -1, lastWaveformVersion = -1, lastNoticeVersion = 0;
    int lastThemeVersion = UiSettings::themeVersion.load();
    juce::String lastParamsJson, lastPlaybackJson;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlacierRiserEditor)
};
