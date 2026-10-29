#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    namespace IDs
    {
        const juce::String reverse         { "reverse" };
        const juce::String fade            { "fade" };
        const juce::String size            { "size" };
        const juce::String phoneOn         { "phoneOn" };
        const juce::String chorusOn        { "chorusOn" };
        const juce::String tremoloOn       { "tremoloOn" };
        const juce::String includeOriginal { "includeOriginal" };
        const juce::String phoneMix        { "phoneMix" };
        const juce::String phoneTone       { "phoneTone" };
        const juce::String chorusMix       { "chorusMix" };
        const juce::String chorusRate      { "chorusRate" };
        const juce::String tremoloMix      { "tremoloMix" };
        const juce::String tremoloTiming   { "tremoloTiming" };
        const juce::String damping         { "damping" };
        const juce::String width           { "width" };
        const juce::String level           { "level" };
        const juce::String lowCut          { "lowCut" };
        const juce::String lock            { "lock" };
        const juce::String dry             { "dry" };
    }

    const juce::StringArray allParameterIds { IDs::reverse, IDs::fade, IDs::size,
                                              IDs::phoneOn, IDs::chorusOn, IDs::tremoloOn, IDs::includeOriginal, IDs::phoneMix, IDs::phoneTone,
                                              IDs::chorusMix, IDs::chorusRate, IDs::tremoloMix, IDs::tremoloTiming,
                                              IDs::level, IDs::lowCut, IDs::damping, IDs::width, IDs::dry };

    constexpr double reverseBars[] = { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0 };
    constexpr int defaultReverseIndex = 3;   // 1 BAR
    constexpr double tremoloBeats[] = { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };

    constexpr double maxSourceSeconds = 600.0;     // cap loaded audio at 10 minutes
    constexpr double minSelectionSeconds = 0.01;
    constexpr double longFileSeconds = 8.0;
    constexpr int numPeakBuckets = 720;
    constexpr int renderDebounceMs = 80;

    std::vector<float> computePeaks (const juce::AudioBuffer<float>& buffer, int numBuckets)
    {
        const int n = buffer.getNumSamples();

        if (n == 0 || buffer.getNumChannels() == 0)
            return {};

        std::vector<float> peaks ((size_t) numBuckets, 0.0f);

        for (int i = 0; i < numBuckets; ++i)
        {
            const int s0 = (int) ((juce::int64) n * i / numBuckets);
            const int s1 = juce::jmin (n, juce::jmax (s0 + 1, (int) ((juce::int64) n * (i + 1) / numBuckets)));
            float m = 0.0f;

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                m = juce::jmax (m, buffer.getMagnitude (ch, s0, s1 - s0));

            peaks[(size_t) i] = m;
        }

        return peaks;
    }

    juce::var peaksToVar (const std::vector<float>& peaks)
    {
        juce::Array<juce::var> arr;
        arr.ensureStorageAllocated ((int) peaks.size());

        for (auto p : peaks)
            arr.add (std::round (juce::jlimit (0.0f, 1.0f, p) * 1000.0f) / 1000.0);

        return arr;
    }

    juce::var toVarArray (const juce::StringArray& items)
    {
        juce::Array<juce::var> arr;

        for (const auto& s : items)
            arr.add (s);

        return arr;
    }

    juce::NamedValueSet makeValues (std::initializer_list<std::pair<juce::String, float>> items)
    {
        juce::NamedValueSet set;

        for (const auto& [id, value] : items)
            set.set (juce::Identifier (id), value);

        return set;
    }

    juce::String formatTime (double seconds)
    {
        const bool negative = seconds < 0.0;
        const int totalMs = juce::roundToInt (std::abs (seconds) * 1000.0);
        return (negative ? "-" : "") + juce::String (totalMs / 60000) + ":"
             + juce::String ((totalMs / 1000) % 60).paddedLeft ('0', 2) + "."
             + juce::String (totalMs % 1000).paddedLeft ('0', 3);
    }

    bool readAudioFile (juce::AudioFormatManager& formats, const juce::File& file,
                        juce::AudioBuffer<float>& out, double& rate, juce::String& errorMessage)
    {
        if (! file.existsAsFile())
        {
            errorMessage = "File not found: " + file.getFullPathName();
            return false;
        }

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader == nullptr)
        {
            errorMessage = "Unsupported or unreadable audio file: " + file.getFileName();
            return false;
        }

        const auto maxSamples = (juce::int64) (reader->sampleRate * maxSourceSeconds);
        const auto length = (int) juce::jmin (reader->lengthInSamples, maxSamples);
        const int numChannels = juce::jlimit (1, 2, (int) reader->numChannels);

        if (length <= 0)
        {
            errorMessage = "The file contains no audio: " + file.getFileName();
            return false;
        }

        out.setSize (numChannels, length);
        reader->read (&out, 0, length, 0, true, numChannels > 1);
        rate = reader->sampleRate;
        return true;
    }

    bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double rate)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());

        if (stream == nullptr || stream->failedToOpen())
            return false;

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), rate,
                                                                              (unsigned int) buffer.getNumChannels(), 24, {}, 0));
        if (writer == nullptr)
            return false;

        stream.release();   // the writer owns the stream now
        return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }

    /** The first phrase: from the first onset until a 250 ms pause (at most 4 s). */
    std::pair<double, double> findFirstPhrase (const juce::AudioBuffer<float>& buffer, double rate)
    {
        const int n = buffer.getNumSamples();
        const double total = n / rate;
        const int frame = juce::jmax (16, (int) (rate * 0.010));
        const int numFrames = n / frame;

        if (numFrames < 2)
            return { 0.0, total };

        std::vector<float> rms ((size_t) numFrames);
        float loudest = 0.0f;

        for (int f = 0; f < numFrames; ++f)
        {
            float m = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                m = juce::jmax (m, buffer.getRMSLevel (ch, f * frame, frame));
            rms[(size_t) f] = m;
            loudest = juce::jmax (loudest, m);
        }

        const float threshold = juce::jmax (loudest * 0.1f, 0.0005f);
        int onset = -1;

        for (int f = 0; f < numFrames && onset < 0; ++f)
            if (rms[(size_t) f] >= threshold)
                onset = f;

        if (onset < 0)
            return { 0.0, juce::jmin (total, 4.0) };

        int lastLoud = onset;

        for (int f = onset; f < numFrames; ++f)
        {
            if (rms[(size_t) f] >= threshold)
                lastLoud = f;

            if (f - lastLoud >= 25 || f - onset >= 400)
                break;
        }

        const double start = juce::jmax (0.0, onset * frame / rate - 0.010);
        const double end = juce::jmin (total, juce::jmax (start + 0.2, (lastLoud + 1) * frame / rate + 0.030));
        return { start, end };
    }
}

