#include "KitView.h"
#include "Theme.h"

using midimap::Articulation;

// Laid out roughly as the kit sits in front of you: hats left, ride right,
// toms across the middle, snare and kick front and centre.
const std::array<KitView::Pad, KitView::NumPads> KitView::pads =
{{
    { "KICK",    { 0.39f, 0.50f, 0.25f, 0.36f }, false },
    { "SNARE",   { 0.19f, 0.55f, 0.19f, 0.25f }, false },
    { "HI-HAT",  { 0.02f, 0.33f, 0.19f, 0.19f }, true  },
    { "TOM 1",   { 0.27f, 0.19f, 0.15f, 0.19f }, false },
    { "TOM 2",   { 0.45f, 0.16f, 0.16f, 0.20f }, false },
    { "TOM 3",   { 0.63f, 0.19f, 0.15f, 0.19f }, false },
    { "TOM 4",   { 0.67f, 0.59f, 0.17f, 0.22f }, false },
    { "CRASH 1", { 0.03f, 0.02f, 0.20f, 0.18f }, true  },
    { "CRASH 2", { 0.77f, 0.02f, 0.20f, 0.18f }, true  },
    { "RIDE",    { 0.78f, 0.32f, 0.21f, 0.20f }, true  }
}};

namespace
{
    /** The colour of an incandescent lamp at a given brightness. A filament
        runs warm white when fully driven and cools through amber to a deep
        orange as it dims, so a fading hit changes colour rather than just
        turning transparent - that shift is what reads as a real bulb. */
    juce::Colour lampColour (float glow)
    {
        const juce::Colour warmWhite  { 0xffffe7b0 };
        const juce::Colour amber      { 0xffffb347 };
        const juce::Colour deepOrange { 0xffe0521c };

        if (glow > 0.6f)
            return amber.interpolatedWith (warmWhite, (glow - 0.6f) / 0.4f);

        return deepOrange.interpolatedWith (amber, juce::jlimit (0.0f, 1.0f, glow / 0.6f));
    }

    constexpr double kLampTestSeconds = 1.3;
}

KitView::KitView()
{
    glowLevels.fill (0.0f);
    startTimerHz (60);
}

KitView::PadId KitView::padForArticulation (Articulation a)
{
    switch (a)
    {
        case Articulation::Kick:            return PadKick;

        case Articulation::SnareHead:
        case Articulation::SnareRimshot:
        case Articulation::SnareCrossStick: return PadSnare;

        case Articulation::Tom1:
        case Articulation::Tom1Rim:         return PadTom1;
        case Articulation::Tom2:
        case Articulation::Tom2Rim:         return PadTom2;
        case Articulation::Tom3:
        case Articulation::Tom3Rim:         return PadTom3;
        case Articulation::Tom4:
        case Articulation::Tom4Rim:         return PadTom4;

        case Articulation::HiHatClosedBow:
        case Articulation::HiHatClosedEdge:
        case Articulation::HiHatOpenBow:
        case Articulation::HiHatOpenEdge:
        case Articulation::HiHatPedal:      return PadHiHat;

        case Articulation::Crash1Bow:
        case Articulation::Crash1Edge:      return PadCrash1;
        case Articulation::Crash2Bow:
        case Articulation::Crash2Edge:      return PadCrash2;

        case Articulation::RideBow:
        case Articulation::RideEdge:
        case Articulation::RideBell:        return PadRide;

        case Articulation::None:
        case Articulation::NumArticulations:
        default:                            return NumPads;
    }
}

void KitView::registerHit (Articulation articulation, float velocity)
{
    const auto pad = padForArticulation (articulation);

    if (pad == NumPads)
        return;

    // Keep the brighter of the lamp's current state and this hit, so a soft
    // hit never visibly cuts short a loud one still cooling.
    glowLevels[(size_t) pad] = juce::jmax (glowLevels[(size_t) pad],
                                           juce::jlimit (0.25f, 1.0f, velocity));
}

void KitView::timerCallback()
{
    bool active = hiHatOpenness > 0.01f;

    if (lampTestStart > 0.0)
    {
        const double elapsed = juce::Time::getMillisecondCounterHiRes() / 1000.0 - lampTestStart;

        if (elapsed > kLampTestSeconds)
        {
            lampTestStart = 0.0;
        }
        else
        {
            // Sweep left to right: each lamp comes on as the sweep passes its
            // position, then cools on its own like any other hit.
            for (size_t i = 0; i < pads.size(); ++i)
            {
                const double position = pads[i].relativeBounds.getCentreX();
                const double onAt = position * 0.75;

                if (elapsed >= onAt && elapsed < onAt + 0.05)
                    glowLevels[i] = juce::jmax (glowLevels[i], 0.85f);
            }

            active = true;
        }
    }

    for (auto& glow : glowLevels)
    {
        if (glow > 0.0f)
        {
            // Filaments cool fastest while they are hottest.
            glow = juce::jmax (0.0f, glow - (0.022f + glow * 0.030f));
            active = true;
        }
    }

    if (active)
        repaint();
}

juce::Rectangle<float> KitView::boundsFor (const Pad& pad) const
{
    const auto area = getLocalBounds().toFloat().reduced (6.0f);

    return { area.getX() + pad.relativeBounds.getX() * area.getWidth(),
             area.getY() + pad.relativeBounds.getY() * area.getHeight(),
             pad.relativeBounds.getWidth() * area.getWidth(),
             pad.relativeBounds.getHeight() * area.getHeight() };
}

