#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using Attributes = juce::AudioParameterFloatAttributes;
        const auto fmt = [] (int decimals, const char* unit) {
            return Attributes().withLabel (unit).withStringFromValueFunction ([decimals] (float v, int) { return juce::String (v, decimals); });
        };
        const auto percent = fmt (0, "%");
        const auto range = [] (float lo, float hi, float centre) {
            juce::NormalisableRange<float> r { lo, hi };
            r.setSkewForCentre (centre);
            return r;
        };

        // added in presetParamIds order; defaults come from the first preset
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        size_t index = 0;
        const auto add = [&] (const char* id, const char* name, juce::NormalisableRange<float> r, Attributes attributes) {
            jassert (std::string_view (id) == presetParamIds[index]);
            const auto def = factoryPresets[0].values[index++];
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def, attributes));
        };
        add ("size", "Size", { 0.0f, 100.0f }, percent);
        add ("decay", "Decay", range (0.1f, 30.0f, 2.0f), fmt (2, "s"));
        add ("predelay", "Pre-delay", range (0.0f, 250.0f, 40.0f), fmt (0, "ms"));
        add ("mix", "Mix", { 0.0f, 100.0f }, percent);
        add ("damping", "Damping", { 0.0f, 100.0f }, percent);
        add ("diffusion", "Diffusion", { 0.0f, 100.0f }, percent);
        add ("modrate", "Mod Rate", range (0.05f, 5.0f, 0.5f), fmt (2, "Hz"));
        add ("moddepth", "Mod Depth", { 0.0f, 100.0f }, percent);
        add ("width", "Width", { 0.0f, 100.0f }, percent);
        add ("shimmer", "Shimmer", { 0.0f, 100.0f }, percent);
        add ("spring", "Spring", { 0.0f, 100.0f }, percent);
        add ("lofi", "Lo-fi", { 0.0f, 100.0f }, percent);
        add ("output", "Output", { -24.0f, 12.0f, 0.1f }, fmt (1, "dB")); // step makes 0 dB exact
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "freeze", 1 }, "Freeze", false));
        return layout;
    }
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
        params[i] = apvts.getRawParameterValue (presetParamIds[i]);
    freeze = apvts.getRawParameterValue ("freeze");
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
    if (freeze->load() > 0.5f)
        return std::numeric_limits<double>::infinity();
    return params[Param::decay]->load() + params[Param::predelay]->load() * 0.001;
}

int PluginProcessor::getNumPrograms()
{
    return (int) factoryPresets.size();
}

int PluginProcessor::getCurrentProgram()
{
    return currentProgram;
}

// Applies every preset value; the reverb tail is left ringing and glides to the new settings
void PluginProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;

    currentProgram = index;
    const auto& preset = factoryPresets[(size_t) index];
    for (size_t i = 0; i < presetParamIds.size(); ++i)
    {
        auto* p = apvts.getParameter (presetParamIds[i]);
        p->setValueNotifyingHost (p->convertTo0to1 (preset.values[i]));
    }
    apvts.getParameter ("freeze")->setValueNotifyingHost (0.0f);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? factoryPresets[(size_t) index].name : "";
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
ReverbParams PluginProcessor::readParams() const
{
    const auto pct = [this] (Param::Index i) { return params[i]->load() * 0.01f; };
    ReverbParams p;
    p.size = pct (Param::size);
    p.decaySeconds = params[Param::decay]->load();
    p.preDelayMs = params[Param::predelay]->load();
    p.damping = pct (Param::damping);
    p.diffusion = pct (Param::diffusion);
    p.modRateHz = params[Param::modrate]->load();
    p.modDepth = pct (Param::moddepth);
    p.width = pct (Param::width);
    p.shimmer = pct (Param::shimmer);
    p.spring = pct (Param::spring);
    p.lofi = pct (Param::lofi);
    p.freeze = freeze->load() > 0.5f;
    return p;
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    engine.setParameters (readParams(), true);
    wetBuffer.setSize (2, samplesPerBlock);

    mix.reset (sampleRate, 0.05);
    mix.setCurrentAndTargetValue (params[Param::mix]->load() * 0.01f);
    outputGain.reset (sampleRate, 0.05);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params[Param::output]->load()));
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
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

    engine.setParameters (readParams());
    mix.setTargetValue (params[Param::mix]->load() * 0.01f);
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (params[Param::output]->load()));

    auto* left = buffer.getWritePointer (0);
    auto* right = numIn > 1 ? buffer.getWritePointer (1) : nullptr;

    // hosts may send blocks larger than announced, so work in wetBuffer-sized pieces
    for (int start = 0; start < buffer.getNumSamples(); start += wetBuffer.getNumSamples())
    {
        const auto n = juce::jmin (wetBuffer.getNumSamples(), buffer.getNumSamples() - start);
        auto* wetL = wetBuffer.getWritePointer (0);
        auto* wetR = wetBuffer.getWritePointer (1);
        engine.process (left + start, right != nullptr ? right + start : left + start, wetL, wetR, n);

        for (int i = 0; i < n; ++i)
        {
            // equal-power dry/wet; mix == 0 leaves dry bit-exact
            const auto angle = mix.getNextValue() * juce::MathConstants<float>::halfPi;
            const auto dryGain = std::cos (angle), wetGain = std::sin (angle);
            const auto gain = outputGain.getNextValue();
            if (right != nullptr)
            {
                left[start + i] = (dryGain * left[start + i] + wetGain * wetL[i]) * gain;
                right[start + i] = (dryGain * right[start + i] + wetGain * wetR[i]) * gain;
            }
            else
            {
                left[start + i] = (dryGain * left[start + i] + wetGain * 0.5f * (wetL[i] + wetR[i])) * gain;
            }
        }
    }
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) state.getProperty ("program", 0));
    apvts.replaceState (state);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
