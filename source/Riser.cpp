#include "Riser.h"
#include "DelayLine.h"

namespace
{
    constexpr float onsetThreshold = 0.01f; // -40 dBFS
    constexpr float syllableDrop = 0.25f; // -12 dB below the syllable's peak ends it
    constexpr double envelopeMs = 5.0, minSyllableMs = 60.0, maxSyllableMs = 400.0, preRollMs = 5.0;

    constexpr float deEssHz = 5000.0f, deEssRatio = 0.35f, deEssMaxCut = 0.316f; // -10 dB

    constexpr float fadeInMs = 10.0f, peakTarget = 0.891f; // -1 dBFS
    constexpr int renderBlock = 512;

    constexpr float grainMs = 50.0f;
    constexpr float chorusBaseMs = 12.0f, chorusDepthMs = 3.0f;

    constexpr auto twoPi = juce::MathConstants<float>::twoPi;

    // one-pole envelope coefficient for a time constant in ms
    float followerCoef (double sampleRate, double ms) { return 1.0f - (float) std::exp (-1.0 / (ms * 0.001 * sampleRate)); }

    // sliding RMS of the louder channel
    std::vector<float> rmsEnvelope (const juce::AudioBuffer<float>& b, double sampleRate)
    {
        const auto window = juce::jmax (1, (int) (envelopeMs * 0.001 * sampleRate));
        std::vector<float> env ((size_t) b.getNumSamples());
        std::vector<double> sum ((size_t) b.getNumChannels(), 0.0);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            float loudest = 0.0f;
            for (int c = 0; c < b.getNumChannels(); ++c)
            {
                const auto x = (double) b.getSample (c, i);
                sum[(size_t) c] += x * x;
                if (i >= window)
                {
                    const auto old = (double) b.getSample (c, i - window);
                    sum[(size_t) c] -= old * old;
                }
                loudest = juce::jmax (loudest, (float) std::sqrt (juce::jmax (0.0, sum[(size_t) c]) / window));
            }
            env[(size_t) i] = loudest;
        }
        return env;
    }

    void reverse (juce::AudioBuffer<float>& b)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            std::reverse (b.getWritePointer (c), b.getWritePointer (c) + b.getNumSamples());
    }

    // granular pitch shift following the curve; two sin²-windowed grains half a window apart (as the shimmer does)
    void pitchShift (juce::AudioBuffer<float>& b, const Riser::FxSettings& fx, double sampleRate)
    {
        const auto n = b.getNumSamples();
        const auto window = grainMs * 0.001f * (float) sampleRate;
        // the grains read on average window/2 + 2 samples late; read that far ahead so the riser stays on the grid
        const auto latency = (int) (window * 0.5f) + 2;

        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            DelayLine line;
            line.prepare ((int) window + 8);
            auto* x = b.getWritePointer (c);
            std::vector<float> out ((size_t) n);
            float phase = 0.0f;
            for (int i = 0; i < n + latency; ++i)
            {
                line.push (i < n ? x[i] : 0.0f);
                const auto t = (float) juce::jmax (0, i - latency) / (float) juce::jmax (1, n - 1);
                const auto ratio = std::exp2 (Riser::pitchAt (fx, t) / 12.0f);
                phase += (ratio - 1.0f) / window;
                phase -= std::floor (phase);
                const auto phase2 = phase < 0.5f ? phase + 0.5f : phase - 0.5f;
                const auto w1 = std::sin (juce::MathConstants<float>::pi * phase);
                const auto w2 = std::sin (juce::MathConstants<float>::pi * phase2);
                const auto y = w1 * w1 * line.readHermite (2.0f + (1.0f - phase) * window)
                               + w2 * w2 * line.readHermite (2.0f + (1.0f - phase2) * window);
                if (i >= latency)
                    out[(size_t) (i - latency)] = y;
            }
            std::copy (out.begin(), out.end(), x);
        }
    }

    void chorus (juce::AudioBuffer<float>& b, const Riser::FxSettings& fx, double sampleRate)
    {
        const auto ms = (float) sampleRate * 0.001f;
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            DelayLine line;
            line.prepare ((int) ((chorusBaseMs + chorusDepthMs) * ms) + 4);
            auto* x = b.getWritePointer (c);
            auto phase = 0.25f * (float) c; // quarter cycle apart for width
            const auto inc = fx.chorusRateHz / (float) sampleRate;
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                line.push (x[i]);
                const auto delayed = line.readHermite ((chorusBaseMs + chorusDepthMs * std::sin (twoPi * phase)) * ms);
                phase += inc;
                phase -= std::floor (phase);
                x[i] += fx.chorusMix * (0.5f * (x[i] + delayed) - x[i]);
            }
        }
    }

    // anchored to the riser's end, so the pulses sit on the grid
    void tremolo (juce::AudioBuffer<float>& b, const Riser::FxSettings& fx, double bpm, double sampleRate)
    {
        const auto period = (float) (fx.tremoloBeats * 60.0 / bpm * sampleRate);
        const auto n = b.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            const auto lfo = 0.5f + 0.5f * std::cos (twoPi * (float) (n - i) / period); // 1 on the beat
            const auto gain = 1.0f - fx.tremoloMix * (1.0f - lfo);
            for (int c = 0; c < b.getNumChannels(); ++c)
                b.getWritePointer (c)[i] *= gain;
        }
    }
}

