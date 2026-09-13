#pragma once

#include <juce_core/juce_core.h>
#include <array>

/**
    Translates raw MIDI from a Roland TD module into kit articulations.

    Roland modules send a distinct note number per zone (snare head vs rim vs
    cross-stick, cymbal bow vs edge vs bell), so the note number alone tells us
    the articulation. The hi-hat is the exception: the module picks the note
    from the pedal position, but it *also* streams continuous pedal position on
    CC4, which is what lets a hi-hat sound properly half-open.
*/
namespace midimap
{

/** Every articulation the engine knows how to play. */
enum class Articulation
{
    None = 0,

    Kick,
    SnareHead,
    SnareRimshot,
    SnareCrossStick,
    Tom1, Tom1Rim,
    Tom2, Tom2Rim,
    Tom3, Tom3Rim,
    Tom4, Tom4Rim,
    HiHatClosedBow, HiHatClosedEdge,
    HiHatOpenBow,   HiHatOpenEdge,
    HiHatPedal,
    Crash1Bow, Crash1Edge,
    Crash2Bow, Crash2Edge,
    RideBow, RideEdge, RideBell,

    NumArticulations
};

juce::String getArticulationName (Articulation a);

/** True for articulations whose sound the hi-hat pedal should cut off. */
bool isOpenHiHat (Articulation a);

/** The hi-hat pedal controller. Standard across Roland TD modules. */
constexpr int kHiHatPedalCC = 4;

/**
    Note-number to articulation table, plus the running hi-hat pedal state.

    Defaults match the Roland TD factory map (TD-17/TD-27 and close relatives).
    Individual notes can be reassigned at runtime for MIDI learn.
*/
class Mapping
{
public:
    Mapping();

    /** Articulation for a note number, or Articulation::None if unmapped. */
    Articulation articulationForNote (int noteNumber) const;

    /** Reassigns a note - the MIDI learn path. */
    void assign (int noteNumber, Articulation articulation);

    /** Feeds in a CC4 value (0-127) from the hi-hat pedal. */
    void setPedalRaw (int ccValue);

    /** Pedal position as 0.0 = fully closed .. 1.0 = fully open. */
    float getOpenness() const noexcept { return openness; }

    /** Modules disagree on whether CC4 0 means open or closed, and it is not
        something we can detect reliably, so it stays user-switchable. */
    void setPedalInverted (bool shouldInvert);
    bool isPedalInverted() const noexcept { return pedalInverted; }

    /** Below this openness the hi-hat counts as closed - used to decide when
        stepping on the pedal should choke a ringing open hi-hat. */
    static constexpr float kClosedThreshold = 0.15f;

private:
    std::array<Articulation, 128> noteTable;
    float openness = 0.0f;
    int   lastRawPedal = 0;
    bool  pedalInverted = false;
};

} // namespace midimap
