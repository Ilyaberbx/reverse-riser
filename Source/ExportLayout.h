#pragma once

#include "BarAlignment.h"
#include <algorithm>
#include <cmath>

/**
    Where the rendered clip sits inside an exported WAV, and how long the file is.

    The clip is padded with silence in front so it starts on a bar line (see BarAlignment.h), and
    with silence after it so the file is a whole power-of-two number of bars (1, 2, 4, 8, 16) at
    the project tempo. DAWs that infer a dropped file's tempo from its length (Ableton's
    auto-warp guesses how many bars it spans) then read the exact project tempo and play it
    1:1, instead of guessing a nearby tempo and time-stretching it.
*/
struct ExportLayout
{
    int padStart = 0;        // silence before the clip
    int totalSamples = 0;    // length of the file
    double bars = 0.0;       // file length in bars at the project tempo
};

inline ExportLayout layoutExport (int clipSamples, const BarAlignment& align, double bpm, double beatsPerBar, double sampleRate)
{
    ExportLayout l;
    l.padStart = align.valid ? std::max (0, align.padSamples) : 0;
    l.totalSamples = l.padStart + clipSamples;

    const double samplesPerBar = beatsPerBar * 60.0 / bpm * sampleRate;

    if (! (samplesPerBar > 0.0))
        return l;

    // Round up to the next power-of-two bar count that holds the padded clip.
    int bars = 1;
    while (bars * samplesPerBar < l.totalSamples - 0.5 && bars < (1 << 12))
        bars *= 2;

    l.bars = bars;
    l.totalSamples = std::max (l.totalSamples, (int) std::llround (bars * samplesPerBar));
    return l;
}
