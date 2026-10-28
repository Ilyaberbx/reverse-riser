#pragma once

#include "DelayLine.h"
#include <juce_dsp/juce_dsp.h>

// Normalised (0..1) controls unless a unit is given
struct ReverbParams
{
    float size = 0.5f;
    float decaySeconds = 2.0f;
    float preDelayMs = 0.0f;
    float damping = 0.3f;
    float diffusion = 0.7f;
    float modRateHz = 0.5f;
    float modDepth = 0.2f;
    float width = 1.0f;
    float shimmer = 0.0f;
    float spring = 0.0f;
    float lofi = 0.0f;
    bool freeze = false;
};

// Stereo-in, 100% wet stereo-out reverb:
// pre-delay -> spring dispersion -> input diffusers -> 8-line FDN (modulated, damped, shimmer in the loop)
// -> width -> fixed wet EQ -> lo-fi
class ReverbEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    // snap = jump straight to the new values instead of smoothing (use after prepare)
    void setParameters (const ReverbParams& p, bool snap = false);
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples);

private:
    static constexpr int numLines = 8;
    static constexpr int numDiffusers = 4;
    static constexpr int numSpringAllpasses = 16;
    static constexpr int chunkSize = 32; // coefficient update rate

    void updateCoefficients (int numSamples);

    double sr = 44100.0;

    DelayLine preDelay[2];

    DelayLine springDelay;
    int springLength = 0;
    float springX1[numSpringAllpasses] {}, springY1[numSpringAllpasses] {};
    float springLp = 0.0f, springFeedback = 0.0f;

    DelayLine diffusers[2][numDiffusers];
    int diffuserLength[2][numDiffusers] {};

    DelayLine lines[numLines];
    float baseLength[numLines] {};
    float dampState[numLines] {};
    float lfoPhase[numLines] {};

    DelayLine shimmerDelay;
    float shimmerWindow = 0.0f, shimmerPhase = 0.0f;

    juce::dsp::StateVariableTPTFilter<float> highCut, lowCut;

    DelayLine wowDelay[2];
    float wowPhase = 0.0f, hold[2] {}, holdCounter = 0.0f;

    juce::SmoothedValue<float> sizeScale, preDelaySamples, freezeAmount;
    juce::SmoothedValue<float> decay, damping, diffusion, modDepth, width, shimmer, spring, lofi;
    float modRateHz = 0.5f;

    // per-chunk coefficients
    float lineGain[numLines] {};
    float dampCoef = 1.0f, diffusionGain = 0.0f, modSamples = 0.0f, widthNow = 1.0f;
    float shimmerNow = 0.0f, springNow = 0.0f, lofiNow = 0.0f, holdStep = 1.0f, crushStep = 0.0f, wowDepth = 0.0f;
};