//==============================================================================
const juce::StringArray& AtmosRiserProcessor::uiParameterIds()
{
    static const juce::StringArray ids { IDs::reverse, IDs::fade, IDs::size,
                                         IDs::phoneOn, IDs::chorusOn, IDs::tremoloOn, IDs::includeOriginal, IDs::phoneMix, IDs::phoneTone,
                                         IDs::chorusMix, IDs::chorusRate, IDs::tremoloMix, IDs::tremoloTiming,
                                         IDs::level, IDs::lowCut, IDs::lock };
    return ids;
}

const juce::StringArray& AtmosRiserProcessor::reverseChoiceNames()
{
    static const juce::StringArray names { "1/8 BAR", "1/4 BAR", "1/2 BAR", "1 BAR", "2 BARS", "4 BARS", "8 BARS" };
    return names;
}

const juce::StringArray& AtmosRiserProcessor::tremoloTimingNames()
{
    static const juce::StringArray names { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
    return names;
}

AtmosRiserProcessor::AtmosRiserProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();

    for (const auto& id : allParameterIds)
        apvts.addParameterListener (id, this);

    factoryPresets = {
        { "Default",     makeValues ({}), {} },
        { "Short Snap",  makeValues ({ { IDs::reverse, 1 }, { IDs::fade, 60 },  { IDs::size, 80 },  { IDs::chorusOn, 0 } }), {} },
        { "Ghost Vocal", makeValues ({ { IDs::reverse, 4 }, { IDs::fade, 100 }, { IDs::size, 140 }, { IDs::chorusMix, 75 }, { IDs::chorusRate, 0.5f }, { IDs::damping, 0.55f } }), {} },
        { "Old Radio",   makeValues ({ { IDs::reverse, 3 }, { IDs::fade, 90 },  { IDs::size, 100 }, { IDs::phoneOn, 1 }, { IDs::chorusOn, 0 }, { IDs::phoneTone, 35 } }), {} },
        { "Pulse Riser", makeValues ({ { IDs::reverse, 4 }, { IDs::fade, 100 }, { IDs::size, 130 }, { IDs::tremoloOn, 1 }, { IDs::chorusOn, 0 }, { IDs::tremoloTiming, 2 } }), {} },
        { "Glacier",     makeValues ({ { IDs::reverse, 5 }, { IDs::fade, 100 }, { IDs::size, 150 }, { IDs::chorusOn, 0 }, { IDs::damping, 0.25f } }), {} },
        { "With Original", makeValues ({ { IDs::includeOriginal, 1 } }), {} },
    };

    refreshUserPresets();

    // In a DAW the plugin starts listening to its track; the standalone app starts with a demo sound.
    if (wrapperType == wrapperType_Standalone)
        loadDemoSource();
    else
        captureState = captureArmed;

    startTimer (30);
}

