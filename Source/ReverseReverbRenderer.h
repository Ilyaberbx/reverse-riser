#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <utility>
#include <vector>

/** Everything the renderer needs to turn a segment of audio into a reverse-reverb clip. */
struct ReverseReverbSettings
{
    double    sampleRate      = 44100.0;  // sample rate of the rendered output
    double    bpm             = 120.0;    // used by the tremolo mode
    float     tailSeconds     = 2.0f;     // length of the swell that precedes the sound
    float     preDelayMs      = 20.0f;    // silence between the end of the swell and the original
    float     diffusion       = 0.7f;     // 0..1, density of the allpass pre-diffuser
    float     fade            = 1.0f;     // 0..1, portion of the swell covered by a fade-in
    float     roomSize        = 0.8f;     // 0..1, Freeverb room size
    float     damping         = 0.4f;     // 0..1
    float     width           = 1.0f;     // 0..1
    float     wetGain         = 1.0f;     // level of the swell
    float     dryGain         = 1.0f;     // level of the original segment
    bool      includeOriginal = true;     // append the untouched segment after the swell
    float     edgeFadeMs      = 5.0f;     // fade applied to the cut segment edges (click removal)

    // Swell character: each effect is switched on or off on its own (applied in this order)
    bool      phoneOn         = false;
    bool      chorusOn        = true;
    bool      tremoloOn       = false;
    float     phoneMix        = 1.0f;     // 0..1
    float     phoneTone       = 0.5f;     // 0..1, moves the telephone band from dark to bright
    float     chorusMix       = 0.6f;     // 0..1
    float     chorusRateHz    = 0.8f;     // LFO rate of the chorus voices
    float     tremoloMix      = 1.0f;     // 0..1, depth of the tremolo
    double    tremoloBeats    = 0.5;      // length of one tremolo cycle in quarter-note beats

    // Automatic sibilance detection: tames "s"/"sh" energy in the signal that feeds the reverb.
    bool      sibilanceControl = true;
};

/** A rendered clip plus the information needed to show where the original sits inside it. */
struct RenderedAudio
{
    juce::AudioBuffer<float> buffer;          // the processed clip (swell + optional original), stereo
    juce::AudioBuffer<float> swellOnly;       // the same clip without the original, for in-sync playback
    juce::AudioBuffer<float> dry;             // the selected segment at the output rate, stereo
    double sampleRate = 44100.0;
    int originalStartSample = 0;              // where the original segment starts (or would start)
    int originalLengthSamples = 0;
    int swellLengthSamples = 0;               // samples before the original starts
    bool includesOriginal = false;

    /** Sibilant passages found in the segment, in seconds from the segment start. */
    std::vector<std::pair<double, double>> sibilantRegions;

    /** Timeline placement (set by the processor when the source came from the DAW track). */
    bool hasHostPosition = false;
    juce::int64 hostClipStartSample = 0;      // host sample position of buffer[0]
};

class ReverseReverbRenderer
{
public:
    /** Renders the reverse-reverb effect for [startSample, startSample + numSamples) of source.
        Runs offline and is safe to call from any non-realtime thread. */
    static std::shared_ptr<RenderedAudio> render (const juce::AudioBuffer<float>& source,
                                                  double sourceSampleRate,
                                                  int startSample,
                                                  int numSamples,
                                                  const ReverseReverbSettings& settings);

    /** Finds sibilant passages ("s", "sh", "t") in a buffer. Returns [start, end) sample ranges. */
    static std::vector<std::pair<int, int>> detectSibilance (const juce::AudioBuffer<float>& buffer, double sampleRate);
};
