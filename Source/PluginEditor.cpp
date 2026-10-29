#include "PluginEditor.h"
#include "GlacierRiserUIData.h"

namespace
{
    constexpr int designWidth = 860, designHeight = 680;
    constexpr int maxDroppedBytes = 200 * 1024 * 1024;
    const juce::Colour backdrop { 0xff071526 };

    juce::String argString (const juce::Array<juce::var>& args, int index)
    {
        return index < args.size() ? args[index].toString() : juce::String();
    }

    double argDouble (const juce::Array<juce::var>& args, int index, double fallback = 0.0)
    {
        return index < args.size() ? (double) args[index] : fallback;
    }
}

//==============================================================================
GlacierRiserEditor::GlacierRiserEditor (GlacierRiserProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    auto options = createBrowserOptions();

    if (juce::WebBrowserComponent::areOptionsSupported (options))
    {
        browser = std::make_unique<juce::WebBrowserComponent> (options);
        addAndMakeVisible (*browser);
        browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    }
    else
    {
        fallbackLabel.setText ("Glacier Riser needs the Microsoft Edge WebView2 Runtime to draw its interface.\n"
                               "Install it from microsoft.com/edge/webview2 and reopen the plugin.",
                               juce::dontSendNotification);
        fallbackLabel.setJustificationType (juce::Justification::centred);
        fallbackLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (fallbackLabel);
    }

    setResizable (true, true);
    setResizeLimits (645, 510, 1240, 980);   // keeps the 860:680 shape

    if (auto* sizeConstrainer = getConstrainer())
        sizeConstrainer->setFixedAspectRatio ((double) designWidth / (double) designHeight);

    setSize (designWidth, designHeight);
    startTimerHz (30);
}

GlacierRiserEditor::~GlacierRiserEditor()
{
    stopTimer();
}

void GlacierRiserEditor::paint (juce::Graphics& g)
{
    g.fillAll (backdrop);
}

void GlacierRiserEditor::resized()
{
    if (browser != nullptr)
        browser->setBounds (getLocalBounds());

    fallbackLabel.setBounds (getLocalBounds().reduced (40));
}

//==============================================================================
std::optional<juce::WebBrowserComponent::Resource> GlacierRiserEditor::serveResource (const juce::String& url)
{
    const auto path = url.upToFirstOccurrenceOf ("?", false, false);

    if (path == "/" || path == "/index.html")
    {
        const auto* bytes = reinterpret_cast<const std::byte*> (GlacierRiserUIData::index_html);
        juce::WebBrowserComponent::Resource resource;
        resource.data = std::vector<std::byte> (bytes, bytes + GlacierRiserUIData::index_htmlSize);
        resource.mimeType = "text/html";
        return resource;
    }

    return std::nullopt;
}

juce::WebBrowserComponent::Options GlacierRiserEditor::createBrowserOptions()
{
    using Options = juce::WebBrowserComponent::Options;

    // WebView2 needs a writable profile folder: next to the host .exe (the default) usually is not.
    const auto userData = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("GlacierRiserWebView2");

    return Options {}
        .withBackend (Options::Backend::webview2)
        .withWinWebView2Options (Options::WinWebView2 {}
                                     .withUserDataFolder (userData)
                                     .withBackgroundColour (backdrop)
                                     .withStatusBarDisabled())
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeIntegrationEnabled()
        .withResourceProvider ([] (const juce::String& url) { return serveResource (url); })
        .withInitialisationData ("theme", UiSettings::getTheme())

        .withNativeFunction ("ready", [this] (const Args&, Completion done)
        {
            pageReady = true;
            done (makeInitialPayload());
        })
        .withNativeFunction ("setParam", [this] (const Args& args, Completion done)
        {
            processor.setParameterFromUi (argString (args, 0), (float) argDouble (args, 1));
            done (true);
        })
        .withNativeFunction ("beginGesture", [this] (const Args& args, Completion done)
        {
            processor.beginParameterGesture (argString (args, 0));
            done (true);
        })
        .withNativeFunction ("endGesture", [this] (const Args& args, Completion done)
        {
            processor.endParameterGesture (argString (args, 0));
            done (true);
        })
        .withNativeFunction ("setSelection", [this] (const Args& args, Completion done)
        {
            processor.setSelection (argDouble (args, 0), argDouble (args, 1));
            done (true);
        })
        .withNativeFunction ("play", [this] (const Args& args, Completion done)
        {
            processor.startPlayback (argString (args, 0) == "raw" ? GlacierRiserProcessor::PlaySource::raw
                                                                  : GlacierRiserProcessor::PlaySource::processed);
            done (true);
        })
        .withNativeFunction ("stop", [this] (const Args&, Completion done)
        {
            processor.stopPlayback();
            done (true);
        })
        .withNativeFunction ("setLoop", [this] (const Args& args, Completion done)
        {
            processor.setLoopEnabled (args.size() > 0 && (bool) args[0]);
            done (true);
        })
        .withNativeFunction ("reset", [this] (const Args&, Completion done)
        {
            processor.clearRecording();
            done (true);
        })
        .withNativeFunction ("loadPreset", [this] (const Args& args, Completion done)
        {
            done (makeResult (processor.loadPreset ((int) argDouble (args, 0, -1.0))));
        })
        .withNativeFunction ("savePreset", [this] (const Args& args, Completion done)
        {
            juce::String error;
            const bool ok = processor.saveUserPreset (argString (args, 0), error);
            done (makeResult (ok, ok ? "Preset saved" : error));
        })
        .withNativeFunction ("openFile", [this] (const Args&, Completion done)
        {
            openFileChooser();
            done (true);
        })
        .withNativeFunction ("exportWav", [this] (const Args&, Completion done)
        {
            exportWithChooser();
            done (true);
        })
        .withNativeFunction ("openExportsFolder", [this] (const Args&, Completion done)
        {
            auto dir = processor.getExportsDirectory();
            dir.createDirectory();
            dir.startAsProcess();
            done (true);
        })
        .withNativeFunction ("setTheme", [] (const Args& args, Completion done)
        {
            UiSettings::setTheme (argString (args, 0));
            done (true);
        })
        .withNativeFunction ("startDragOut", [this] (const Args&, Completion done)
        {
            juce::String error;
            const auto file = processor.writeExportForDrag (error);

            if (! file.existsAsFile())
            {
                done (makeResult (false, error));
                return;
            }

            const bool started = juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
            done (makeResult (started, started ? "Drop the clip on a track" : "Could not start the drag"));
        })
        .withNativeFunction ("loadAudioData", [this] (const Args& args, Completion done)
        {
            const auto name = argString (args, 0);
            const auto base64 = argString (args, 1);

            if (base64.length() > maxDroppedBytes / 3 * 4 + 4)
            {
                done (makeResult (false, "That file is too large to drop (200 MB max). Use Load audio file instead."));
                return;
            }

            juce::MemoryOutputStream decoded;

            if (! juce::Base64::convertFromBase64 (decoded, base64))
            {
                done (makeResult (false, "Could not read the dropped file."));
                return;
            }

            juce::String error;
            const bool ok = processor.importAudioData (name, decoded.getMemoryBlock(), error);
            done (makeResult (ok, ok ? "Loaded " + name : error));
        });
}