AtmosRiserProcessor::~AtmosRiserProcessor()
{
    stopTimer();

    for (const auto& id : allParameterIds)
        apvts.removeParameterListener (id, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout AtmosRiserProcessor::createParameterLayout()
{
    using Float  = juce::AudioParameterFloat;
    using Choice = juce::AudioParameterChoice;
    using Bool   = juce::AudioParameterBool;
    using Range  = juce::NormalisableRange<float>;
    using Attrs  = juce::AudioParameterFloatAttributes;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<Choice> (juce::ParameterID { IDs::reverse, 1 }, "Reverse Length", reverseChoiceNames(), defaultReverseIndex));
    params.push_back (std::make_unique<Float> (juce::ParameterID { IDs::fade, 1 },      "Fade",      Range (0.0f, 100.0f, 1.0f), 100.0f, Attrs().withLabel ("%")));
    params.push_back (std::make_unique<Float> (juce::ParameterID { IDs::size, 1 },      "Size",      Range (10.0f, 150.0f, 1.0f), 120.0f, Attrs().withLabel ("%")));
    params.push_back (std::make_unique<Bool> (juce::ParameterID { IDs::phoneOn, 1 },   "Phone",   false));
    params.push_back (std::make_unique<Bool> (juce::ParameterID { IDs::chorusOn, 1 },  "Chorus",  true));
    params.push_back (std::make_unique<Bool> (juce::ParameterID { IDs::tremoloOn, 1 }, "Tremolo", false));
    params.push_back (std::make_unique<Bool> (juce::ParameterID { IDs::includeOriginal, 1 }, "Include Original", false));
    params.push_back (std::make_unique<Float> (juce::ParameterID { IDs::phoneMix, 1 },   "Phone Mix",   Range (0.0f, 100.0f, 1.0f), 100.0f, Attrs().withLabel ("%")));
    params.push_back (std::make_unique<Float> (juce::ParameterID { IDs::phoneTone, 1 },  "Phone Tone",  Range (0.0f, 100.0f, 1.0f), 50.0f,  Attrs().withLabel ("%")));
    params.push_back (std::make_unique<Float> (juce::ParameterID { IDs::chorusMix, 1 },  "Chorus Mix",  Range (0.0f, 100.0f, 1.0f), 60.0f,  Attrs().withLabel ("%")));
    params.push_back (std::make_unique<Float> (juce::ParameterID { IDs::chorusRate, 1 }, "Chorus Rate", Range (0.1f, 5.0f, 0.01f, 0.5f), 0.8f, Attrs().withLabel ("Hz")));
    params.push_back (std::make_unique<Float> (juce::ParameterID { IDs::tremoloMix, 1 }, "Tremolo Mix", Range (0.0f, 100.0f, 1.0f), 100.0f, Attrs().withLabel ("%")));
    params.push_back (std::make_unique<Choice> (juce::ParameterID { IDs::tremoloTiming, 1 }, "Tremolo Timing", tremoloTimingNames(), 1));
    params.push_back (std::make_unique<Float> (juce::ParameterID { IDs::damping, 1 },   "Damping",   Range (0.0f, 1.0f, 0.001f), 0.4f));
    params.push_back (std::make_unique<Float> (juce::ParameterID { IDs::width, 1 },     "Width",     Range (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<Float> (juce::ParameterID { IDs::level, 1 },     "Level",     Range (-24.0f, 6.0f, 0.1f), 0.0f, Attrs().withLabel ("dB")));
    params.push_back (std::make_unique<Float> (juce::ParameterID { IDs::lowCut, 1 },    "Low Cut",   Range (20.0f, 1000.0f, 1.0f, 0.35f), 20.0f, Attrs().withLabel ("Hz")));
    params.push_back (std::make_unique<Bool> (juce::ParameterID { IDs::lock, 1 }, "Lock", false));
    params.push_back (std::make_unique<Float> (juce::ParameterID { IDs::dry, 1 },       "Dry",       Range (0.0f, 1.5f, 0.01f), 1.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
void AtmosRiserProcessor::prepareToPlay (double sampleRate, int)
{
    const double previous = currentSampleRate.exchange (sampleRate);
    playing = false;
    playPosition = 0;

    // The capture ring holds the most recent 90 s of the track at the host rate.
    const int capacity = (int) (sampleRate * maxCaptureSeconds);

    if (captureRing.getNumSamples() != capacity)
        captureRing.setSize (2, capacity, false, true, false);

    captureRate = sampleRate;

    if (captureState.load() == captureRecording)
        captureState = captureArmed;

    // The clip is rendered at the host rate, so a rate change needs a re-render.
    if (std::abs (previous - sampleRate) > 1.0e-6)
        requestRender();
}

bool AtmosRiserProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == out;
}

void AtmosRiserProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numInputs = getTotalNumInputChannels();

    for (int ch = numInputs; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    //-- Host transport -------------------------------------------------------------
    bool hasTransport = false, transportPlaying = false;
    juce::Optional<juce::int64> timeInSamples;
    juce::Optional<double> ppq;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
        {
            hasTransport = true;
            transportPlaying = position->getIsPlaying();
            timeInSamples = position->getTimeInSamples();
            ppq = position->getPpqPosition();

            if (const auto bpm = position->getBpm())
                if (*bpm > 1.0)
                    hostBpm = *bpm;

            if (const auto sig = position->getTimeSignature())
            {
                timeSigNumerator = juce::jmax (1, sig->numerator);
                timeSigDenominator = juce::jmax (1, sig->denominator);
            }
        }
    }

    hostPlaying = transportPlaying;

    //-- Capture the incoming track audio (before anything is added to it) ----------------
    int capture = captureState.load();
    const int capacity = captureRing.getNumSamples();

    // Every DAW playback captures the track; when it ends, the new take replaces the old one.
    // After a loop or jump the take closes and capturing waits for the next press of play,
    // so a loop cycle never leaves a partial take behind.
    if (! transportPlaying)
        waitForTransportStop = false;

    // Exports and bounces run the transport too: never capture while rendering offline.
    // The lock keeps the current take no matter what the DAW plays.
    const bool locked = apvts.getRawParameterValue (IDs::lock)->load() > 0.5f;

    if (capture == captureRecording && (locked || isNonRealtime()))
    {
        discardTake = true;
        captureState = captureFinished;
        capture = captureFinished;
    }

    if (capture == captureArmed && capacity > 0 && hasTransport && transportPlaying && ! waitForTransportStop
        && ! locked && ! isNonRealtime())
    {
        captureWritePos = 0;
        captureRecorded = 0;
        captureHasHostPos = timeInSamples.hasValue();
        captureStartSample = timeInSamples.orFallback ((juce::int64) 0);
        captureExpectedNext = captureStartSample;
        captureHasPpq = ppq.hasValue();
        captureStartPpq = ppq.orFallback (0.0);
        captureStopRequested = false;
        captureState = captureRecording;
        capture = captureRecording;
    }

    if (capture == captureRecording)
    {
        const bool jumped = captureHasHostPos && timeInSamples.hasValue() && *timeInSamples != captureExpectedNext;
        const bool stopped = hasTransport && ! transportPlaying;

        if (captureStopRequested.exchange (false) || stopped || jumped)
        {
            waitForTransportStop = jumped;
            captureState = captureFinished;   // the message thread collects the take
        }
        else
        {
            int remaining = numSamples, src = 0;

            while (remaining > 0)
            {
                const int chunk = juce::jmin (remaining, capacity - captureWritePos);

                for (int ch = 0; ch < 2; ++ch)
                {
                    if (numInputs > 0)
                        captureRing.copyFrom (ch, captureWritePos, buffer, juce::jmin (ch, numInputs - 1), src, chunk);
                    else
                        captureRing.clear (ch, captureWritePos, chunk);
                }

                captureWritePos = (captureWritePos + chunk) % capacity;
                src += chunk;
                remaining -= chunk;
            }

            captureRecorded += numSamples;
            captureExpectedNext += numSamples;
            captureSecondsForUi = (double) captureRecorded / captureRate;

            // Live level for the recording animation: keep the loudest block until the UI reads it.
            float blockPeak = 0.0f;
            for (int ch = 0; ch < juce::jmin (2, numInputs); ++ch)
                blockPeak = juce::jmax (blockPeak, buffer.getMagnitude (ch, 0, numSamples));

            if (blockPeak > captureLevel.load (std::memory_order_relaxed))
                captureLevel.store (blockPeak, std::memory_order_relaxed);
        }
    }

    //-- Preview requests -------------------------------------------------------------
    // Any note-on retriggers the processed clip from the start (one-shot).
    for (const auto metadata : midiMessages)
        if (metadata.getMessage().isNoteOn())
            requestedSource = (int) PlaySource::processed;

    if (const int requested = requestedSource.exchange (0); requested != 0)
    {
        activeSource = requested;
        playPosition = 0;
        playing = true;
    }

    if (stopRequested.exchange (false))
        playing = false;

    if (! playing)
        return;

    const juce::SpinLock::ScopedTryLockType lock (renderedLock);

    if (! lock.isLocked() || renderedForAudio == nullptr)
        return;   // leave the input untouched for this block

    //-- Preview (Listen / Loop / MIDI note) ----------------------------------------------

    const auto& clip = activeSource.load() == (int) PlaySource::raw ? renderedForAudio->dry
                                                                    : renderedForAudio->buffer;
    const int clipLength = clip.getNumSamples();

    if (clipLength == 0)
    {
        playing = false;
        return;
    }

    int pos = juce::jlimit (0, clipLength, playPosition.load());
    int written = 0;

    while (written < numSamples)
    {
        const int toCopy = juce::jmin (numSamples - written, clipLength - pos);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addFrom (ch, written, clip, juce::jmin (ch, clip.getNumChannels() - 1), pos, toCopy);

        pos += toCopy;
        written += toCopy;

        if (pos >= clipLength)
        {
            if (! loopEnabled)
            {
                playing = false;
                break;
            }

            pos = 0;
        }
    }

    playPosition = pos;
}

juce::AudioProcessorEditor* AtmosRiserProcessor::createEditor()
{
    return new AtmosRiserEditor (*this);
}

//==============================================================================
void AtmosRiserProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    const bool hasFile = sourceKind == SourceKind::file || sourceKind == SourceKind::capture;

    state.setProperty ("sourceKind", (int) sourceKind, nullptr);
    state.setProperty ("sourceFile", hasFile ? sourceFile.getFullPathName() : juce::String(), nullptr);
    state.setProperty ("selectionStart", selectionStart, nullptr);
    state.setProperty ("selectionEnd", selectionEnd, nullptr);
    state.setProperty ("takeHasTimeline", takeHasTimeline, nullptr);
    state.setProperty ("takeStartSeconds", takeStartSeconds, nullptr);
    state.setProperty ("takeHasPpq", takeHasPpq, nullptr);
    state.setProperty ("takeStartPpq", takeStartPpq, nullptr);
    state.setProperty ("preset", currentPreset, nullptr);
    state.setProperty ("presetModified", presetModified.load(), nullptr);
    state.setProperty ("loop", loopEnabled.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AtmosRiserProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);

    // A session saved by a plugin this one replaces carries foreign parameters: keep our defaults.
    if (! state.getChildWithProperty ("id", IDs::reverse).isValid())
        return;

    apvts.replaceState (state);
    loopEnabled = (bool) state.getProperty ("loop", false);

    const juce::ScopedLock sl (pendingLock);
    pendingState = state.createCopy();
    hasPendingRestore = true;
}

//==============================================================================
void AtmosRiserProcessor::setSource (juce::AudioBuffer<float>&& audio, double rate, SourceKind kind, const juce::File& file)
{
    sourceBuffer = std::move (audio);
    sourceSampleRate = rate;
    sourceKind = kind;
    sourceFile = file;

    if (kind != SourceKind::capture)
    {
        takeHasTimeline = false;
        takeHasPpq = false;
    }

    const auto [start, end] = defaultSelection();
    selectionStart = start;
    selectionEnd = end;

    rebuildRawPeaks();
    bumpState();
    requestRender();
}

std::pair<double, double> AtmosRiserProcessor::defaultSelection() const
{
    const double total = sourceSampleRate > 0.0 ? sourceBuffer.getNumSamples() / sourceSampleRate : 0.0;

    if (total <= 0.0)
        return { 0.0, 0.0 };

    // Captures and long files start on their first phrase; short files are used whole.
    if (sourceKind == SourceKind::capture || total > longFileSeconds)
        return findFirstPhrase (sourceBuffer, sourceSampleRate);

    return { 0.0, total };
}

void AtmosRiserProcessor::loadDemoSource()
{
    // A short, bright A-major pluck so the standalone app has something to show and play.
    constexpr double sr = 44100.0;
    constexpr double lengthSeconds = 1.8;
    const int n = (int) (sr * lengthSeconds);
    const double notes[] = { 220.0, 329.63, 440.0, 554.37, 659.25 };

    juce::AudioBuffer<float> demo (2, n);
    demo.clear();
    juce::Random random (1234);

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* data = demo.getWritePointer (ch);
        const double detune = ch == 0 ? 1.0 : 1.0015;

        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double attack = juce::jmin (1.0, t / 0.004);
            double v = 0.0;

            for (int k = 0; k < (int) std::size (notes); ++k)
                for (int p = 1; p <= 3; ++p)
                    v += std::sin (juce::MathConstants<double>::twoPi * notes[k] * p * detune * t + k)
                         * (p == 1 ? 1.0 : (p == 2 ? 0.35 : 0.15))
                         * std::exp (-t * 3.0 * (1.0 + 0.5 * (p - 1)));

            v *= attack;
            v += (random.nextFloat() * 2.0f - 1.0f) * std::exp (-t * 400.0) * 0.6;   // pick noise
            data[i] = (float) v;
        }
    }

    demo.applyGain (0.8f / juce::jmax (1.0e-6f, demo.getMagnitude (0, n)));
    demo.applyGainRamp (n - 2000, 2000, 1.0f, 0.0f);
    setSource (std::move (demo), sr, SourceKind::demo, {});
}

bool AtmosRiserProcessor::loadSourceFile (const juce::File& file, juce::String& errorMessage)
{
    JUCE_ASSERT_MESSAGE_THREAD

    juce::AudioBuffer<float> audio;
    double rate = 44100.0;

    if (! readAudioFile (formatManager, file, audio, rate, errorMessage))
        return false;

    // A chosen file replaces any take in progress (the next DAW playback captures over it).
    if (captureState.load() == captureRecording)
    {
        discardTake = true;
        captureStopRequested = true;
    }

    setSource (std::move (audio), rate, SourceKind::file, file);
    return true;
}

bool AtmosRiserProcessor::importAudioData (const juce::String& fileName, const juce::MemoryBlock& data, juce::String& errorMessage)
{
    auto dir = getImportDirectory();

    if (! dir.createDirectory())
    {
        errorMessage = "Could not create " + dir.getFullPathName();
        return false;
    }

    auto legalName = juce::File::createLegalFileName (fileName);

    if (legalName.isEmpty())
        legalName = "dropped_audio.wav";

    auto target = dir.getChildFile (legalName);

    // Re-dropping the same file reuses the stored copy instead of piling up duplicates.
    if (target.existsAsFile() && target.getSize() == (juce::int64) data.getSize())
        return loadSourceFile (target, errorMessage);

    target = target.getNonexistentSibling (false);

    if (! target.replaceWithData (data.getData(), data.getSize()))
    {
        errorMessage = "Could not store " + target.getFullPathName();
        return false;
    }

    return loadSourceFile (target, errorMessage);
}

void AtmosRiserProcessor::setSelection (double startSeconds, double endSeconds)
{
    JUCE_ASSERT_MESSAGE_THREAD

    const double length = sourceSampleRate > 0.0 ? sourceBuffer.getNumSamples() / sourceSampleRate : 0.0;

    if (length <= 0.0)
        return;

    if (startSeconds > endSeconds)
        std::swap (startSeconds, endSeconds);

    startSeconds = juce::jlimit (0.0, length, startSeconds);
    endSeconds   = juce::jlimit (0.0, length, endSeconds);

    if (endSeconds - startSeconds < minSelectionSeconds)
        endSeconds = juce::jmin (length, startSeconds + minSelectionSeconds);

    if (std::abs (startSeconds - selectionStart) < 1.0e-9 && std::abs (endSeconds - selectionEnd) < 1.0e-9)
        return;

    selectionStart = startSeconds;
    selectionEnd = endSeconds;

    bumpState();
    requestRender();
}

void AtmosRiserProcessor::rebuildRawPeaks()
{
    rawPeaks = computePeaks (sourceBuffer, numPeakBuckets);
    ++waveformVersion;
}

//==============================================================================
void AtmosRiserProcessor::clearRecording()
{
    // Reset means "capture again", so it also releases the lock.
    if (auto* lockParam = apvts.getParameter (IDs::lock); lockParam != nullptr && lockParam->getValue() > 0.5f)
    {
        lockParam->beginChangeGesture();
        lockParam->setValueNotifyingHost (0.0f);
        lockParam->endChangeGesture();
    }

    // A take still being recorded is thrown away when the audio thread closes it.
    if (captureState.load() == captureRecording)
    {
        discardTake = true;
        captureStopRequested = true;
    }

    stopPlayback();

    {
        std::shared_ptr<const RenderedAudio> previous;
        const juce::SpinLock::ScopedLockType sl (renderedLock);
        previous = std::move (renderedForAudio);
    }

    renderedForUi = nullptr;
    sourceBuffer.setSize (0, 0);
    sourceKind = SourceKind::none;
    sourceFile = juce::File();
    takeHasTimeline = takeHasPpq = false;
    selectionStart = selectionEnd = 0.0;
    rawPeaks.clear();
    editPeaks.clear();
    renderPending = false;
    renderRequested = false;

    if (captureState.load() != captureRecording)
        captureState = captureArmed;   // the next playback captures again

    ++waveformVersion;
    bumpState();
}

void AtmosRiserProcessor::finalizeCapture()
{
    if (discardTake.exchange (false))
    {
        captureState = captureArmed;
        bumpState();
        return;
    }

    const int capacity = captureRing.getNumSamples();
    const juce::int64 recorded = captureRecorded;
    const int count = (int) juce::jmin (recorded, (juce::int64) capacity);

    if (count < (int) (captureRate * 0.05))
    {
        captureState = captureArmed;   // too short to be a take (e.g. a click on play and stop)
        bumpState();
        return;
    }

    // Unroll the ring buffer into chronological order.
    juce::AudioBuffer<float> take (2, count);
    const int startIndex = recorded > capacity ? captureWritePos : 0;
    const int firstPart = juce::jmin (count, capacity - startIndex);

    for (int ch = 0; ch < 2; ++ch)
    {
        take.copyFrom (ch, 0, captureRing, ch, startIndex, firstPart);

        if (count > firstPart)
            take.copyFrom (ch, firstPart, captureRing, ch, 0, count - firstPart);
    }

    if (take.getMagnitude (0, count) < 0.001f)
    {
        captureState = captureArmed;   // a silent playback keeps the previous take
        bumpState();
        return;
    }

    const juce::int64 dropped = recorded - count;   // samples that fell out of the 90 s window

    takeHasTimeline = captureHasHostPos;
    takeStartSeconds = (double) (captureStartSample + dropped) / captureRate;
    takeHasPpq = captureHasPpq;
    takeStartPpq = captureStartPpq + (double) dropped / captureRate * hostBpm.load() / 60.0;

    // One file per take, named uniquely so duplicated tracks never share it. The previous take
    // this instance wrote in this session is deleted, so playback after playback doesn't fill the
    // disk; takes restored from a saved project are left alone (another save may still use them).
    const auto file = getCaptureDirectory().getChildFile ("take_" + juce::Uuid().toString() + ".wav");
    const bool saved = writeWav (file, take, captureRate);

    if (saved)
    {
        if (lastSessionTake != juce::File() && lastSessionTake != file)
            lastSessionTake.deleteFile();

        lastSessionTake = file;
    }
    const double seconds = count / captureRate;

    captureState = captureArmed;   // keep listening: the next playback replaces this take
    setSource (std::move (take), captureRate, SourceKind::capture, saved ? file : juce::File());

    juce::ignoreUnused (seconds);
}

juce::String AtmosRiserProcessor::describeTimelinePosition (double sourceSeconds) const
{
    if (sourceKind != SourceKind::capture)
        return {};

    if (takeHasPpq)
    {
        const double ppq = takeStartPpq + sourceSeconds * hostBpm.load() / 60.0;
        const double beatsPerBar = timeSigNumerator.load() * 4.0 / (double) timeSigDenominator.load();
        const double barFloat = std::floor (ppq / beatsPerBar);
        const double beat = std::floor (ppq - barFloat * beatsPerBar);
        return "bar " + juce::String ((int) barFloat + 1) + "." + juce::String ((int) beat + 1);
    }

    if (takeHasTimeline)
        return formatTime (takeStartSeconds + sourceSeconds);

    return {};
}

void AtmosRiserProcessor::postNotice (const juce::String& text, bool isError)
{
    notice = text;
    noticeIsError = isError;
    ++noticeVersion;
}

//==============================================================================
void AtmosRiserProcessor::setParameterFromUi (const juce::String& id, float realValue)
{
    if (auto* param = apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (realValue));
}

void AtmosRiserProcessor::beginParameterGesture (const juce::String& id)
{
    if (auto* param = apvts.getParameter (id))
        param->beginChangeGesture();
}

void AtmosRiserProcessor::endParameterGesture (const juce::String& id)
{
    if (auto* param = apvts.getParameter (id))
        param->endChangeGesture();
}

void AtmosRiserProcessor::parameterChanged (const juce::String&, float)
{
    requestRender();

    if (! applyingPreset && ! presetModified.exchange (true))
        bumpState();
}

//==============================================================================
juce::File AtmosRiserProcessor::getUserPresetDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("AtmosRiser").getChildFile ("Presets");
}

