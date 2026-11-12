#include "PumpProcessor.h"

namespace
{
const juce::Colour bgTop { 0xff1a1d26 }, bgBottom { 0xff0e1015 }, panel { 0xff232733 }, panelHover { 0xff2d3240 },
    screenTop { 0xff171a22 }, screenBottom { 0xff0b0c10 }, accent { 0xff3ddc97 }, text { 0xffe6e9ef }, dim { 0xff7d8698 };

struct PumpLook : juce::LookAndFeel_V4
{
    PumpLook()
    {
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, dim);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
        setColour (juce::PopupMenu::highlightedTextColourId, bgBottom);
        setColour (juce::TextButton::textColourOffId, dim);
        setColour (juce::TextButton::textColourOnId, bgBottom);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float position,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (5.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + position * (endAngle - startAngle);
        const float alpha = slider.isEnabled() ? 1.0f : 0.25f;
        const juce::PathStrokeType stroke (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track, value;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour (panel);
        g.strokePath (track, stroke);
        g.setColour (accent.withAlpha (alpha));
        g.strokePath (value, stroke);

        const float knob = radius - 9.0f;
        g.setGradientFill ({ panelHover.brighter (0.15f), centre.x, centre.y - knob, panel.darker (0.3f), centre.x, centre.y + knob, false });
        g.fillEllipse (juce::Rectangle<float> (knob * 2.0f, knob * 2.0f).withCentre (centre));
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawEllipse (juce::Rectangle<float> (knob * 2.0f, knob * 2.0f).withCentre (centre), 1.0f);

        g.setColour (text.withAlpha (alpha));
        g.drawLine ({ centre.getPointOnCircumference (knob * 0.4f, angle), centre.getPointOnCircumference (knob - 4.0f, angle) }, 2.5f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool) override
    {
        g.setColour (button.getToggleState() ? accent : over ? panelHover : panel);
        g.fillRoundedRectangle (button.getLocalBounds().toFloat().reduced (1.0f), 5.0f);
    }
};

// A row of buttons bound to a choice parameter; exactly one is lit.
class Segmented : public juce::Component
{
public:
    explicit Segmented (juce::RangedAudioParameter& param)
        : attachment (param, [this] (float value)
                      {
                          if (auto* b = buttons[(int) value])
                              b->setToggleState (true, juce::dontSendNotification);
                      })
    {
        const auto names = param.getAllValueStrings();
        for (int i = 0; i < names.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (names[i]));
            b->setClickingTogglesState (true);
            b->setRadioGroupId (1);
            b->onClick = [this, i, b]
            {
                if (b->getToggleState())
                    attachment.setValueAsCompleteGesture ((float) i);
            };
            addAndMakeVisible (b);
        }
        attachment.sendInitialUpdate();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        const int width = area.getWidth() / juce::jmax (1, buttons.size());
        for (auto* b : buttons)
            b->setBounds (area.removeFromLeft (width).reduced (2, 0));
    }

private:
    juce::OwnedArray<juce::TextButton> buttons;
    juce::ParameterAttachment attachment; // after buttons: its callback uses them
};
}

//==============================================================================
class PumpEditor : public juce::AudioProcessorEditor, private juce::Timer
{
    using APVTS = juce::AudioProcessorValueTreeState;

public:
    explicit PumpEditor (PumpProcessor& p)
        : AudioProcessorEditor (p), proc (p),
          rate (*p.params.getParameter ("rate")), trigger (*p.params.getParameter ("trigger"))
    {
        setLookAndFeel (&look);

        shape.addItemList (p.params.getParameter ("shape")->getAllValueStrings(), 1);
        shapeLink = std::make_unique<APVTS::ComboBoxAttachment> (p.params, "shape", shape);
        previous.onClick = [this] { stepShape (-1); };
        next.onClick = [this] { stepShape (1); };

        addKnob (mix, "mix", " %");
        addKnob (thresh, "thresh", " dB");
        addKnob (xover, "xover", " Hz");

        band.setClickingTogglesState (true);
        bandLink = std::make_unique<APVTS::ButtonAttachment> (p.params, "band", band);

        for (auto* c : std::initializer_list<juce::Component*> { &shape, &previous, &next, &rate, &trigger, &band })
            addAndMakeVisible (c);

        setSize (640, 470);
        timerCallback();
        startTimerHz (30);
    }

    ~PumpEditor() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.setGradientFill ({ bgTop, 0.0f, 0.0f, bgBottom, 0.0f, (float) getHeight(), false });
        g.fillAll();

