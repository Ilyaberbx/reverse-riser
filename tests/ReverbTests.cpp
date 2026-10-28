#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace
{
    constexpr int blockSize = 512;

    void setParam (PluginProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    float getParam (PluginProcessor& p, const char* id)
    {
        auto* param = p.apvts.getParameter (id);
        return param->convertFrom0to1 (param->getValue());
    }

    void prepare (PluginProcessor& p, double sampleRate, int channels = 2)
    {
        p.setPlayConfigDetails (channels, channels, sampleRate, blockSize);
        p.prepareToPlay (sampleRate, blockSize);
    }

    // processes `buffer` in place, block by block, like a host would
    void render (PluginProcessor& p, juce::AudioBuffer<float>& buffer)
    {
        juce::MidiBuffer midi;
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
        {
            juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, juce::jmin (blockSize, buffer.getNumSamples() - start));
            p.processBlock (block, midi);
        }
    }

    juce::AudioBuffer<float> noise (int channels, int numSamples, float amplitude, int seed = 1)
    {
        juce::Random random (seed);
        juce::AudioBuffer<float> buffer (channels, numSamples);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < numSamples; ++i)
                buffer.setSample (c, i, amplitude * (2.0f * random.nextFloat() - 1.0f));
        return buffer;
    }

    // bit test, because -ffast-math in Release may fold std::isfinite away
    bool isFinite (float x)
    {
        return (std::bit_cast<uint32_t> (x) & 0x7f800000u) != 0x7f800000u;
    }

    float rms (const juce::AudioBuffer<float>& b, int start, int num)
    {
        return b.getRMSLevel (0, start, num);
    }

    // wet-only reverb with everything that colours the decay turned off
    void neutralWet (PluginProcessor& p)
    {
        setParam (p, "mix", 100);
        setParam (p, "damping", 0);
        setParam (p, "moddepth", 0);
        setParam (p, "predelay", 0);
        setParam (p, "shimmer", 0);
        setParam (p, "spring", 0);
        setParam (p, "lofi", 0);
        setParam (p, "output", 0);
    }
}

TEST_CASE ("every preset is stable and decays to silence", "[reverb][safety]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    const auto channels = GENERATE (1, 2);

    for (int program = 0; program < (int) factoryPresets.size(); ++program)
    {
        CAPTURE (sampleRate, channels, factoryPresets[(size_t) program].name);
        PluginProcessor p;
        p.setCurrentProgram (program);
        prepare (p, sampleRate, channels);

        const auto inputLength = (int) (2 * sampleRate);
        const auto tailLength = (int) ((2 * getParam (p, "decay") + 1) * sampleRate);
        auto buffer = noise (channels, inputLength + tailLength, 0.5f);
        for (int c = 0; c < channels; ++c)
            buffer.clear (c, inputLength, tailLength);

        render (p, buffer);

        bool allFinite = true;
        float peak = 0.0f;
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                allFinite = allFinite && isFinite (buffer.getSample (c, i));
                peak = std::max (peak, std::abs (buffer.getSample (c, i)));
            }
        CHECK (allFinite);
        CHECK (peak < 4.0f);

        const auto lastBit = (int) (0.1 * sampleRate);
        CHECK (buffer.getMagnitude (buffer.getNumSamples() - lastBit, lastBit) < juce::Decibels::decibelsToGain (-90.0f));
    }
}

TEST_CASE ("decay time matches the Decay parameter", "[reverb][behavior]")
{
    const auto decay = GENERATE (1.0f, 4.0f);
    constexpr double sampleRate = 48000.0;

    PluginProcessor p;
    neutralWet (p);
    setParam (p, "decay", decay);
    prepare (p, sampleRate);

    juce::AudioBuffer<float> buffer (2, (int) ((3 * decay + 1) * sampleRate));
    buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    buffer.setSample (1, 0, 1.0f);
    render (p, buffer);

    // Schroeder backward integration, then T20 (-5 dB .. -25 dB) extrapolated to 60 dB
    std::vector<double> energy ((size_t) buffer.getNumSamples());
    double sum = 0.0;
    for (int i = buffer.getNumSamples() - 1; i >= 0; --i)
    {
        sum += (double) buffer.getSample (0, i) * buffer.getSample (0, i);
        energy[(size_t) i] = sum;
    }
    const auto timeAt = [&] (double db) {
        const auto threshold = energy[0] * std::pow (10.0, db / 10.0);
        const auto it = std::ranges::find_if (energy, [&] (double e) { return e < threshold; });
        return (double) std::distance (energy.begin(), it) / sampleRate;
    };
    const auto measured = 3.0 * (timeAt (-25.0) - timeAt (-5.0));

    CAPTURE (decay, measured);
    CHECK (measured > decay * 0.7);
    CHECK (measured < decay * 1.3);
}