juce::File AtmosRiserProcessor::getImportDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("AtmosRiser").getChildFile ("Imported Audio");
}

juce::File AtmosRiserProcessor::getCaptureDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("AtmosRiser").getChildFile ("Captures");
}

juce::File AtmosRiserProcessor::getExportsDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("AtmosRiser Exports");
}

void AtmosRiserProcessor::refreshUserPresets()
{
    userPresets.clear();

    for (const auto& entry : juce::RangedDirectoryIterator (getUserPresetDirectory(), false, "*.atmospreset"))
    {
        const auto file = entry.getFile();
        const auto json = juce::JSON::parse (file);

        if (! json.isObject())
            continue;

        Preset preset;
        preset.name = json.getProperty ("name", file.getFileNameWithoutExtension()).toString();
        preset.file = file;

        if (auto* params = json.getProperty ("params", {}).getDynamicObject())
            for (const auto& prop : params->getProperties())
                preset.values.set (prop.name, (float) prop.value);

        userPresets.push_back (std::move (preset));
    }

    std::sort (userPresets.begin(), userPresets.end(),
               [] (const Preset& a, const Preset& b) { return a.name.compareIgnoreCase (b.name) < 0; });
}

juce::StringArray AtmosRiserProcessor::getPresetNames() const
{
    juce::StringArray names;

    for (const auto& p : factoryPresets) names.add (p.name);
    for (const auto& p : userPresets)    names.add (p.name);

    return names;
}

