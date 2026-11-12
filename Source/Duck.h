#pragma once
#include <cmath>
#include <iterator>

// A ducking shape over one cycle (phase 0..1): silent until `hold`, then rises to
// full volume at `end`. power < 1 recovers fast, > 1 slow. `smooth` eases both ends.
// `repeats` plays the shape that many times per cycle.
struct Shape
{
    const char* name;
    float hold, end, power;
    bool smooth;
    int repeats;
};

inline constexpr Shape shapes[] = {
    { "Classic",    0.00f, 1.00f, 1.0f, false, 1 },
    { "Punchy",     0.00f, 0.50f, 1.0f, false, 1 },
    { "Tight",      0.00f, 0.25f, 1.0f, false, 1 },
    { "Snappy",     0.00f, 0.50f, 0.5f, false, 1 },
    { "Bounce",     0.00f, 1.00f, 0.5f, false, 1 },
    { "Lazy",       0.00f, 1.00f, 2.0f, false, 1 },
    { "Deep",       0.00f, 1.00f, 3.5f, false, 1 },
    { "Late",       0.10f, 0.90f, 2.0f, false, 1 },
    { "Hold",       0.15f, 0.75f, 1.0f, false, 1 },
    { "Long Hold",  0.30f, 1.00f, 1.0f, false, 1 },
    { "Smooth",     0.00f, 1.00f, 1.0f, true,  1 },
    { "Smooth Short", 0.00f, 0.60f, 1.0f, true, 1 },
    { "Gate",       0.50f, 0.52f, 1.0f, false, 1 },
    { "Stab",       0.75f, 0.77f, 1.0f, false, 1 },
    { "Double",     0.00f, 1.00f, 1.0f, false, 2 },
    { "Triplet",    0.00f, 1.00f, 1.0f, false, 3 },
};
inline constexpr int numShapes = (int) std::size (shapes);

// Volume for a shape at a cycle position, scaled by mix (0 = untouched, 1 = full duck).
// Total on purpose: values come from saved project state, so anything out of range
// (or NaN) is clamped or returns unity gain. Phase >= 1 means "cycle finished".
inline float duckGain (int shape, float phase, float mix)
{
    if (! std::isfinite (mix) || ! (phase >= 0.0f) || phase >= 1.0f)
        return 1.0f;

    mix = std::fmin (std::fmax (mix, 0.0f), 1.0f);
    const Shape& s = shapes[shape < 0 ? 0 : shape >= numShapes ? numShapes - 1 : shape];

    float p = phase * (float) s.repeats;
    p -= std::floor (p);

    float g = 1.0f;
    if (p < s.hold)
        g = 0.0f;
    else if (p < s.end)
    {
        g = std::pow ((p - s.hold) / (s.end - s.hold), s.power);
        if (s.smooth)
            g = g * g * (3.0f - 2.0f * g);
    }
    return 1.0f - mix * (1.0f - g);
}
