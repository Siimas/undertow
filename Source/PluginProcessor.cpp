#include "UndertowProcessor.h"
#include "Version.h"
#include <BinaryData.h>

namespace
{
const juce::Colour bgTop { 0xff1c1f29 }, bgBottom { 0xff0d0f14 }, card { 0xff222632 }, cardLow { 0xff1a1d27 },
    raised { 0xff2c3140 }, raisedHover { 0xff363c4d }, screenTop { 0xff0a0b0f }, screenBottom { 0xff12141b },
    accent { 0xff3ddc97 }, text { 0xffe6e9ef }, dim { 0xff7d8698 };

// Space Grotesk, bundled. Shared by everything that draws text and freed with the last editor.
struct Fonts
{
    juce::Typeface::Ptr medium = juce::Typeface::createSystemTypefaceFor (BinaryData::SpaceGroteskMedium_ttf, BinaryData::SpaceGroteskMedium_ttfSize),
                        bold = juce::Typeface::createSystemTypefaceFor (BinaryData::SpaceGroteskBold_ttf, BinaryData::SpaceGroteskBold_ttfSize);

    juce::FontOptions get (float height, bool heavy = false) const { return juce::FontOptions (heavy ? bold : medium).withHeight (height); }
};

// The name is the logo: bold caps, with the pull ("TOW") picked out in the accent colour
void drawWordmark (juce::Graphics& g, const Fonts& fonts, juce::Rectangle<float> area)
{
    const juce::Font font (fonts.get (area.getHeight(), true).withKerningFactor (0.08f));
    juce::AttributedString name;
    name.setJustification (juce::Justification::centredLeft);
    name.append ("UNDER", font, text);
    name.append ("TOW", font, accent);
    name.draw (g, area);
}

// The "Hardware" look: smooth black metal, machined knobs, a glass display. Off = the flat modern look.
bool hardware = false;

// Button and tile faces: lifted off the panel in the flat look, near-black in the hardware look
juce::Colour face (bool hovered) { return hardware ? juce::Colour (hovered ? 0xff26282e : 0xff1b1d22) : hovered ? raisedHover : raised; }

void drawMetal (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour base)
{
    g.setGradientFill ({ base.brighter (0.10f), 0.0f, area.getY(), base.darker (0.40f), 0.0f, area.getBottom(), false });
    g.fillRect (area);
}

const juce::DropShadow cardShadow { juce::Colours::black.withAlpha (0.55f), 16, { 0, 6 } };
const juce::DropShadow smallShadow { juce::Colours::black.withAlpha (0.5f), 6, { 0, 2 } };

// A raised surface: soft shadow underneath, gradient face, thin highlight along the top edge
void drawCard (juce::Graphics& g, juce::Rectangle<float> area, float radius = 10.0f)
{
    juce::Path outline;
    outline.addRoundedRectangle (area, radius);
    cardShadow.drawForPath (g, outline);
    if (hardware)
    {
        {
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (outline);
            drawMetal (g, area, juce::Colour (0xff17191d));
        }
        g.setGradientFill ({ juce::Colours::white.withAlpha (0.12f), 0.0f, area.getY(), juce::Colours::black.withAlpha (0.7f), 0.0f, area.getBottom(), false });
        g.strokePath (outline, juce::PathStrokeType (1.5f)); // machined bevel
        return;
    }
    g.setGradientFill ({ card, 0.0f, area.getY(), cardLow, 0.0f, area.getBottom(), false });
    g.fillPath (outline);
    g.setGradientFill ({ juce::Colours::white.withAlpha (0.10f), 0.0f, area.getY(), juce::Colours::transparentWhite, 0.0f, area.getY() + 24.0f, false });
    g.strokePath (outline, juce::PathStrokeType (1.0f));
}

// The y of every x across `area` for a shape, as a path
juce::Path shapePath (juce::Rectangle<float> area, int shape, float mix, float shift, float length)
{
    juce::Path path;
    for (float x = 0.0f; x <= area.getWidth(); x += 1.0f)
    {
        const float gain = duckGain (shape, shiftedPhase (0.999f * x / area.getWidth(), shift) / length, mix);
        const float y = area.getBottom() - gain * area.getHeight();
        if (x == 0.0f)
            path.startNewSubPath (area.getX(), y);
        else
            path.lineTo (area.getX() + x, y);
    }
    return path;
}

struct UndertowLook : juce::LookAndFeel_V4
{
    UndertowLook()
    {
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::TextButton::textColourOffId, dim);
        setColour (juce::TextButton::textColourOnId, bgBottom);
        setColour (juce::PopupMenu::backgroundColourId, card);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
        setColour (juce::PopupMenu::highlightedTextColourId, bgBottom);
    }

