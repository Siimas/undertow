#include "PumpProcessor.h"

//==============================================================================
class PumpEditor : public juce::AudioProcessorEditor, private juce::Timer
{
    using APVTS = juce::AudioProcessorValueTreeState;
    const juce::Colour bg { 0xff14161c }, panel { 0xff1e222b }, accent { 0xff3ddc97 }, dim { 0xff8a93a6 };

public:
    explicit PumpEditor (PumpProcessor& p) : AudioProcessorEditor (p), proc (p)
    {
        laf.setColour (juce::Slider::rotarySliderFillColourId, accent);
        laf.setColour (juce::Slider::rotarySliderOutlineColourId, panel);
        laf.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        laf.setColour (juce::ComboBox::backgroundColourId, panel);
        laf.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        laf.setColour (juce::PopupMenu::backgroundColourId, panel);
        laf.setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.3f));
        laf.setColour (juce::ToggleButton::tickColourId, accent);
        setLookAndFeel (&laf);

        addCombo (shape, "shape");
        addCombo (rate, "rate");
        addCombo (trigger, "trigger");
        addKnob (mix, "mix", " %");
        addKnob (xover, "xover", " Hz");
        addKnob (thresh, "thresh", " dB");

        band.setButtonText ("Low band only");
        addAndMakeVisible (band);
        buttonLinks.add (new APVTS::ButtonAttachment (proc.params, "band", band));

        setSize (560, 400);
        startTimerHz (30);
    }

    ~PumpEditor() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (bg);
        g.setColour (accent);
        g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
        g.drawText ("PUMP", 16, 10, 200, 28, juce::Justification::centredLeft);

        // Curve display: the current shape at the current mix, with a moving playhead
        const auto d = display.toFloat();
        g.setColour (panel);
        g.fillRoundedRectangle (d, 6.0f);

        const int shapeIndex = shape.getSelectedItemIndex();
        const float mixNow = (float) mix.getValue() / 100.0f;
        const auto inner = d.reduced (10.0f);
        juce::Path curve;
        for (float x = 0.0f; x <= inner.getWidth(); x += 1.0f)
        {
            // stop just short of 1.0, which means "cycle finished"
            const float gain = duckGain (shapeIndex, 0.999f * x / inner.getWidth(), mixNow);
            const float y = inner.getBottom() - gain * inner.getHeight();
            if (x == 0.0f)
                curve.startNewSubPath (inner.getX(), y);
            else
                curve.lineTo (inner.getX() + x, y);
        }
        g.setColour (accent);
        g.strokePath (curve, juce::PathStrokeType (2.0f));
        curve.lineTo (inner.getRight(), inner.getBottom());
        curve.lineTo (inner.getX(), inner.getBottom());
        curve.closeSubPath();
        g.setColour (accent.withAlpha (0.15f));
        g.fillPath (curve);

        if (const float phase = proc.displayPhase.load(); phase >= 0.0f && phase < 1.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillRect (inner.getX() + phase * inner.getWidth(), inner.getY(), 1.5f, inner.getHeight());
        }

        g.setColour (dim);
        g.setFont (juce::FontOptions (12.0f));
        for (auto [component, text] : { std::pair<juce::Component*, const char*> { &shape, "SHAPE" }, { &rate, "RATE" }, { &trigger, "TRIGGER" },
                                        { &mix, "MIX" }, { &xover, "CROSSOVER" }, { &thresh, "THRESHOLD" } })
            g.drawText (text, component->getX(), component->getY() - 16, component->getWidth(), 14, juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (16);
        area.removeFromTop (30);

        auto knobs = area.removeFromBottom (96);
        const int knobWidth = knobs.getWidth() / 4;
        mix.setBounds (knobs.removeFromLeft (knobWidth));
        xover.setBounds (knobs.removeFromLeft (knobWidth));
        thresh.setBounds (knobs.removeFromLeft (knobWidth));
        band.setBounds (knobs.withSizeKeepingCentre (knobs.getWidth() - 8, 24));

        area.removeFromBottom (24);
        auto combos = area.removeFromBottom (26);
        const int comboWidth = combos.getWidth() / 3;
        shape.setBounds (combos.removeFromLeft (comboWidth).reduced (4, 0));
        rate.setBounds (combos.removeFromLeft (comboWidth).reduced (4, 0));
        trigger.setBounds (combos.reduced (4, 0));

        area.removeFromBottom (24);
        display = area;
    }

private:
    void timerCallback() override
    {
        xover.setEnabled (band.getToggleState());
        thresh.setEnabled (trigger.getSelectedItemIndex() == audio);
        repaint (display);
    }

    void addCombo (juce::ComboBox& box, const char* id)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.params.getParameter (id)))
            box.addItemList (choice->choices, 1);
        addAndMakeVisible (box);
        comboLinks.add (new APVTS::ComboBoxAttachment (proc.params, id, box));
    }

    void addKnob (juce::Slider& knob, const char* id, const char* suffix)
    {
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        knob.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (knob);
        sliderLinks.add (new APVTS::SliderAttachment (proc.params, id, knob));
        knob.setTextValueSuffix (suffix);
    }

    PumpProcessor& proc;
    juce::LookAndFeel_V4 laf;
    juce::ComboBox shape, rate, trigger;
    juce::Slider mix, xover, thresh;
    juce::ToggleButton band;
    juce::Rectangle<int> display;
    // Declared after the controls so the attachments are destroyed first
    juce::OwnedArray<APVTS::ComboBoxAttachment> comboLinks;
    juce::OwnedArray<APVTS::SliderAttachment> sliderLinks;
    juce::OwnedArray<APVTS::ButtonAttachment> buttonLinks;
};

juce::AudioProcessorEditor* PumpProcessor::createEditor() { return new PumpEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PumpProcessor(); }
