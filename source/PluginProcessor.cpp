#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float onsetThreshold = 0.01f; // -40 dBFS, same gate as Riser::detectSyllable

    juce::StringArray names (auto const& list)
    {
        juce::StringArray result;
        for (const auto& item : list)
            result.add (item.name);
        return result;
    }

    juce::StringArray names (const char* const* begin, size_t count)
    {
        return juce::StringArray (begin, (int) count);
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using Attributes = juce::AudioParameterFloatAttributes;
        const auto fmt = [] (int decimals, const char* unit) {
            return Attributes().withLabel (unit).withStringFromValueFunction ([decimals] (float v, int) { return juce::String (v, decimals); });
        };

        // added in riserParamIds order; defaults come from the first preset
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        size_t index = 0;
        const auto next = [&] (const char* id) {
            jassert (std::string_view (id) == riserParamIds[index]);
            return std::pair { juce::ParameterID { id, 1 }, riserPresets[0].values[index++] };
        };
        const auto addChoice = [&] (const char* id, const char* name, juce::StringArray choices) {
            const auto [pid, def] = next (id);
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, name, choices, (int) def));
        };
        const auto addBool = [&] (const char* id, const char* name) {
            const auto [pid, def] = next (id);
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, name, def > 0.5f));
        };
        const auto addFloat = [&] (const char* id, const char* name, juce::NormalisableRange<float> r, Attributes attributes) {
            const auto [pid, def] = next (id);
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, name, r, def, attributes));
        };

        addChoice ("length", "Reverse", names (lengthNames.data(), lengthNames.size()));
        addChoice ("fx", "Render", { "RAW", "FX" });
        addBool ("reverbOn", "Reverb");
        addChoice ("reverbType", "Reverb Type", names (reverbTypes));
        addBool ("chorusOn", "Chorus");
        addFloat ("chorusMix", "Chorus Mix", { 0.0f, 100.0f }, fmt (0, "%"));
        addFloat ("chorusRate", "Chorus Rate", { 0.1f, 10.0f, 0.1f }, fmt (1, "Hz"));
        addBool ("tremoloOn", "Tremolo");
        addFloat ("tremoloMix", "Tremolo Mix", { 0.0f, 100.0f }, fmt (0, "%"));
        addChoice ("tremoloTiming", "Tremolo Timing", names (tremoloTimingNames.data(), tremoloTimingNames.size()));
        addBool ("pitchOn", "Pitch");
        addFloat ("pitchStart", "Pitch Start", { -24.0f, 24.0f, 1.0f }, fmt (0, "st"));
        addFloat ("pitchEnd", "Pitch End", { -24.0f, 24.0f, 1.0f }, fmt (0, "st"));
        addFloat ("pitchShape", "Pitch Shape", { -1.0f, 1.0f }, fmt (2, ""));
        return layout;
    }

    // one-pole release coefficient for a time constant in ms
    float releaseCoef (double sampleRate, double ms) { return 1.0f - (float) std::exp (-1.0 / (ms * 0.001 * sampleRate)); }
}

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "Parameters", createLayout())
{
    for (size_t i = 0; i < params.size(); ++i)
        params[i] = apvts.getRawParameterValue (riserParamIds[i]);
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return (int) riserPresets.size();
}

int PluginProcessor::getCurrentProgram()
{
    return currentProgram.load();
}

void PluginProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;

    currentProgram = index;
    const auto& preset = riserPresets[(size_t) index];
    for (size_t i = 0; i < riserParamIds.size(); ++i)
        setValue (riserParamIds[i], preset.values[i]);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? riserPresets[(size_t) index].name : "";
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void PluginProcessor::setValue (const char* id, float v)
{
    auto* p = apvts.getParameter (id);
    p->setValueNotifyingHost (p->convertTo0to1 (v));
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    if (! juce::exactlyEqual (sampleRate, sr))
        needsRender = true;
    sr = sampleRate;

    captureBuffer.setSize (2, (int) (captureSeconds * sampleRate));
    if (captureState.load() == recording)
        captureState = waiting;
    captureLength = 0;
    quietNeeded = (int) (silenceBeforeOnsetMs * 0.001 * sampleRate);
    quietSamples = quietNeeded; // the very first sound counts as an onset
    gateEnv = 0.0f;
    gateRelease = releaseCoef (sampleRate, gateReleaseMs);
    previewPos = 0;
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    const auto numIn = getTotalNumInputChannels();
    for (auto i = numIn; i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    if (numIn == 0)
        return;

    if (auto* head = getPlayHead())
        if (const auto position = head->getPosition())
            if (const auto tempo = position->getBpm(); tempo && *tempo > 0.0)
                hostBpm = *tempo;

    auto* left = buffer.getWritePointer (0);
    auto* right = numIn > 1 ? buffer.getWritePointer (1) : nullptr;
    const auto n = buffer.getNumSamples();

    // capture: wait for silence, then record from the first onset until the buffer is full
    if (auto state = captureState.load(); state != done)
    {
        for (int i = 0; i < n && state != done; ++i)
        {
            const auto l = left[i], r = right != nullptr ? right[i] : l;
            const auto peak = juce::jmax (std::abs (l), std::abs (r));
            gateEnv = peak > gateEnv ? peak : gateEnv - gateRelease * gateEnv;

            if (state == waiting)
            {
                if (gateEnv < onsetThreshold)
                    quietSamples = juce::jmin (quietSamples + 1, quietNeeded);
                else if (quietSamples >= quietNeeded)
                {
                    state = recording;
                    captureLength = 0;
                }
                else
                    quietSamples = 0;
            }

            if (state == recording)
            {
                captureBuffer.setSample (0, captureLength, l);
                captureBuffer.setSample (1, captureLength, r);
                if (++captureLength == captureBuffer.getNumSamples())
                {
                    state = done;
                    captureReady = true;
                }
            }
        }
        captureState = state;
    }

    // preview: the riser on top of the untouched input
    if (listening.load() && ! previewBusy.exchange (true))
    {
        if (restartPreview.exchange (false))
            previewPos = 0;
        const auto length = previewBuffer.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            if (previewPos >= length)
            {
                if (! looping.load() || length == 0)
                {
                    listening = false;
                    break;
                }
                previewPos = 0;
            }
            const auto l = previewBuffer.getSample (0, previewPos), r = previewBuffer.getSample (1, previewPos);
            ++previewPos;
            if (right != nullptr)
            {
                left[i] += l;
                right[i] += r;
            }
            else
                left[i] += 0.5f * (l + r);
        }
        previewBusy = false;
    }
}

//==============================================================================
std::array<float, riserParamIds.size()> PluginProcessor::settingsSnapshot() const
{
    std::array<float, riserParamIds.size()> values {};
    for (size_t i = 0; i < values.size(); ++i)
        values[i] = params[i]->load();
    return values;
}

Riser::FxSettings PluginProcessor::fxSettings() const
{
    using namespace Param;
    Riser::FxSettings s;
    s.pitch = value (pitchOn) > 0.5f;
    s.pitchStart = value (pitchStart);
    s.pitchEnd = value (pitchEnd);
    s.pitchShape = value (pitchShape);
    s.chorus = value (chorusOn) > 0.5f;
    s.chorusMix = value (chorusMix) * 0.01f;
    s.chorusRateHz = value (chorusRate);
    s.tremolo = value (tremoloOn) > 0.5f;
    s.tremoloMix = value (tremoloMix) * 0.01f;
    s.tremoloBeats = tremoloTimingBeats[(size_t) juce::jlimit (0, (int) tremoloTimingBeats.size() - 1, (int) value (tremoloTiming))];
    return s;
}

namespace
{
    int reverbIndex (float on, float type)
    {
        return on > 0.5f ? juce::jlimit (0, (int) reverbTypes.size() - 1, (int) type) : defaultReverbType;
    }
}

// ponytail: renders on the message thread; move to a background job if the 4-bar/slow-BPM render ever measures above ~50 ms
bool PluginProcessor::updateRiser()
{
    if (captureState.load() == done && captureReady.exchange (false))
    {
        captured.makeCopyOf (captureBuffer);
        syllable = Riser::detectSyllable (captured, sr);
        syllableAudio.setSize (2, syllable.getLength());
        for (int c = 0; c < 2; ++c)
            syllableAudio.copyFrom (c, 0, captured, c, syllable.getStart(), syllable.getLength());
        Riser::deEss (syllableAudio, sr);
        needsRender = true;
    }

    bool changed = false;
    const auto settings = settingsSnapshot();
    const auto tempo = bpm();
    if (syllableAudio.getNumSamples() > 0 && (needsRender || settings != renderedSettings || std::abs (tempo - renderedBpm) > 0.01))
    {
        using namespace Param;
        const auto bars = lengthBars[(size_t) juce::jlimit (0, (int) lengthBars.size() - 1, (int) settings[length])];
        const auto& reverb = reverbTypes[(size_t) reverbIndex (settings[reverbOn], settings[reverbType])].params;
        raw = Riser::renderRaw (syllableAudio, reverb, Riser::lengthInSamples (bars, tempo, sr), sr);
        fx = Riser::renderFx (raw, fxSettings(), tempo, sr);
        renderedSettings = settings;
        renderedBpm = tempo;
        needsRender = false;
        changed = true;

        pendingPreview.makeCopyOf (riser());
        previewPending = true;
    }

    if (previewPending && ! previewBusy.exchange (true))
    {
        std::swap (previewBuffer, pendingPreview); // moves pointers only; the old buffer is freed here, not on the audio thread
        previewBusy = false;
        previewPending = false;
    }
    return changed;
}

