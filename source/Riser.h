#pragma once

#include "ReverbEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>

// Offline reverse-reverb riser: capture -> first syllable -> de-ess -> reverse(reverb(reverse(syllable))) -> FX.
// Everything here allocates and runs on the message thread.
namespace Riser
{
    struct FxSettings
    {
        bool pitch = false;
        float pitchStart = -12.0f, pitchEnd = 0.0f, pitchShape = 0.0f; // semitones; shape -1 log .. 0 linear .. +1 exp
        bool chorus = false;
        float chorusMix = 1.0f, chorusRateHz = 2.0f;
        bool tremolo = false;
        float tremoloMix = 0.5f, tremoloBeats = 0.5f;
    };

    // whole samples in `bars` 4/4 bars at `bpm`
    int lengthInSamples (float bars, double bpm, double sampleRate);

    // the first syllable: onset above -40 dBFS until the level drops 12 dB below its peak (at most 400 ms); empty if silent
    juce::Range<int> detectSyllable (const juce::AudioBuffer<float>& capture, double sampleRate);

    // turns sharp "S" sounds (above 5 kHz) down by up to 10 dB, so they don't hiss backwards
    void deEss (juce::AudioBuffer<float>& buffer, double sampleRate);

    // stereo riser of exactly lengthSamples that swells into the syllable's start; peak at -1 dBFS
    juce::AudioBuffer<float> renderRaw (const juce::AudioBuffer<float>& syllable, const ReverbParams& reverb, int lengthSamples, double sampleRate);

    // pitch curve -> chorus -> tremolo; same length as raw
    juce::AudioBuffer<float> renderFx (const juce::AudioBuffer<float>& raw, const FxSettings& fx, double bpm, double sampleRate);

    // pitch offset in semitones at t (0 = riser start, 1 = the vocal)
    float pitchAt (const FxSettings& fx, float t);

    bool writeWav (const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::File& file);
}