//==============================================================================
juce::var GlacierRiserEditor::makeResult (bool ok, const juce::String& message) const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("ok", ok);
    o->setProperty ("message", message);
    return juce::var (o);
}

juce::var GlacierRiserEditor::makeInitialPayload()
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("state", processor.getStateSnapshot());
    o->setProperty ("params", processor.getParameterSnapshot());
    o->setProperty ("waveforms", processor.getWaveformSnapshot());
    o->setProperty ("playback", processor.getPlaybackSnapshot());

    lastStateVersion = processor.getStateVersion();
    lastWaveformVersion = processor.getWaveformVersion();
    lastNoticeVersion = processor.getNoticeVersion();
    lastParamsJson = juce::JSON::toString (o->getProperty ("params"), true);
    lastPlaybackJson = juce::JSON::toString (o->getProperty ("playback"), true);

    return juce::var (o);
}

void GlacierRiserEditor::emit (const juce::String& eventId, const juce::var& payload)
{
    if (browser != nullptr && pageReady)
        browser->emitEventIfBrowserIsVisible (juce::Identifier (eventId), payload);
}

void GlacierRiserEditor::toast (const juce::String& text, bool isError)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("text", text);
    o->setProperty ("error", isError);
    emit ("glacier:toast", juce::var (o));
}

void GlacierRiserEditor::timerCallback()
{
    if (! pageReady)
        return;

    if (const int v = processor.getStateVersion(); v != lastStateVersion)
    {
        lastStateVersion = v;
        emit ("glacier:state", processor.getStateSnapshot());
    }

    if (const int v = processor.getWaveformVersion(); v != lastWaveformVersion)
    {
        lastWaveformVersion = v;
        emit ("glacier:waveforms", processor.getWaveformSnapshot());
    }

    // The theme is shared by every instance: follow a change made in another editor.
    if (const int v = UiSettings::themeVersion.load(); v != lastThemeVersion)
    {
        lastThemeVersion = v;
        emit ("glacier:theme", UiSettings::getTheme());
    }

    if (const int v = processor.getNoticeVersion(); v != lastNoticeVersion)
    {
        lastNoticeVersion = v;
        bool isError = false;
        const auto text = processor.getNotice (isError);
        toast (text, isError);
    }

    const auto params = processor.getParameterSnapshot();

    if (auto json = juce::JSON::toString (params, true); json != lastParamsJson)
    {
        lastParamsJson = json;
        emit ("glacier:params", params);
    }

    const auto playback = processor.getPlaybackSnapshot();

    if (auto json = juce::JSON::toString (playback, true); json != lastPlaybackJson)
    {
        lastPlaybackJson = json;
        emit ("glacier:playback", playback);
    }
}

//==============================================================================
void GlacierRiserEditor::openFileChooser()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Load an audio file",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       processor.formatManager.getWildcardForAllFormats());

    juce::Component::SafePointer<GlacierRiserEditor> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& fc)
                              {
                                  if (safeThis == nullptr)
                                      return;

                                  const auto result = fc.getResult();

                                  if (! result.existsAsFile())
                                      return;

                                  juce::String error;

                                  if (safeThis->processor.loadSourceFile (result, error))
                                      safeThis->toast ("Loaded " + result.getFileName());
                                  else
                                      safeThis->toast (error, true);
                              });
}

void GlacierRiserEditor::exportWithChooser()
{
    auto dir = processor.getExportsDirectory();
    dir.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> ("Export as WAV", dir.getChildFile (processor.getDefaultExportName()), "*.wav");

    juce::Component::SafePointer<GlacierRiserEditor> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis] (const juce::FileChooser& fc)
                              {
                                  if (safeThis == nullptr)
                                      return;

                                  const auto result = fc.getResult();

                                  if (result == juce::File())
                                      return;

                                  const auto target = result.withFileExtension ("wav");
                                  juce::String error;

                                  if (safeThis->processor.exportRenderedToFile (target, error))
                                      safeThis->toast ("Saved " + target.getFileName());
                                  else
                                      safeThis->toast (error, true);
                              });
}
