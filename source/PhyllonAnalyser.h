#pragma once

#include <juce_dsp/juce_dsp.h>

// Peer Play's audio analysis (Unity "AudioPeer"), as used by Phýllon:
// 512-bin spectrum -> 8 frequency bands -> band buffers (fast attack, accelerating fall) -> normalised 0..1
// Runs on the message thread, once per frame.
class PhyllonAnalyser
{
public:
    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int numBands = 8;

    PhyllonAnalyser();

    // keeps the most recent fftSize samples
    void pushSamples (const float* samples, int numSamples);
    // call once per frame
    void analyse();

    float band (int i) const { return audioBand[(size_t) i]; }
    float bandBuffer (int i) const { return audioBandBuffer[(size_t) i]; }
    float rawBand (int i) const { return freqBand[(size_t) i]; }
    float rawBandBuffer (int i) const { return bandBuf[(size_t) i]; }
    float amplitude() const { return amp; }
    float amplitudeBuffer() const { return ampBuffer; }

private:
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> history {};
    std::array<float, 2 * fftSize> fftData {};
    int writePos = 0;

    std::array<float, numBands> freqBand {}, bandBuf {}, decrease {}, highest {}, audioBand {}, audioBandBuffer {};
    float amp = 0.0f, ampBuffer = 0.0f;
};
