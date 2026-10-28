#include "ReverbEngine.h"

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    const float invSqrt8 = 1.0f / std::sqrt (8.0f);

    // mutually prime-ish line lengths at Size = 100%
    constexpr float lineMs[] = { 31.3f, 37.9f, 43.7f, 51.1f, 59.3f, 68.9f, 81.7f, 101.3f };
    constexpr float diffuserMs[2][4] = { { 4.771f, 3.595f, 12.73f, 9.307f }, { 4.93f, 3.71f, 12.11f, 9.79f } };
    // orthogonal sign patterns -> decorrelated left/right taps
    constexpr float tapL[] = { 1, -1, 1, -1, 1, -1, 1, -1 };
    constexpr float tapR[] = { 1, 1, -1, -1, 1, 1, -1, -1 };
    constexpr float inputSign[] = { 1, 1, -1, 1, 1, -1, -1, -1 };
    constexpr float shimmerSign[] = { 1, -1, -1, 1, 1, -1, -1, 1 };

    constexpr float minSize = 0.15f;
    constexpr float maxModMs = 1.0f;
    constexpr float springMs = 37.0f;
    constexpr float springAllpassCoef = 0.6f;
    constexpr float shimmerWindowMs = 50.0f;
    constexpr float maxWowMs = 3.0f;

    void hadamard8 (float* x)
    {
        for (int step = 1; step < 8; step *= 2)
            for (int i = 0; i < 8; i += 2 * step)
                for (int j = i; j < i + step; ++j)
                {
                    const auto a = x[j], b = x[j + step];
                    x[j] = a + b;
                    x[j + step] = a - b;
                }
    }

    // transparent below |1|, bounded at |2|; only a safety net for the shimmer loop
    float softClip (float x)
    {
        const auto a = std::abs (x);
        return a <= 1.0f ? x : std::copysign (1.0f + std::tanh (a - 1.0f), x);
    }
}

//==============================================================================
void ReverbEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    const auto ms = [this] (float milliseconds) { return (float) (milliseconds * 0.001 * sr); };

    for (auto& d : preDelay)
        d.prepare ((int) ms (250.0f) + 2);

    springLength = (int) ms (springMs);
    springDelay.prepare (springLength);

    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < numDiffusers; ++k)
        {
            diffuserLength[c][k] = juce::jmax (1, (int) ms (diffuserMs[c][k]));
            diffusers[c][k].prepare (diffuserLength[c][k]);
        }

    for (int l = 0; l < numLines; ++l)
    {
        baseLength[l] = ms (lineMs[l]);
        lines[l].prepare ((int) (baseLength[l] + ms (maxModMs)) + 4);
        lfoPhase[l] = (float) l / numLines;
    }

    shimmerWindow = ms (shimmerWindowMs);
    shimmerDelay.prepare ((int) shimmerWindow + 4);

    for (auto& d : wowDelay)
        d.prepare ((int) ms (maxWowMs) + 4);

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlockSize, 2 };
    lowCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    lowCut.prepare (spec);
    lowCut.setCutoffFrequency (80.0f);
    highCut.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    highCut.prepare (spec);

    sizeScale.reset (sr, 0.2);
    preDelaySamples.reset (sr, 0.1);
    freezeAmount.reset (sr, 0.05);
    for (auto* s : { &decay, &damping, &diffusion, &modDepth, &width, &shimmer, &spring, &lofi })
        s->reset (sr, 0.05);

    reset();
}

void ReverbEngine::reset()
{
    for (auto& d : preDelay)
        d.clear();
    springDelay.clear();
    std::fill (std::begin (springX1), std::end (springX1), 0.0f);
    std::fill (std::begin (springY1), std::end (springY1), 0.0f);
    springLp = springFeedback = 0.0f;
    for (auto& channel : diffusers)
        for (auto& d : channel)
            d.clear();
    for (auto& d : lines)
        d.clear();
    std::fill (std::begin (dampState), std::end (dampState), 0.0f);
    shimmerDelay.clear();
    shimmerPhase = 0.0f;
    for (auto& d : wowDelay)
        d.clear();
    hold[0] = hold[1] = holdCounter = 0.0f;
    lowCut.reset();
    highCut.reset();
}