        g.setColour (accent);
        g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
        g.drawText ("PUMP", 20, 14, 90, 30, juce::Justification::centredLeft);
        g.setColour (dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("VOLUME SHAPER", 92, 20, 140, 20, juce::Justification::centredLeft);

        paintScreen (g);

        g.setColour (dim);
        g.setFont (juce::FontOptions (11.0f));
        for (auto [component, label] : { std::pair<juce::Component*, const char*> { &rate, "RATE" }, { &trigger, "TRIGGER" },
                                         { &mix, "MIX" }, { &thresh, "THRESHOLD" }, { &xover, "CROSSOVER" }, { &band, "BAND SPLIT" } })
            g.drawText (label, component->getX(), component->getY() - 18, component->getWidth(), 14, juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (20, 14);

        auto header = area.removeFromTop (30).removeFromRight (280);
        previous.setBounds (header.removeFromLeft (30).reduced (0, 2));
        next.setBounds (header.removeFromRight (30).reduced (0, 2));
        shape.setBounds (header.reduced (4, 2));

        auto knobs = area.removeFromBottom (92);
        const int column = knobs.getWidth() / 4;
        mix.setBounds (knobs.removeFromLeft (column));
        thresh.setBounds (knobs.removeFromLeft (column));
        xover.setBounds (knobs.removeFromLeft (column));
        band.setBounds (knobs.withSizeKeepingCentre (110, 30));

        area.removeFromBottom (30);
        auto row = area.removeFromBottom (30);
        rate.setBounds (row.removeFromLeft (row.getWidth() * 3 / 5).withTrimmedRight (10));
        trigger.setBounds (row.withTrimmedLeft (10));

        area.removeFromBottom (28);
        screen = area.withTrimmedTop (12);
    }

private:
    // The display: grid, the current shape at the current mix, and a playhead riding the curve
    void paintScreen (juce::Graphics& g)
    {
        const auto frame = screen.toFloat();
        g.setGradientFill ({ screenTop, 0.0f, frame.getY(), screenBottom, 0.0f, frame.getBottom(), false });
        g.fillRoundedRectangle (frame, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (frame.reduced (0.5f), 8.0f, 1.0f);

        const auto plot = frame.reduced (14.0f);
        for (int i = 1; i < 4; ++i)
        {
            g.fillRect (plot.getX() + plot.getWidth() * (float) i / 4.0f, plot.getY(), 1.0f, plot.getHeight());
            g.fillRect (plot.getX(), plot.getY() + plot.getHeight() * (float) i / 4.0f, plot.getWidth(), 1.0f);
        }

        const int shapeIndex = shape.getSelectedItemIndex();
        const float mixNow = (float) mix.getValue() / 100.0f;
        // stop just short of phase 1.0, which means "cycle finished"
        const auto yAt = [&] (float phase) { return plot.getBottom() - duckGain (shapeIndex, 0.999f * phase, mixNow) * plot.getHeight(); };

        juce::Path curve;
        curve.startNewSubPath (plot.getX(), yAt (0.0f));
        for (float x = 1.0f; x <= plot.getWidth(); x += 1.0f)
            curve.lineTo (plot.getX() + x, yAt (x / plot.getWidth()));

        juce::Path fill (curve);
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();
        g.setGradientFill ({ accent.withAlpha (0.35f), 0.0f, plot.getY(), accent.withAlpha (0.02f), 0.0f, plot.getBottom(), false });
        g.fillPath (fill);

        const juce::PathStrokeType::JointStyle joint = juce::PathStrokeType::curved;
        g.setColour (accent.withAlpha (0.15f)); // soft glow under the line
        g.strokePath (curve, juce::PathStrokeType (7.0f, joint));
        g.setColour (accent);
        g.strokePath (curve, juce::PathStrokeType (2.2f, joint));

        if (const float phase = proc.displayPhase.load(); phase >= 0.0f && phase < 1.0f)
        {
            const juce::Point<float> dot (plot.getX() + phase * plot.getWidth(), yAt (phase));
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.fillRect (dot.x - 0.5f, plot.getY(), 1.0f, plot.getHeight());
            g.setColour (accent.withAlpha (0.3f));
            g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (dot));
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (dot));
        }
    }

    void timerCallback() override
    {
        xover.setEnabled (band.getToggleState());
        thresh.setEnabled ((int) proc.params.getRawParameterValue ("trigger")->load() == audio);
        band.setButtonText (band.getToggleState() ? "Low only" : "Full range");
        repaint (screen);
    }

    void stepShape (int direction)
    {
        const int count = shape.getNumItems();
        if (count > 0)
            shape.setSelectedItemIndex ((shape.getSelectedItemIndex() + direction + count) % count);
    }

    void addKnob (juce::Slider& knob, const char* id, const char* suffix)
    {
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        knob.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (knob);
        knobLinks.add (new APVTS::SliderAttachment (proc.params, id, knob));
        knob.setTextValueSuffix (suffix);
    }

    PumpProcessor& proc;
    PumpLook look; // before the controls, so it outlives them
    juce::ComboBox shape;
    juce::TextButton previous { "<" }, next { ">" }, band;
    Segmented rate, trigger;
    juce::Slider mix, thresh, xover;
    juce::Rectangle<int> screen;
    // After the controls, so the attachments are destroyed first
    std::unique_ptr<APVTS::ComboBoxAttachment> shapeLink;
    std::unique_ptr<APVTS::ButtonAttachment> bandLink;
    juce::OwnedArray<APVTS::SliderAttachment> knobLinks;
};

juce::AudioProcessorEditor* PumpProcessor::createEditor() { return new PumpEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PumpProcessor(); }
