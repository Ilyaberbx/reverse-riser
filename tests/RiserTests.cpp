#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace
{
    constexpr double testSampleRate = 48000.0;
    constexpr int blockSize = 512;

    int ms (double milliseconds) { return (int) (milliseconds * 0.001 * testSampleRate); }

    // stereo buffer of `totalMs` with sine bursts at { startMs, lengthMs }
    juce::AudioBuffer<float> bursts (double totalMs, std::initializer_list<std::pair<double, double>> list, float freq = 440.0f)
    {
        juce::AudioBuffer<float> b (2, ms (totalMs));
        b.clear();
        for (const auto& [start, length] : list)
            for (int i = ms (start); i < ms (start + length); ++i)
                for (int c = 0; c < 2; ++c)
                    b.setSample (c, i, 0.5f * std::sin (juce::MathConstants<float>::twoPi * freq * (float) i / (float) testSampleRate));
        return b;
    }

    float rms (const juce::AudioBuffer<float>& b, int start, int num) { return b.getRMSLevel (0, start, num); }

    void prepare (PluginProcessor& p)
    {
        p.setPlayConfigDetails (2, 2, testSampleRate, blockSize);
        p.prepareToPlay (testSampleRate, blockSize);
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

    void setParam (PluginProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    // a phrase: silence, a syllable, a gap, more singing, then enough silence for the capture to finish
    juce::AudioBuffer<float> phrase() { return bursts (1800, { { 200, 150 }, { 380, 300 } }); }

    bool identical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        if (a.getNumSamples() != b.getNumSamples() || a.getNumChannels() != b.getNumChannels())
            return false;
        for (int c = 0; c < a.getNumChannels(); ++c)
            if (! std::equal (a.getReadPointer (c), a.getReadPointer (c) + a.getNumSamples(), b.getReadPointer (c)))
                return false;
        return true;
    }
}

TEST_CASE ("the first syllable is found", "[riser][syllable]")
{
    const auto capture = bursts (1000, { { 100, 150 }, { 280, 200 } });
    const auto range = Riser::detectSyllable (capture, testSampleRate);
    CAPTURE (range.getStart(), range.getEnd());
    CHECK (std::abs (range.getStart() - ms (95)) <= ms (1));
    CHECK (range.getEnd() >= ms (248));
    CHECK (range.getEnd() <= ms (280));

    juce::AudioBuffer<float> silence (2, ms (500));
    silence.clear();
    CHECK (Riser::detectSyllable (silence, testSampleRate).isEmpty());
}

TEST_CASE ("the riser is exactly the chosen number of bars", "[riser][timing]")
{
    const auto bpm = GENERATE (100.0, 140.0);
    const auto syllable = bursts (150, { { 0, 150 } });
    for (auto bars : lengthBars)
    {
        CAPTURE (bpm, bars);
        const auto length = Riser::lengthInSamples (bars, bpm, testSampleRate);
        CHECK (length == juce::roundToInt (bars * 4.0 * 60.0 / bpm * testSampleRate));
        CHECK (Riser::renderRaw (syllable, reverbTypes[2].params, length, testSampleRate).getNumSamples() == length);
    }
    CHECK (Riser::lengthInSamples (1.0f, 100.0, testSampleRate) == 115200);
}

TEST_CASE ("the riser swells into the vocal", "[riser][shape]")
{
    const auto syllable = bursts (150, { { 0, 150 } });
    const auto length = Riser::lengthInSamples (1.0f, 120.0, testSampleRate);
    const auto riser = Riser::renderRaw (syllable, reverbTypes[defaultReverbType].params, length, testSampleRate);
    const auto quarter = length / 4;
    CHECK (rms (riser, length - quarter, quarter) > 4.0f * rms (riser, 0, quarter));
    CHECK (riser.getMagnitude (0, length) <= 0.9f);
    CHECK (riser.getMagnitude (0, length) > 0.5f);

    SECTION ("FX with every effect off is the RAW render")
    {
        CHECK (identical (Riser::renderFx (riser, {}, 120.0, testSampleRate), riser));
    }

    SECTION ("the reverb type changes the riser")
    {
        CHECK (! identical (Riser::renderRaw (syllable, reverbTypes[0].params, length, testSampleRate), riser));
    }
}

TEST_CASE ("pitch follows the curve and keeps the length", "[riser][pitch]")
{
    const auto sine = bursts (1000, { { 0, 1000 } });
    Riser::FxSettings fx;
    fx.pitch = true;
    fx.pitchStart = fx.pitchEnd = 12.0f;
    const auto shifted = Riser::renderFx (sine, fx, 120.0, testSampleRate);
    REQUIRE (shifted.getNumSamples() == sine.getNumSamples());

    // rising zero crossings over the middle half second: 440 Hz up an octave -> about 440 of them
    int crossings = 0;
    for (int i = ms (250); i < ms (750); ++i)
        if (shifted.getSample (0, i - 1) < 0.0f && shifted.getSample (0, i) >= 0.0f)
            ++crossings;
    CHECK (crossings >= 431);
    CHECK (crossings <= 449);

    SECTION ("the curve runs from start to end")
    {
        fx.pitchStart = -12.0f;
        fx.pitchEnd = 0.0f;
        CHECK (juce::exactlyEqual (Riser::pitchAt (fx, 0.0f), -12.0f));
        CHECK (juce::exactlyEqual (Riser::pitchAt (fx, 1.0f), 0.0f));
        CHECK (juce::exactlyEqual (Riser::pitchAt (fx, 0.5f), -6.0f));
        fx.pitchShape = 1.0f; // exponential: stays low, then shoots up
        CHECK (Riser::pitchAt (fx, 0.5f) < -10.0f);
    }
}

TEST_CASE ("the plugin passes audio through and latches the first phrase", "[riser][processor]")
{
    PluginProcessor p;
    prepare (p);

    auto audio = phrase();
    const auto input = audio;
    render (p, audio);
    CHECK (identical (audio, input));

    REQUIRE (p.updateRiser());
    REQUIRE (p.hasRiser());
    CHECK (p.riser().getNumSamples() == Riser::lengthInSamples (1.0f, PluginProcessor::defaultBpm, testSampleRate));
    CHECK (p.syllableRange().getLength() < ms (200)); // the first syllable only, not the whole phrase
    const auto first = p.capturedAudio();

    SECTION ("a later phrase doesn't replace it")
    {
        auto later = bursts (1800, { { 200, 400 } }, 880.0f);
        render (p, later);
        CHECK (! p.updateRiser());
        CHECK (identical (p.capturedAudio(), first));
    }

    SECTION ("RESET waits for the next phrase")
    {
        p.resetCapture();
        CHECK (! p.hasRiser());
        auto next = phrase();
        render (p, next);
        CHECK (p.updateRiser());
        CHECK (p.hasRiser());
    }

    SECTION ("changing a setting re-renders")
    {
        setParam (p, "length", 4);
        CHECK (p.updateRiser());
        CHECK (p.riser().getNumSamples() == Riser::lengthInSamples (4.0f, PluginProcessor::defaultBpm, testSampleRate));
    }

    SECTION ("LISTEN plays the riser on top of the input")
    {
        p.setListening (true);
        juce::AudioBuffer<float> silence (2, Riser::lengthInSamples (1.0f, PluginProcessor::defaultBpm, testSampleRate) + blockSize);
        silence.clear();
        render (p, silence);
        CHECK (silence.getMagnitude (0, silence.getNumSamples()) > 0.5f);
        CHECK (! p.isListening()); // stops at the end without LOOP
    }

    SECTION ("the riser and settings survive a reload")
    {
        setParam (p, "length", 3);
        setParam (p, "pitchOn", 1);
        juce::MemoryBlock state;
        p.getStateInformation (state);

        PluginProcessor restored;
        prepare (restored);
        restored.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (juce::exactlyEqual (restored.apvts.getRawParameterValue ("length")->load(), 3.0f));
        CHECK (juce::exactlyEqual (restored.apvts.getRawParameterValue ("pitchOn")->load(), 1.0f));
        REQUIRE (restored.updateRiser());
        CHECK (restored.capturedAudio().getNumSamples() == p.syllableRange().getLength());
        CHECK (restored.riser().getNumSamples() == Riser::lengthInSamples (2.0f, PluginProcessor::defaultBpm, testSampleRate));
    }
}

TEST_CASE ("an exported riser is a 24-bit WAV named after its settings", "[riser][export]")
{
    PluginProcessor p;
    prepare (p);
    auto audio = phrase();
    render (p, audio);
    REQUIRE (p.updateRiser());

    const auto file = p.exportRiser();
    REQUIRE (file.existsAsFile());
    CHECK (file.getFileName() == "Riser_FX_ConcertHall_1bars_120bpm.wav");

    juce::WavAudioFormat wav;
    const auto reader = std::unique_ptr<juce::AudioFormatReader> (wav.createReaderFor (file.createInputStream().release(), true));
    REQUIRE (reader != nullptr);
    CHECK (reader->bitsPerSample == 24);
    CHECK (reader->numChannels == 2);
    CHECK ((int) reader->lengthInSamples == p.riser().getNumSamples());
    file.deleteFile();
}