    juce::SharedResourcePointer<Fonts> fonts;
    juce::Font getTextButtonFont (juce::TextButton&, int height) override { return juce::Font (fonts->get (juce::jmin (15.0f, (float) height * 0.5f))); }
    juce::Font getLabelFont (juce::Label&) override { return juce::Font (fonts->get (14.0f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (fonts->get (15.0f)); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float position,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (5.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + position * (endAngle - startAngle);
        const float alpha = slider.isEnabled() ? 1.0f : 0.25f;
        if (hardware)
        {
            drawMachinedKnob (g, centre, radius, position, startAngle, endAngle, alpha);
            return;
        }
        const juce::PathStrokeType stroke (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track, value;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (track, stroke);
        g.setColour (accent.withAlpha (alpha));
        g.strokePath (value, stroke);

        const float size = (radius - 9.0f) * 2.0f;
        const auto face = juce::Rectangle<float> (size, size).withCentre (centre);
        juce::Path cap;
        cap.addEllipse (face);
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 10, { 0, 4 }).drawForPath (g, cap);
        g.setGradientFill ({ raisedHover.brighter (0.2f), 0.0f, face.getY(), raised.darker (0.5f), 0.0f, face.getBottom(), false });
        g.fillPath (cap);
        g.setGradientFill ({ juce::Colours::white.withAlpha (0.28f), 0.0f, face.getY(), juce::Colours::transparentWhite, 0.0f, face.getCentreY(), false });
        g.drawEllipse (face.reduced (0.5f), 1.0f);

        g.setColour (text.withAlpha (alpha));
        g.drawLine ({ centre.getPointOnCircumference (size * 0.2f, angle), centre.getPointOnCircumference (size * 0.5f - 4.0f, angle) }, 2.5f);
    }

    // A knurled aluminium knob with a ring of ticks that light up to the current value
    static void drawMachinedKnob (juce::Graphics& g, juce::Point<float> centre, float radius, float position, float startAngle, float endAngle, float alpha)
    {
        constexpr int ticks = 25;
        for (int i = 0; i < ticks; ++i)
        {
            const float t = (float) i / (float) (ticks - 1), angle = startAngle + t * (endAngle - startAngle);
            const bool lit = t <= position + 0.001f;
            g.setColour (lit ? accent.withAlpha (alpha) : juce::Colours::white.withAlpha (0.09f));
            g.drawLine ({ centre.getPointOnCircumference (radius - 5.0f, angle), centre.getPointOnCircumference (radius, angle) }, lit ? 2.0f : 1.4f);
        }

        const float outer = radius - 10.0f;
        const auto skirt = juce::Rectangle<float> (outer * 2.0f, outer * 2.0f).withCentre (centre);
        juce::Path skirtPath;
        skirtPath.addEllipse (skirt);
        juce::DropShadow (juce::Colours::black.withAlpha (0.85f), 14, { 0, 7 }).drawForPath (g, skirtPath);
        g.setGradientFill ({ juce::Colour (0xff2a2c31), 0.0f, skirt.getY(), juce::Colour (0xff050506), 0.0f, skirt.getBottom(), false });
        g.fillPath (skirtPath);
        for (int i = 0; i < 48; ++i) // knurling
        {
            const float angle = juce::MathConstants<float>::twoPi * (float) i / 48.0f;
            g.setColour ((i % 2 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (0.10f));
            g.drawLine ({ centre.getPointOnCircumference (outer - 5.0f, angle), centre.getPointOnCircumference (outer - 0.5f, angle) }, 1.2f);
        }

        const float capRadius = outer - 6.0f;
        const auto cap = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre);
        g.setGradientFill ({ juce::Colour (0xff474a52), 0.0f, cap.getY(), juce::Colour (0xff101113), 0.0f, cap.getBottom(), false });
        g.fillEllipse (cap);
        {
            juce::Graphics::ScopedSaveState clip (g); // spun-metal highlights
            juce::Path capPath;
            capPath.addEllipse (cap);
            g.reduceClipRegion (capPath);
            for (const float at : { -0.75f, 2.4f })
            {
                juce::Path wedge;
                wedge.addPieSegment (cap.expanded (4.0f), at - 0.32f, at + 0.32f, 0.0f);
                g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.0f), centre.x, centre.y,
                                                         juce::Colours::white.withAlpha (0.13f), centre.getPointOnCircumference (capRadius, at).x, centre.getPointOnCircumference (capRadius, at).y, true));
                g.fillPath (wedge);
            }
        }
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (cap, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawEllipse (cap.reduced (1.0f), 0.6f);

        const float angle = startAngle + position * (endAngle - startAngle);
        const juce::Line<float> pointer (centre.getPointOnCircumference (capRadius * 0.30f, angle), centre.getPointOnCircumference (capRadius - 3.0f, angle));
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawLine (pointer, 4.5f);
        g.setColour (accent.withAlpha (alpha));
        g.drawLine (pointer, 2.4f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down) override
    {
        const auto area = button.getLocalBounds().toFloat().reduced (3.0f, 4.0f);
        juce::Path outline;
        outline.addRoundedRectangle (area, 6.0f);

        if (button.getToggleState())
        {
            juce::DropShadow (accent.withAlpha (0.45f), 8, {}).drawForPath (g, outline); // glow
            g.setGradientFill ({ accent.brighter (0.25f), 0.0f, area.getY(), accent.darker (0.15f), 0.0f, area.getBottom(), false });
        }
        else
        {
            if (! down)
                smallShadow.drawForPath (g, outline);
            const auto top = face (over);
            g.setGradientFill ({ top, 0.0f, area.getY(), top.darker (0.25f), 0.0f, area.getBottom(), false });
        }
        g.fillPath (outline);
        if (hardware) // a physical key: dark surround, bright top edge
        {
            g.setColour (juce::Colours::black.withAlpha (0.85f));
            g.strokePath (outline, juce::PathStrokeType (2.0f));
            g.setGradientFill ({ juce::Colours::white.withAlpha (0.16f), 0.0f, area.getY(), juce::Colours::transparentWhite, 0.0f, area.getY() + 8.0f, false });
            g.fillRoundedRectangle (area.reduced (2.0f, 1.5f).withHeight (7.0f), 4.0f);
            return;
        }
        g.setGradientFill ({ juce::Colours::white.withAlpha (0.16f), 0.0f, area.getY(), juce::Colours::transparentWhite, 0.0f, area.getCentreY(), false });
        g.strokePath (outline, juce::PathStrokeType (1.0f));
    }
};

// Asks GitHub, once per editor window and off the UI thread, whether a newer release exists.
// Sends nothing but the request itself. The reply is only ever parsed as a version number.
struct UpdateCheck : juce::Thread
{
    UpdateCheck() : Thread ("Undertow update check")
    {
        stream.withConnectionTimeout (3000).withExtraHeaders ("User-Agent: Undertow\r\nAccept: application/vnd.github+json");
        startThread();
    }

    ~UpdateCheck() override
    {
        // Never force-kill the thread (that corrupts the host): cancel the request, then wait for a clean exit.
        signalThreadShouldExit();
        stream.cancel();
        stopThread (-1);
    }

    void run() override
    {
        if (! stream.connect (nullptr))
            return;

        juce::MemoryOutputStream reply;
        char chunk[4096];
        while (! threadShouldExit() && reply.getDataSize() < 256 * 1024)
        {
            const int got = stream.read (chunk, (int) sizeof (chunk));
            if (got <= 0)
                break;
            reply.write (chunk, (size_t) got);
        }
        if (threadShouldExit())
            return;

        const Version found (juce::JSON::parse (reply.toString())["tag_name"].toString().toStdString());
        if (found.newerThan (Version (JucePlugin_VersionString)))
        {
            latest = juce::String (found.major) + "." + juce::String (found.minor) + "." + juce::String (found.patch);
            available = true;
        }
    }

    juce::WebInputStream stream { juce::URL ("https://api.github.com/repos/Siimas/undertow/releases/latest"), false };
    juce::String latest; // written before `available` is set, read only after
    std::atomic<bool> available { false };
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
            b->setBounds (area.removeFromLeft (width));
    }

private:
    juce::OwnedArray<juce::TextButton> buttons;
    juce::ParameterAttachment attachment; // after buttons: its callback uses them
};

// All shapes drawn as clickable thumbnails, so you can see a curve before picking it.
class ShapeGrid : public juce::Component
{
public:
    explicit ShapeGrid (juce::RangedAudioParameter& param)
        : attachment (param, [this] (float value)
                      {
                          selected = juce::jlimit (0, numShapes - 1, (int) value);
                          repaint();
                          if (onChange)
                              onChange();
                      })
    {
        attachment.sendInitialUpdate();
    }