TEST_CASE ("Mix at 0 leaves the dry signal bit-exact", "[reverb][behavior]")
{
    PluginProcessor p;
    p.setCurrentProgram (5); // Shimmer Heaven: the busiest preset
    setParam (p, "mix", 0);
    setParam (p, "output", 0);
    prepare (p, 44100.0);

    const auto input = noise (2, 44100, 0.5f);
    auto buffer = input;
    render (p, buffer);

    bool identical = true;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            identical = identical && juce::exactlyEqual (buffer.getSample (c, i), input.getSample (c, i));
    CHECK (identical);
}

TEST_CASE ("pre-delay holds back the wet signal", "[reverb][behavior]")
{
    constexpr double sampleRate = 48000.0;
    PluginProcessor p;
    neutralWet (p);
    setParam (p, "predelay", 100);
    prepare (p, sampleRate);

    juce::AudioBuffer<float> buffer (2, (int) sampleRate);
    buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    buffer.setSample (1, 0, 1.0f);
    render (p, buffer);

    int first = -1;
    for (int i = 0; i < buffer.getNumSamples() && first < 0; ++i)
        if (std::abs (buffer.getSample (0, i)) > 1.0e-6f)
            first = i;

    CHECK (first >= (int) (0.1 * sampleRate));
    CHECK (buffer.getMagnitude (0, 0, buffer.getNumSamples()) > 1.0e-3f);
}

TEST_CASE ("Freeze sustains the tail and ignores new input", "[reverb][behavior]")
{
    constexpr double sampleRate = 44100.0;
    const auto second = (int) sampleRate;

    // A gets silence while frozen, B gets loud noise
    auto run = [&] (bool feedWhileFrozen) {
        PluginProcessor p;
        p.setCurrentProgram (2); // Chamber
        setParam (p, "mix", 100);
        prepare (p, sampleRate);

        auto before = noise (2, second, 0.5f);
        render (p, before);

        setParam (p, "freeze", 1);
        auto frozen = feedWhileFrozen ? noise (2, 4 * second, 0.5f, 2) : juce::AudioBuffer<float> (2, 4 * second);
        if (! feedWhileFrozen)
            frozen.clear();
        // keep the first 100 ms (the freeze ramp) identical in both runs
        frozen.clear (0, second / 10);
        render (p, frozen);
        return frozen;
    };

    const auto a = run (false);
    const auto b = run (true);

    const auto early = rms (a, second / 5, second / 2);
    const auto late = rms (a, 3 * second + second / 5, second / 2);
    CAPTURE (early, late);
    CHECK (early > 1.0e-3f);
    CHECK (late * late >= 0.5f * early * early);

    float maxDiff = 0.0f;
    for (int c = 0; c < 2; ++c)
        for (int i = second / 5; i < a.getNumSamples(); ++i)
            maxDiff = std::max (maxDiff, std::abs (a.getSample (c, i) - b.getSample (c, i)));
    CHECK (maxDiff < 1.0e-5f);
}

TEST_CASE ("programs apply every preset value", "[reverb][presets]")
{
    PluginProcessor p;
    REQUIRE (p.getNumPrograms() == (int) factoryPresets.size());

    for (int program = 0; program < p.getNumPrograms(); ++program)
    {
        const auto& preset = factoryPresets[(size_t) program];
        CAPTURE (preset.name);
        setParam (p, "freeze", 1);
        p.setCurrentProgram (program);

        CHECK (p.getCurrentProgram() == program);
        CHECK (p.getProgramName (program) == preset.name);
        CHECK (getParam (p, "freeze") == 0.0f);
        for (size_t i = 0; i < presetParamIds.size(); ++i)
        {
            CAPTURE (presetParamIds[i]);
            CHECK (std::abs (getParam (p, presetParamIds[i]) - preset.values[i]) < 1.0e-3f * std::max (1.0f, std::abs (preset.values[i])));
        }
    }
}

TEST_CASE ("state round-trips parameters and the program", "[reverb][state]")
{
    PluginProcessor source;
    source.setCurrentProgram (6);
    setParam (source, "size", 73);
    setParam (source, "freeze", 1);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    PluginProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK (restored.getCurrentProgram() == 6);
    CHECK (getParam (restored, "freeze") == 1.0f);
    for (auto* id : presetParamIds)
    {
        CAPTURE (id);
        CHECK (juce::exactlyEqual (getParam (restored, id), getParam (source, id)));
    }
}
