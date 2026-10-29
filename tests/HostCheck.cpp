// End-to-end check of the built VST3, driven like a DAW would drive it:
//   - transport plays over a phrase -> the plugin captures it (passing the track through untouched)
//   - Listen (here: a MIDI note) plays a swell that rises into the captured phrase
//   - playing the DAW again over other material replaces the take
//   - a silent playback keeps the previous take
// Also checks the sibilance detector on a synthetic "s" + vowel.
//
// Usage: GlacierRiserHostCheck <path to Glacier Riser.vst3>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/ReverseReverbRenderer.h"
#include "../Source/BarAlignment.h"

namespace
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;
    constexpr double bpm = 120.0;

    struct TestPlayHead : juce::AudioPlayHead
    {
        bool playing = false;
        juce::int64 samples = 0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setIsPlaying (playing);
            info.setTimeInSamples (samples);
            info.setTimeInSeconds ((double) samples / sr);
            info.setPpqPosition ((double) samples / sr * bpm / 60.0);
            info.setBpm (bpm);
            info.setTimeSignature (TimeSignature { 4, 4 });
            return info;
        }
    };

    /** 0.08 s of bright noise ("s") followed by a decaying vowel-like tone. */
    juce::AudioBuffer<float> makePhrase (double vowelSeconds = 0.7)
    {
        const int sN = (int) (0.08 * sr), vN = (int) (vowelSeconds * sr);
        juce::AudioBuffer<float> b (2, sN + vN);
        juce::Random rnd (7);
        float prev = 0.0f;

        for (int i = 0; i < sN; ++i)
        {
            const float white = rnd.nextFloat() * 2.0f - 1.0f;
            const float bright = white - prev;   // first difference: tilts the noise to the highs
            prev = white;
            b.setSample (0, i, 0.35f * bright);
        }

        for (int i = 0; i < vN; ++i)
        {
            const double t = i / sr;
            const double v = (std::sin (juce::MathConstants<double>::twoPi * 220.0 * t)
                              + 0.5 * std::sin (juce::MathConstants<double>::twoPi * 440.0 * t)
                              + 0.25 * std::sin (juce::MathConstants<double>::twoPi * 660.0 * t))
                             * std::exp (-t * 2.0) * juce::jmin (1.0, t / 0.01) * 0.45;
            b.setSample (0, sN + i, (float) v);
        }

        b.copyFrom (1, 0, b, 0, 0, b.getNumSamples());
        return b;
    }

    double rms (const std::vector<float>& x, double fromSec, double toSec, double originSec)
    {
        const auto a = (size_t) juce::jmax (0.0, (fromSec - originSec) * sr);
        const auto b = (size_t) juce::jmin ((double) x.size(), (toSec - originSec) * sr);
        double s = 0.0;
        for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
        return b > a ? std::sqrt (s / (double) (b - a)) : 0.0;
    }

    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << std::endl;
        failures += ok ? 0 : 1;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc < 2)
    {
        std::cout << "usage: GlacierRiserHostCheck <Glacier Riser.vst3>" << std::endl;
        return 2;
    }

    //-- Sibilance detector ----------------------------------------------------------------
    std::cout << "Sibilance detection" << std::endl;
    {
        auto phrase = makePhrase();
        const auto regions = ReverseReverbRenderer::detectSibilance (phrase, sr);
        bool coversS = false, coversVowel = false;

        for (const auto& [a, b] : regions)
        {
            std::cout << "    region " << a / sr << " s .. " << b / sr << " s" << std::endl;
            coversS |= a < 0.08 * sr && b > 0.02 * sr;
            coversVowel |= b > 0.2 * sr;
        }

        check (coversS, "finds the 's' at the start of the phrase");
        check (! coversVowel, "leaves the vowel alone");
    }

    //-- Bar alignment of exported clips ------------------------------------------------------
    std::cout << "Bar alignment" << std::endl;
    {
        // 120 BPM, 4/4: a clip starting at quarter note 33.5 is 1.5 beats (0.75 s) into bar 9.
        const auto a = alignToBar (33.5, 120.0, 4.0, 48000.0);
        const auto b = alignToBar (32.0, 120.0, 4.0, 48000.0);
        std::cout << "    ppq 33.5 -> bar " << a.bar << ", pad " << a.padSamples << " samples; ppq 32 -> bar "
                  << b.bar << ", pad " << b.padSamples << std::endl;
        check (a.valid && a.bar == 9 && a.padSamples == 36000, "a clip mid-bar is padded back to its bar line");
        check (b.valid && b.bar == 9 && b.padSamples == 0, "a clip already on a bar line gets no padding");
    }

    const auto captureDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                .getChildFile ("AtmosRiser").getChildFile ("Captures");
    const int takeFilesBefore = captureDir.getNumberOfChildFiles (juce::File::findFiles, "take_*.wav");

    //-- Load the plugin ----------------------------------------------------------------------
    juce::AudioPluginFormatManager formats;
    formats.addFormat (new juce::VST3PluginFormat());

    juce::OwnedArray<juce::PluginDescription> found;
    juce::VST3PluginFormat vst3;
    vst3.findAllTypesForFile (found, juce::String (argv[1]));

    if (found.isEmpty())
    {
        std::cout << "Could not load " << argv[1] << std::endl;
        return 2;
    }

    juce::String error;
    auto plugin = formats.createPluginInstance (*found[0], sr, block, error);

    if (plugin == nullptr)
    {
        std::cout << "createPluginInstance failed: " << error << std::endl;
        return 2;
    }

    std::cout << "Loaded " << plugin->getName() << std::endl;

    TestPlayHead playHead;
    plugin->setPlayHead (&playHead);
    plugin->setPlayConfigDetails (2, 2, sr, block);
    plugin->prepareToPlay (sr, block);

    const auto pump = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };
    pump (300);

    juce::AudioBuffer<float> io (2, block);
    juce::MidiBuffer midi;

    // Plays [fromSec, toSec) of the timeline with `phrase` placed at phraseAt (or silence), then stops.
    // Returns the largest difference between output and input (the plugin should pass the track through).
    auto playTimeline = [&] (double fromSec, double toSec, const juce::AudioBuffer<float>* phraseIn, double phraseAt)
    {
        float maxDiff = 0.0f;
        playHead.playing = true;
        playHead.samples = (juce::int64) (fromSec * sr);
        juce::AudioBuffer<float> in (2, block);

        while (playHead.samples < (juce::int64) (toSec * sr))
        {
            in.clear();

            if (phraseIn != nullptr)
            {
                const juce::int64 start = (juce::int64) (phraseAt * sr);
                for (int i = 0; i < block; ++i)
                {
                    const juce::int64 k = playHead.samples + i - start;
                    if (k >= 0 && k < phraseIn->getNumSamples())
                        for (int ch = 0; ch < 2; ++ch)
                            in.setSample (ch, i, phraseIn->getSample (ch, (int) k));
                }
            }

            io.makeCopyOf (in);
            plugin->processBlock (io, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                    maxDiff = juce::jmax (maxDiff, std::abs (io.getSample (ch, i) - in.getSample (ch, i)));

            playHead.samples += block;
        }

        playHead.playing = false;   // the user presses stop
        io.clear();
        plugin->processBlock (io, midi);
        pump (700);                 // message thread collects the take and renders
        return maxDiff;
    };

    // Presses Listen (a MIDI note) with the transport stopped and records 5 s of output.
    auto listen = [&]
    {
        std::vector<float> out;
        playHead.playing = false;
        juce::MidiBuffer note;
        note.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

        for (int n = 0; n < (int) (5.0 * sr / block); ++n)
        {
            io.clear();
            plugin->processBlock (io, n == 0 ? note : midi);
            for (int i = 0; i < block; ++i)
                out.push_back (0.5f * (io.getSample (0, i) + io.getSample (1, i)));
        }

        return out;
    };

    auto lastSound = [] (const std::vector<float>& x)
    {
        for (size_t i = x.size(); i-- > 0;)
            if (std::abs (x[i]) > 1.0e-4f)
                return (double) i / sr;
        return 0.0;
    };

    //-- Pass 1: the DAW plays over a phrase at 10 s (bar 6) -> captured. ---------------------
    std::cout << "Capture on DAW playback" << std::endl;
    const auto phraseA = makePhrase (0.7);
    const float passThroughA = playTimeline (8.0, 12.0, &phraseA, 10.0);
    std::cout << "    max output-input difference while the DAW plays: " << passThroughA << std::endl;
    check (passThroughA < 1.0e-6f, "passes the track through untouched while capturing");

    io.clear();
    playHead.playing = false;
    plugin->processBlock (io, midi);
    check (io.getMagnitude (0, block) < 1.0e-6f, "silent while the transport is stopped");

    //-- Listen: swell rising into the phrase, and nothing of the phrase itself ---------------
    std::cout << "Listen" << std::endl;
    const auto a = listen();
    // Default settings: 1 bar = 2 s swell at 120 BPM, swell only (no original appended).
    const double early = rms (a, 0.2, 0.9, 0.0), late = rms (a, 1.2, 1.95, 0.0);
    const double after = rms (a, 2.02, 4.0, 0.0);
    int peakIndex = 0;
    float peak = 0.0f;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::abs (a[i]) > peak) { peak = std::abs (a[i]); peakIndex = (int) i; }
    const double peakTime = peakIndex / sr, endA = lastSound (a);
    std::cout << "    rms early swell " << early << ", late swell " << late << ", after the swell " << after
              << ", loudest at " << peakTime << " s, sound ends at " << endA << " s" << std::endl;
    check (late > early * 1.5 && late > 1.0e-3, "the swell rises");
    check (peakTime > 1.6 && peakTime <= 2.01, "the swell peaks right before the phrase would start (2 s)");
    check (endA <= 2.01 && after < 1.0e-5, "nothing of the phrase is heard after the swell");

    auto maxDifference = [] (const std::vector<float>& x, const std::vector<float>& y)
    {
        float d = 0.0f;
        for (size_t i = 0; i < juce::jmin (x.size(), y.size()); ++i)
            d = juce::jmax (d, std::abs (x[i] - y[i]));
        return d;
    };

    //-- Pass 2: the DAW plays again over a different phrase -> replaces the take. ------------
    std::cout << "Playing again replaces the take" << std::endl;
    const auto phraseB = makePhrase (1.6);
    playTimeline (18.0, 23.0, &phraseB, 20.0);
    const auto bOut = listen();
    const float changed = maxDifference (a, bOut);
    std::cout << "    max difference between the old and new swell: " << changed << std::endl;
    check (changed > 1.0e-3f, "the new playback replaced the previous take");

    //-- Pass 3: a silent playback keeps the take. ---------------------------------------------
    std::cout << "Silent playback keeps the take" << std::endl;
    playTimeline (30.0, 33.0, nullptr, 0.0);
    const float unchanged = maxDifference (bOut, listen());
    std::cout << "    max difference after a silent playback: " << unchanged << std::endl;
    check (unchanged < 1.0e-6f, "a silent playback did not wipe the take");

    auto setParamNorm = [&] (const juce::String& name, float normalised)
    {
        for (auto* param : plugin->getParameters())
            if (param->getName (64) == name)
            {
                param->setValueNotifyingHost (normalised);
                return true;
            }
        return false;
    };

    auto peakOf = [] (const std::vector<float>& x)
    {
        float m = 0.0f;
        for (auto v : x) m = juce::jmax (m, std::abs (v));
        return m;
    };

    //-- Exporting / bouncing (offline render) must not capture ---------------------------------
    std::cout << "Offline export" << std::endl;
    plugin->setNonRealtime (true);
    playTimeline (40.0, 43.0, &phraseA, 41.0);
    plugin->setNonRealtime (false);
    const float afterExport = maxDifference (bOut, listen());
    std::cout << "    difference after an offline render over another phrase: " << afterExport << std::endl;
    check (afterExport < 1.0e-6f, "an offline export/bounce does not replace the take");

    //-- Lock --------------------------------------------------------------------------------------
    std::cout << "Lock" << std::endl;
    check (setParamNorm ("Lock", 1.0f), "the host sees the Lock switch");
    pump (100);
    playTimeline (50.0, 53.0, &phraseA, 51.0);
    const float whileLocked = maxDifference (bOut, listen());
    std::cout << "    difference after playing another phrase while locked: " << whileLocked << std::endl;
    check (whileLocked < 1.0e-6f, "a locked take survives DAW playback");

    setParamNorm ("Lock", 0.0f);
    pump (100);
    playTimeline (60.0, 63.0, &phraseA, 61.0);
    const auto unlockedOut = listen();
    const float afterUnlock = maxDifference (bOut, unlockedOut);
    std::cout << "    difference after playing it again unlocked: " << afterUnlock << std::endl;
    check (afterUnlock > 1.0e-3f, "after unlocking, DAW playback captures again");

    //-- Level and Low Cut -------------------------------------------------------------------------
    std::cout << "Level and Low Cut" << std::endl;
    const float basePeak = peakOf (unlockedOut);
    const double baseRms = rms (unlockedOut, 0.5, 2.0, 0.0);
    setParamNorm ("Level", (-12.0f + 24.0f) / 30.0f);   // -12 dB on a -24..+6 dB range
    pump (400);
    const float quietPeak = peakOf (listen());
    setParamNorm ("Level", (0.0f + 24.0f) / 30.0f);
    std::cout << "    peak at 0 dB " << basePeak << ", at -12 dB " << quietPeak << " (ratio " << quietPeak / basePeak << ")" << std::endl;
    check (std::abs (quietPeak / basePeak - 0.251f) < 0.03f, "Level -12 dB makes the swell 12 dB quieter");

    setParamNorm ("Low Cut", 1.0f);   // 1 kHz
    pump (400);
    const double cutRms = rms (listen(), 0.5, 2.0, 0.0);
    setParamNorm ("Low Cut", 0.0f);   // off
    pump (400);
    std::cout << "    swell rms with low cut off " << baseRms << ", at 1 kHz " << cutRms << std::endl;
    check (cutRms < baseRms * 0.5, "Low Cut at 1 kHz removes the low end of the swell");

    //-- Effects are independent switches ----------------------------------------------------------
    std::cout << "Effect switches" << std::endl;
    auto setSwitch = [&] (const juce::String& name, bool on)
    {
        for (auto* param : plugin->getParameters())
            if (param->getName (64) == name)
            {
                param->setValueNotifyingHost (on ? 1.0f : 0.0f);
                return true;
            }
        return false;
    };

    // How deeply the swell pulses: average min/max of 20 ms RMS inside each eighth note (0.25 s).
    auto pulseDepth = [&] (const std::vector<float>& x)
    {
        double sum = 0.0;
        int cycles = 0;
        for (double c = 1.0; c + 0.25 <= 1.95; c += 0.25, ++cycles)
        {
            double lo = 1.0e9, hi = 0.0;
            for (double w = c; w + 0.02 <= c + 0.25; w += 0.02)
            {
                const double r = rms (x, w, w + 0.02, 0.0);
                lo = juce::jmin (lo, r);
                hi = juce::jmax (hi, r);
            }
            sum += hi > 0.0 ? lo / hi : 1.0;
        }
        return cycles > 0 ? sum / cycles : 1.0;
    };

    const bool switchesFound = setSwitch ("Chorus", false) && setSwitch ("Phone", false) && setSwitch ("Tremolo", false);
    check (switchesFound, "the host sees separate Phone, Chorus and Tremolo switches");
    pump (400);
    const auto plain = listen();
    setSwitch ("Tremolo", true);
    pump (400);
    const auto tremolo = listen();
    std::cout << "    pulse depth (min/max per 1/8): plain " << pulseDepth (plain) << ", tremolo " << pulseDepth (tremolo) << std::endl;
    check (pulseDepth (tremolo) < 0.15 && pulseDepth (tremolo) < pulseDepth (plain) * 0.5, "tremolo alone pulses the swell (dips at least twice as deep)");

    setSwitch ("Phone", true);
    setSwitch ("Chorus", true);
    pump (400);
    const auto all = listen();
    const float allPeak = [&] { float m = 0.0f; for (auto v : all) m = juce::jmax (m, std::abs (v)); return m; }();
    std::cout << "    all three on: peak " << allPeak << ", difference from tremolo only " << maxDifference (all, tremolo)
              << ", pulse depth " << pulseDepth (all) << std::endl;
    check (allPeak > 0.01f && maxDifference (all, tremolo) > 1.0e-3f && pulseDepth (all) < 0.5,
           "Phone, Chorus and Tremolo work together (tremolo stays on)");

    plugin->releaseResources();
    plugin.reset();

    //-- Disk: one take file per instance -------------------------------------------------------------
    const int takeFilesAfter = captureDir.getNumberOfChildFiles (juce::File::findFiles, "take_*.wav");
    std::cout << "Take files" << std::endl << "    before " << takeFilesBefore << ", after " << takeFilesAfter
              << " (this run captured 3 takes)" << std::endl;
    check (takeFilesAfter - takeFilesBefore <= 1, "repeated captures keep only one take file per instance");
    pump (100);

    std::cout << (failures == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
