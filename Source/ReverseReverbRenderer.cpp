#include "ReverseReverbRenderer.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    int msToSamples (double ms, double sampleRate)
    {
        return juce::jmax (0, (int) std::lround (ms * 0.001 * sampleRate));
    }

    juce::AudioBuffer<float> extractStereoSegment (const juce::AudioBuffer<float>& source, int start, int num)
    {
        juce::AudioBuffer<float> out (2, num);

        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, 0, source, juce::jmin (ch, source.getNumChannels() - 1), start, num);

        return out;
    }

    juce::AudioBuffer<float> resampleBuffer (const juce::AudioBuffer<float>& in, double inRate, double outRate)
    {
        const int numIn = in.getNumSamples();

        if (numIn == 0 || std::abs (inRate - outRate) < 1.0e-6)
            return in;

        const double ratio = inRate / outRate;
        const int numOut = juce::jmax (1, (int) std::floor (numIn / ratio));

        juce::AudioBuffer<float> out (in.getNumChannels(), numOut);
        juce::AudioBuffer<float> padded (1, numIn + 32);

        for (int ch = 0; ch < in.getNumChannels(); ++ch)
        {
            padded.clear();
            padded.copyFrom (0, 0, in, ch, 0, numIn);

            juce::LagrangeInterpolator interpolator;
            interpolator.reset();
            interpolator.process (ratio, padded.getReadPointer (0), out.getWritePointer (ch), numOut);
        }

        return out;
    }

    void reverseBuffer (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            std::reverse (data, data + n);
        }
    }

    void applyEdgeFades (juce::AudioBuffer<float>& buffer, int fadeSamples)
    {
        const int n = buffer.getNumSamples();
        fadeSamples = juce::jmin (fadeSamples, n / 2);

        if (fadeSamples <= 0)
            return;

        buffer.applyGainRamp (0, fadeSamples, 0.0f, 1.0f);
        buffer.applyGainRamp (n - fadeSamples, fadeSamples, 1.0f, 0.0f);
    }

    void matchPeak (juce::AudioBuffer<float>& buffer, float targetPeak)
    {
        const float peak = buffer.getMagnitude (0, buffer.getNumSamples());

        if (peak > 1.0e-6f && targetPeak > 0.0f)
            buffer.applyGain (targetPeak / peak);
    }

    //==============================================================================
    /** Schroeder allpass: flat magnitude, smears transients in time. */
    struct Allpass
    {
        std::vector<float> line;
        size_t index = 0;
        float gain = 0.5f;

        void prepare (int lengthSamples, float g)
        {
            line.assign ((size_t) juce::jmax (1, lengthSamples), 0.0f);
            index = 0;
            gain = g;
        }

        float process (float x) noexcept
        {
            const float delayed = line[index];
            const float v = x + gain * delayed;
            line[index] = v;
            index = (index + 1) % line.size();
            return delayed - gain * v;
        }
    };

    /** Diffusion = a chain of short allpasses ahead of the reverb. Higher = smoother, denser onset. */
    void applyDiffusion (juce::AudioBuffer<float>& buffer, float diffusion, double sampleRate)
    {
        if (diffusion <= 0.001f)
            return;

        static constexpr double delaysMs[2][4] = { { 4.77, 3.59, 12.73, 9.30 },
                                                   { 5.11, 3.83, 13.37, 8.89 } };
        const float g = 0.75f * juce::jlimit (0.0f, 1.0f, diffusion);

        for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        {
            Allpass stages[4];

            for (int i = 0; i < 4; ++i)
                stages[i].prepare (msToSamples (delaysMs[ch][i], sampleRate), g);

            auto* data = buffer.getWritePointer (ch);

            for (int s = 0; s < buffer.getNumSamples(); ++s)
            {
                float x = data[s];

                for (auto& stage : stages)
                    x = stage.process (x);

                data[s] = x;
            }
        }
    }

    //==============================================================================
    void highPass (float* data, int n, double sampleRate, double cutoff)
    {
        const auto coeffs = juce::IIRCoefficients::makeHighPass (sampleRate, juce::jmin (cutoff, sampleRate * 0.45));
        juce::IIRFilter a, b;
        a.setCoefficients (coeffs);
        b.setCoefficients (coeffs);
        a.processSamples (data, n);
        b.processSamples (data, n);
    }

    constexpr double sibilanceCutoffHz = 4500.0;

    /** Turns down the high band during sibilant passages (about -18 dB), leaving the rest intact. */
    void applySibilanceControl (juce::AudioBuffer<float>& buffer, double sampleRate,
                                const std::vector<std::pair<int, int>>& regions)
    {
        if (regions.empty())
            return;

        const int n = buffer.getNumSamples();
        std::vector<float> target ((size_t) n, 1.0f);

        for (const auto& [start, end] : regions)
            for (int i = juce::jmax (0, start); i < juce::jmin (n, end); ++i)
                target[(size_t) i] = 0.125f;

        // Fast attack, slower release, so the reduction never clicks.
        const float attack  = 1.0f - std::exp (-1.0f / (float) (0.002 * sampleRate));
        const float release = 1.0f - std::exp (-1.0f / (float) (0.030 * sampleRate));
        std::vector<float> gain ((size_t) n);
        float g = 1.0f;

        for (int i = 0; i < n; ++i)
        {
            const float t = target[(size_t) i];
            g += (t - g) * (t < g ? attack : release);
            gain[(size_t) i] = g;
        }

        std::vector<float> high ((size_t) n);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            std::copy (data, data + n, high.begin());
            highPass (high.data(), n, sampleRate, sibilanceCutoffHz);

            for (int i = 0; i < n; ++i)
                data[i] -= (1.0f - gain[(size_t) i]) * high[(size_t) i];   // x = low + high exactly when g = 1
        }
    }

    //==============================================================================
    void applyPhone (juce::AudioBuffer<float>& buffer, double sampleRate, float mix, float tone)
    {
        const float peakBefore = buffer.getMagnitude (0, buffer.getNumSamples());
        const double hpHz = 200.0 * std::pow (4.0, (double) tone);     // 200 .. 800 Hz
        const double lpHz = 2000.0 * std::pow (2.5, (double) tone);    // 2 .. 5 kHz
        const auto hp = juce::IIRCoefficients::makeHighPass (sampleRate, hpHz);
        const auto lp = juce::IIRCoefficients::makeLowPass (sampleRate, juce::jmin (lpHz, sampleRate * 0.45));
        constexpr float drive = 2.5f;
        const float norm = 1.0f / std::tanh (drive);
        const int n = buffer.getNumSamples();
        juce::AudioBuffer<float> processed (buffer);

        for (int ch = 0; ch < processed.getNumChannels(); ++ch)
        {
            juce::IIRFilter filters[4];
            filters[0].setCoefficients (hp);
            filters[1].setCoefficients (hp);
            filters[2].setCoefficients (lp);
            filters[3].setCoefficients (lp);

            auto* data = processed.getWritePointer (ch);

            for (auto& f : filters)
                f.processSamples (data, n);

            const float scale = peakBefore > 1.0e-6f ? 1.0f / peakBefore : 1.0f;

            for (int s = 0; s < n; ++s)
                data[s] = std::tanh (drive * data[s] * scale) * norm * peakBefore;
        }

        matchPeak (processed, peakBefore);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.applyGain (ch, 0, n, 1.0f - mix);
            buffer.addFrom (ch, 0, processed, ch, 0, n, mix);
        }

        matchPeak (buffer, peakBefore);
    }

    void applyChorus (juce::AudioBuffer<float>& buffer, double sampleRate, float mix, float rateHz)
    {
        const float peakBefore = buffer.getMagnitude (0, buffer.getNumSamples());
        const int n = buffer.getNumSamples();
        const int maxDelay = msToSamples (40.0, sampleRate) + 4;

        struct Voice { double baseMs, depthMs, rateHz, phase; };
        const Voice voices[2] = { { 12.0, 4.0, (double) rateHz, 0.0 }, { 19.0, 5.5, (double) rateHz * 1.47, 1.7 } };

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const double stereoOffset = ch == 0 ? 0.0 : juce::MathConstants<double>::halfPi;
            std::vector<float> line ((size_t) maxDelay, 0.0f);
            size_t write = 0;
            auto* data = buffer.getWritePointer (ch);

            for (int s = 0; s < n; ++s)
            {
                const float x = data[s];
                line[write] = x;

                float wet = 0.0f;

                for (const auto& v : voices)
                {
                    const double lfo = std::sin (twoPi * v.rateHz * s / sampleRate + v.phase + stereoOffset);
                    const double delaySamples = (v.baseMs + v.depthMs * lfo) * 0.001 * sampleRate;
                    double readPos = (double) write - delaySamples;

                    while (readPos < 0.0)
                        readPos += (double) maxDelay;

                    const auto i0 = (size_t) readPos % (size_t) maxDelay;
                    const auto i1 = (i0 + 1) % (size_t) maxDelay;
                    const float frac = (float) (readPos - std::floor (readPos));
                    wet += line[i0] + frac * (line[i1] - line[i0]);
                }

                data[s] = x * (1.0f - 0.4f * mix) + mix * 0.4f * wet;   // wet sums two voices
                write = (write + 1) % (size_t) maxDelay;
            }
        }

        matchPeak (buffer, peakBefore);
    }

    /** Tempo-synced tremolo, phase-locked so the gain is 1 exactly where the original begins. */
    void applyTremolo (juce::AudioBuffer<float>& buffer, double sampleRate, double bpm, double beatsPerCycle,
                       float mix, int referenceSample)
    {
        const double rateHz = juce::jlimit (20.0, 400.0, bpm) / 60.0 / juce::jmax (1.0 / 64.0, beatsPerCycle);
        const float depth = 0.95f * juce::jlimit (0.0f, 1.0f, mix);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int s = 0; s < buffer.getNumSamples(); ++s)
            {
                const double t = (double) (s - referenceSample) / sampleRate;
                const float g = 1.0f - depth * 0.5f * (1.0f - (float) std::cos (twoPi * rateHz * t));
                data[s] *= g;
            }
        }
    }
}