void ReverbEngine::setParameters (const ReverbParams& p, bool snap)
{
    const auto set = [snap] (juce::SmoothedValue<float>& s, float v) {
        if (snap)
            s.setCurrentAndTargetValue (v);
        else
            s.setTargetValue (v);
    };

    set (sizeScale, minSize + (1.0f - minSize) * p.size);
    set (preDelaySamples, (float) (p.preDelayMs * 0.001 * sr));
    set (freezeAmount, p.freeze ? 1.0f : 0.0f);
    set (decay, p.decaySeconds);
    set (damping, p.damping);
    set (diffusion, p.diffusion);
    set (modDepth, p.modDepth);
    set (width, p.width);
    set (shimmer, p.shimmer);
    set (spring, p.spring);
    set (lofi, p.lofi);
    modRateHz = p.modRateHz;
}

void ReverbEngine::updateCoefficients (int numSamples)
{
    const auto rt60 = decay.skip (numSamples);
    const auto scale = sizeScale.getCurrentValue();
    for (int l = 0; l < numLines; ++l)
        lineGain[l] = std::pow (10.0f, -3.0f * baseLength[l] * scale / (rt60 * (float) sr));

    // damping 0 = no HF loss, 1 = loop lowpass at 1.5 kHz
    const auto damp = damping.skip (numSamples);
    const auto cutoff = juce::jmin (20000.0f * std::pow (1500.0f / 20000.0f, damp), 0.45f * (float) sr);
    dampCoef = damp <= 0.0f ? 1.0f : 1.0f - std::exp (-twoPi * cutoff / (float) sr);

    diffusionGain = 0.75f * diffusion.skip (numSamples);
    modSamples = modDepth.skip (numSamples) * maxModMs * 0.001f * (float) sr;
    widthNow = width.skip (numSamples);
    shimmerNow = shimmer.skip (numSamples);
    springNow = spring.skip (numSamples);

    // lo-fi: narrower band, lower sample rate, fewer bits, more wow
    lofiNow = lofi.skip (numSamples);
    highCut.setCutoffFrequency (juce::jmin (16000.0f * std::pow (3500.0f / 16000.0f, lofiNow), 0.45f * (float) sr));
    const auto targetRate = juce::jmap (lofiNow, (float) sr, juce::jmin (8000.0f, (float) sr));
    holdStep = (float) sr / targetRate;
    crushStep = lofiNow > 0.0f ? std::pow (2.0f, -(16.0f - 8.0f * lofiNow - 1.0f)) : 0.0f;
    wowDepth = lofiNow * maxWowMs * 0.001f * (float) sr;
}