    void step (int direction) { select ((selected + direction + numShapes) % numShapes); }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < numShapes; ++i)
        {
            const auto area = cell (i).toFloat();
            const bool on = i == selected;
            juce::Path outline;
            outline.addRoundedRectangle (area, 6.0f);

            if (on)
                juce::DropShadow (accent.withAlpha (0.35f), 8, {}).drawForPath (g, outline);
            else
                smallShadow.drawForPath (g, outline);
            const auto top = on ? accent.withMultipliedBrightness (0.32f) : face (i == hovered);
            g.setGradientFill ({ top, 0.0f, area.getY(), top.darker (0.3f), 0.0f, area.getBottom(), false });
            g.fillPath (outline);
            g.setColour (on ? accent : juce::Colours::white.withAlpha (0.07f));
            g.strokePath (outline, juce::PathStrokeType (on ? 1.5f : 1.0f));

            auto inner = area.reduced (7.0f, 5.0f);
            const auto name = inner.removeFromBottom (13.0f);
            g.setColour (on ? accent : text.withAlpha (0.6f));
            g.strokePath (shapePath (inner.reduced (0.0f, 2.0f), i, 1.0f, 0.0f, 1.0f), juce::PathStrokeType (1.6f));
            g.setColour (on ? text : dim);
            g.setFont (fonts->get (10.0f));
            g.drawText (shapes[i].name, name, juce::Justification::centredBottom);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (const int i = cellAt (e.getPosition()); i >= 0)
            select (i);
    }