void AtmosRiserProcessor::applyPresetValues (const juce::NamedValueSet& values)
{
    applyingPreset = true;

    for (const auto& id : allParameterIds)
    {
        if (auto* param = apvts.getParameter (id))
        {
            if (const auto* v = values.getVarPointer (juce::Identifier (id)))
                param->setValueNotifyingHost (param->convertTo0to1 ((float) *v));
            else
                param->setValueNotifyingHost (param->getDefaultValue());
        }
    }

    applyingPreset = false;
}

bool AtmosRiserProcessor::loadPreset (int index)
{
    const int numFactory = (int) factoryPresets.size();
    const int total = numFactory + (int) userPresets.size();

    if (index < 0 || index >= total)
        return false;

    const auto& preset = index < numFactory ? factoryPresets[(size_t) index]
                                            : userPresets[(size_t) (index - numFactory)];
    applyPresetValues (preset.values);
    currentPreset = index;
    presetModified = false;
    bumpState();
    return true;
}

bool AtmosRiserProcessor::saveUserPreset (const juce::String& rawName, juce::String& errorMessage)
{
    const auto name = rawName.trim().substring (0, 48);

    if (name.isEmpty())
    {
        errorMessage = "Give the preset a name.";
        return false;
    }

    for (const auto& p : factoryPresets)
    {
        if (p.name.equalsIgnoreCase (name))
        {
            errorMessage = "\"" + name + "\" is a factory preset. Choose another name.";
            return false;
        }
    }

    auto dir = getUserPresetDirectory();

    if (! dir.createDirectory())
    {
        errorMessage = "Could not create " + dir.getFullPathName();
        return false;
    }

    auto params = std::make_unique<juce::DynamicObject>();

    for (const auto& id : allParameterIds)
        if (auto* param = apvts.getParameter (id))
            params->setProperty (juce::Identifier (id), param->convertFrom0to1 (param->getValue()));

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("name", name);
    root->setProperty ("params", juce::var (params.release()));

    const auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".atmospreset");

    if (! file.replaceWithText (juce::JSON::toString (juce::var (root.release()))))
    {
        errorMessage = "Could not write " + file.getFullPathName();
        return false;
    }

    refreshUserPresets();

    for (size_t i = 0; i < userPresets.size(); ++i)
        if (userPresets[i].file == file)
            currentPreset = (int) (factoryPresets.size() + i);

    presetModified = false;
    bumpState();
    return true;
}

