#pragma once

#include "DrumEngine.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

/**
    The kit, drawn as a lamp panel behind the display glass.

    Each piece is printed on the glass as an outline and lit from behind like
    an incandescent indicator when it is hit, the way status was shown on
    equipment of this era. It is the part that makes the app feel like an
    instrument rather than a MIDI monitor: you see at a glance that a pad
    triggered, how hard, and - when something is wrong - that it did not.
*/
class KitView : public juce::Component,
                private juce::Timer
{
public:
    KitView();

    /** Lights the lamp belonging to this articulation. */
    void registerHit (midimap::Articulation articulation, float velocity);

    /** Drives the gap between the two hi-hat cymbals. */
    void setHiHatOpenness (float openness) { hiHatOpenness = openness; }

    void paint (juce::Graphics& g) override;

private:
    enum PadId
    {
        PadKick = 0, PadSnare, PadHiHat,
        PadTom1, PadTom2, PadTom3, PadTom4,
        PadCrash1, PadCrash2, PadRide,
        NumPads
    };

    struct Pad
    {
        const char* label;
        juce::Rectangle<float> relativeBounds;   // fractions of the view
        bool isCymbal;
    };

    static PadId padForArticulation (midimap::Articulation a);

    void timerCallback() override;
    void drawPad (juce::Graphics& g, const Pad& pad, float glow) const;
    juce::Rectangle<float> boundsFor (const Pad& pad) const;

    std::array<float, NumPads> glowLevels {};
    float hiHatOpenness = 0.0f;

    // Lamp test: one sweep across the kit at launch, as panels of the era
    // lit every lamp to prove none had failed. Negative until first shown,
    // zero once it has finished.
    double lampTestStart = -1.0;

    static const std::array<Pad, NumPads> pads;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KitView)
};
