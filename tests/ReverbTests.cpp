#include <Presets.h>
#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace
{
    // bit test, because -ffast-math in Release may fold std::isfinite away
    bool isFinite (float x)
    {
        return (std::bit_cast<uint32_t> (x) & 0x7f800000u) != 0x7f800000u;
    }
}

TEST_CASE ("every reverb type is stable and decays to silence", "[reverb][safety]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    constexpr int block = 512;

    for (const auto& type : reverbTypes)
    {
        CAPTURE (sampleRate, type.name);
        ReverbEngine engine;
        engine.prepare (sampleRate, block);
        engine.setParameters (type.params, true);

        const auto inputLength = (int) (2 * sampleRate);
        const auto total = inputLength + (int) ((2 * type.params.decaySeconds + 1) * sampleRate);
        juce::AudioBuffer<float> in (2, total), out (2, total);
        in.clear();
        juce::Random random (1);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < inputLength; ++i)
                in.setSample (c, i, 0.5f * (2.0f * random.nextFloat() - 1.0f));

        for (int start = 0; start < total; start += block)
        {
            const auto n = juce::jmin (block, total - start);
            engine.process (in.getReadPointer (0, start), in.getReadPointer (1, start), out.getWritePointer (0, start), out.getWritePointer (1, start), n);
        }

        bool allFinite = true;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < total; ++i)
                allFinite = allFinite && isFinite (out.getSample (c, i));
        CHECK (allFinite);
        CHECK (out.getMagnitude (0, total) < 4.0f);

        const auto lastBit = (int) (0.1 * sampleRate);
        CHECK (out.getMagnitude (total - lastBit, lastBit) < juce::Decibels::decibelsToGain (-90.0f));
    }
}
