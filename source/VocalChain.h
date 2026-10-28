#pragma once

#include "DelayLine.h"
#include <juce_dsp/juce_dsp.h>

// Fixed vocal chain behind the "Lil Uzi Vert" preset, modelled on the Luv Is Rage 2 / XO Tour Llif3 vocal:
// clean-up EQ -> peak compressor -> de-esser -> tone EQ -> levelling compressor -> doubler -> ducked ping-pong delay
class VocalChain
{
public:
    static constexpr double defaultBpm = 155.0; // XO Tour Llif3

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setTempo (double bpm);
    // stereo in place; the vocal is treated as mono
    void process (float* left, float* right, int numSamples);

private:
    float bandLimitEcho (int channel, float x);

    using Filter = juce::dsp::IIR::Filter<float>;

    double sr = 44100.0;
    Filter lowCut, mudCut, sibilanceBand, presence, air;
    juce::dsp::Compressor<float> peakComp, levelComp;
    float deEssEnv = 0.0f, deEssAttack = 0.0f, deEssRelease = 0.0f;

    DelayLine doubler[2];
    float doublerPhase[2] {};

    DelayLine echo[2];
    juce::SmoothedValue<float> echoSamples;
    float echoLp[2] {}, echoHp[2] {}, echoLpCoef = 0.0f, echoHpCoef = 0.0f, wobblePhase = 0.0f;
    float duckEnv = 0.0f, duckAttack = 0.0f, duckRelease = 0.0f;
};
