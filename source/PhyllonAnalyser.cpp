#include "PhyllonAnalyser.h"

namespace
{
    // seeds the running maximum so silence at start-up doesn't normalise noise to full scale
    constexpr float audioProfile = 0.5f;
    // lets the normalisation adapt to quieter material over time (Peer Play's never decays)
    constexpr float highestDecay = 0.9995f;
}

PhyllonAnalyser::PhyllonAnalyser()
{
    highest.fill (audioProfile);
}

void PhyllonAnalyser::pushSamples (const float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        history[(size_t) writePos] = samples[i];
        writePos = (writePos + 1) % fftSize;
    }
}

void PhyllonAnalyser::analyse()
{
    // oldest sample first
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = history[(size_t) ((writePos + i) % fftSize)];
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    // band i averages 2^(i+1) bins (the last one takes 2 extra), weighted by bin index to lift the highs
    int bin = 0;
    for (int i = 0; i < numBands; ++i)
    {
        auto count = 1 << (i + 1);
        if (i == numBands - 1)
            count += 2;

        float average = 0.0f;
        for (int j = 0; j < count; ++j, ++bin)
            average += fftData[(size_t) bin] * (2.0f / fftSize) * (float) (bin + 1);
        freqBand[(size_t) i] = average / (float) count * 10.0f;
    }

    float sum = 0.0f, sumBuffer = 0.0f, sumHighest = 0.0f;
    for (size_t i = 0; i < (size_t) numBands; ++i)
    {
        if (freqBand[i] > bandBuf[i])
        {
            bandBuf[i] = freqBand[i];
            decrease[i] = 0.005f;
        }
        else
        {
            bandBuf[i] = std::max (0.0f, bandBuf[i] - decrease[i]);
            decrease[i] *= 1.2f;
        }

        highest[i] = std::max ({ freqBand[i], highest[i] * highestDecay, audioProfile * 0.1f });
        audioBand[i] = std::clamp (freqBand[i] / highest[i], 0.0f, 1.0f);
        audioBandBuffer[i] = std::clamp (bandBuf[i] / highest[i], 0.0f, 1.0f);

        sum += freqBand[i];
        sumBuffer += bandBuf[i];
        sumHighest += highest[i];
    }
    amp = std::clamp (sum / sumHighest, 0.0f, 1.0f);
    ampBuffer = std::clamp (sumBuffer / sumHighest, 0.0f, 1.0f);
}
