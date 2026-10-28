#include "VocalChain.h"

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    // de-esser: high band above 5 kHz turned down by up to 8 dB (3:1 above -30 dBFS)
    constexpr float deEssThresholdDb = -30.0f, deEssRatio = 3.0f, deEssMaxCutDb = 8.0f;
    constexpr float makeupGain = 1.585f; // +4 dB after the levelling compressor

    // doubler: two short, slowly modulated copies, one per side
    constexpr float doublerMs[] = { 11.0f, 17.0f }, doublerRateHz[] = { 0.9f, 0.7f };
    constexpr float doublerDepthMs = 1.5f, doublerLevel = 0.3f;

    // quarter-note ping-pong, repeats band-limited to 400 Hz..3.5 kHz
    constexpr float echoFeedback = 0.45f, echoLevel = 0.5f, maxEchoSeconds = 2.0f;
    constexpr float wobbleMs = 0.4f, wobbleRateHz = 0.5f;
    constexpr float duckMaxCut = 0.75f, duckFullAt = 0.1f; // -12 dB once the vocal is above -20 dBFS

    // one-pole envelope coefficient for a time constant in ms
    float followerCoef (float sampleRate, float ms) { return 1.0f - std::exp (-1.0f / (ms * 0.001f * sampleRate)); }
}

void VocalChain::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    const auto fs = (float) sr;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;
    lowCut.coefficients = Coefficients::makeHighPass (sr, 100.0f);
    mudCut.coefficients = Coefficients::makePeakFilter (sr, 300.0f, 1.0f, juce::Decibels::decibelsToGain (-3.0f));
    sibilanceBand.coefficients = Coefficients::makeHighPass (sr, 5000.0f);
    presence.coefficients = Coefficients::makePeakFilter (sr, 3000.0f, 0.8f, juce::Decibels::decibelsToGain (3.0f));
    air.coefficients = Coefficients::makeHighShelf (sr, juce::jmin (10000.0f, 0.4f * fs), 0.707f, juce::Decibels::decibelsToGain (4.0f));

    const juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlockSize, 1 };
    for (auto* f : { &lowCut, &mudCut, &sibilanceBand, &presence, &air })
        f->prepare (spec);

    peakComp.prepare (spec);
    peakComp.setThreshold (-20.0f);
    peakComp.setRatio (4.0f);
    peakComp.setAttack (3.0f);
    peakComp.setRelease (60.0f);

    levelComp.prepare (spec);
    levelComp.setThreshold (-14.0f);
    levelComp.setRatio (2.0f);
    levelComp.setAttack (20.0f);
    levelComp.setRelease (150.0f);

    deEssAttack = followerCoef (fs, 1.0f);
    deEssRelease = followerCoef (fs, 60.0f);
    duckAttack = followerCoef (fs, 5.0f);
    duckRelease = followerCoef (fs, 250.0f);

    for (auto& d : doubler)
        d.prepare ((int) ((doublerMs[1] + doublerDepthMs) * 0.001f * fs) + 4);
    for (auto& d : echo)
        d.prepare ((int) ((maxEchoSeconds + wobbleMs * 0.001f) * fs) + 4);

    echoLpCoef = 1.0f - std::exp (-twoPi * juce::jmin (3500.0f, 0.45f * fs) / fs);
    echoHpCoef = 1.0f - std::exp (-twoPi * 400.0f / fs);

    echoSamples.reset (sr, 0.3);
    setTempo (defaultBpm);
    reset();
}

void VocalChain::reset()
{
    for (auto* f : { &lowCut, &mudCut, &sibilanceBand, &presence, &air })
        f->reset();
    peakComp.reset();
    levelComp.reset();
    deEssEnv = duckEnv = wobblePhase = 0.0f;
    for (int c = 0; c < 2; ++c)
    {
        doubler[c].clear();
        echo[c].clear();
        doublerPhase[c] = 0.5f * (float) c;
        echoLp[c] = echoHp[c] = 0.0f;
    }
    echoSamples.setCurrentAndTargetValue (echoSamples.getTargetValue());
}

void VocalChain::setTempo (double bpm)
{
    const auto quarterNote = 60.0 / juce::jlimit (40.0, 300.0, bpm);
    echoSamples.setTargetValue ((float) (juce::jmin (quarterNote, (double) maxEchoSeconds) * sr));
}

float VocalChain::bandLimitEcho (int channel, float x)
{
    echoLp[channel] += echoLpCoef * (x - echoLp[channel]);
    echoHp[channel] += echoHpCoef * (echoLp[channel] - echoHp[channel]);
    return echoLp[channel] - echoHp[channel];
}

void VocalChain::process (float* left, float* right, int numSamples)
{
    const auto fs = (float) sr;
    const auto thresholdGain = juce::Decibels::decibelsToGain (deEssThresholdDb);

    for (int i = 0; i < numSamples; ++i)
    {
        // EQ + dynamics on the mono vocal
        auto x = 0.5f * (left[i] + right[i]);
        x = mudCut.processSample (lowCut.processSample (x));
        x = peakComp.processSample (0, x);

        const auto high = sibilanceBand.processSample (x);
        const auto level = std::abs (high);
        deEssEnv += (level > deEssEnv ? deEssAttack : deEssRelease) * (level - deEssEnv);
        if (deEssEnv > thresholdGain)
        {
            const auto overDb = juce::Decibels::gainToDecibels (deEssEnv) - deEssThresholdDb;
            const auto cutDb = juce::jmin (deEssMaxCutDb, overDb * (1.0f - 1.0f / deEssRatio));
            x -= (1.0f - juce::Decibels::decibelsToGain (-cutDb)) * high;
        }

        x = air.processSample (presence.processSample (x));
        x = levelComp.processSample (0, x) * makeupGain;

        // doubler
        float doubled[2];
        for (int c = 0; c < 2; ++c)
        {
            doublerPhase[c] += doublerRateHz[c] / fs;
            if (doublerPhase[c] >= 1.0f)
                doublerPhase[c] -= 1.0f;
            doubler[c].push (x);
            const auto ms = doublerMs[c] + doublerDepthMs * std::sin (twoPi * doublerPhase[c]);
            doubled[c] = doubler[c].readHermite (ms * 0.001f * fs);
        }

        // ping-pong: left line takes the vocal + right feedback, right line takes the left echo
        wobblePhase += wobbleRateHz / fs;
        if (wobblePhase >= 1.0f)
            wobblePhase -= 1.0f;
        const auto time = echoSamples.getNextValue() + wobbleMs * 0.001f * fs * std::sin (twoPi * wobblePhase);
        const auto echoL = bandLimitEcho (0, echo[0].readHermite (time));
        const auto echoR = bandLimitEcho (1, echo[1].readHermite (time));
        echo[0].push (std::tanh (x + echoFeedback * echoR));
        echo[1].push (std::tanh (echoFeedback * echoL));

        // echoes duck under the vocal and bloom in the gaps
        const auto vocalLevel = std::abs (x);
        duckEnv += (vocalLevel > duckEnv ? duckAttack : duckRelease) * (vocalLevel - duckEnv);
        const auto echoGain = echoLevel * (1.0f - duckMaxCut * juce::jmin (1.0f, duckEnv / duckFullAt));

        left[i] = x + doublerLevel * doubled[0] + echoGain * echoL;
        right[i] = x + doublerLevel * doubled[1] + echoGain * echoR;
    }
}
