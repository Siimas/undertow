#undef NDEBUG
#include <cassert>
#include "Duck.h"
#include "Version.h"

int main()
{
    assert (numShapes == 16);
    assert (duckGain (0, 0.03f, 1.0f) == 0.0f);   // Classic: silent just after the hit...
    assert (duckGain (0, 0.80f, 1.0f) == 1.0f);   // ...and fully back well before the next one
    assert (duckGain (0, 1.0f, 1.0f) == 1.0f);    // cycle finished: full volume
    assert (duckGain (0, 0.03f, 0.5f) == 0.5f);   // half mix: half duck
    assert (duckGain (12, 0.25f, 1.0f) == 0.0f);  // Gate: silent first half...
    assert (duckGain (12, 0.75f, 1.0f) == 1.0f);  // ...open second half
    assert (duckGain (14, 0.53f, 1.0f) < 0.1f);   // Double: ducks again mid-cycle
    assert (duckGain (13, 0.02f, 1.0f) == 1.0f);  // Fade Out is reversed: starts open...
    assert (duckGain (13, 0.97f, 1.0f) == 0.0f);  // ...ends silent
    assert (duckGain (8, 0.999f, 1.0f) < 0.3f);   // Anticipate dips before the next hit

    for (int s = 0; s < numShapes; ++s)
    {
        float low = 1.0f, high = 0.0f, previous = duckGain (s, 0.0f, 1.0f), biggestStep = 0.0f;
        for (int i = 0; i < 4000; ++i)
        {
            const float g = duckGain (s, (float) i / 4000.0f, 1.0f);
            assert (g >= 0.0f && g <= 1.0f);              // always a valid volume
            assert (duckGain (s, (float) i / 4000.0f, 0.0f) == 1.0f); // mix 0 never touches the audio
            low = std::fmin (low, g);
            high = std::fmax (high, g);
            biggestStep = std::fmax (biggestStep, std::fabs (g - previous));
            previous = g;
        }
        assert (low == 0.0f && high > 0.99f);             // every shape fully ducks and fully recovers

        // Shapes with a soft fall must loop without a jump (no click at the cycle boundary)
        if (shapes[s].fall > 0.0f)
            assert (std::fabs (duckGain (s, 0.0f, 1.0f) - duckGain (s, 0.9999f, 1.0f)) < 0.02f && biggestStep < 0.05f * (float) shapes[s].repeats);
    }

    assert (duckGain (0, NAN, 1.0f) == 1.0f);     // bad input: leave audio untouched
    assert (duckGain (0, 0.0f, NAN) == 1.0f);
    assert (duckGain (0, 0.03f, 99.0f) == 0.0f);  // out-of-range mix is clamped
    assert (duckGain (999, 0.01f, 1.0f) == 0.0f); // out-of-range shape is clamped to the last one, no crash
    assert (duckGain (-5, 0.03f, 1.0f) == 0.0f);

    assert (shiftedPhase (0.0f, 0.0f) == 0.0f);
    assert (shiftedPhase (0.0f, 1.0e-9f) < 1.0f);  // must wrap to the start, never land on "finished"
    assert (shiftedPhase (0.5f, 0.25f) == 0.25f);  // positive shift: the shape happens later
    assert (shiftedPhase (0.1f, 0.25f) > 0.84f);
    assert (shiftedPhase (NAN, 0.0f) == 1.0f && shiftedPhase (0.5f, INFINITY) == 1.0f); // bad input means "leave audio alone"

    assert (Version ("v1.3.0").newerThan (Version ("1.2.9")));
    assert (Version ("1.10.0").newerThan (Version ("1.9.5")));  // numeric, not alphabetical
    assert (! Version ("1.2.0").newerThan (Version ("1.2.0")));
    assert (! Version ("v1.1.9").newerThan (Version ("1.2.0")));
    assert (! Version ("").newerThan (Version ("1.2.0")));      // junk from the network never counts as newer
    assert (! Version ("<html>").newerThan (Version ("1.2.0")));
    assert (! Version ("99999999999.0.0").newerThan (Version ("1.2.0")) || Version ("99999999999.0.0").major <= 9999);
    return 0;
}
