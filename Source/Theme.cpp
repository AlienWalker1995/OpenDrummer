#include "Theme.h"

// Decibels lives in juce_audio_basics, which the header does not pull in.
#include <juce_audio_basics/juce_audio_basics.h>

namespace theme
{
namespace
{
    // Bahnschrift is the squarest technical face that ships with Windows - the
    // nearest thing to the lettering silk-screened on period faceplates.
    const char* kLegendTypeface = "Bahnschrift";

    // File scope: MSVC will not let a lambda read a function-local constexpr
    // without an explicit capture.
    constexpr int kSegments = 40;
}

juce::FontOptions legendFont (float height, bool bold)
{
    return juce::FontOptions (kLegendTypeface, height, bold ? juce::Font::bold : juce::Font::plain);
}

juce::FontOptions readoutFont (float height)
{
    return juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, 0);
}

//==============================================================================
void drawBrushedMetal (juce::Graphics& g, juce::Rectangle<float> bounds, float corner)
{
    // Vertical light falloff, then horizontal grain over the top. The grain is
    // what separates brushed aluminium from flat grey, and it has to run
    // horizontally - vertical streaks read as plastic.
    g.setGradientFill (juce::ColourGradient (alu.brighter (0.10f), bounds.getCentreX(), bounds.getY(),
                                             aluMid, bounds.getCentreX(), bounds.getBottom(), false));

    if (corner > 0.0f)
        g.fillRoundedRectangle (bounds, corner);
    else
        g.fillRect (bounds);

    juce::Random rng (0x5eed);

    for (float y = bounds.getY(); y < bounds.getBottom(); y += 2.0f)
    {
        const float a = rng.nextFloat() * 0.045f;
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (a));
        g.drawHorizontalLine ((int) y, bounds.getX(), bounds.getRight());
    }
}

