#pragma once
#include <cmath>
#include <iterator>

// One ducking shape over a cycle (phase 0..1). The volume:
//   falls to silence over `fall`, stays silent until `hold`, rises to full by `end`
//   (power < 1 rises fast, > 1 slow; `smooth` makes the rise an S-curve),
//   then, from `dipStart`, eases down to `dipTo` to anticipate the next hit.
// `reverse` plays the shape backwards; `repeats` plays it that many times per cycle.
struct Shape
{
    const char* name;
    float fall, hold, end, power;
    bool smooth;
    float dipStart, dipTo; // dipStart 1 = no dip
    bool reverse;
    int repeats;
};

inline constexpr Shape shapes[] = {
    //  name          fall   hold   end    power smooth dipStart dipTo reverse repeats
    { "Classic",      0.02f, 0.05f, 0.55f, 1.0f, true,  1.00f, 1.00f, false, 1 },
    { "Punchy",       0.01f, 0.03f, 0.35f, 1.0f, true,  1.00f, 1.00f, false, 1 },
    { "Tight",        0.01f, 0.02f, 0.20f, 1.0f, true,  1.00f, 1.00f, false, 1 },
    { "Snappy",       0.00f, 0.00f, 0.40f, 0.5f, false, 1.00f, 1.00f, false, 1 },
    { "Deep",         0.02f, 0.15f, 0.75f, 1.0f, true,  1.00f, 1.00f, false, 1 },
    { "Lazy",         0.03f, 0.05f, 0.95f, 1.5f, true,  1.00f, 1.00f, false, 1 },
    { "Hold",         0.02f, 0.25f, 0.70f, 1.0f, true,  1.00f, 1.00f, false, 1 },
    { "Long Hold",    0.02f, 0.40f, 0.90f, 1.0f, true,  1.00f, 1.00f, false, 1 },
    { "Anticipate",   0.03f, 0.06f, 0.50f, 1.0f, true,  0.86f, 0.25f, false, 1 },
    { "Soft Dip",     0.05f, 0.07f, 0.55f, 1.0f, true,  0.75f, 0.55f, false, 1 },
    { "Linear",       0.00f, 0.00f, 1.00f, 1.0f, false, 1.00f, 1.00f, false, 1 },
    { "Slow",         0.00f, 0.00f, 1.00f, 2.5f, false, 1.00f, 1.00f, false, 1 },
    { "Gate",         0.00f, 0.50f, 0.52f, 1.0f, false, 1.00f, 1.00f, false, 1 },
    { "Fade Out",     0.00f, 0.05f, 0.70f, 1.0f, true,  1.00f, 1.00f, true,  1 },
    { "Double",       0.02f, 0.05f, 0.60f, 1.0f, true,  1.00f, 1.00f, false, 2 },
    { "Triplet",      0.02f, 0.05f, 0.65f, 1.0f, true,  1.00f, 1.00f, false, 3 },
};
inline constexpr int numShapes = (int) std::size (shapes);

inline float easeInOut (float t) { return t * t * (3.0f - 2.0f * t); }

// The bare shape: volume 0..1 at phase 0..1.
inline float shapeGain (const Shape& s, float p)
{
    p *= (float) s.repeats;
    p -= std::floor (p);
    if (s.reverse)
        p = 1.0f - p;

    float g = 1.0f;
    if (p < s.fall)
        g = (s.dipStart < 1.0f ? s.dipTo : 1.0f) * (1.0f - easeInOut (p / s.fall)); // continues from where the last cycle ended
    else if (p < s.hold)
        g = 0.0f;
    else if (p < s.end)
    {
        g = std::pow ((p - s.hold) / (s.end - s.hold), s.power);
        if (s.smooth)
            g = easeInOut (g);
    }

    if (p > s.dipStart)
        g *= 1.0f - (1.0f - s.dipTo) * easeInOut ((p - s.dipStart) / (1.0f - s.dipStart));
    return g;
}

// Volume for a shape at a cycle position, scaled by mix (0 = untouched, 1 = full duck).
// Total on purpose: values come from saved project state, so anything out of range
// (or NaN) is clamped or returns unity gain. Phase >= 1 means "cycle finished".
inline float duckGain (int shape, float phase, float mix)
{
    if (! std::isfinite (mix) || ! (phase >= 0.0f) || phase >= 1.0f)
        return 1.0f;

    mix = std::fmin (std::fmax (mix, 0.0f), 1.0f);
    const float g = shapeGain (shapes[shape < 0 ? 0 : shape >= numShapes ? numShapes - 1 : shape], phase);
    return 1.0f - mix * (1.0f - std::fmin (std::fmax (g, 0.0f), 1.0f));
}

// Slides the shape within the cycle: a positive shift (fraction of a cycle) makes the duck land later.
inline float shiftedPhase (float position, float shift)
{
    if (! std::isfinite (position) || ! std::isfinite (shift))
        return 1.0f; // "finished": bad input leaves the audio untouched rather than silencing it
    const float p = position - shift;
    const float wrapped = p - std::floor (p);
    return wrapped >= 0.0f && wrapped < 1.0f ? wrapped : 0.0f; // a tiny negative p rounds up to exactly 1.0
}