void ReverbEngine::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples)
{
    const auto lfoInc = modRateHz / (float) sr;
    const auto shimmerInc = 1.0f / shimmerWindow;
    const auto wowInc = 0.8f / (float) sr;

    for (int start = 0; start < numSamples; start += chunkSize)
    {
        const auto end = juce::jmin (numSamples, start + chunkSize);
        updateCoefficients (end - start);

        for (int i = start; i < end; ++i)
        {
            // pre-delay
            float x[2] = { inL[i], inR[i] };
            const auto pd = 1.0f + preDelaySamples.getNextValue();
            for (int c = 0; c < 2; ++c)
            {
                preDelay[c].push (x[c]);
                x[c] = preDelay[c].readLinear (pd);
            }

            // spring: dispersive allpass chain with a short feedback path -> "boing" chirps
            auto s = 0.5f * (x[0] + x[1]) + springFeedback;
            for (int k = 0; k < numSpringAllpasses; ++k)
            {
                const auto y = springAllpassCoef * (s - springY1[k]) + springX1[k];
                springX1[k] = s;
                springY1[k] = y;
                s = y;
            }
            springDelay.push (s);
            springLp += 0.35f * (springDelay.read (springLength) - springLp);
            springFeedback = 0.55f * springLp;
            for (auto& v : x)
                v += springNow * (s - v);

            // input diffusion (Schroeder allpasses)
            for (int c = 0; c < 2; ++c)
                for (int k = 0; k < numDiffusers; ++k)
                {
                    const auto delayed = diffusers[c][k].read (diffuserLength[c][k]);
                    const auto v = x[c] + diffusionGain * delayed;
                    diffusers[c][k].push (v);
                    x[c] = delayed - diffusionGain * v;
                }

            // FDN
            const auto scale = sizeScale.getNextValue();
            const auto fz = freezeAmount.getNextValue();
            float out[numLines], fb[numLines];
            for (int l = 0; l < numLines; ++l)
            {
                lfoPhase[l] += lfoInc;
                if (lfoPhase[l] >= 1.0f)
                    lfoPhase[l] -= 1.0f;
                const auto mod = modSamples * (0.5f + 0.5f * std::sin (twoPi * lfoPhase[l]));
                out[l] = lines[l].readHermite (baseLength[l] * scale + mod);

                dampState[l] += dampCoef * (out[l] - dampState[l]);
                const auto damped = dampState[l] + fz * (out[l] - dampState[l]);
                fb[l] = damped * (lineGain[l] + fz * (1.0f - lineGain[l])) * invSqrt8;
            }
            hadamard8 (fb);

            // shimmer: octave-up grains of the loop signal, crossfaded back in (rank-1, so loop gain stays <= 1)
            float shimmerIn = 0.0f;
            for (int l = 0; l < numLines; ++l)
                shimmerIn += fb[l] * shimmerSign[l];
            shimmerDelay.push (shimmerIn * invSqrt8);
            shimmerPhase += shimmerInc;
            if (shimmerPhase >= 1.0f)
                shimmerPhase -= 1.0f;
            const auto phase2 = shimmerPhase < 0.5f ? shimmerPhase + 0.5f : shimmerPhase - 0.5f;
            const auto w1 = std::sin (juce::MathConstants<float>::pi * shimmerPhase);
            const auto w2 = std::sin (juce::MathConstants<float>::pi * phase2);
            const auto pitched = w1 * w1 * shimmerDelay.readHermite (2.0f + (1.0f - shimmerPhase) * shimmerWindow)
                                 + w2 * w2 * shimmerDelay.readHermite (2.0f + (1.0f - phase2) * shimmerWindow);
            const auto shimmerMix = 0.5f * shimmerNow * (1.0f - fz);

            const auto inputGain = 0.5f * (1.0f - fz);
            for (int l = 0; l < numLines; ++l)
            {
                const auto loop = fb[l] + shimmerMix * (pitched * shimmerSign[l] * invSqrt8 - fb[l]);
                lines[l].push (softClip (loop + inputGain * inputSign[l] * x[l & 1]));
            }

            // output taps + width
            float wetL = 0.0f, wetR = 0.0f;
            for (int l = 0; l < numLines; ++l)
            {
                wetL += tapL[l] * out[l];
                wetR += tapR[l] * out[l];
            }
            const auto mid = 0.5f * (wetL + wetR) * invSqrt8;
            const auto side = 0.5f * (wetL - wetR) * invSqrt8 * widthNow;
            float y[2] = { mid + side, mid - side };

            // fixed wet EQ + lo-fi (wow -> band-limit -> sample-and-hold -> bit-crush)
            wowPhase += wowInc;
            if (wowPhase >= 1.0f)
                wowPhase -= 1.0f;
            const auto wow = 2.0f + wowDepth * (0.5f + 0.5f * std::sin (twoPi * wowPhase));
            holdCounter += 1.0f;
            const auto sampleNow = holdCounter >= holdStep;
            if (sampleNow)
                holdCounter -= holdStep;

            for (int c = 0; c < 2; ++c)
            {
                wowDelay[c].push (y[c]);
                auto v = wowDelay[c].readHermite (wow);
                v = highCut.processSample (c, lowCut.processSample (c, v));
                if (sampleNow)
                    hold[c] = v;
                v = hold[c];
                if (crushStep > 0.0f)
                    v = crushStep * std::round (v / crushStep);
                y[c] = v;
            }

            outL[i] = y[0];
            outR[i] = y[1];
        }
    }
}