    void mouseMove (const juce::MouseEvent& e) override { setHovered (cellAt (e.getPosition())); }
    void mouseExit (const juce::MouseEvent&) override { setHovered (-1); }

    std::function<void()> onChange;
    int selected = 0;

private:
    void select (int index) { attachment.setValueAsCompleteGesture ((float) index); }

    void setHovered (int index)
    {
        if (hovered != index)
        {
            hovered = index;
            repaint();
        }
    }

    juce::Rectangle<int> cell (int index) const
    {
        constexpr int columns = 8;
        const int width = getWidth() / columns, height = getHeight() / (numShapes / columns);
        return juce::Rectangle<int> ((index % columns) * width, (index / columns) * height, width, height).reduced (4);
    }

    int cellAt (juce::Point<int> point) const
    {
        for (int i = 0; i < numShapes; ++i)
            if (cell (i).contains (point))
                return i;
        return -1;
    }

    int hovered = -1;
    juce::SharedResourcePointer<Fonts> fonts;
    juce::ParameterAttachment attachment; // last: its callback uses the members above
};
}

//==============================================================================
class UndertowEditor : public juce::AudioProcessorEditor, private juce::Timer
{
    using APVTS = juce::AudioProcessorValueTreeState;

public:
    explicit UndertowEditor (UndertowProcessor& p)
        : AudioProcessorEditor (p), proc (p),
          grid (*p.params.getParameter ("shape")),
          rate (*p.params.getParameter ("rate")), trigger (*p.params.getParameter ("trigger"))
    {
        setLookAndFeel (&look);

        grid.onChange = [this] { repaint(); };
        previous.onClick = [this] { grid.step (-1); };
        next.onClick = [this] { grid.step (1); };

        addKnob (mix, "mix", " %");
        addKnob (shift, "shift", " %");
        addKnob (length, "length", " %");
        addKnob (thresh, "thresh", " dB");
        addKnob (xover, "xover", " Hz");

        for (auto [button, id] : { std::pair<juce::Button*, const char*> { &band, "band" }, { &loop, "loop" }, { &listen, "listen" } })
        {
            button->setClickingTogglesState (true);
            buttonLinks.add (new APVTS::ButtonAttachment (p.params, id, *button));
        }
        menu.onClick = [this] { showMenu(); };

        for (auto* c : std::initializer_list<juce::Component*> { &grid, &previous, &next, &rate, &trigger, &band, &loop, &listen, &menu })
            addAndMakeVisible (c);

        // Hidden until a newer release is found; opens the download page (fixed address, never one from the network)
        addChildComponent (update);
        update.setToggleState (true, juce::dontSendNotification);
        update.onClick = [] { juce::URL ("https://github.com/Siimas/undertow/releases/latest").launchInDefaultBrowser(); };

        setSize (740, 640);
        userScale = juce::jlimit (1.0f, 2.0f, finiteOr ((float) p.params.state.getProperty ("uiScale", 1.0f), 1.0f));
        setScaleFactor (1.0f);
        timerCallback();
        startTimerHz (30);
    }

