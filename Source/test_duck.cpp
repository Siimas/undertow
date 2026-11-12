#undef NDEBUG
#include <cassert>
#include "Duck.h"

int main()
{
    assert (duckGain (0.0f, 1.0f, 1.0f) == 0.0f);   // start of cycle: silent at full depth
    assert (duckGain (1.0f, 2.0f, 1.0f) == 1.0f);   // end of cycle: full volume
    assert (duckGain (0.3f, 2.0f, 0.0f) == 1.0f);   // zero depth: untouched
    assert (duckGain (0.0f, 1.0f, 0.5f) == 0.5f);   // half depth: half duck
    assert (duckGain (0.5f, 0.5f, 1.0f) > duckGain (0.5f, 2.0f, 1.0f)); // low curve recovers faster
    return 0;
}