void drawGlassWindow (juce::Graphics& g, juce::Rectangle<float> bounds, float corner)
{
    // Shadow cast by the faceplate onto the recessed glass.
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.fillRoundedRectangle (bounds.expanded (1.5f), corner + 1.0f);

    g.setGradientFill (juce::ColourGradient (glass, bounds.getCentreX(), bounds.getY(),
                                             glassDeep, bounds.getCentreX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, corner);

    // Depth. The faceplate overhangs the glass, so its top edge throws a soft
    // shadow down into the window...
    const float shadowDepth = juce::jmin (10.0f, bounds.getHeight() * 0.25f);

    if (shadowDepth > 2.0f)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.45f),
                                                 bounds.getCentreX(), bounds.getY(),
                                                 juce::Colours::transparentBlack,
                                                 bounds.getCentreX(), bounds.getY() + shadowDepth, false));
        g.fillRoundedRectangle (bounds.withHeight (shadowDepth), corner);
    }

    // ...and the glass catches a faint diagonal reflection. Kept very low: it
    // has to read as glass without costing the text any contrast.
    if (bounds.getHeight() > 30.0f)
    {
        juce::Path clip;
        clip.addRoundedRectangle (bounds, corner);

        juce::Path glare;
        glare.startNewSubPath (bounds.getX(), bounds.getY());
        glare.lineTo (bounds.getX() + bounds.getWidth() * 0.42f, bounds.getY());
        glare.lineTo (bounds.getX() + bounds.getWidth() * 0.22f, bounds.getBottom());
        glare.lineTo (bounds.getX(), bounds.getBottom());
        glare.closeSubPath();

        g.saveState();
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.035f),
                                                 bounds.getX(), bounds.getY(),
                                                 juce::Colours::transparentWhite,
                                                 bounds.getX() + bounds.getWidth() * 0.3f, bounds.getBottom(), false));
        g.fillPath (glare);
        g.restoreState();
    }

    g.setColour (glassEdge.withAlpha (0.8f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);

    // Metal lip catching light along the bottom edge of the cutout.
    g.setColour (alu.withAlpha (0.4f));
    g.drawLine (bounds.getX() + corner, bounds.getBottom() + 1.0f,
                bounds.getRight() - corner, bounds.getBottom() + 1.0f, 1.0f);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> bounds, float corner, bool raised)
{
    if (raised)
    {
        drawBrushedMetal (g, bounds, corner);
        g.setColour (aluEdge.withAlpha (0.8f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);
    }
    else
    {
        drawGlassWindow (g, bounds, corner);
    }
}

//==============================================================================
void drawDialText (juce::Graphics& g, const juce::String& textToDraw,
                   juce::Rectangle<int> bounds, juce::Justification justification,
                   float fontSize, juce::Colour colour)
{
    g.setFont (readoutFont (fontSize));

    // Offset passes stand in for the halo a lamp throws on the glass; JUCE has
    // no blur and this costs nothing.
    g.setColour (colour.withAlpha (0.18f));
    g.drawText (textToDraw, bounds.translated (0, -1), justification, false);
    g.drawText (textToDraw, bounds.translated (1, 1), justification, false);
    g.drawText (textToDraw, bounds.translated (-1, 1), justification, false);

    g.setColour (colour);
    g.drawText (textToDraw, bounds, justification, false);
}

//==============================================================================
void drawWordmark (juce::Graphics& g, juce::Rectangle<int> bounds, float height)
{
    const juce::String mark = "OpenDrummer";
    const auto font = legendFont (height, true);

    g.setFont (font);

    // Tracking placed by hand; JUCE has no letter-spacing control, and tight
    // default spacing is most of what makes a name read as a label not a mark.
    const float tracking = height * 0.14f;
    float x = (float) bounds.getX();
    const float baseline = (float) bounds.getCentreY() + height * 0.33f;

    for (int i = 0; i < mark.length(); ++i)
    {
        const auto glyph = mark.substring (i, i + 1);

        // Engraved: a light line under a dark one, as though cut into metal.
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawSingleLineText (glyph, juce::roundToInt (x), juce::roundToInt (baseline) + 1);

        g.setColour (silk);
        g.drawSingleLineText (glyph, juce::roundToInt (x), juce::roundToInt (baseline));

        juce::GlyphArrangement arrangement;
        arrangement.addLineOfText (juce::Font (font), glyph, 0.0f, 0.0f);
        x += arrangement.getBoundingBox (0, -1, true).getWidth() + tracking;
    }
}

//==============================================================================
void drawWalnutCheek (juce::Graphics& g, juce::Rectangle<float> bounds, bool lightFromLeft)
{
    g.setGradientFill (juce::ColourGradient (
        lightFromLeft ? walnut : walnutDark, bounds.getX(), bounds.getY(),
        lightFromLeft ? walnutDark : walnut, bounds.getRight(), bounds.getY(), false));
    g.fillRect (bounds);

    // Grain. Even spacing would read as stripes, so these are uneven.
    const float offsets[] = { 0.13f, 0.27f, 0.38f, 0.55f, 0.71f, 0.84f, 0.93f };

    g.setColour (walnutDark.withAlpha (0.5f));

    for (const float o : offsets)
        g.fillRect (bounds.getX() + bounds.getWidth() * o, bounds.getY(), 1.0f, bounds.getHeight());

    // Hard shadow where the cabinet meets the faceplate.
    g.setColour (juce::Colours::black.withAlpha (0.5f));

    if (lightFromLeft)
        g.fillRect (bounds.getRight() - 1.5f, bounds.getY(), 1.5f, bounds.getHeight());
    else
        g.fillRect (bounds.getX(), bounds.getY(), 1.5f, bounds.getHeight());
}

//==============================================================================
void drawVuMeter (juce::Graphics& g, juce::Rectangle<float> bounds, float needleVu, bool peakLit)
{
    // A VU scale is linear in voltage, not in decibels: +3 VU is full swing and
    // 0 VU sits at about 71%. That is why the -20 to -5 marks bunch up on the
    // left - drawing them evenly spaced is the tell of a fake meter.
    const auto swing = [] (float vu)
    {
        const float clamped = juce::jlimit (-22.0f, 3.5f, vu);
        return std::pow (10.0f, clamped / 20.0f) / std::pow (10.0f, 3.0f / 20.0f);
    };

    // 38 degrees either side keeps the end labels inside a 3:2 face; wider and
    // the 20 and +3 fall off its edges.
    const float halfAngle = juce::degreesToRadians (38.0f);
    const auto angleFor = [&] (float vu) { return -halfAngle + 2.0f * halfAngle * swing (vu); };

    // --- bezel and face -------------------------------------------------------
    g.setColour (juce::Colour (0xff0b0b0a));
    g.fillRoundedRectangle (bounds, 3.0f);

    const auto face = bounds.reduced (2.5f);

    // Lit from behind, the lamp low in the housing: warm and bright at the
    // bottom centre, falling off to amber at the corners.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6e8bd), face.getCentreX(), face.getBottom(),
                                             juce::Colour (0xffb98a3e), face.getX(), face.getY(), true));
    g.fillRoundedRectangle (face, 2.0f);

    g.saveState();
    g.reduceClipRegion (face.toNearestInt());

    // The pivot sits below the visible face, as on the real movement, so the
    // needle sweeps in from off-panel.
    const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.42f);
    const float arcRadius = face.getHeight() * 0.98f;

    const auto pointAt = [&] (float angle, float radius)
    {
        return juce::Point<float> (pivot.x + radius * std::sin (angle), pivot.y - radius * std::cos (angle));
    };

    const juce::Colour ink { 0xff201a12 };
    const juce::Colour redZone { 0xffb3261e };

    // Red band above 0 VU.
    {
        juce::Path band;
        band.addCentredArc (pivot.x, pivot.y, arcRadius, arcRadius, 0.0f, angleFor (0.0f), angleFor (3.0f), true);
        g.setColour (redZone);
        g.strokePath (band, juce::PathStrokeType (3.2f));
    }

    // Scale arc left of zero.
    {
        juce::Path arc;
        arc.addCentredArc (pivot.x, pivot.y, arcRadius, arcRadius, 0.0f, angleFor (-20.0f), angleFor (0.0f), true);
        g.setColour (ink);
        g.strokePath (arc, juce::PathStrokeType (1.0f));
    }

    struct Mark { float vu; const char* label; };
    const Mark marks[] = { { -20.0f, "20" }, { -10.0f, "10" }, { -7.0f, "7" }, { -5.0f, "5" },
                           { -3.0f, "3" }, { -2.0f, "" }, { -1.0f, "" }, { 0.0f, "0" },
                           { 1.0f, "" }, { 2.0f, "" }, { 3.0f, "+3" } };

    g.setFont (legendFont (juce::jlimit (7.0f, 9.5f, face.getHeight() * 0.13f), true));

    for (const auto& mark : marks)
    {
        const float a = angleFor (mark.vu);
        const bool hot = mark.vu > 0.0f;
        const bool labelled = mark.label[0] != 0;

        g.setColour (hot ? redZone : ink);
        g.drawLine ({ pointAt (a, arcRadius - (labelled ? 6.0f : 4.0f)), pointAt (a, arcRadius + 1.0f) },
                    labelled ? 1.3f : 1.0f);

        if (labelled)
        {
            const auto at = pointAt (a, arcRadius + 7.0f);
            g.drawText (mark.label, juce::Rectangle<float> (22.0f, 11.0f).withCentre (at),
                        juce::Justification::centred, false);
        }
    }

    g.setColour (ink.withAlpha (0.8f));
    g.setFont (legendFont (juce::jlimit (8.0f, 11.0f, face.getHeight() * 0.17f), true));
    g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.52f).withHeight (face.getHeight() * 0.25f),
                juce::Justification::centred, false);

    // --- needle ---------------------------------------------------------------
    const float needleAngle = angleFor (needleVu);
    const auto tip = pointAt (needleAngle, arcRadius + 3.0f);

    g.setColour (juce::Colours::black.withAlpha (0.18f));   // faint shadow on the face
    g.drawLine ({ pivot.translated (1.5f, 1.0f), tip.translated (1.5f, 1.0f) }, 1.4f);

    g.setColour (juce::Colour (0xff111111));
    g.drawLine ({ pivot, tip }, 1.2f);

    g.restoreState();

    // --- peak lamp: the needle is far too slow to catch a drum clipping -----
    const auto lamp = juce::Rectangle<float> (6.0f, 6.0f)
                        .withCentre ({ face.getRight() - 9.0f, face.getY() + 8.0f });

    if (peakLit)
    {
        g.setColour (juce::Colour (0xffff3b2f).withAlpha (0.35f));
        g.fillEllipse (lamp.expanded (3.0f));
    }

    g.setColour (peakLit ? juce::Colour (0xffff4a3a) : juce::Colour (0xff5a1d17));
    g.fillEllipse (lamp);

    g.setColour (ink.withAlpha (0.75f));
    g.setFont (legendFont (7.5f, true));
    g.drawText ("PEAK", juce::Rectangle<float> (30.0f, 9.0f).withCentre ({ lamp.getCentreX() - 20.0f, lamp.getCentreY() }),
                juce::Justification::centredRight, false);

    // A touch of glass over the whole face.
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), face.getX(), face.getY(),
                                             juce::Colours::transparentWhite, face.getX(), face.getCentreY(), false));
    g.fillRoundedRectangle (face, 2.0f);
}