    ~UndertowEditor() override { setLookAndFeel (nullptr); }

    // Hosts call this with the screen's scale; the user's chosen window size multiplies it.
    void setScaleFactor (float scale) override
    {
        hostScale = scale;
        AudioProcessorEditor::setScaleFactor (hostScale * userScale);
    }

    void paint (juce::Graphics& g) override
    {
        if (hardware)
        {
            drawMetal (g, getLocalBounds().toFloat(), juce::Colour (0xff111214));
        }
        else
        {
            g.setGradientFill ({ bgTop, 0.0f, 0.0f, bgBottom, 0.0f, (float) getHeight(), false });
            g.fillAll();
        }

        drawWordmark (g, *fonts, { 20.0f, 16.0f, 200.0f, 28.0f });

        g.setColour (text);
        g.setFont (fonts->get (15.0f));
        g.drawText (shapes[grid.selected].name, previous.getRight(), previous.getY(), next.getX() - previous.getRight(), previous.getHeight(),
                    juce::Justification::centred);

        paintScreen (g);
        drawCard (g, gridCard.toFloat());
        drawCard (g, controlCard.toFloat());

        g.setColour (dim);
        g.setFont (fonts->get (11.0f));
        for (auto [component, label] : { std::pair<juce::Component*, const char*> { &rate, "RATE" }, { &trigger, "TRIGGER" },
                                         { &mix, "MIX" }, { &shift, "SHIFT" }, { &length, "LENGTH" },
                                         { &thresh, "THRESHOLD" }, { &xover, "CROSSOVER" }, { &band, "BAND SPLIT" } })
            g.drawText (label, component->getX(), component->getY() - 17, component->getWidth(), 14, juce::Justification::centred);

        g.setColour (dim.withAlpha (0.6f));
        g.setFont (fonts->get (10.0f));
        g.drawText ("Undertow " JucePlugin_VersionString "  |  Free software, AGPL-3.0, no warranty  |  Built with JUCE  |  VST is a registered trademark of Steinberg Media Technologies GmbH",
                    0, getHeight() - 20, getWidth(), 14, juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (20, 14);
        area.removeFromBottom (12); // legal line

        auto header = area.removeFromTop (32);
        update.setBounds (header.withTrimmedLeft (228).removeFromLeft (150));
        menu.setBounds (header.removeFromRight (64));
        header.removeFromRight (8);
        header = header.removeFromRight (220);
        previous.setBounds (header.removeFromLeft (36));
        next.setBounds (header.removeFromRight (36));

        controlCard = area.removeFromBottom (196);
        auto controls = controlCard.reduced (14, 10);
        controls.removeFromTop (18);
        auto row = controls.removeFromTop (36);
        rate.setBounds (row.removeFromLeft (row.getWidth() * 3 / 5).withTrimmedRight (12));
        loop.setBounds (row.removeFromRight (64));
        trigger.setBounds (row.withTrimmedLeft (12));
        controls.removeFromTop (26);
        const int column = controls.getWidth() / 6;
        for (auto* knob : { &mix, &shift, &length, &thresh, &xover })
            knob->setBounds (controls.removeFromLeft (column));
        band.setBounds (controls.withSizeKeepingCentre (104, 34).translated (0, -12));
        listen.setBounds (controls.withSizeKeepingCentre (104, 30).translated (0, 24));

        area.removeFromBottom (14);
        gridCard = area.removeFromBottom (124);
        grid.setBounds (gridCard.reduced (8));

        area.removeFromBottom (14);
        screen = area.withTrimmedTop (10);
    }

private:
    // The display, sunk into the panel: grid, your audio's level, the shape, and a playhead riding it
    void paintScreen (juce::Graphics& g)
    {
        const auto frame = screen.toFloat();
        if (hardware) // a raised bezel around the glass
        {
            const auto bezel = frame.expanded (5.0f);
            g.setGradientFill ({ juce::Colour (0xff040405), 0.0f, bezel.getY(), juce::Colour (0xff2a2c31), 0.0f, bezel.getBottom(), false });
            g.fillRoundedRectangle (bezel, 13.0f);
        }
        g.setGradientFill ({ screenTop, 0.0f, frame.getY(), screenBottom, 0.0f, frame.getBottom(), false });
        g.fillRoundedRectangle (frame, 10.0f);
        {
            juce::Graphics::ScopedSaveState clip (g); // inner shadow along the top edge
            juce::Path outline;
            outline.addRoundedRectangle (frame, 10.0f);
            g.reduceClipRegion (outline);
            g.setGradientFill ({ juce::Colours::black.withAlpha (0.7f), 0.0f, frame.getY(), juce::Colours::transparentBlack, 0.0f, frame.getY() + 22.0f, false });
            g.fillRect (frame.withHeight (22.0f));
        }
        g.setGradientFill ({ juce::Colours::black.withAlpha (0.6f), 0.0f, frame.getY(), juce::Colours::white.withAlpha (0.10f), 0.0f, frame.getBottom(), false });
        g.drawRoundedRectangle (frame.reduced (0.5f), 10.0f, 1.0f);

        const auto plot = frame.reduced (16.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        for (int i = 1; i < 4; ++i)
        {
            g.fillRect (plot.getX() + plot.getWidth() * (float) i / 4.0f, plot.getY(), 1.0f, plot.getHeight());
            g.fillRect (plot.getX(), plot.getY() + plot.getHeight() * (float) i / 4.0f, plot.getWidth(), 1.0f);
        }

        const float phase = proc.displayPhase.load();
        const bool running = phase >= 0.0f && phase < 1.0f;
        const float mixNow = (float) mix.getValue() / 100.0f, shiftNow = (float) shift.getValue() / 100.0f,
                    lengthNow = juce::jmax (0.1f, (float) length.getValue() / 100.0f);

        if (running) // output level across the cycle, mirrored around the middle
        {
            juce::Path wave;
            const float mid = plot.getCentreY(), step = plot.getWidth() / (float) UndertowProcessor::scopeSize;
            for (int i = 0; i < UndertowProcessor::scopeSize; ++i)
            {
                const float half = juce::jlimit (0.0f, 1.0f, proc.scope[(size_t) i].load()) * plot.getHeight() * 0.5f;
                wave.addRectangle (plot.getX() + (float) i * step, mid - half, step + 0.5f, half * 2.0f);
            }
            g.setColour (juce::Colours::white.withAlpha (0.13f));
            g.fillPath (wave);
        }

        const auto curve = shapePath (plot, grid.selected, mixNow, shiftNow, lengthNow);
        juce::Path fill (curve);
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();
        g.setGradientFill ({ accent.withAlpha (0.30f), 0.0f, plot.getY(), accent.withAlpha (0.02f), 0.0f, plot.getBottom(), false });
        g.fillPath (fill);

        const juce::PathStrokeType::JointStyle joint = juce::PathStrokeType::curved;
        g.setColour (accent.withAlpha (0.15f)); // soft glow under the line
        g.strokePath (curve, juce::PathStrokeType (7.0f, joint));
        g.setColour (accent);
        g.strokePath (curve, juce::PathStrokeType (2.2f, joint));

        if (hardware) // glare across the glass
        {
            juce::Graphics::ScopedSaveState clip (g);
            juce::Path glass;
            glass.addRoundedRectangle (frame, 10.0f);
            g.reduceClipRegion (glass);
            juce::Path glare;
            glare.addQuadrilateral (frame.getX(), frame.getY(), frame.getX() + frame.getWidth() * 0.62f, frame.getY(),
                                    frame.getX() + frame.getWidth() * 0.40f, frame.getBottom(), frame.getX(), frame.getBottom());
            g.setGradientFill ({ juce::Colours::white.withAlpha (0.09f), frame.getX(), frame.getY(), juce::Colours::white.withAlpha (0.01f), frame.getCentreX(), frame.getBottom(), false });
            g.fillPath (glare);
        }

        if (running)
        {
            const float gain = duckGain (grid.selected, shiftedPhase (phase, shiftNow) / lengthNow, mixNow);
            const juce::Point<float> dot (plot.getX() + phase * plot.getWidth(), plot.getBottom() - gain * plot.getHeight());
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
        const int mode = (int) proc.params.getRawParameterValue ("trigger")->load();
        xover.setEnabled (band.getToggleState());
        thresh.setEnabled (mode == audio);
        shift.setEnabled (mode == sync);
        loop.setEnabled (mode != sync);
        listen.setEnabled (band.getToggleState());
        band.setButtonText (band.getToggleState() ? "Low only" : "Full range");
        if (updates.available && ! update.isVisible())
        {
            update.setButtonText ("Update to " + updates.latest);
            update.setVisible (true);
        }
        repaint (screen);
    }

    void showMenu()
    {
        juce::PopupMenu sizes;
        for (int percent : { 100, 125, 150, 200 })
            sizes.addItem (percent, juce::String (percent) + "%", true, std::abs (userScale * 100.0f - (float) percent) < 1.0f);

        juce::PopupMenu items;
        items.setLookAndFeel (&look);
        items.addSubMenu ("Window size", sizes);
        items.addItem (1, "Manual");
        items.addItem (2, "Get the latest version");
        items.addSeparator();
        items.addItem (3, "Undertow " JucePlugin_VersionString "  -  AGPL-3.0", false);
        items.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menu),
                             [safe = juce::Component::SafePointer<UndertowEditor> (this)] (int choice)
                             {
                                 if (safe == nullptr)
                                     return;
                                 if (choice >= 100)
                                 {
                                     safe->userScale = juce::jlimit (1.0f, 2.0f, (float) choice / 100.0f);
                                     safe->proc.params.state.setProperty ("uiScale", safe->userScale, nullptr); // saved with the project
                                     safe->setScaleFactor (safe->hostScale);
                                 }
                                 else if (choice == 1)
                                     juce::URL ("https://github.com/Siimas/undertow#using-it").launchInDefaultBrowser();
                                 else if (choice == 2)
                                     juce::URL ("https://github.com/Siimas/undertow/releases/latest").launchInDefaultBrowser();
                             });
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

    UndertowProcessor& proc;
    juce::SharedResourcePointer<Fonts> fonts;
    UndertowLook look; // before the controls, so it outlives them
    ShapeGrid grid;
    juce::TextButton previous { "<" }, next { ">" }, band, update, loop { "Loop" }, listen { "Listen" }, menu { "Menu" };
    float hostScale = 1.0f, userScale = 1.0f;
    Segmented rate, trigger;
    juce::Slider mix, shift, length, thresh, xover;
    juce::Rectangle<int> screen, gridCard, controlCard;
    // After the controls, so the attachments are destroyed first
    juce::OwnedArray<APVTS::ButtonAttachment> buttonLinks;
    juce::OwnedArray<APVTS::SliderAttachment> knobLinks;
    UpdateCheck updates; // last: started once everything else exists, stopped first
};

juce::AudioProcessorEditor* UndertowProcessor::createEditor() { return new UndertowEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new UndertowProcessor(); }