//==============================================================================
void AtmosRiserProcessor::startPlayback (PlaySource source) noexcept
{
    requestedSource = (int) source;
}

void AtmosRiserProcessor::setLoopEnabled (bool shouldLoop) noexcept
{
    if (loopEnabled.exchange (shouldLoop) != shouldLoop)
        bumpState();
}

//==============================================================================
void AtmosRiserProcessor::timerCallback()
{
    // Finish a state restore (setStateInformation may run on any thread).
    juce::ValueTree restore;

    {
        const juce::ScopedLock sl (pendingLock);

        if (hasPendingRestore)
        {
            restore = pendingState;
            hasPendingRestore = false;
        }
    }

    if (restore.isValid())
    {
        const auto kind = (SourceKind) (int) restore.getProperty ("sourceKind", (int) SourceKind::file);
        const juce::File file (restore.getProperty ("sourceFile", "").toString());
        juce::AudioBuffer<float> audio;
        double rate = 44100.0;
        juce::String error;

        if (file != juce::File() && readAudioFile (formatManager, file, audio, rate, error))
        {
            const auto restoredKind = kind == SourceKind::capture ? SourceKind::capture : SourceKind::file;

            if (restoredKind == SourceKind::capture)
            {
                takeHasTimeline = (bool) restore.getProperty ("takeHasTimeline", false);
                takeStartSeconds = (double) restore.getProperty ("takeStartSeconds", 0.0);
                takeHasPpq = (bool) restore.getProperty ("takeHasPpq", false);
                takeStartPpq = (double) restore.getProperty ("takeStartPpq", 0.0);
            }

            sourceKind = restoredKind;   // so setSource keeps the timeline of a capture
            setSource (std::move (audio), rate, restoredKind, file);

            const double selStart = (double) restore.getProperty ("selectionStart", -1.0);
            const double selEnd = (double) restore.getProperty ("selectionEnd", -1.0);

            if (selStart >= 0.0 && selEnd > selStart)
                setSelection (selStart, selEnd);
        }
        else if (file != juce::File())
        {
            postNotice ("The audio this project used is missing: " + file.getFullPathName(), true);
        }

        if (captureState.load() != captureRecording)
            captureState = captureArmed;

        refreshUserPresets();
        currentPreset = juce::jlimit (0, getPresetNames().size() - 1, (int) restore.getProperty ("preset", 0));
        presetModified = (bool) restore.getProperty ("presetModified", false);
        bumpState();
        requestRender();
    }

    // A take finished on the audio thread.
    if (captureState.load() == captureFinished)
        finalizeCapture();

    // Tempo or time-signature changes move the musical length of the swell. Only ask once:
    // re-requesting while a render is pending would keep pushing the debounce back forever.
    if (! renderPending && ! renderRequested.load()
        && renderedTailSeconds >= 0.0 && std::abs (computeTailSeconds() - renderedTailSeconds) > 1.0e-3)
        requestRender();

    // Debounce: knob drags and selection drags fire many updates in a row.
    const auto now = juce::Time::getMillisecondCounter();

    if (renderRequested.exchange (false))
    {
        renderPending = true;
        renderDueAtMs = now + (juce::uint32) renderDebounceMs;
    }

    if (renderPending && now >= renderDueAtMs)
    {
        renderPending = false;
        renderNow();
    }
}

