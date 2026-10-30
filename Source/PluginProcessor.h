#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "ReverseReverbRenderer.h"
#include "BarAlignment.h"
#include "ExportLayout.h"

/**
    Glacier Riser: listens to the track it sits on (or a dropped file), lets you pick a segment,
    and renders a reverse-reverb swell that rises into it.

    Track workflow
      1. Every time the DAW transport plays, the incoming track audio is recorded with its
         timeline position. When the transport stops, the new take replaces the previous one
         (a silent playback keeps the previous take).
      2. The first phrase is selected automatically and the swell is rendered, with automatic
         sibilance control on the reverb feed.
      3. Listen / Loop (or a MIDI note) audition the result inside the plugin without
         re-recording, and the clip can be dragged onto the timeline as a WAV.
      4. Reset clears the take.

    Threading model
      - Source audio, selection, presets and rendering live on the message thread.
      - The audio thread records into a preallocated ring buffer and reads the latest rendered
        clip through a spin-lock protected shared_ptr.
      - The editor polls the version counters and snapshots, so nothing here knows about the UI.
*/
class GlacierRiserProcessor : public juce::AudioProcessor,
                            private juce::AudioProcessorValueTreeState::Listener,
                            private juce::Timer
{
public:
    GlacierRiserProcessor();
    ~GlacierRiserProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Lets VST3 hosts open Glacier Riser wherever a project used the plugins it replaces. */
    juce::VST3ClientExtensions* getVST3ClientExtensions() override { return &vst3Extensions; }

    //==============================================================================
    // Everything below is message-thread only unless noted.

    /** Parameter IDs exposed to the UI. */
    static const juce::StringArray& uiParameterIds();
    static const juce::StringArray& reverseChoiceNames();
    static const juce::StringArray& tremoloTimingNames();

    bool loadSourceFile (const juce::File& file, juce::String& errorMessage);
    bool importAudioData (const juce::String& fileName, const juce::MemoryBlock& data, juce::String& errorMessage);
    void setSelection (double startSeconds, double endSeconds);

    void setParameterFromUi (const juce::String& id, float realValue);
    void beginParameterGesture (const juce::String& id);
    void endParameterGesture (const juce::String& id);

    /** Forgets the captured (or loaded) audio, so the next DAW playback captures the track again. */
    void clearRecording();

    // Presets
    juce::StringArray getPresetNames() const;
    bool loadPreset (int index);
    bool saveUserPreset (const juce::String& name, juce::String& errorMessage);

    // Preview playback (any thread)
    enum class PlaySource { processed = 1, raw = 2 };
    void startPlayback (PlaySource source) noexcept;
    void stopPlayback() noexcept { stopRequested = true; }
    void setLoopEnabled (bool shouldLoop) noexcept;
    bool isPlaying() const noexcept { return playing; }

    // Files
    juce::File getExportsDirectory() const;
    juce::File writeExportForDrag (juce::String& errorMessage);
    bool exportRenderedToFile (const juce::File& file, juce::String& errorMessage) const;
    juce::String getDefaultExportName() const;
    /** Bar to drop an exported clip on, and the silence that puts its start on that bar line. */
    BarAlignment getExportAlignment() const;

    // Snapshots for the UI
    int getStateVersion() const noexcept { return stateVersion.load(); }
    int getWaveformVersion() const noexcept { return waveformVersion.load(); }
    int getNoticeVersion() const noexcept { return noticeVersion; }
    juce::String getNotice (bool& isError) const { isError = noticeIsError; return notice; }
    juce::var getStateSnapshot() const;
    juce::var getParameterSnapshot() const;
    juce::var getWaveformSnapshot() const;
    juce::var getPlaybackSnapshot() const;

    juce::AudioFormatManager formatManager;
    juce::AudioProcessorValueTreeState apvts;

private:
    struct Vst3Extensions : public juce::VST3ClientExtensions
    {
        /** Class IDs of earlier plugins this one replaces (read from their moduleinfo.json).
            "Reverse Reverb" (the first version of this plugin) already shares this plugin's ID (so does AtmosRiser, its former name). */
        std::vector<juce::String> getCompatibleClasses() const override
        {
            return { "ABCDEF019182FAEB496C6278546E726D" };   // Reverse Riser 0.0.1 (Ilbx / Tnrm)
        }
    };

    struct Preset
    {
        juce::String name;
        juce::NamedValueSet values;   // real (denormalised) parameter values
        juce::File file;              // empty for factory presets
    };

    enum class SourceKind { none, demo, file, capture };
    enum CaptureState { captureOff = 0, captureArmed = 1, captureRecording = 2, captureFinished = 3 };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;

    void requestRender() noexcept { renderRequested = true; }
    void renderNow();
    double computeTailSeconds() const;
    ReverseReverbSettings currentSettings() const;
    void loadDemoSource();
    void setSource (juce::AudioBuffer<float>&& audio, double rate, SourceKind kind, const juce::File& file);
    void rebuildRawPeaks();
    void bumpState() noexcept { ++stateVersion; }
    void postNotice (const juce::String& text, bool isError = false);
    std::pair<double, double> defaultSelection() const;
    juce::File getUserPresetDirectory() const;
    juce::File getImportDirectory() const;
    juce::File getCaptureDirectory() const;
    void refreshUserPresets();
    void applyPresetValues (const juce::NamedValueSet& values);
    void finalizeCapture();
    juce::String describeTimelinePosition (double sourceSeconds) const;

    Vst3Extensions vst3Extensions;

    // Source
    SourceKind sourceKind = SourceKind::none;
    juce::File sourceFile;
    juce::AudioBuffer<float> sourceBuffer;
    double sourceSampleRate = 44100.0;
    double selectionStart = 0.0, selectionEnd = 0.0;

    // Where a captured take sits on the DAW timeline
    bool takeHasTimeline = false;
    double takeStartSeconds = 0.0;       // host time of the take's first sample
    bool takeHasPpq = false;
    double takeStartPpq = 0.0;

    // Result
    std::shared_ptr<const RenderedAudio> renderedForUi;
    std::shared_ptr<const RenderedAudio> renderedForAudio;
    juce::SpinLock renderedLock;
    std::vector<float> rawPeaks, editPeaks;
    double renderedTailSeconds = -1.0;

    // Render scheduling
    std::atomic<bool> renderRequested { false };
    juce::uint32 renderDueAtMs = 0;
    bool renderPending = false;

    // Host state (written on the audio thread)
    std::atomic<double> currentSampleRate { 0.0 }, hostBpm { 120.0 };
    std::atomic<int> timeSigNumerator { 4 }, timeSigDenominator { 4 };
    std::atomic<bool> hostPlaying { false };

    // Capture (ring buffer owned by the audio thread while recording)
    static constexpr double maxCaptureSeconds = 90.0;
    juce::AudioBuffer<float> captureRing;
    double captureRate = 44100.0;
    std::atomic<int> captureState { captureOff };
    std::atomic<bool> captureStopRequested { false };
    int captureWritePos = 0;
    juce::int64 captureRecorded = 0;
    std::atomic<double> captureSecondsForUi { 0.0 };
    mutable std::atomic<float> captureLevel { 0.0f };   // input peak since the UI last looked
    bool captureHasHostPos = false, captureHasPpq = false;
    juce::int64 captureStartSample = 0, captureExpectedNext = 0;
    double captureStartPpq = 0.0;

    std::atomic<bool> discardTake { false };   // set by clearRecording() while a take is being recorded
    bool waitForTransportStop = false;           // audio thread: a loop/jump ended the take
    juce::File lastSessionTake;                  // take file written by this instance in this session

    // Preview playback
    std::atomic<int> requestedSource { 0 }, activeSource { 0 };
    std::atomic<bool> stopRequested { false }, playing { false }, loopEnabled { false };
    std::atomic<int> playPosition { 0 };

    // Presets
    std::vector<Preset> factoryPresets, userPresets;
    int currentPreset = 0;
    std::atomic<bool> presetModified { false };
    std::atomic<bool> applyingPreset { false };

    // UI change tracking
    std::atomic<int> stateVersion { 0 }, waveformVersion { 0 };
    int noticeVersion = 0;
    juce::String notice;
    bool noticeIsError = false;

    // State restore hand-off to the message thread
    juce::CriticalSection pendingLock;
    bool hasPendingRestore = false;
    juce::ValueTree pendingState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlacierRiserProcessor)
};
