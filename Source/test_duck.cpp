#undef NDEBUG
#include <cassert>
#include "Duck.h"

int main()
{
    assert (numShapes == 16);
    assert (duckGain (0, 0.0f, 1.0f) == 0.0f);    // start of cycle: silent at full mix
    assert (duckGain (0, 0.5f, 1.0f) == 0.5f);    // Classic is a straight ramp
    assert (duckGain (0, 1.0f, 1.0f) == 1.0f);    // cycle finished: full volume
    assert (duckGain (0, 0.0f, 0.5f) == 0.5f);    // half mix: half duck
    assert (duckGain (12, 0.25f, 1.0f) == 0.0f);  // Gate: silent first half...
    assert (duckGain (12, 0.75f, 1.0f) == 1.0f);  // ...open second half
    assert (duckGain (14, 0.5f, 1.0f) == 0.0f);   // Double: ducks again mid-cycle

    for (int s = 0; s < numShapes; ++s)           // every shape stays in 0..1 and ends open
    {
        for (int i = 0; i < 1000; ++i)
        {
            const float g = duckGain (s, (float) i / 1000.0f, 1.0f);
            assert (g >= 0.0f && g <= 1.0f);
        }
        assert (duckGain (s, 0.0f, 0.0f) == 1.0f);
    }

    assert (duckGain (0, NAN, 1.0f) == 1.0f);     // bad input: leave audio untouched
    assert (duckGain (0, 0.0f, NAN) == 1.0f);
    assert (duckGain (0, 0.0f, 99.0f) == 0.0f);   // out-of-range mix is clamped
    assert (duckGain (999, 0.0f, 1.0f) == 0.0f);  // out-of-range shape is clamped, no crash
    assert (duckGain (-5, 0.0f, 1.0f) == 0.0f);
    return 0;
}
