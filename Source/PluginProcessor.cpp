#include <juce_audio_processors/juce_audio_processors.h>
#include "Duck.h"

class PumpProcessor : public juce::AudioProcessor
{
public:
    PumpProcessor()
        : AudioProcessor (BusesProperties()
                              .withInput ("Input", juce::AudioChannelSet::stereo())
                              .withOutput ("Output", juce::AudioChannelSet::stereo())),
          params (*this, nullptr, "params",
                  { std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "depth", 1 }, "Depth", 0.0f, 1.0f, 1.0f),
                    std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "rate", 1 }, "Rate",
                                                                  juce::StringArray { "1/8", "1/4", "1/2", "1 bar" }, 1),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "curve", 1 }, "Curve",
                                                                 juce::NormalisableRange<float> (0.25f, 4.0f, 0.0f, 0.5f), 1.0f) })
    {
    }

    void prepareToPlay (double sampleRate, int) override
    {
        // 2 ms smoothing so the drop at the start of each cycle doesn't click
        smooth = 1.0f - std::exp (-1.0f / (0.002f * (float) sampleRate));
        gain = 1.0f;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;

        double bpm = 120.0;
        if (auto* playHead = getPlayHead())
            if (auto pos = playHead->getPosition())
            {
                if (auto b = pos->getBpm())
                    bpm = *b;
                // Lock to the song position while playing; free-run from wherever we were when stopped.
                if (auto ppq = pos->getPpqPosition(); ppq && pos->getIsPlaying())
                    beats = *ppq;
            }

        // ponytail: "1 bar" assumes 4/4; read the host time signature if odd meters matter
        static constexpr double beatsPerCycle[] = { 0.5, 1.0, 2.0, 4.0 };
        // rate comes from saved state: never index with it unchecked
        const float r = rate->load();
        const double cycle = beatsPerCycle[std::isfinite (r) ? juce::jlimit (0, 3, (int) r) : 1];
        const double beatsPerSample = bpm / (60.0 * getSampleRate());
        const float d = depth->load(), c = curve->load();

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            double phase = beats / cycle;
            phase -= std::floor (phase);
            gain += smooth * (duckGain ((float) phase, c, d) - gain);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[i] *= gain;

            beats += beatsPerSample;
        }
    }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        const auto out = layouts.getMainOutputChannelSet();
        return out == layouts.getMainInputChannelSet()
            && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
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

    // ponytail: host-style sliders, no custom UI; add an editor with a curve display when looks matter
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor (*this); }
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Pump"; }
    void releaseResources() override {}
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

private:
    juce::AudioProcessorValueTreeState params;
    std::atomic<float>* depth = params.getRawParameterValue ("depth");
    std::atomic<float>* rate = params.getRawParameterValue ("rate");
    std::atomic<float>* curve = params.getRawParameterValue ("curve");
    double beats = 0.0;
    float gain = 1.0f, smooth = 1.0f;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PumpProcessor(); }
