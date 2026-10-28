#pragma once

#include <array>

// Order of the continuous parameters stored in each preset (Freeze is always off in presets)
inline constexpr std::array<const char*, 13> presetParamIds {
    "size", "decay", "predelay", "mix", "damping", "diffusion", "modrate", "moddepth", "width", "shimmer", "spring", "lofi", "output"
};

// indices into presetParamIds / Preset::values
namespace Param
{
    enum Index { size, decay, predelay, mix, damping, diffusion, modrate, moddepth, width, shimmer, spring, lofi, output };
}

struct Preset
{
    const char* name;
    std::array<float, 13> values;
};

// gets the built-in VocalChain (delay, doubler, EQ, de-esser, compression) on top of its knob values
inline constexpr int lilUziVertPreset = 0;

// clang-format off
inline constexpr std::array<Preset, 9> factoryPresets { {
    //                      size  decay  pre  mix  damp diff rate  depth width shim spring lofi out
    { "Lil Uzi Vert",    { 90,   4.5f,  70,  35,  15,  90, 0.35f, 40,  100,  30,   0,     0, -2 } }, // Luv Is Rage 2: bright, wide wash behind dry, upfront vocals
    { "Small Room",      { 25,   0.5f,   5,  25,  45,  70, 0.6f,  15,   80,   0,   0,     0,  0 } },
    { "Medium Room",     { 45,   1.0f,  12,  28,  40,  75, 0.5f,  20,   90,   0,   0,     0,  0 } },
    { "Chamber",         { 55,   1.6f,  18,  30,  30,  85, 0.4f,  25,  100,   0,   0,     0,  0 } },
    { "Concert Hall",    { 85,   3.2f,  30,  32,  35,  80, 0.3f,  30,  100,   0,   0,     0,  0 } },
    { "Ambient Pad",     { 100, 12.0f,  60,  45,  50,  90, 0.2f,  60,  100,   0,   0,     0, -3 } },
    { "Shimmer Heaven",  { 95,  10.0f,  40,  45,  25,  85, 0.25f, 45,  100,  60,   0,     0, -3 } },
    { "Spring Tank",     { 30,   2.2f,   0,  30,  55,  40, 0.9f,  10,   40,   0,  85,     0,  0 } },
    { "Lo-fi Tape Verb", { 60,   2.5f,  20,  40,  60,  70, 0.4f,  35,   70,   0,   0,    70,  0 } },
} };
// clang-format on
