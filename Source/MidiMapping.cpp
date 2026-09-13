#include "MidiMapping.h"

namespace midimap
{

juce::String getArticulationName (Articulation a)
{
    switch (a)
    {
        case Articulation::Kick:            return "Kick";
        case Articulation::SnareHead:       return "Snare";
        case Articulation::SnareRimshot:    return "Snare Rimshot";
        case Articulation::SnareCrossStick: return "Cross Stick";
        case Articulation::Tom1:            return "Tom 1";
        case Articulation::Tom1Rim:         return "Tom 1 Rim";
        case Articulation::Tom2:            return "Tom 2";
        case Articulation::Tom2Rim:         return "Tom 2 Rim";
        case Articulation::Tom3:            return "Tom 3";
        case Articulation::Tom3Rim:         return "Tom 3 Rim";
        case Articulation::Tom4:            return "Tom 4";
        case Articulation::Tom4Rim:         return "Tom 4 Rim";
        case Articulation::HiHatClosedBow:  return "HH Closed";
        case Articulation::HiHatClosedEdge: return "HH Closed Edge";
        case Articulation::HiHatOpenBow:    return "HH Open";
        case Articulation::HiHatOpenEdge:   return "HH Open Edge";
        case Articulation::HiHatPedal:      return "HH Pedal";
        case Articulation::Crash1Bow:       return "Crash 1";
        case Articulation::Crash1Edge:      return "Crash 1 Edge";
        case Articulation::Crash2Bow:       return "Crash 2";
        case Articulation::Crash2Edge:      return "Crash 2 Edge";
        case Articulation::RideBow:         return "Ride";
        case Articulation::RideEdge:        return "Ride Edge";
        case Articulation::RideBell:        return "Ride Bell";
        case Articulation::None:
        case Articulation::NumArticulations:
        default:                            return "-";
    }
}

bool isOpenHiHat (Articulation a)
{
    return a == Articulation::HiHatOpenBow || a == Articulation::HiHatOpenEdge;
}

Mapping::Mapping()
{
    noteTable.fill (Articulation::None);

    // Roland TD factory map. Mostly GM-compatible, with the extra zone notes
    // that Roland adds for rims, cymbal edges and the ride bell.
    noteTable[36] = Articulation::Kick;

    noteTable[38] = Articulation::SnareHead;
    noteTable[40] = Articulation::SnareRimshot;
    noteTable[37] = Articulation::SnareCrossStick;

    noteTable[48] = Articulation::Tom1;      noteTable[50] = Articulation::Tom1Rim;
    noteTable[45] = Articulation::Tom2;      noteTable[47] = Articulation::Tom2Rim;
    noteTable[43] = Articulation::Tom3;      noteTable[58] = Articulation::Tom3Rim;
    noteTable[41] = Articulation::Tom4;      noteTable[39] = Articulation::Tom4Rim;

    noteTable[42] = Articulation::HiHatClosedBow;
    noteTable[22] = Articulation::HiHatClosedEdge;
    noteTable[46] = Articulation::HiHatOpenBow;
    noteTable[26] = Articulation::HiHatOpenEdge;
    noteTable[44] = Articulation::HiHatPedal;

    noteTable[49] = Articulation::Crash1Bow;  noteTable[55] = Articulation::Crash1Edge;
    noteTable[57] = Articulation::Crash2Bow;  noteTable[52] = Articulation::Crash2Edge;

    noteTable[51] = Articulation::RideBow;
    noteTable[59] = Articulation::RideEdge;
    noteTable[53] = Articulation::RideBell;
}

Articulation Mapping::articulationForNote (int noteNumber) const
{
    if (! juce::isPositiveAndBelow (noteNumber, 128))
        return Articulation::None;

    return noteTable[(size_t) noteNumber];
}

void Mapping::assign (int noteNumber, Articulation articulation)
{
    if (juce::isPositiveAndBelow (noteNumber, 128))
        noteTable[(size_t) noteNumber] = articulation;
}

void Mapping::setPedalRaw (int ccValue)
{
    lastRawPedal = juce::jlimit (0, 127, ccValue);

    const float normalised = (float) lastRawPedal / 127.0f;
    openness = pedalInverted ? normalised : 1.0f - normalised;
}

void Mapping::setPedalInverted (bool shouldInvert)
{
    pedalInverted = shouldInvert;
    setPedalRaw (lastRawPedal);   // re-evaluate with the new convention
}

} // namespace midimap