//==============================================================================
std::shared_ptr<RenderedAudio> ReverseReverbRenderer::render (const juce::AudioBuffer<float>& source,
                                                              double sourceSampleRate,
                                                              int startSample,
                                                              int numSamples,
                                                              const ReverseReverbSettings& s)
{
    auto result = std::make_shared<RenderedAudio>();
    result->sampleRate = s.sampleRate;

    if (numSamples <= 0 || source.getNumSamples() == 0 || source.getNumChannels() == 0)
        return result;

    startSample = juce::jlimit (0, source.getNumSamples() - 1, startSample);
    numSamples  = juce::jmin (numSamples, source.getNumSamples() - startSample);

    // 1. The selected segment, stereo, at the output sample rate, with click-free edges.
    auto segment = resampleBuffer (extractStereoSegment (source, startSample, numSamples), sourceSampleRate, s.sampleRate);
    const int n = segment.getNumSamples();
    applyEdgeFades (segment, msToSamples (s.edgeFadeMs, s.sampleRate));
    const float dryPeak = segment.getMagnitude (0, n);
    result->dry = segment;

    // 2. Automatic sibilance detection: the reverb is fed a copy whose "s" sounds are tamed,
    //    so they never smear backwards across the swell. The original stays untouched.
    juce::AudioBuffer<float> reverbFeed (segment);
    const auto sibilance = detectSibilance (segment, s.sampleRate);

    for (const auto& [a, b] : sibilance)
        result->sibilantRegions.emplace_back (a / s.sampleRate, b / s.sampleRate);

    if (s.sibilanceControl)
        applySibilanceControl (reverbFeed, s.sampleRate, sibilance);

    // 3. Reverse it and leave room for the reverb to decay.
    const int tail = msToSamples (s.tailSeconds * 1000.0, s.sampleRate);
    juce::AudioBuffer<float> reversed (reverbFeed);
    reverseBuffer (reversed);

    juce::AudioBuffer<float> wet (2, n + tail);   // reversed segment, then silence for the tail
    wet.clear();

    for (int ch = 0; ch < 2; ++ch)
        wet.copyFrom (ch, 0, reversed, ch, 0, n);

    // 3. Diffuse, then run through the reverb (100% wet).
    applyDiffusion (wet, s.diffusion, s.sampleRate);

    juce::Reverb reverb;
    reverb.setSampleRate (s.sampleRate);

    juce::Reverb::Parameters params;
    params.roomSize   = juce::jlimit (0.0f, 1.0f, s.roomSize);
    params.damping    = juce::jlimit (0.0f, 1.0f, s.damping);
    params.width      = juce::jlimit (0.0f, 1.0f, s.width);
    params.wetLevel   = 1.0f;
    params.dryLevel   = 0.0f;
    params.freezeMode = 0.0f;
    reverb.setParameters (params);
    reverb.reset();
    reverb.processStereo (wet.getWritePointer (0), wet.getWritePointer (1), wet.getNumSamples());

    // 4. Keep only the reverb tail: what rings on after the reversed phrase has finished playing
    //    into the reverb. The part recorded while the phrase was still playing contains the
    //    phrase itself; once reversed it would be heard at the end of the swell.
    {
        juce::AudioBuffer<float> tailOnly (2, juce::jmax (1, tail));
        tailOnly.clear();

        for (int ch = 0; ch < 2; ++ch)
            tailOnly.copyFrom (ch, 0, wet, ch, n, juce::jmin (tail, wet.getNumSamples() - n));

        wet = std::move (tailOnly);
    }

    // Match the swell peak to the dry peak so the Wet level behaves predictably.
    if (wet.getMagnitude (0, wet.getNumSamples()) > 1.0e-6f)
        matchPeak (wet, dryPeak);
    else
        wet.clear();

    wet.applyGain (s.wetGain);

    // 5. Reverse the tail: the decay now swells *into* the moment the phrase begins, and stops there.
    reverseBuffer (wet);

    const int preDelay = msToSamples (s.preDelayMs, s.sampleRate);
    const int dryStart = tail + preDelay;

    // 6. Character of the swell.
    // Low cut keeps the swell from muddying the mix (24 dB/oct).
    if (s.lowCutHz > 21.0f)
        for (int ch = 0; ch < wet.getNumChannels(); ++ch)
            highPass (wet.getWritePointer (ch), wet.getNumSamples(), s.sampleRate, s.lowCutHz);

    if (s.phoneOn)   applyPhone (wet, s.sampleRate, s.phoneMix, s.phoneTone);
    if (s.chorusOn)  applyChorus (wet, s.sampleRate, s.chorusMix, s.chorusRateHz);
    if (s.tremoloOn) applyTremolo (wet, s.sampleRate, s.bpm, s.tremoloBeats, s.tremoloMix, dryStart);

    // 7. Fade the swell in over a portion of its length.
    const int fadeLength = juce::roundToInt (juce::jlimit (0.0f, 1.0f, s.fade) * (float) tail);

    if (fadeLength > 1)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* data = wet.getWritePointer (ch);

            for (int i = 0; i < fadeLength; ++i)
            {
                const float x = (float) i / (float) fadeLength;
                data[i] *= x * x;
            }
        }
    }

    // The swell ends at full energy right where the phrase begins; 4 ms keeps that cut click-free.
    {
        const int fadeOut = juce::jmin (wet.getNumSamples() / 2, msToSamples (4.0, s.sampleRate));
        if (fadeOut > 0)
            wet.applyGainRamp (wet.getNumSamples() - fadeOut, fadeOut, 1.0f, 0.0f);
    }

    // 8. Compose: swell first, then (optionally) the untouched original after the pre-delay.
    const int outLen = s.includeOriginal ? juce::jmax (wet.getNumSamples(), dryStart + n)
                                         : wet.getNumSamples();

    auto& out = result->buffer;
    out.setSize (2, outLen);
    out.clear();

    for (int ch = 0; ch < 2; ++ch)
        out.addFrom (ch, 0, wet, ch, 0, wet.getNumSamples());

    // The swell alone, for playing in sync under the track's own (already playing) original.
    auto& swellOnly = result->swellOnly;
    swellOnly.makeCopyOf (wet);
    applyEdgeFades (swellOnly, msToSamples (10.0, s.sampleRate));

    if (s.includeOriginal)
        for (int ch = 0; ch < 2; ++ch)
            out.addFrom (ch, dryStart, segment, ch, 0, n, s.dryGain);

    result->originalStartSample   = dryStart;
    result->originalLengthSamples = n;
    result->swellLengthSamples    = dryStart;
    result->includesOriginal      = s.includeOriginal;

    // 9. Keep the clip click-free and inside 0 dBFS.
    applyEdgeFades (out, msToSamples (10.0, s.sampleRate));

    const float outPeak = out.getMagnitude (0, outLen);
    if (outPeak > 0.99f)
        out.applyGain (0.99f / outPeak);

    const float swellPeak = swellOnly.getMagnitude (0, swellOnly.getNumSamples());
    if (swellPeak > 0.99f)
        swellOnly.applyGain (0.99f / swellPeak);

    return result;
}