double AtmosRiserProcessor::computeTailSeconds() const
{
    const auto index = juce::jlimit (0, (int) std::size (reverseBars) - 1,
                                     (int) apvts.getRawParameterValue (IDs::reverse)->load());
    const double bpm = juce::jlimit (20.0, 400.0, hostBpm.load());
    const double beatsPerBar = timeSigNumerator.load() * 4.0 / (double) timeSigDenominator.load();

    return juce::jlimit (0.05, 32.0, reverseBars[index] * beatsPerBar * 60.0 / bpm);
}

ReverseReverbSettings AtmosRiserProcessor::currentSettings() const
{
    auto raw = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };

    ReverseReverbSettings s;
    const double hostRate = currentSampleRate.load();
    s.sampleRate       = hostRate > 0.0 ? hostRate : sourceSampleRate;
    s.bpm              = hostBpm.load();
    s.tailSeconds      = (float) computeTailSeconds();
    s.preDelayMs       = 20.0f;   // fixed: only shifts the placement of a swell-only clip
    s.diffusion        = 0.7f;    // fixed: a smooth, dense onset suits almost every source
    s.fade             = raw (IDs::fade) / 100.0f;
    s.damping          = raw (IDs::damping);
    s.width            = raw (IDs::width);
    s.wetGain          = juce::Decibels::decibelsToGain (raw (IDs::level), -100.0f);
    s.lowCutHz         = raw (IDs::lowCut);
    s.dryGain          = raw (IDs::dry);
    s.includeOriginal  = raw (IDs::includeOriginal) > 0.5f;
    s.phoneOn          = raw (IDs::phoneOn) > 0.5f;
    s.chorusOn         = raw (IDs::chorusOn) > 0.5f;
    s.tremoloOn        = raw (IDs::tremoloOn) > 0.5f;
    s.phoneMix         = raw (IDs::phoneMix) / 100.0f;
    s.phoneTone        = raw (IDs::phoneTone) / 100.0f;
    s.chorusMix        = raw (IDs::chorusMix) / 100.0f;
    s.chorusRateHz     = raw (IDs::chorusRate);
    s.tremoloMix       = raw (IDs::tremoloMix) / 100.0f;
    s.tremoloBeats     = tremoloBeats[juce::jlimit (0, (int) std::size (tremoloBeats) - 1, (int) raw (IDs::tremoloTiming))];
    s.sibilanceControl = true;   // automatic sibilance detection is always on

    // Size: 100 % is a medium-large room, 150 % approaches Freeverb's maximum feedback.
    const float size = raw (IDs::size);
    s.roomSize = size <= 100.0f ? 0.7f * size / 100.0f
                                : 0.7f + (size - 100.0f) / 50.0f * 0.28f;

    return s;
}

void AtmosRiserProcessor::renderNow()
{
    JUCE_ASSERT_MESSAGE_THREAD

    std::shared_ptr<RenderedAudio> result;
    const auto settings = currentSettings();

    if (sourceBuffer.getNumSamples() > 0)
    {
        const int total = sourceBuffer.getNumSamples();
        const int start = juce::jlimit (0, total - 1, juce::roundToInt (selectionStart * sourceSampleRate));
        const int end   = juce::jlimit (start + 1, total, juce::roundToInt (selectionEnd * sourceSampleRate));

        result = ReverseReverbRenderer::render (sourceBuffer, sourceSampleRate, start, end - start, settings);

        // A captured take knows where it sits on the timeline, so the clip can play in sync:
        // the original phrase starts at take start + selection start.
        if (sourceKind == SourceKind::capture && takeHasTimeline)
        {
            const double phraseSeconds = takeStartSeconds + selectionStart;
            result->hasHostPosition = true;
            result->hostClipStartSample = (juce::int64) std::llround (phraseSeconds * settings.sampleRate)
                                          - result->originalStartSample;
        }
    }

    {
        // Swap under the lock, but free the previous clip outside of it.
        std::shared_ptr<const RenderedAudio> previous;

        {
            const juce::SpinLock::ScopedLockType sl (renderedLock);
            previous = std::move (renderedForAudio);
            renderedForAudio = result;
        }
    }

    renderedForUi = result;
    renderedTailSeconds = settings.tailSeconds;
    editPeaks = result != nullptr ? computePeaks (result->buffer, numPeakBuckets) : std::vector<float>();

    ++waveformVersion;
    bumpState();
}

//==============================================================================
BarAlignment AtmosRiserProcessor::getExportAlignment() const
{
    auto clip = renderedForUi;

    if (clip == nullptr || sourceKind != SourceKind::capture || ! takeHasPpq)
        return {};

    const double bpm = juce::jlimit (20.0, 400.0, hostBpm.load());
    const double beatsPerBar = timeSigNumerator.load() * 4.0 / (double) timeSigDenominator.load();
    const double clipStartSeconds = selectionStart - clip->originalStartSample / clip->sampleRate;
    const double clipStartPpq = takeStartPpq + clipStartSeconds * bpm / 60.0;

    return alignToBar (clipStartPpq, bpm, beatsPerBar, clip->sampleRate);
}

juce::String AtmosRiserProcessor::getDefaultExportName() const
{
    juce::String base;

    switch (sourceKind)
    {
        case SourceKind::demo:    base = "demo"; break;
        case SourceKind::capture: base = "track"; break;
        case SourceKind::file:    base = sourceFile.getFileNameWithoutExtension(); break;
        case SourceKind::none:    base = "clip"; break;
    }

    // Captured risers say which bar to drop them on (their first sample sits on that bar line).
    if (const auto align = getExportAlignment(); align.valid)
        base << "_drop-on-bar-" << align.bar;

    return base + "_atmosriser.wav";
}