//==============================================================================
void drawSegmentMeter (juce::Graphics& g, juce::Rectangle<float> bounds, float level, float hold)
{
    drawGlassWindow (g, bounds, 2.0f);

    const float gap = 1.5f;
    const auto inner = bounds.reduced (2.0f);
    const float segmentWidth = (inner.getWidth() - gap * (kSegments - 1)) / (float) kSegments;

    const auto toSegment = [] (float linear)
    {
        const float db = juce::Decibels::gainToDecibels (linear, -60.0f);
        return juce::jmap (juce::jlimit (-60.0f, 6.0f, db), -60.0f, 6.0f, 0.0f, (float) kSegments);
    };

    const int lit = (int) toSegment (level);
    const int holdSegment = (int) toSegment (hold);

    for (int i = 0; i < kSegments; ++i)
    {
        const auto segment = juce::Rectangle<float> (
            inner.getX() + (float) i * (segmentWidth + gap),
            inner.getY(), segmentWidth, inner.getHeight());

        const float position = (float) i / (float) kSegments;
        const auto colour = position > 0.92f ? red.brighter (0.6f)
                          : position > 0.80f ? mustard
                                             : dial;

        if (i < lit)
        {
            g.setColour (colour);
            g.fillRect (segment);
        }
        else if (i == holdSegment && holdSegment > 0)
        {
            g.setColour (colour.withAlpha (0.8f));
            g.fillRect (segment);
        }
        else
        {
            g.setColour (colour.withAlpha (0.10f));
            g.fillRect (segment);
        }
    }
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, alu);

    // Default label ink is silkscreen: most labels sit on the faceplate.
    setColour (juce::Label::textColourId, silk);

    setColour (juce::TextButton::textColourOnId, silk);
    setColour (juce::TextButton::textColourOffId, silk);

    // Combo boxes are little glass windows, so their text is backlit.
    setColour (juce::ComboBox::backgroundColourId, glass);
    setColour (juce::ComboBox::textColourId, dial);
    setColour (juce::ComboBox::outlineColourId, aluEdge);
    setColour (juce::ComboBox::arrowColourId, dial);

    setColour (juce::PopupMenu::backgroundColourId, glass);
    setColour (juce::PopupMenu::textColourId, inkOnGlass);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, dial.withAlpha (0.28f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    setColour (juce::TextEditor::backgroundColourId, glass);
    setColour (juce::TextEditor::textColourId, inkOnGlass);
    setColour (juce::TextEditor::outlineColourId, aluEdge);
    setColour (juce::TextEditor::highlightColourId, dial.withAlpha (0.3f));

    setColour (juce::ListBox::backgroundColourId, glass);
    setColour (juce::ListBox::textColourId, inkOnGlass);

    setColour (juce::ScrollBar::thumbColourId, aluDark);

    setColour (juce::Slider::textBoxTextColourId, silk);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    setColour (juce::ToggleButton::textColourId, silkDim);
    setColour (juce::ToggleButton::tickColourId, orange);

    setColour (juce::GroupComponent::outlineColourId, aluEdge);
    setColour (juce::GroupComponent::textColourId, silkDim);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                        const juce::Colour&,
                                        bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();
    const float corner = 2.5f;

    // Chunky keys with real travel. Lit ones take the CR-78's warm preset
    // colour; the rest are bare metal.
    if (on || down)
    {
        g.setGradientFill (juce::ColourGradient (orange.brighter (0.22f), bounds.getCentreX(), bounds.getY(),
                                                 orange.darker (0.28f), bounds.getCentreX(), bounds.getBottom(),
                                                 false));
        g.fillRoundedRectangle (bounds, corner);

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (
            (highlighted ? juce::Colours::white : alu.brighter (0.06f)), bounds.getCentreX(), bounds.getY(),
            aluDark, bounds.getCentreX(), bounds.getBottom(), false));
        g.fillRoundedRectangle (bounds, corner);

        g.setColour (aluEdge);
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }

    // Shadow under the key, so it sits proud of the panel.
    g.setColour (juce::Colours::black.withAlpha (0.20f));
    g.drawLine (bounds.getX() + corner, bounds.getBottom(),
                bounds.getRight() - corner, bounds.getBottom(), 1.0f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                  bool, bool)
{
    g.setColour (button.isEnabled() ? silk : silkFaint);
    g.setFont (legendFont (12.0f, true));

    g.drawText (button.getButtonText(), button.getLocalBounds(),
                juce::Justification::centred, false);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                    bool highlighted, bool)
{
    auto bounds = button.getLocalBounds().toFloat();
    const float boxSize = juce::jmin (15.0f, bounds.getHeight() - 2.0f);

    auto box = juce::Rectangle<float> (bounds.getX(),
                                       bounds.getCentreY() - boxSize * 0.5f,
                                       boxSize, boxSize);

    g.setGradientFill (juce::ColourGradient (aluDark, box.getCentreX(), box.getY(),
                                             alu, box.getCentreX(), box.getBottom(), false));
    g.fillRoundedRectangle (box, 2.0f);

    g.setColour (highlighted ? orange : aluEdge);
    g.drawRoundedRectangle (box.reduced (0.5f), 2.0f, 1.0f);

    if (button.getToggleState())
    {
        g.setColour (orange);
        g.fillRoundedRectangle (box.reduced (3.5f), 1.0f);
    }

    g.setColour (button.findColour (juce::ToggleButton::textColourId));
    g.setFont (legendFont (12.0f));
    g.drawText (button.getButtonText(),
                bounds.withTrimmedLeft (boxSize + 8.0f),
                juce::Justification::centredLeft, false);
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                    float sliderPos, float, float,
                                    juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                          0.0f, 0.0f, style, slider);
        return;
    }

    const float centreY = (float) y + (float) height * 0.5f;

    // A slot routed into the faceplate.
    auto slot = juce::Rectangle<float> ((float) x, centreY - 3.0f, (float) width, 6.0f);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (slot, 3.0f);

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawLine (slot.getX(), slot.getBottom() + 1.0f, slot.getRight(), slot.getBottom() + 1.0f, 1.0f);

    auto filled = slot.reduced (1.5f, 1.5f).withRight (sliderPos);

    if (filled.getWidth() > 1.0f)
    {
        g.setColour (orange.withAlpha (0.92f));
        g.fillRoundedRectangle (filled, 1.5f);
    }

    // Fader cap: a milled metal lozenge with a centre line.
    const float capWidth = 13.0f;
    const float capHeight = juce::jmin (22.0f, (float) height - 2.0f);

    auto cap = juce::Rectangle<float> (capWidth, capHeight)
                 .withCentre ({ sliderPos, centreY });

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (cap.translated (0.0f, 1.5f), 2.0f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f2ec), cap.getCentreX(), cap.getY(),
                                             juce::Colour (0xff85827a), cap.getCentreX(), cap.getBottom(),
                                             false));
    g.fillRoundedRectangle (cap, 2.0f);

    g.setColour (aluEdge);
    g.drawRoundedRectangle (cap.reduced (0.5f), 2.0f, 1.0f);

    g.setColour (juce::Colour (0xff3a3833));
    g.fillRect (cap.getCentreX() - 0.75f, cap.getY() + 3.0f, 1.5f, cap.getHeight() - 6.0f);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                    float position, float startAngle, float endAngle,
                                    juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();

    const float scaleRadius = size * 0.5f - 1.0f;
    const float knobRadius = size * 0.37f;
    const float angle = startAngle + position * (endAngle - startAngle);

    // --- scale, silk-screened on the faceplate around the knob ---------------
    constexpr int kTicks = 11;

    for (int i = 0; i < kTicks; ++i)
    {
        const float a = startAngle + (float) i / (float) (kTicks - 1) * (endAngle - startAngle);
        const bool major = (i == 0 || i == kTicks - 1 || i == kTicks / 2);

        const float inner = knobRadius + 4.0f;
        const float outer = major ? scaleRadius : scaleRadius - 3.0f;

        const juce::Point<float> from (centre.x + inner * std::sin (a), centre.y - inner * std::cos (a));
        const juce::Point<float> to   (centre.x + outer * std::sin (a), centre.y - outer * std::cos (a));

        g.setColour (silk.withAlpha (major ? 0.85f : 0.5f));
        g.drawLine ({ from, to }, major ? 1.5f : 1.0f);
    }

    const auto skirt = juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre);

    // --- shadow on the faceplate ----------------------------------------------
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (skirt.translated (0.0f, 2.5f).expanded (1.0f));

    // --- knurled skirt: the grip ring ----------------------------------------
    g.setColour (juce::Colour (0xff2e2c29));
    g.fillEllipse (skirt);

    constexpr int kRidges = 44;

    for (int r = 0; r < kRidges; ++r)
    {
        const float a = (float) r * juce::MathConstants<float>::twoPi / (float) kRidges;
        const juce::Point<float> from (centre.x + knobRadius * 0.80f * std::sin (a), centre.y - knobRadius * 0.80f * std::cos (a));
        const juce::Point<float> to   (centre.x + knobRadius * 0.99f * std::sin (a), centre.y - knobRadius * 0.99f * std::cos (a));

        // Ridges facing the light (top-left) catch it; the rest stay dark.
        const float lit = 0.5f + 0.5f * std::cos (a + juce::MathConstants<float>::pi * 0.25f);
        g.setColour (juce::Colours::white.withAlpha (0.05f + 0.20f * lit));
        g.drawLine ({ from, to }, 1.0f);
    }

    // --- face: machined aluminium ---------------------------------------------
    const auto face = skirt.reduced (knobRadius * 0.22f);

    g.setGradientFill (juce::ColourGradient (alu.brighter (0.30f), face.getX(), face.getY(),
                                             aluDark.darker (0.10f), face.getRight(), face.getBottom(), false));
    g.fillEllipse (face);

    // Concentric lathe marks - what makes it read as turned metal, not plastic.
    for (int ring = 1; ring <= 4; ++ring)
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawEllipse (face.reduced (face.getWidth() * 0.085f * (float) ring), 0.7f);
    }

    g.setColour (juce::Colours::black.withAlpha (slider.isMouseOverOrDragging() ? 0.55f : 0.35f));
    g.drawEllipse (face, 1.0f);

    // --- indicator: a machined slot, as on late-70s receiver knobs ------------
    const float faceRadius = face.getWidth() * 0.5f;
    juce::Path slot;
    slot.addRoundedRectangle (-1.7f, -faceRadius * 0.92f, 3.4f, faceRadius * 0.58f, 1.3f);

    const auto rotate = juce::AffineTransform::rotation (angle).translated (centre.x, centre.y);

    g.setColour (juce::Colour (0xff1b1a18));
    g.fillPath (slot, rotate);

    // A lit lower edge so the slot reads as cut in, not painted on.
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.strokePath (slot, juce::PathStrokeType (0.6f), rotate.translated (0.4f, 0.6f));
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    drawGlassWindow (g, bounds, 2.5f);

    if (box.isMouseOver())
    {
        g.setColour (dial.withAlpha (0.4f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 2.5f, 1.0f);
    }

    const float cx = (float) width - 13.0f;
    const float cy = (float) height * 0.5f;

    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.5f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);

    g.setColour (dial);
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (readoutFont (12.0f));
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (juce::Font (readoutFont (12.0f)));
}

void LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height,
                                            juce::TextEditor&)
{
    drawGlassWindow (g, juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height)
                          .reduced (0.5f), 2.5f);
}

void LookAndFeel::drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&)
{
    // The glass window already draws its own edge.
}

void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&,
                                 int x, int y, int width, int height,
                                 bool isVertical, int thumbStart, int thumbSize,
                                 bool isMouseOver, bool isMouseDown)
{
    if (thumbSize <= 0)
        return;

    auto thumb = isVertical
        ? juce::Rectangle<float> ((float) x + (float) width * 0.32f, (float) thumbStart,
                                  (float) width * 0.36f, (float) thumbSize)
        : juce::Rectangle<float> ((float) thumbStart, (float) y + (float) height * 0.32f,
                                  (float) thumbSize, (float) height * 0.36f);

    g.setColour (isMouseDown ? dial
               : isMouseOver ? aluMid
                             : aluDark.withAlpha (0.7f));

    g.fillRoundedRectangle (thumb, juce::jmin (thumb.getWidth(), thumb.getHeight()) * 0.5f);
}

} // namespace theme
