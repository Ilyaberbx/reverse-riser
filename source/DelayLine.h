#pragma once

#include <juce_core/juce_core.h>

// Power-of-two circular buffer with integer, linear and cubic reads
class DelayLine
{
public:
    void prepare (int maxDelay)
    {
        const auto size = juce::nextPowerOfTwo (maxDelay + 4);
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void clear() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    void push (float x)
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    // sample pushed `delay` pushes ago, delay >= 1
    float read (int delay) const { return buffer[(size_t) ((writePos - delay) & mask)]; }

    float readLinear (float delay) const
    {
        const auto i = (int) delay;
        const auto f = delay - (float) i;
        const auto a = read (i);
        return a + f * (read (i + 1) - a);
    }

    // cubic Hermite, delay >= 2
    float readHermite (float delay) const
    {
        const auto i = (int) delay;
        const auto f = delay - (float) i;
        const auto ym1 = read (i - 1), y0 = read (i), y1 = read (i + 1), y2 = read (i + 2);
        const auto c1 = 0.5f * (y1 - ym1);
        const auto c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const auto c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * f + c2) * f + c1) * f + y0;
    }

private:
    std::vector<float> buffer;
    int mask = 0;
    int writePos = 0;
};