juce::File AtmosRiserProcessor::writeExportForDrag (juce::String& errorMessage)
{
    auto file = getExportsDirectory().getChildFile (getDefaultExportName()).getNonexistentSibling();
    return exportRenderedToFile (file, errorMessage) ? file : juce::File();
}

bool AtmosRiserProcessor::exportRenderedToFile (const juce::File& file, juce::String& errorMessage) const
{
    auto clip = renderedForUi;

    if (clip == nullptr || clip->buffer.getNumSamples() == 0)
    {
        errorMessage = "Nothing to export yet. Capture the track or load a file first.";
        return false;
    }

    // Pad the front with silence so the clip starts exactly on a bar line.
    const auto align = getExportAlignment();
    const int pad = align.valid ? juce::jmax (0, align.padSamples) : 0;
    juce::AudioBuffer<float> padded (clip->buffer.getNumChannels(), clip->buffer.getNumSamples() + pad);
    padded.clear();

    for (int ch = 0; ch < padded.getNumChannels(); ++ch)
        padded.copyFrom (ch, pad, clip->buffer, ch, 0, clip->buffer.getNumSamples());

    if (! writeWav (file, padded, clip->sampleRate))
    {
        errorMessage = "Could not write " + file.getFullPathName();
        return false;
    }

    return true;
}

//==============================================================================
juce::var AtmosRiserProcessor::getStateSnapshot() const
{
    auto* o = new juce::DynamicObject();
    const double sourceSeconds = sourceSampleRate > 0.0 ? sourceBuffer.getNumSamples() / sourceSampleRate : 0.0;

    const char* kindNames[] = { "none", "demo", "file", "capture" };
    juce::String fileName;

    switch (sourceKind)
    {
        case SourceKind::demo:    fileName = "Demo pluck"; break;
        case SourceKind::capture: fileName = "Track capture"; break;
        case SourceKind::file:    fileName = sourceFile.getFileName(); break;
        case SourceKind::none:    break;
    }

    o->setProperty ("hasSource", sourceBuffer.getNumSamples() > 0);
    o->setProperty ("sourceKind", kindNames[(int) sourceKind]);
    o->setProperty ("isDemo", sourceKind == SourceKind::demo);
    o->setProperty ("fileName", fileName);
    o->setProperty ("sourceSeconds", sourceSeconds);
    o->setProperty ("sourceRate", sourceSampleRate);
    o->setProperty ("channels", sourceBuffer.getNumChannels());
    o->setProperty ("selStart", selectionStart);
    o->setProperty ("selEnd", selectionEnd);
    o->setProperty ("hasTimeline", sourceKind == SourceKind::capture && takeHasTimeline);
    o->setProperty ("phraseAt", describeTimelinePosition (selectionStart));
    o->setProperty ("swellAt", juce::String());
    o->setProperty ("sibilantRegions", juce::Array<juce::var>());

    if (auto clip = renderedForUi; clip != nullptr && clip->buffer.getNumSamples() > 0)
    {
        auto* r = new juce::DynamicObject();
        r->setProperty ("seconds", clip->buffer.getNumSamples() / clip->sampleRate);
        r->setProperty ("sampleRate", clip->sampleRate);
        r->setProperty ("originalStart", clip->originalStartSample / clip->sampleRate);
        r->setProperty ("originalEnd", (clip->originalStartSample + clip->originalLengthSamples) / clip->sampleRate);
        r->setProperty ("includesOriginal", clip->includesOriginal);
        o->setProperty ("rendered", juce::var (r));

        o->setProperty ("swellAt", describeTimelinePosition (selectionStart - clip->originalStartSample / clip->sampleRate));

        juce::Array<juce::var> regions;

        for (const auto& [a, b] : clip->sibilantRegions)
            regions.add (juce::Array<juce::var> { selectionStart + a, selectionStart + b });

        o->setProperty ("sibilantRegions", regions);
    }
    else
    {
        o->setProperty ("rendered", juce::var());
    }

    const char* captureNames[] = { "off", "armed", "recording", "recording" };
    o->setProperty ("capture", captureNames[juce::jlimit (0, 3, captureState.load())]);
    o->setProperty ("presets", toVarArray (getPresetNames()));
    o->setProperty ("factoryPresetCount", (int) factoryPresets.size());
    o->setProperty ("presetIndex", currentPreset);
    o->setProperty ("presetModified", presetModified.load());
    o->setProperty ("bpm", hostBpm.load());
    o->setProperty ("tailSeconds", computeTailSeconds());
    o->setProperty ("loop", loopEnabled.load());
    o->setProperty ("exportsFolder", getExportsDirectory().getFullPathName());
    o->setProperty ("isStandalone", wrapperType == wrapperType_Standalone);

    return juce::var (o);
}

juce::var AtmosRiserProcessor::getParameterSnapshot() const
{
    auto* o = new juce::DynamicObject();

    for (const auto& id : uiParameterIds())
        o->setProperty (juce::Identifier (id), apvts.getRawParameterValue (id)->load());

    return juce::var (o);
}

juce::var AtmosRiserProcessor::getWaveformSnapshot() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("raw", peaksToVar (rawPeaks));
    o->setProperty ("edit", peaksToVar (editPeaks));
    o->setProperty ("version", waveformVersion.load());
    return juce::var (o);
}

juce::var AtmosRiserProcessor::getPlaybackSnapshot() const
{
    auto* o = new juce::DynamicObject();
    const bool isRaw = activeSource.load() == (int) PlaySource::raw;
    const double rate = renderedForUi != nullptr ? renderedForUi->sampleRate : 44100.0;
    const int capture = captureState.load();

    o->setProperty ("playing", playing.load());
    o->setProperty ("source", isRaw ? "raw" : "edit");
    o->setProperty ("position", playing ? playPosition.load() / rate : 0.0);
    o->setProperty ("hostPlaying", hostPlaying.load());
    o->setProperty ("captureSeconds", capture == captureRecording ? std::round (captureSecondsForUi.load() * 10.0) / 10.0 : 0.0);
    o->setProperty ("recording", capture == captureRecording);
    o->setProperty ("level", capture == captureRecording ? std::round (captureLevel.exchange (0.0f) * 1000.0f) / 1000.0 : 0.0);
    return juce::var (o);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AtmosRiserProcessor();
}
