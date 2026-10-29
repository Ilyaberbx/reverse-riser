#pragma once

#include "PluginProcessor.h"
#include "UiSettings.h"

/**
    Hosts the ATMOSRISER web UI (ui/ -> bundled into a single index.html) inside a WebView2
    browser and bridges it to the processor.

    Bridge protocol (see ui/src/bridge/juceBackend.ts for the other side)
      Native functions (JS -> C++, each returns a promise):
        ready, setParam, beginGesture, endGesture, setSelection, play, stop, setLoop, reset,
        loadPreset, savePreset, openFile, exportWav, openExportsFolder,
        startDragOut, loadAudioData, setTheme
      Events (C++ -> JS):
        atmos:state, atmos:params, atmos:waveforms, atmos:playback, atmos:toast, atmos:theme
      Initialisation data: theme (the saved UI theme, applied before the page draws)
*/
class AtmosRiserEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit AtmosRiserEditor (AtmosRiserProcessor&);
    ~AtmosRiserEditor() override;

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

    AtmosRiserProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;
    juce::Label fallbackLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    bool pageReady = false;
    int lastStateVersion = -1, lastWaveformVersion = -1, lastNoticeVersion = 0;
    int lastThemeVersion = UiSettings::themeVersion.load();
    juce::String lastParamsJson, lastPlaybackJson;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtmosRiserEditor)
};
