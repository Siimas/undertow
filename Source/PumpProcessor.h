#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Duck.h"

namespace
{
const juce::StringArray rateNames { "1/16", "1/8", "1/4", "1/2", "1 bar" };
// ponytail: "1 bar" assumes 4/4; read the host time signature if odd meters matter
constexpr double rateBeats[] = { 0.25, 0.5, 1.0, 2.0, 4.0 };
const juce::StringArray triggerNames { "Sync", "Audio", "MIDI" };
enum Trigger { sync, audio, midi };

juce::StringArray shapeNames()
{
    juce::StringArray names;
    for (const auto& s : shapes)
        names.add (s.name);
    return names;
}

// Choice values come from saved state: clamp before converting or indexing.
int choiceIndex (const std::atomic<float>* value, int count)
{
    const float v = value->load();
    return std::isfinite (v) ? (int) juce::jlimit (0.0f, (float) count - 1.0f, v) : 0;
}

float finiteOr (float v, float fallback) { return std::isfinite (v) ? v : fallback; }
}

//==============================================================================
class PumpProcessor : public juce::AudioProcessor
{
public:
    PumpProcessor()
        : AudioProcessor (BusesProperties()
                              .withInput ("Input", juce::AudioChannelSet::stereo())
                              .withOutput ("Output", juce::AudioChannelSet::stereo())
                              .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
          params (*this, nullptr, "params",
                  { std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "shape", 1 }, "Shape", shapeNames(), 0),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "mix", 1 }, "Mix",
                                                                 juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f),
                    std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "rate", 1 }, "Rate", rateNames, 2),
                    std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "trigger", 1 }, "Trigger", triggerNames, 0),
                    std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "band", 1 }, "Low Band Only", false),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "xover", 1 }, "Crossover",
                                                                 juce::NormalisableRange<float> (40.0f, 2000.0f, 1.0f, 0.4f), 200.0f),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "thresh", 1 }, "Threshold",
                                                                 juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f), -18.0f) })
    {
    }

    void prepareToPlay (double sampleRate, int blockSize) override
    {
        rate = sampleRate;
        // 2 ms smoothing so the drop at the start of each cycle doesn't click
        smooth = 1.0f - std::exp (-1.0f / (0.002f * (float) sampleRate));
        envDecay = std::exp (-1.0f / (0.05f * (float) sampleRate));
        gain = 1.0f;
        env = 0.0f;
        armed = true;
        oneShot = 1.0;
        crossover.prepare ({ sampleRate, (juce::uint32) blockSize, 2 });
        crossover.reset();
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiIn) override
    {
        juce::ScopedNoDenormals noDenormals;
        auto main = getBusBuffer (buffer, true, 0);
        auto side = getBusBuffer (buffer, true, 1); // zero channels when nothing is routed to it

        double bpm = 120.0;
        if (auto* playHead = getPlayHead())
            if (auto pos = playHead->getPosition())
            {
                if (auto b = pos->getBpm(); b && *b > 0.0)
                    bpm = *b;
                // Lock to the song position while playing; free-run from wherever we were when stopped.
                if (auto ppq = pos->getPpqPosition(); ppq && pos->getIsPlaying())
                    beats = *ppq;
            }

        const int shape = choiceIndex (shapeParam, numShapes);
        const int mode = choiceIndex (triggerParam, triggerNames.size());
        const double cycle = rateBeats[choiceIndex (rateParam, (int) std::size (rateBeats))];
        const double beatsPerSample = bpm / (60.0 * rate);
        const float mix = finiteOr (mixParam->load(), 100.0f) / 100.0f;
        const bool split = bandParam->load() > 0.5f;
        const float threshold = juce::Decibels::decibelsToGain (juce::jlimit (-48.0f, 0.0f, finiteOr (threshParam->load(), -18.0f)));
        crossover.setCutoffFrequency (juce::jlimit (40.0f, 2000.0f, finiteOr (xoverParam->load(), 200.0f)));

        const int channels = juce::jmin (main.getNumChannels(), 2);
        auto midiEvent = midiIn.begin();
        float phase = 1.0f;

        for (int i = 0; i < main.getNumSamples(); ++i)
        {
            // Retrigger sources restart a one-shot cycle
            for (; midiEvent != midiIn.end() && (*midiEvent).samplePosition <= i; ++midiEvent)
                if (mode == midi && (*midiEvent).getMessage().isNoteOn())
                    oneShot = 0.0;

            float level = 0.0f;
            for (int ch = 0; ch < side.getNumChannels(); ++ch)
                level = juce::jmax (level, std::abs (side.getSample (ch, i)));
            env = juce::jmax (finiteOr (level, 0.0f), env * envDecay);
            if (armed && level > threshold)
            {
                armed = false;
                if (mode == audio)
                    oneShot = 0.0;
            }
            else if (env < threshold * 0.5f)
                armed = true; // re-arm once the hit has died away

            if (mode == sync)
            {
                double p = beats / cycle;
                phase = (float) (p - std::floor (p));
            }
            else
            {
                phase = (float) oneShot;
                oneShot = juce::jmin (1.0, oneShot + beatsPerSample / cycle);
            }

            gain += smooth * (duckGain (shape, phase, mix) - gain);

            for (int ch = 0; ch < channels; ++ch)
            {
                float* data = main.getWritePointer (ch);
                float low = 0.0f, high = 0.0f;
                crossover.processSample (ch, data[i], low, high); // always running so the toggle never clicks
                data[i] = split ? low * gain + high : data[i] * gain;
            }

            beats += beatsPerSample;
        }

        displayPhase.store (phase);
    }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        const auto mono = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();
        const auto out = layouts.getMainOutputChannelSet();
        const auto sc = layouts.getChannelSet (true, 1);
        return out == layouts.getMainInputChannelSet()
            && (out == mono || out == stereo)
            && (sc.isDisabled() || sc == mono || sc == stereo);
    }

    void getStateInformation (juce::MemoryBlock& dest) override
    {
        if (auto xml = params.copyState().createXml())
            copyXmlToBinary (*xml, dest);
    }

    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size); xml && xml->hasTagName (params.state.getType()))
            params.replaceState (juce::ValueTree::fromXml (*xml));
    }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Pump"; }
    void releaseResources() override {}
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    juce::AudioProcessorValueTreeState params;
    std::atomic<float> displayPhase { 1.0f }; // for the editor's playhead; >= 1 means idle

private:
    std::atomic<float>* shapeParam = params.getRawParameterValue ("shape");
    std::atomic<float>* mixParam = params.getRawParameterValue ("mix");
    std::atomic<float>* rateParam = params.getRawParameterValue ("rate");
    std::atomic<float>* triggerParam = params.getRawParameterValue ("trigger");
    std::atomic<float>* bandParam = params.getRawParameterValue ("band");
    std::atomic<float>* xoverParam = params.getRawParameterValue ("xover");
    std::atomic<float>* threshParam = params.getRawParameterValue ("thresh");

    juce::dsp::LinkwitzRileyFilter<float> crossover;
    double rate = 44100.0, beats = 0.0, oneShot = 1.0;
    float gain = 1.0f, smooth = 1.0f, env = 0.0f, envDecay = 0.0f;
    bool armed = true;
};