namespace Riser
{
    int lengthInSamples (float bars, double bpm, double sampleRate)
    {
        return juce::roundToInt (bars * 4.0 * 60.0 / bpm * sampleRate);
    }

    juce::Range<int> detectSyllable (const juce::AudioBuffer<float>& capture, double sampleRate)
    {
        const auto env = rmsEnvelope (capture, sampleRate);
        const auto n = (int) env.size();
        const auto toSamples = [sampleRate] (double ms) { return (int) (ms * 0.001 * sampleRate); };

        auto onset = 0;
        while (onset < n && env[(size_t) onset] < onsetThreshold)
            ++onset;
        if (onset == n)
            return {};

        const auto earliestEnd = juce::jmin (n, onset + toSamples (minSyllableMs));
        const auto latestEnd = juce::jmin (n, onset + toSamples (maxSyllableMs));
        float peak = 0.0f;
        auto end = onset;
        for (; end < latestEnd; ++end)
        {
            peak = juce::jmax (peak, env[(size_t) end]);
            if (end >= earliestEnd && env[(size_t) end] < peak * syllableDrop)
                break;
        }
        return { juce::jmax (0, onset - toSamples (preRollMs)), end };
    }

    void deEss (juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        juce::dsp::StateVariableTPTFilter<float> highBand;
        highBand.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        highBand.prepare ({ sampleRate, (juce::uint32) buffer.getNumSamples(), (juce::uint32) buffer.getNumChannels() });
        highBand.setCutoffFrequency (juce::jmin (deEssHz, 0.45f * (float) sampleRate));

        const auto attack = followerCoef (sampleRate, 1.0), release = followerCoef (sampleRate, 60.0);
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            auto* x = buffer.getWritePointer (c);
            float highEnv = 0.0f, fullEnv = 0.0f;
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto high = highBand.processSample (c, x[i]);
                const auto follow = [&] (float& env, float v) { env += (v > env ? attack : release) * (v - env); };
                follow (highEnv, std::abs (high));
                follow (fullEnv, std::abs (x[i]));
                // sibilant when the top band carries more than its share of the level
                const auto limit = deEssRatio * fullEnv;
                const auto gain = highEnv > limit ? juce::jmax (deEssMaxCut, limit / highEnv) : 1.0f;
                x[i] += (gain - 1.0f) * high;
            }
        }
    }

    juce::AudioBuffer<float> renderRaw (const juce::AudioBuffer<float>& syllable, const ReverbParams& reverb, int lengthSamples, double sampleRate)
    {
        // reverse(reverb(reverse(syllable))): the part of the reversed tail that lies before the syllable is the riser
        const auto syllableLength = syllable.getNumSamples();
        const auto total = syllableLength + lengthSamples;
        juce::AudioBuffer<float> input (2, total), wet (2, total);
        input.clear();
        for (int c = 0; c < 2; ++c)
        {
            const auto* src = syllable.getReadPointer (juce::jmin (c, syllable.getNumChannels() - 1));
            std::reverse_copy (src, src + syllableLength, input.getWritePointer (c));
        }

        ReverbEngine engine;
        engine.prepare (sampleRate, renderBlock);
        auto params = reverb;
        params.freeze = false;
        // the tail must last the whole riser, or short spaces only swell in the last beat
        params.decaySeconds = juce::jmax (params.decaySeconds, (float) (lengthSamples / sampleRate));
        engine.setParameters (params, true);
        for (int start = 0; start < total; start += renderBlock)
        {
            const auto n = juce::jmin (renderBlock, total - start);
            engine.process (input.getReadPointer (0, start), input.getReadPointer (1, start), wet.getWritePointer (0, start), wet.getWritePointer (1, start), n);
        }

        juce::AudioBuffer<float> riser (2, lengthSamples);
        for (int c = 0; c < 2; ++c)
            riser.copyFrom (c, 0, wet, c, syllableLength, lengthSamples);
        reverse (riser);

        riser.applyGainRamp (0, juce::jmin (lengthSamples, (int) (fadeInMs * 0.001f * (float) sampleRate)), 0.0f, 1.0f);
        if (const auto peak = riser.getMagnitude (0, lengthSamples); peak > 1.0e-6f)
            riser.applyGain (peakTarget / peak);
        return riser;
    }

    juce::AudioBuffer<float> renderFx (const juce::AudioBuffer<float>& raw, const FxSettings& fx, double bpm, double sampleRate)
    {
        auto out = raw;
        if (fx.pitch)
            pitchShift (out, fx, sampleRate);
        if (fx.chorus)
            chorus (out, fx, sampleRate);
        if (fx.tremolo)
            tremolo (out, fx, bpm, sampleRate);
        return out;
    }

    float pitchAt (const FxSettings& fx, float t)
    {
        const auto curved = std::pow (juce::jlimit (0.0f, 1.0f, t), std::pow (4.0f, fx.pitchShape));
        return fx.pitchStart + (fx.pitchEnd - fx.pitchStart) * curved;
    }

    bool writeWav (const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::File& file)
    {
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        if (stream == nullptr)
            return false;
        const auto options = juce::AudioFormatWriterOptions().withSampleRate (sampleRate).withNumChannels (buffer.getNumChannels()).withBitsPerSample (24);
        const auto writer = juce::WavAudioFormat().createWriterFor (stream, options);
        return writer != nullptr && writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }
}
