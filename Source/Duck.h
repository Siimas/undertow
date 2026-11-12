#pragma once
#include <cmath>

// Volume for a position in the cycle (phase 0..1): fully ducked at the start,
// back to full volume by the end. curve < 1 recovers fast, > 1 recovers slow.
// Total on purpose: values come from saved project state, so bad input returns
// unity gain or is clamped, and the result is always finite and in 0..1.
inline float duckGain (float phase, float curve, float depth)
{
    if (! (std::isfinite (phase) && std::isfinite (curve) && std::isfinite (depth)))
        return 1.0f;

    phase = std::fmin (std::fmax (phase, 0.0f), 1.0f);
    curve = std::fmin (std::fmax (curve, 0.25f), 4.0f);
    depth = std::fmin (std::fmax (depth, 0.0f), 1.0f);
    return 1.0f - depth * (1.0f - std::pow (phase, curve));
}
