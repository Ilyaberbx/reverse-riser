#pragma once

#include "BarAlignment.h"
#include <algorithm>

/**
    Where the rendered clip sits inside an exported WAV, and how long the file is.

    The clip is padded with silence in front so it starts on a bar line (see BarAlignment.h). The
    file ends where the clip ends: no silent tail after the swell. Its length is rarely a whole
    number of bars, so it is tagged as a one-shot and should be played unwarped (in Ableton: Warp
    off), which keeps it at the speed it was rendered at for the project tempo.
*/
struct ExportLayout
{
    int padStart = 0;        // silence before the clip
    int totalSamples = 0;    // length of the file
    double beats = 0.0;      // file length in beats at the project tempo
};

inline ExportLayout layoutExport (int clipSamples, const BarAlignment& align, double bpm, double sampleRate)
{
    ExportLayout l;
    l.padStart = align.valid ? std::max (0, align.padSamples) : 0;
    l.totalSamples = l.padStart + clipSamples;

    if (bpm > 0.0 && sampleRate > 0.0)
        l.beats = l.totalSamples / sampleRate * bpm / 60.0;

    return l;
}