void KitView::drawPad (juce::Graphics& g, const Pad& pad, float glow) const
{
    auto bounds = boundsFor (pad);

    // Cymbals are printed flat, as seen from the drum stool, so drums and
    // metal differ in shape and never depend on colour to tell apart.
    if (pad.isCymbal)
        bounds = bounds.withSizeKeepingCentre (bounds.getWidth(), bounds.getHeight() * 0.50f);

    const bool lit = glow > 0.01f;
    const auto lamp = lampColour (glow);
    const auto printed = theme::dial.withAlpha (0.42f);

    // --- lamp light, behind the printing --------------------------------------
    if (lit)
    {
        // Bloom spilling onto the glass around the legend.
        for (int ring = 3; ring >= 1; --ring)
        {
            const float spread = 1.0f + 0.06f * (float) ring * glow;
            g.setColour (lamp.withAlpha (glow * 0.07f));
            g.fillEllipse (bounds.withSizeKeepingCentre (bounds.getWidth() * spread,
                                                         bounds.getHeight() * spread));
        }

        // The lit area itself: hottest at the centre, where the filament is.
        g.setGradientFill (juce::ColourGradient (lamp.withAlpha (juce::jmin (1.0f, glow * 1.1f)),
                                                 bounds.getCentreX(), bounds.getCentreY(),
                                                 lamp.withAlpha (glow * 0.55f),
                                                 bounds.getX(), bounds.getY(), true));
        g.fillEllipse (pad.isCymbal ? bounds : bounds.reduced (bounds.getWidth() * 0.08f));
    }

    const auto line = lit ? lamp.withAlpha (0.55f + 0.45f * glow) : printed;

    // --- the printed outline ----------------------------------------------------
    if (pad.isCymbal)
    {
        g.setColour (line);
        g.drawEllipse (bounds, 1.5f);

        // Lathe rings and the bell.
        g.setColour (line.withMultipliedAlpha (0.6f));

        for (const float scale : { 0.72f, 0.46f })
            g.drawEllipse (bounds.withSizeKeepingCentre (bounds.getWidth() * scale, bounds.getHeight() * scale), 0.9f);

        g.setColour (line);
        g.drawEllipse (bounds.withSizeKeepingCentre (bounds.getWidth() * 0.16f, bounds.getHeight() * 0.32f), 1.2f);

        // A part-open hi-hat shows its top cymbal lifted clear of the bottom one.
        if (pad.label[0] == 'H' && hiHatOpenness > 0.02f)
        {
            const auto top = bounds.translated (0.0f, -bounds.getHeight() * (0.30f + hiHatOpenness * 0.95f));
            g.setColour (line.withMultipliedAlpha (0.85f));
            g.drawEllipse (top, 1.3f);
        }
    }
    else
    {
        // Hoop and head, with tension lugs round the hoop - the same drum the
        // app icon uses, so the icon and the kit read as one thing.
        const auto head = bounds.reduced (bounds.getWidth() * 0.08f);

        g.setColour (line);
        g.drawEllipse (bounds, 1.6f);
        g.setColour (line.withMultipliedAlpha (0.7f));
        g.drawEllipse (head, 1.0f);

        const int lugs = bounds.getWidth() > 150.0f ? 10 : 8;
        const auto centre = bounds.getCentre();
        const float rx = bounds.getWidth() * 0.5f;
        const float ry = bounds.getHeight() * 0.5f;

        g.setColour (line);

        for (int i = 0; i < lugs; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) lugs;
            const juce::Point<float> outer (centre.x + rx * std::sin (a), centre.y - ry * std::cos (a));
            const juce::Point<float> inner (centre.x + rx * 0.90f * std::sin (a), centre.y - ry * 0.90f * std::cos (a));
            g.drawLine ({ inner, outer }, 2.2f);
        }
    }

    // --- legend -------------------------------------------------------------------
    // Printed ink on the glass when dark; once the lamp behind it is bright,
    // the legend reads dark against the light, as a backlit legend does.
    const float labelSize = pad.isCymbal
                          ? juce::jlimit (9.0f, 12.0f, bounds.getWidth() * 0.10f)
                          : juce::jlimit (9.5f, 13.0f, bounds.getHeight() * 0.20f);

    g.setFont (theme::legendFont (labelSize, true));

    if (glow > 0.45f)
        g.setColour (juce::Colour (0xff1c1206).withAlpha (0.9f));
    else
        g.setColour (theme::dial.withAlpha (0.78f));

    g.drawText (pad.label, bounds, juce::Justification::centred, false);
}

void KitView::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();

    // Start the lamp test on the first real paint, not at construction: the
    // window only appears once the audio driver has opened, which can take
    // longer than the whole sweep.
    if (lampTestStart < 0.0 && isShowing())
        lampTestStart = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    // The kit lives behind the faceplate's display window.
    theme::drawGlassWindow (g, area, 5.0f);

    // Cymbals first, so the drums sit in front of them as on a real kit.
    for (size_t i = 0; i < pads.size(); ++i)
        if (pads[i].isCymbal)
            drawPad (g, pads[i], glowLevels[i]);

    for (size_t i = 0; i < pads.size(); ++i)
        if (! pads[i].isCymbal)
            drawPad (g, pads[i], glowLevels[i]);
}
