#pragma once

#include "PhyllonAnalyser.h"
#include "Theme.h"

// Peer Play's Phýllon, refined: a faint sunflower field of phyllotaxis seeds (inner = bass, outer = treble)
// that breathes with the bands, and 8 fine trails (one per band) gliding along its spiral arms.
// A trail steps a Fibonacci number of seeds (n -> n+21), which lands on the neighbouring seed of the
// same parastichy arm, so motion follows the arms instead of cutting 137.5° chords across the field.
class PhyllonView
{
public:
    static constexpr float goldenAngleDegrees = 137.508f;

    // seed n of a phyllotaxis spiral with spacing c, around the origin
    static juce::Point<float> phyllotaxis (float n, float c);

    PhyllonView();

    void advance (const PhyllonAnalyser& analyser, float seconds);
    // once per frame: redraws the quarter-resolution glow layer (screen colour, washes, halos)
    void render (int width, int height);
    // the screen: soft glow layer stretched over `area`, then crisp trails and heads on top
    void paint (juce::Graphics& g, juce::Rectangle<float> area) const;
    // light that spills out around the screen, behind it
    void paintHalo (juce::Graphics& g, juce::Rectangle<float> area, float spread) const;

private:
    static constexpr int historyLength = 44;
    static constexpr int maxIteration = 420;
    static constexpr int armStep = 21; // Fibonacci: next seed on the same spiral arm

    struct Trail
    {
        int band = 0;
        float n = 1.0f, t = 0.0f, direction = 1.0f;
        float energy = 0.0f, scale = 1.0f;
        std::array<juce::Point<float>, historyLength> history {};
        int historySize = 0, historyHead = 0;
    };

    static juce::Point<float> historyAt (const Trail& trail, int age);
    void drawTrails (juce::Graphics& g, juce::Rectangle<float> area, float widthScale) const;

    std::array<Trail, PhyllonAnalyser::numBands> trails;
    std::array<float, PhyllonAnalyser::numBands> bandEnergy {};
    float rotation = 0.0f, fieldScale = 1.0f;

    static constexpr int layerDownscale = 4;
    juce::Image glowLayer;
};
