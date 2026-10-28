#include "PhyllonView.h"

namespace
{
    // per-trail feel: arm steps per second at silence / full band, spiral breathing range
    constexpr float minSpeed = 1.4f, maxSpeed = 9.0f;
    constexpr float minScale = 0.9f, maxScale = 1.08f;
    // the field's spacing is designed for a ~240 px radius; dots and strokes scale from there
    constexpr float designRadius = 240.0f;

    // shortest signed angle from a to b
    float angleBetween (float a, float b)
    {
        return std::remainder (b - a, juce::MathConstants<float>::twoPi);
    }
}

juce::Point<float> PhyllonView::phyllotaxis (float n, float c)
{
    const auto angle = juce::degreesToRadians (n * goldenAngleDegrees);
    const auto radius = c * std::sqrt (n);
    return { radius * std::cos (angle), radius * std::sin (angle) };
}

PhyllonView::PhyllonView()
{
    // spread the trails across radii and onto different arms
    for (int i = 0; i < (int) trails.size(); ++i)
    {
        trails[(size_t) i].band = i;
        trails[(size_t) i].n = 1.0f + (float) i * 53.0f;
    }
}

void PhyllonView::advance (const PhyllonAnalyser& analyser, float seconds)
{
    rotation += seconds * (0.04f + 0.35f * analyser.amplitudeBuffer());
    fieldScale = juce::jmap (analyser.amplitudeBuffer(), 0.94f, 1.04f);
    for (int i = 0; i < PhyllonAnalyser::numBands; ++i)
        bandEnergy[(size_t) i] = analyser.bandBuffer (i);

    for (auto& trail : trails)
    {
        trail.energy = analyser.bandBuffer (trail.band);
        trail.t += seconds * juce::jmap (trail.energy, minSpeed, maxSpeed);
        while (trail.t >= 1.0f)
        {
            trail.t -= 1.0f;
            trail.n += trail.direction * (float) armStep;
            const auto next = trail.n + trail.direction * (float) armStep;
            if (next > (float) maxIteration || next < 1.0f)
                trail.direction = -trail.direction;
        }

        // ease along the arm in polar coordinates, so the path curves with the spiral
        // (spiral normalised so seed maxIteration sits at radius 1)
        trail.scale = juce::jmap (trail.energy, minScale, maxScale);
        const auto c = trail.scale / std::sqrt ((float) maxIteration);
        const auto from = phyllotaxis (trail.n, c);
        const auto to = phyllotaxis (trail.n + trail.direction * (float) armStep, c);
        const auto fromAngle = std::atan2 (from.y, from.x);
        const auto angle = fromAngle + trail.t * angleBetween (fromAngle, std::atan2 (to.y, to.x));
        const auto r = juce::jmap (trail.t, from.getDistanceFromOrigin(), to.getDistanceFromOrigin());
        const auto head = juce::Point<float> (r * std::cos (angle), r * std::sin (angle)).rotatedAboutOrigin (rotation);

        trail.historyHead = (trail.historyHead + 1) % historyLength;
        trail.history[(size_t) trail.historyHead] = head;
        trail.historySize = juce::jmin (trail.historySize + 1, historyLength);
    }
}

juce::Point<float> PhyllonView::historyAt (const Trail& trail, int age)
{
    return trail.history[(size_t) ((trail.historyHead - age + historyLength) % historyLength)];
}

