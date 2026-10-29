#pragma once

#include <cmath>

/**
    Where an exported clip should start so it can be dropped on a bar line.

    The clip is padded with silence at the front so that its first sample falls exactly on the
    bar line before the swell begins. Dropped on that bar with grid snapping on, the swell then
    lands sample-accurately in front of the phrase.
*/
struct BarAlignment
{
    bool valid = false;
    int bar = 0;           // 1-based bar number to drop the clip on
    int padSamples = 0;    // silence to put in front of the clip
};

/** clipStartPpq: position of the clip's first sample in quarter notes from the song start. */
inline BarAlignment alignToBar (double clipStartPpq, double bpm, double beatsPerBar, double sampleRate)
{
    BarAlignment a;

    if (bpm <= 0.0 || beatsPerBar <= 0.0 || sampleRate <= 0.0)
        return a;

    const double barIndex = std::floor (clipStartPpq / beatsPerBar + 1.0e-9);
    const double offsetBeats = clipStartPpq - barIndex * beatsPerBar;

    a.valid = true;
    a.bar = (int) barIndex + 1;
    a.padSamples = (int) std::llround (offsetBeats * 60.0 / bpm * sampleRate);
    return a;
}