void PluginProcessor::resetCapture()
{
    listening = false;
    captured.setSize (0, 0);
    syllableAudio.setSize (0, 0);
    raw.setSize (0, 0);
    fx.setSize (0, 0);
    syllable = {};
    pendingPreview.setSize (0, 0);
    previewPending = true;
    captureReady = false;
    captureState = waiting;
}

const juce::AudioBuffer<float>& PluginProcessor::riser() const
{
    return value (Param::fx) > 0.5f ? fx : raw;
}

juce::File PluginProcessor::exportRiser() const
{
    using namespace Param;
    const auto bars = lengthBars[(size_t) juce::jlimit (0, (int) lengthBars.size() - 1, (int) value (length))];
    const auto reverbName = juce::String (reverbTypes[(size_t) reverbIndex (value (reverbOn), value (reverbType))].name).removeCharacters (" -");
    auto barsText = juce::String (bars);
    if (barsText.endsWith (".0"))
        barsText = barsText.dropLastCharacters (2);
    const auto name = juce::String ("Riser_") + (value (Param::fx) > 0.5f ? "FX_" : "RAW_") + reverbName + "_"
                      + barsText + "bars_" + juce::String (juce::roundToInt (renderedBpm)) + "bpm.wav";
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RiserExports");
    folder.createDirectory();
    const auto file = folder.getChildFile (name);
    return Riser::writeWav (riser(), sr, file) ? file : juce::File();
}

void PluginProcessor::setListening (bool shouldListen)
{
    if (shouldListen)
        restartPreview = true;
    listening = shouldListen;
}

//==============================================================================
juce::File PluginProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile (JucePlugin_Manufacturer)
        .getChildFile (JucePlugin_Name)
        .getChildFile ("Presets");
}

void PluginProcessor::saveUserPreset (const juce::File& file) const
{
    juce::XmlElement xml ("RiserPreset");
    for (size_t i = 0; i < riserParamIds.size(); ++i)
        xml.setAttribute (riserParamIds[i], (double) params[i]->load());
    file.getParentDirectory().createDirectory();
    xml.writeTo (file);
}

void PluginProcessor::loadUserPreset (const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("RiserPreset"))
        return;
    for (const auto* id : riserParamIds)
        if (xml->hasAttribute (id))
            setValue (id, (float) xml->getDoubleAttribute (id));
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
// the latched syllable is stored with the settings, so the riser survives a project reload
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (syllable.getLength() > 0)
    {
        juce::MemoryBlock audio;
        for (int c = 0; c < 2; ++c)
            audio.append (captured.getReadPointer (c, syllable.getStart()), (size_t) syllable.getLength() * sizeof (float));
        juce::ValueTree capture ("Capture");
        capture.setProperty ("samples", syllable.getLength(), nullptr);
        capture.setProperty ("audio", audio.toBase64Encoding(), nullptr);
        state.appendChild (capture, nullptr);
    }
    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    const auto capture = state.getChildWithName ("Capture");
    state.removeChild (capture, nullptr);
    apvts.replaceState (state);

    juce::MemoryBlock audio;
    const auto samples = (int) capture.getProperty ("samples", 0);
    if (! capture.isValid() || samples <= 0 || ! audio.fromBase64Encoding (capture.getProperty ("audio").toString())
        || audio.getSize() != (size_t) samples * 2 * sizeof (float))
        return;

    // ponytail: the syllable is replayed at the current rate; resample it if sessions move between sample rates
    resetCapture();
    captured.setSize (2, samples);
    for (int c = 0; c < 2; ++c)
        audio.copyTo (captured.getWritePointer (c), (int) ((size_t) c * (size_t) samples * sizeof (float)), (size_t) samples * sizeof (float));
    syllable = { 0, samples };
    syllableAudio.makeCopyOf (captured);
    Riser::deEss (syllableAudio, sr);
    captureState = done;
    needsRender = true;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
