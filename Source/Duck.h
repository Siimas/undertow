#pragma once
#include <cmath>

// Volume for a position in the cycle (phase 0..1): fully ducked at the start,
// back to full volume by the end. curve < 1 recovers fast, > 1 recovers slow.
inline float duckGain (float phase, float curve, float depth)
{
    return 1.0f - depth * (1.0f - std::pow (phase, curve));
}