//==============================================================================
std::vector<std::pair<int, int>> ReverseReverbRenderer::detectSibilance (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    std::vector<std::pair<int, int>> regions;
    const int n = buffer.getNumSamples();

    if (n == 0 || buffer.getNumChannels() == 0 || sampleRate < 16000.0)
        return regions;

    // Mono mix and its high band.
    std::vector<float> mono ((size_t) n, 0.0f);
    const float chScale = 1.0f / (float) buffer.getNumChannels();

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
            mono[(size_t) i] += data[i] * chScale;
    }

    std::vector<float> high (mono);
    highPass (high.data(), n, sampleRate, sibilanceCutoffHz);

    // 5 ms frames: a frame is sibilant when it is loud enough and most of its energy is high.
    const int frame = juce::jmax (32, msToSamples (5.0, sampleRate));
    const int numFrames = n / frame;
    std::vector<double> total ((size_t) numFrames), hf ((size_t) numFrames);
    double loudest = 0.0;

    for (int f = 0; f < numFrames; ++f)
    {
        double e = 0.0, h = 0.0;

        for (int i = f * frame; i < (f + 1) * frame; ++i)
        {
            e += (double) mono[(size_t) i] * mono[(size_t) i];
            h += (double) high[(size_t) i] * high[(size_t) i];
        }

        total[(size_t) f] = e / frame;
        hf[(size_t) f] = h / frame;
        loudest = juce::jmax (loudest, total[(size_t) f]);
    }

    if (loudest <= 1.0e-10)
        return regions;

    // Loud enough: above -55 dBFS and within 40 dB of the loudest frame.
    const double floor = juce::jmax (std::pow (10.0, -55.0 / 10.0), loudest * std::pow (10.0, -40.0 / 10.0));
    const int minFrames = 3;      // 15 ms
    const int bridgeFrames = 4;   // merge gaps up to 20 ms
    int runStart = -1, lastSibilant = -100;

    auto closeRun = [&] (int endFrame)
    {
        if (runStart >= 0 && endFrame - runStart >= minFrames)
            regions.emplace_back (juce::jmax (0, runStart * frame - frame), juce::jmin (n, endFrame * frame + frame));
        runStart = -1;
    };

    for (int f = 0; f < numFrames; ++f)
    {
        const bool sibilant = total[(size_t) f] > floor && hf[(size_t) f] > 0.45 * total[(size_t) f];

        if (sibilant)
        {
            if (runStart < 0 || f - lastSibilant > bridgeFrames)
            {
                closeRun (lastSibilant + 1);
                runStart = f;
            }
            lastSibilant = f;
        }
    }

    closeRun (lastSibilant + 1);
    return regions;
}