void PhyllonView::drawTrails (juce::Graphics& g, juce::Rectangle<float> area, float widthScale) const
{
    const auto centre = area.getCentre();
    const auto radius = 0.5f * juce::jmax (area.getWidth(), area.getHeight());
    const auto s = widthScale * radius / designRadius;

    // fine strokes, tapering and fading fast; quiet bands nearly vanish
    constexpr int segmentsPerStroke = 4;
    for (const auto& trail : trails)
    {
        const auto colour = Theme::glow[(size_t) trail.band];
        for (int oldest = trail.historySize - 1; oldest > 0; oldest -= segmentsPerStroke)
        {
            const auto newest = juce::jmax (0, oldest - segmentsPerStroke);
            juce::Path stroke;
            stroke.startNewSubPath (centre + historyAt (trail, oldest) * radius);
            for (int age = oldest - 1; age >= newest; --age)
                stroke.lineTo (centre + historyAt (trail, age) * radius);

            const auto freshness = 1.0f - 0.5f * (float) (oldest + newest) / (float) historyLength;
            g.setColour (colour.withAlpha (freshness * freshness * (0.22f + 0.73f * trail.energy)));
            g.strokePath (stroke, juce::PathStrokeType (s * (0.8f + 2.4f * freshness * (0.35f + trail.energy)),
                                      juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
}

void PhyllonView::render (int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    const auto layerWidth = juce::jmax (1, width / layerDownscale), layerHeight = juce::jmax (1, height / layerDownscale);
    if (glowLayer.getWidth() != layerWidth || glowLayer.getHeight() != layerHeight)
        glowLayer = juce::Image (juce::Image::ARGB, layerWidth, layerHeight, true);

    // everything soft lives in the low-res layer: base, washes, head halos
    {
        juce::Graphics g (glowLayer);
        g.fillAll (Theme::palette().screen);

        const auto area = glowLayer.getBounds().toFloat();
        const auto centre = area.getCentre();
        const auto radius = 0.5f * juce::jmax (area.getWidth(), area.getHeight());
        const auto pixel = 1.0f / (float) layerDownscale;
        const auto softGlow = [&] (juce::Point<float> at, juce::Colour colour, float alpha, float size) {
            g.setGradientFill ({ colour.withAlpha (alpha), at, colour.withAlpha (0.0f), at.translated (size, 0.0f), true });
            g.fillEllipse (juce::Rectangle<float> (size * 2.0f, size * 2.0f).withCentre (at));
        };

        for (const auto& trail : trails)
            if (trail.historySize > 0)
                softGlow (centre + historyAt (trail, 0) * radius, Theme::glow[(size_t) trail.band],
                    0.04f + 0.16f * trail.energy, radius * (0.10f + 0.20f * trail.energy));

        for (const auto& trail : trails)
            if (trail.historySize > 0)
                softGlow (centre + historyAt (trail, 0) * radius, Theme::glow[(size_t) trail.band],
                    0.3f + 0.45f * trail.energy, pixel * (10.0f + 18.0f * trail.energy));
    }
}

void PhyllonView::paint (juce::Graphics& g, juce::Rectangle<float> area) const
{
    if (glowLayer.isValid())
        g.drawImage (glowLayer, area, juce::RectanglePlacement::stretchToFit);

    const auto centre = area.getCentre();
    const auto radius = 0.5f * juce::jmax (area.getWidth(), area.getHeight());
    const auto s = radius / designRadius;

    // the sunflower field: every seed, coloured by the band its radius belongs to, swelling with it
    {
        std::array<juce::Path, PhyllonAnalyser::numBands> dots;
        const auto c = fieldScale / std::sqrt ((float) maxIteration);
        for (int n = 1; n <= maxIteration; ++n)
        {
            const auto band = juce::jmin (PhyllonAnalyser::numBands - 1, (int) (std::sqrt ((float) n / (float) maxIteration) * PhyllonAnalyser::numBands));
            const auto size = s * (1.7f + 2.6f * bandEnergy[(size_t) band]);
            const auto at = centre + phyllotaxis ((float) n, c).rotatedAboutOrigin (rotation) * radius;
            dots[(size_t) band].addEllipse (juce::Rectangle<float> (size, size).withCentre (at));
        }
        for (size_t band = 0; band < dots.size(); ++band)
        {
            g.setColour (Theme::glow[band].withAlpha (0.3f + 0.5f * bandEnergy[band]));
            g.fillPath (dots[band]);
        }
    }

    drawTrails (g, area, 1.0f);

    // crisp head cores
    for (const auto& trail : trails)
    {
        if (trail.historySize == 0)
            continue;
        const auto size = s * (2.5f + 3.0f * trail.energy);
        g.setColour (Theme::glow[(size_t) trail.band].brighter (0.3f));
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (centre + historyAt (trail, 0) * radius));
    }
}

void PhyllonView::paintHalo (juce::Graphics& g, juce::Rectangle<float> area, float spread) const
{
    const auto centre = area.getCentre();
    const auto radius = 0.5f * juce::jmax (area.getWidth(), area.getHeight());
    for (const auto& trail : trails)
    {
        if (trail.historySize == 0 || trail.energy <= 0.01f)
            continue;
        // pin the light to the screen edge nearest the head, so it spills outwards
        const auto at = area.getConstrainedPoint (centre + historyAt (trail, 0) * radius);
        const auto colour = Theme::glow[(size_t) trail.band];
        const auto size = spread * (0.6f + 0.8f * trail.energy);
        g.setGradientFill ({ colour.withAlpha (0.2f * trail.energy), at, colour.withAlpha (0.0f), at.translated (size, 0.0f), true });
        g.fillEllipse (juce::Rectangle<float> (size * 2.0f, size * 2.0f).withCentre (at));
    }
}
