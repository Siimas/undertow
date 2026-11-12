// Headless smoke test: pushes audio through the real processor in each trigger mode.
#undef NDEBUG
#include <cassert>
#include "UndertowProcessor.h"

namespace
{
constexpr double sampleRate = 44100.0;
constexpr int blockSize = 4096;

void set (UndertowProcessor& p, const char* id, float value)
{
    auto* param = p.params.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

// Runs one block of constant 1.0 (or a 5 kHz sine) and returns it. With no host playhead
// the plugin free-runs at 120 bpm, so a 1/4 cycle is 22050 samples.
juce::AudioBuffer<float> run (UndertowProcessor& p, juce::MidiBuffer midi = {}, int sidechainHitAt = -1, bool sine = false)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumInputChannels(), blockSize);
    buffer.clear();
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < blockSize; ++i)
            buffer.setSample (ch, i, sine ? std::sin (juce::MathConstants<float>::twoPi * 5000.0f * (float) i / (float) sampleRate) : 1.0f);
    if (sidechainHitAt >= 0)
        buffer.setSample (2, sidechainHitAt, 1.0f);
    p.processBlock (buffer, midi);
    return buffer;
}

std::unique_ptr<UndertowProcessor> make (bool withSidechain = false)
{
    auto p = std::make_unique<UndertowProcessor>();
    if (withSidechain)
    {
        auto layout = p->getBusesLayout();
        layout.inputBuses.set (1, juce::AudioChannelSet::stereo());
        const bool ok = p->setBusesLayout (layout);
        assert (ok);
        assert (p->getTotalNumInputChannels() == 4);
    }
    p->prepareToPlay (sampleRate, blockSize);
    return p;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;

    {   // Sync: ducked right after the cycle starts, recovering afterwards
        auto p = make();
        auto out = run (*p);
        assert (out.getSample (0, 500) < 0.1f);
        assert (out.getSample (0, 4000) > out.getSample (0, 500));
        assert (out.getSample (1, 500) == out.getSample (0, 500));
    }
    {   // Mix at 0: audio untouched
        auto p = make();
        set (*p, "mix", 0.0f);
        assert (run (*p).getSample (0, 500) > 0.999f);
    }
    {   // MIDI: untouched until a note arrives, then ducks from that sample
        auto p = make();
        set (*p, "trigger", (float) midi);
        assert (run (*p).getMagnitude (0, 0, blockSize) > 0.999f && run (*p).getSample (0, 2000) > 0.999f);
        juce::MidiBuffer note;
        note.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 1000);
        auto out = run (*p, note);
        assert (out.getSample (0, 900) > 0.999f);
        assert (out.getSample (0, 1500) < 0.1f);
    }
    {   // Audio: untouched until the sidechain is hit; main output never contains the sidechain
        auto p = make (true);
        set (*p, "trigger", (float) audio);
        assert (run (*p).getSample (0, 2000) > 0.999f);
        auto out = run (*p, {}, 1000);
        assert (out.getSample (0, 900) > 0.999f);
        assert (out.getSample (0, 1500) < 0.1f);
    }
    {   // Audio mode with no sidechain routed: passes audio through
        auto p = make();
        set (*p, "trigger", (float) audio);
        assert (run (*p).getSample (0, 2000) > 0.999f);
    }
    {   // Length 50%: the shape is over by mid-cycle (sample 11025)
        auto p = make();
        set (*p, "length", 50.0f);
        for (int i = 0; i < 3; ++i)
            run (*p);
        assert (run (*p).getSample (0, 1000) > 0.999f); // block 4 starts at sample 12288
    }
    {   // Shift +25%: the duck lands a quarter cycle (5512 samples) late
        auto p = make();
        set (*p, "shift", 25.0f);
        assert (run (*p).getSample (0, 2000) > 0.6f);
        auto second = run (*p); // samples 4096..8191
        assert (second.getSample (0, 2000) < 0.1f);
    }
    {   // Host transport: idle when stopped, ducking when playing
        struct Transport : juce::AudioPlayHead
        {
            bool playing = false;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo info;
                info.setIsPlaying (playing);
                info.setBpm (120.0);
                info.setPpqPosition (0.0);
                return info;
            }
        } transport;
        auto p = make();
        p->setPlayHead (&transport);
        assert (run (*p).getSample (0, 2000) > 0.999f);
        assert (p->displayPhase.load() >= 1.0f);
        transport.playing = true;
        assert (run (*p).getSample (0, 500) < 0.1f);
        assert (p->displayPhase.load() < 1.0f);
        p->setPlayHead (nullptr);
    }
    {   // Loop: a MIDI-triggered cycle plays once by default, and keeps repeating with Loop on
        for (const bool loop : { false, true })
        {
            auto p = make();
            set (*p, "trigger", (float) midi);
            set (*p, "loop", loop ? 1.0f : 0.0f);
            juce::MidiBuffer note;
            note.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
            run (*p, note);
            for (int i = 0; i < 4; ++i)
                run (*p);
            // block 6 covers samples 20480..24575; the second cycle would start at 22050 (sample 1570 here)
            const float secondCycle = run (*p).getSample (0, 2200);
            assert (loop ? secondCycle < 0.1f : secondCycle > 0.999f);
        }
    }
    {   // Band listen: only the ducked low band is heard, so a 5 kHz tone disappears
        auto p = make();
        set (*p, "shape", 12.0f);
        set (*p, "band", 1.0f);
        set (*p, "listen", 1.0f);
        assert (run (*p, {}, -1, true).getRMSLevel (0, 15000 - 12288 < 0 ? 0 : 1000, 3000) < 0.05f);
    }
    {   // Band split: a 5 kHz tone is ducked normally, but left alone with "low band only"
        auto full = make();
        set (*full, "shape", 12.0f); // Gate: silent for the first half cycle
        const float ducked = run (*full, {}, -1, true).getRMSLevel (0, 1000, 3000);

        auto split = make();
        set (*split, "shape", 12.0f);
        set (*split, "band", 1.0f);
        const float kept = run (*split, {}, -1, true).getRMSLevel (0, 1000, 3000);

        assert (ducked < 0.01f);
        assert (kept > 0.6f);
    }
    return 0;
}
