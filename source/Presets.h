#pragma once

#include "ReverbEngine.h"
#include <array>

// The spaces the riser's reverse reverb can use (REVERB dropdown)
struct ReverbType
{
    const char* name;
    ReverbParams params;
};

// clang-format off
inline const std::array<ReverbType, 8> reverbTypes { {
    //                     size  decay  pre  damp diff rate   depth width shim spring lofi
    { "Small Room",      { 0.25f, 0.5f,   5, 0.45f, 0.70f, 0.6f,  0.15f, 0.8f, 0.0f, 0.0f,  0.0f } },
    { "Medium Room",     { 0.45f, 1.0f,  12, 0.40f, 0.75f, 0.5f,  0.20f, 0.9f, 0.0f, 0.0f,  0.0f } },
    { "Chamber",         { 0.55f, 1.6f,  18, 0.30f, 0.85f, 0.4f,  0.25f, 1.0f, 0.0f, 0.0f,  0.0f } },
    { "Concert Hall",    { 0.85f, 3.2f,  30, 0.35f, 0.80f, 0.3f,  0.30f, 1.0f, 0.0f, 0.0f,  0.0f } },
    { "Ambient Pad",     { 1.00f, 12.0f, 60, 0.50f, 0.90f, 0.2f,  0.60f, 1.0f, 0.0f, 0.0f,  0.0f } },
    { "Shimmer Heaven",  { 0.95f, 10.0f, 40, 0.25f, 0.85f, 0.25f, 0.45f, 1.0f, 0.6f, 0.0f,  0.0f } },
    { "Spring Tank",     { 0.30f, 2.2f,   0, 0.55f, 0.40f, 0.9f,  0.10f, 0.4f, 0.0f, 0.85f, 0.0f } },
    { "Lo-fi Tape Verb", { 0.60f, 2.5f,  20, 0.60f, 0.70f, 0.4f,  0.35f, 0.7f, 0.0f, 0.0f,  0.7f } },
} };
// clang-format on

// used when the REVERB toggle is off
inline constexpr int defaultReverbType = 3; // Concert Hall

inline constexpr std::array<const char*, 5> lengthNames { "1/4 Bar", "1/2 Bar", "1 Bar", "2 Bars", "4 Bars" };
inline constexpr std::array<float, 5> lengthBars { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
inline constexpr std::array<const char*, 4> tremoloTimingNames { "1/4", "1/8", "1/16", "1/32" };
inline constexpr std::array<float, 4> tremoloTimingBeats { 1.0f, 0.5f, 0.25f, 0.125f };

// Order of the values stored in each riser preset (choices as indices, toggles as 0/1)
inline constexpr std::array<const char*, 14> riserParamIds {
    "length", "fx", "reverbOn", "reverbType", "chorusOn", "chorusMix", "chorusRate",
    "tremoloOn", "tremoloMix", "tremoloTiming", "pitchOn", "pitchStart", "pitchEnd", "pitchShape"
};

namespace Param
{
    enum Index { length, fx, reverbOn, reverbType, chorusOn, chorusMix, chorusRate, tremoloOn, tremoloMix, tremoloTiming, pitchOn, pitchStart, pitchEnd, pitchShape };
}

struct RiserPreset
{
    const char* name;
    std::array<float, riserParamIds.size()> values;
};

// clang-format off
inline constexpr std::array<RiserPreset, 4> riserPresets { {
    //              len fx  rvOn type chOn chMix chRate trOn trMix trTime piOn  start end  shape
    { "Default", { 2,  1,  0,   3,   0,   100,  2.0f,  0,   50,   1,     0,   -12,   0,   0 } },
    { "Retro",   { 2,  1,  1,   7,   0,   100,  2.0f,  1,   60,   1,     0,   -12,   0,   0 } },
    { "Wide",    { 3,  1,  1,   4,   1,   100,  0.8f,  0,   50,   1,     0,   -12,   0,   0 } },
    { "Dreamy",  { 3,  1,  1,   5,   1,    60,  0.5f,  0,   50,   1,     1,   -12,   0,   0.5f } },
} };
// clang-format on
