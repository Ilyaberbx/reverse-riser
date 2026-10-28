#include <PhyllonView.h>
#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>

namespace
{
    void feedSine (PhyllonAnalyser& analyser, float frequency, float amplitude, double sampleRate = 44100.0)
    {
        std::array<float, PhyllonAnalyser::fftSize> samples {};
        for (size_t i = 0; i < samples.size(); ++i)
            samples[i] = amplitude * std::sin (juce::MathConstants<float>::twoPi * frequency * (float) i / (float) sampleRate);
        analyser.pushSamples (samples.data(), (int) samples.size());
    }

    int loudestBand (const PhyllonAnalyser& analyser)
    {
        int best = 0;
        for (int i = 1; i < PhyllonAnalyser::numBands; ++i)
            if (analyser.rawBand (i) > analyser.rawBand (best))
                best = i;
        return best;
    }
}

TEST_CASE ("Phýllon bands follow the spectrum", "[phyllon]")
{
    // 512 bins of ~43 Hz: band 1 = bins 2-5 (~86-258 Hz), band 5 = bins 62-125 (~2.7-5.4 kHz)
    PhyllonAnalyser low, high;
    feedSine (low, 150.0f, 0.5f);
    feedSine (high, 4000.0f, 0.5f);
    low.analyse();
    high.analyse();

    CHECK (loudestBand (low) == 1);
    CHECK (loudestBand (high) == 5);
    CHECK (low.band (1) > 0.99f); // normalised against its own new peak
}

TEST_CASE ("band buffer jumps up and falls ever faster", "[phyllon]")
{
    PhyllonAnalyser analyser;
    feedSine (analyser, 150.0f, 0.5f);
    analyser.analyse();
    const auto peak = analyser.rawBand (1);
    CHECK (juce::exactlyEqual (analyser.rawBandBuffer (1), peak));

    const std::array<float, PhyllonAnalyser::fftSize> silence {};
    std::array<float, 4> buffer {};
    for (auto& value : buffer)
    {
        analyser.pushSamples (silence.data(), (int) silence.size());
        analyser.analyse();
        value = analyser.rawBandBuffer (1);
    }

    const auto firstDrop = peak - buffer[0], secondDrop = buffer[0] - buffer[1], thirdDrop = buffer[1] - buffer[2];
    CHECK (std::abs (firstDrop - 0.005f) < 1.0e-5f);
    CHECK (secondDrop > firstDrop);
    CHECK (thirdDrop > secondDrop);
}

TEST_CASE ("phyllotaxis seeds sit on the golden-angle spiral", "[phyllon]")
{
    const auto seed = PhyllonView::phyllotaxis (10.0f, 2.0f);
    CHECK (std::abs (seed.getDistanceFromOrigin() - 2.0f * std::sqrt (10.0f)) < 1.0e-4f);

    const auto expected = std::fmod (10.0f * PhyllonView::goldenAngleDegrees, 360.0f);
    auto actual = juce::radiansToDegrees (std::atan2 (seed.y, seed.x));
    if (actual < 0.0f)
        actual += 360.0f;
    CHECK (std::abs (actual - expected) < 0.01f);
}

TEST_CASE ("processBlock feeds the visualizer FIFO with the mono output", "[phyllon]")
{
    PluginProcessor p;
    p.setCurrentProgram (1); // Small Room: no vocal chain
    p.setPlayConfigDetails (2, 2, 44100.0, 256);
    p.prepareToPlay (44100.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    for (int i = 0; i < 256; ++i)
    {
        buffer.setSample (0, i, 0.25f);
        buffer.setSample (1, i, -0.5f);
    }
    juce::MidiBuffer midi;
    p.processBlock (buffer, midi);

    std::array<float, 1024> read {};
    REQUIRE (p.readVisualizerSamples (read.data(), (int) read.size()) == 256);
    for (int i = 0; i < 256; ++i)
        REQUIRE (juce::exactlyEqual (read[(size_t) i], 0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i))));
    CHECK (p.readVisualizerSamples (read.data(), (int) read.size()) == 0);
}
