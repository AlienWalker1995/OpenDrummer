#include "GrooveLibrary.h"

namespace groove
{
namespace
{
    constexpr int kTicksPerQuarter = 960;
    constexpr int kDrumChannel = 10;      // GM percussion

    // Roland TD factory notes, matching MidiMapping.
    enum
    {
        Kick = 36, Snare = 38, SnareRim = 40, CrossStick = 37,
        Tom1 = 48, Tom2 = 45, Tom3 = 43, Tom4 = 41,
        HatClosed = 42, HatOpen = 46, HatPedal = 44,
        Crash1 = 49, Crash2 = 57, Ride = 51, RideBell = 53
    };

    double secondsPerBar (double bpm, int numerator, int denominator)
    {
        const double secondsPerQuarter = 60.0 / juce::jmax (1.0, bpm);
        return secondsPerQuarter * (double) numerator * (4.0 / (double) juce::jmax (1, denominator));
    }

    //==========================================================================
    /** Small helper for writing patterns by beat position. */
    struct PatternWriter
    {
        juce::MidiMessageSequence sequence;
        int numerator = 4;
        int denominator = 4;
        double bpm = 120.0;

        void hit (double beat, int note, int velocity)
        {
            const double tick = beat * (double) kTicksPerQuarter;

            sequence.addEvent (juce::MidiMessage::noteOn (kDrumChannel, note,
                                                          (juce::uint8) juce::jlimit (1, 127, velocity)),
                               tick);

            // Short note-off. Nothing reads it, but a file without matched
            // pairs confuses other software.
            sequence.addEvent (juce::MidiMessage::noteOff (kDrumChannel, note),
                               tick + (double) kTicksPerQuarter * 0.25);
        }

        bool writeTo (const juce::File& file)
        {
            juce::MidiMessageSequence track;

            const int microsecondsPerQuarter = juce::roundToInt (60000000.0 / bpm);
            track.addEvent (juce::MidiMessage::tempoMetaEvent (microsecondsPerQuarter), 0.0);
            track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (numerator, denominator), 0.0);
            track.addSequence (sequence, 0.0);
            track.updateMatchedPairs();

            juce::MidiFile midiFile;
            midiFile.setTicksPerQuarterNote (kTicksPerQuarter);
            midiFile.addTrack (track);

            if (file.existsAsFile())
                return true;

            file.getParentDirectory().createDirectory();

            juce::FileOutputStream stream (file);

            if (! stream.openedOk())
                return false;

            stream.setPosition (0);
            stream.truncate();

            return midiFile.writeTo (stream);
        }
    };

    //==========================================================================
    // The starter grooves. Velocities are shaped by hand: downbeats accented,
    // off-beat hats pulled back, ghost notes soft. A pattern with every hit at
    // 100 sounds like a drum machine, which rather defeats the point.

    void rockStraight (PatternWriter& p)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            const double b = bar * 4.0;

            p.hit (b + 0.0, Kick, 110);
            p.hit (b + 2.0, Kick, 104);
            p.hit (b + 1.0, Snare, 108);
            p.hit (b + 3.0, Snare, 112);

            for (int eighth = 0; eighth < 8; ++eighth)
            {
                const bool onBeat = (eighth % 2) == 0;
                p.hit (b + eighth * 0.5, HatClosed, onBeat ? 92 : 66);
            }
        }
    }

    void rockSixteenths (PatternWriter& p)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            const double b = bar * 4.0;

            p.hit (b + 0.0, Kick, 112);
            p.hit (b + 2.5, Kick, 100);
            p.hit (b + 1.0, Snare, 110);
            p.hit (b + 3.0, Snare, 114);

            for (int s = 0; s < 16; ++s)
            {
                const int velocity = (s % 4) == 0 ? 94 : ((s % 2) == 0 ? 72 : 58);
                p.hit (b + s * 0.25, HatClosed, velocity);
            }
        }
    }

    void halfTime (PatternWriter& p)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            const double b = bar * 4.0;

            p.hit (b + 0.0, Kick, 114);
            p.hit (b + 2.0, Snare, 116);

            for (int eighth = 0; eighth < 8; ++eighth)
                p.hit (b + eighth * 0.5, HatClosed, (eighth % 2) == 0 ? 88 : 62);
        }
    }

    void fourOnTheFloor (PatternWriter& p)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            const double b = bar * 4.0;

            for (int beat = 0; beat < 4; ++beat)
                p.hit (b + beat, Kick, beat == 0 ? 112 : 104);

            p.hit (b + 1.0, Snare, 106);
            p.hit (b + 3.0, Snare, 110);

            // Open hats on the off-beats - the disco signature.
            for (int eighth = 0; eighth < 8; ++eighth)
                if ((eighth % 2) == 1)
                    p.hit (b + eighth * 0.5, HatOpen, 84);
        }
    }

    void shuffle (PatternWriter& p)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            const double b = bar * 4.0;

            p.hit (b + 0.0, Kick, 110);
            p.hit (b + 2.0, Kick, 102);
            p.hit (b + 1.0, Snare, 108);
            p.hit (b + 3.0, Snare, 112);

            // Triplet feel: first and third of each triplet.
            for (int beat = 0; beat < 4; ++beat)
            {
                p.hit (b + beat, HatClosed, 92);
                p.hit (b + beat + 2.0 / 3.0, HatClosed, 68);
            }
        }
    }

    void funkSixteenth (PatternWriter& p)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            const double b = bar * 4.0;

            p.hit (b + 0.0,  Kick, 112);
            p.hit (b + 0.75, Kick, 92);
            p.hit (b + 2.5,  Kick, 104);
            p.hit (b + 3.25, Kick, 88);

            p.hit (b + 1.0, Snare, 114);
            p.hit (b + 3.0, Snare, 110);

            // Ghost notes: the thing that makes funk sit down.
            p.hit (b + 1.75, Snare, 34);
            p.hit (b + 2.25, Snare, 30);

            for (int s = 0; s < 16; ++s)
                p.hit (b + s * 0.25, HatClosed, (s % 4) == 0 ? 90 : ((s % 2) == 0 ? 66 : 50));
        }
    }

    void punkEighths (PatternWriter& p)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            const double b = bar * 4.0;

            for (int eighth = 0; eighth < 8; ++eighth)
            {
                p.hit (b + eighth * 0.5, Kick, (eighth % 2) == 0 ? 112 : 98);
                p.hit (b + eighth * 0.5, HatClosed, (eighth % 2) == 0 ? 96 : 78);
            }

            p.hit (b + 1.0, Snare, 116);
            p.hit (b + 3.0, Snare, 118);
        }
    }

    void rideGroove (PatternWriter& p)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            const double b = bar * 4.0;

            p.hit (b + 0.0, Kick, 106);
            p.hit (b + 2.5, Kick, 98);
            p.hit (b + 1.0, Snare, 104);
            p.hit (b + 3.0, Snare, 108);

            for (int eighth = 0; eighth < 8; ++eighth)
                p.hit (b + eighth * 0.5, Ride, (eighth % 2) == 0 ? 88 : 64);

            p.hit (b + 0.0, RideBell, 70);
        }
    }

    void ballad68 (PatternWriter& p)
    {
        p.numerator = 6;
        p.denominator = 8;

        // In 6/8 an eighth note is half a quarter, so a bar is 3 quarters.
        for (int bar = 0; bar < 2; ++bar)
        {
            const double b = bar * 3.0;

            p.hit (b + 0.0, Kick, 108);
            p.hit (b + 1.5, Kick, 96);
            p.hit (b + 0.75, Snare, 30);
            p.hit (b + 2.25, Snare, 104);

            for (int eighth = 0; eighth < 6; ++eighth)
                p.hit (b + eighth * 0.5, HatClosed, eighth == 0 ? 86 : (eighth == 3 ? 76 : 58));
        }
    }

    void tomGroove (PatternWriter& p)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            const double b = bar * 4.0;

            p.hit (b + 0.0, Kick, 114);
            p.hit (b + 1.5, Kick, 100);
            p.hit (b + 3.0, Kick, 106);

            for (int eighth = 0; eighth < 8; ++eighth)
            {
                const int note = (eighth % 4) < 2 ? Tom2 : Tom4;
                p.hit (b + eighth * 0.5, note, (eighth % 2) == 0 ? 104 : 74);
            }

            p.hit (b + 2.0, Snare, 108);
        }
    }

    void fillAndCrash (PatternWriter& p)
    {
        // One bar of groove, then a bar of fill landing on a crash.
        p.hit (0.0, Kick, 110);
        p.hit (2.0, Kick, 102);
        p.hit (1.0, Snare, 108);
        p.hit (3.0, Snare, 110);

        for (int eighth = 0; eighth < 8; ++eighth)
            p.hit (eighth * 0.5, HatClosed, (eighth % 2) == 0 ? 90 : 64);

        // Descending sixteenth-note fill across the kit.
        const int order[] = { Snare, Snare, Tom1, Tom1, Tom2, Tom2, Tom3, Tom3,
                              Tom3, Tom4, Tom4, Tom4 };

        for (int i = 0; i < 12; ++i)
            p.hit (4.0 + i * 0.25, order[i], 88 + (i % 2 == 0 ? 16 : 0));

        p.hit (7.0, Kick, 118);
        p.hit (7.0, Crash1, 120);
    }

    struct StarterGroove
    {
        const char* name;
        void (*build) (PatternWriter&);
        double bpm;
    };

    const StarterGroove kStarterGrooves[] =
    {
        { "Rock - Straight 8ths",   rockStraight,    120.0 },
        { "Rock - 16th Hats",       rockSixteenths,  110.0 },
        { "Half Time",              halfTime,         92.0 },
        { "Four on the Floor",      fourOnTheFloor,  124.0 },
        { "Shuffle",                shuffle,         104.0 },
        { "Funk - 16th Ghosts",     funkSixteenth,    98.0 },
        { "Punk - Driving 8ths",    punkEighths,     176.0 },
        { "Ride Groove",            rideGroove,      112.0 },
        { "Ballad 6-8",             ballad68,         76.0 },
        { "Floor Tom Groove",       tomGroove,       100.0 },
        { "Groove + Fill to Crash", fillAndCrash,    120.0 }
    };
}

//==============================================================================
juce::File getDefaultGrooveFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("OpenDrummer")
             .getChildFile ("Grooves");
}

void writeStarterGrooves (const juce::File& folder)
{
    folder.createDirectory();

    for (const auto& starter : kStarterGrooves)
    {
        PatternWriter writer;
        writer.bpm = starter.bpm;
        starter.build (writer);

        const auto file = folder.getChildFile (juce::String (starter.name) + ".mid");
        writer.writeTo (file);
    }
}

//==============================================================================
bool loadPattern (const juce::File& file, Pattern& result)
{
    juce::FileInputStream stream (file);

    if (! stream.openedOk())
        return false;

    juce::MidiFile midiFile;

    if (! midiFile.readFrom (stream))
        return false;

    // Work in seconds from here on; the tempo map is baked in by this call, so
    // files with tempo changes behave correctly rather than drifting.
    midiFile.convertTimestampTicksToSeconds();

    result = {};
    result.file = file;
    result.name = file.getFileNameWithoutExtension();

    double bpm = 120.0;
    bool foundTempo = false;
    int numerator = 4;
    int denominator = 4;

    for (int track = 0; track < midiFile.getNumTracks(); ++track)
    {
        const auto* sequence = midiFile.getTrack (track);

        if (sequence == nullptr)
            continue;

        for (int i = 0; i < sequence->getNumEvents(); ++i)
        {
            const auto& message = sequence->getEventPointer (i)->message;

            if (! foundTempo && message.isTempoMetaEvent())
            {
                const double secondsPerQuarter = message.getTempoSecondsPerQuarterNote();

                if (secondsPerQuarter > 0.0)
                {
                    bpm = 60.0 / secondsPerQuarter;
                    foundTempo = true;
                }
            }

            if (message.isTimeSignatureMetaEvent())
                message.getTimeSignatureInfo (numerator, denominator);

            if (message.isNoteOn() && message.getVelocity() > 0)
            {
                Note note;
                note.timeSeconds = message.getTimeStamp();
                note.noteNumber = message.getNoteNumber();
                note.velocity = message.getFloatVelocity();
                result.notes.push_back (note);
            }
        }
    }

    if (result.notes.empty())
        return false;

    std::stable_sort (result.notes.begin(), result.notes.end(),
                      [] (const Note& a, const Note& b) { return a.timeSeconds < b.timeSeconds; });

    result.bpm = bpm;
    result.timeSigNumerator = numerator;
    result.timeSigDenominator = denominator;

    // Round the loop length up to whole bars. Using the last note's time would
    // clip the tail off any groove that does not end exactly on a downbeat,
    // and every loop would land early.
    const double barSeconds = secondsPerBar (bpm, numerator, denominator);
    const double lastNote = result.notes.back().timeSeconds;

    result.bars = juce::jmax (1, (int) std::ceil ((lastNote + 1.0e-4) / barSeconds));
    result.lengthSeconds = result.bars * barSeconds;

    return true;
}

//==============================================================================
void Library::scan (const juce::File& folderToScan)
{
    folder = folderToScan;
    patterns.clear();

    if (! folder.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*.mid;*.midi",
                                                            juce::File::findFiles))
    {
        Pattern pattern;

        if (loadPattern (entry.getFile(), pattern))
            patterns.push_back (std::move (pattern));
    }

    std::stable_sort (patterns.begin(), patterns.end(),
                      [] (const Pattern& a, const Pattern& b)
                      { return a.name.compareNatural (b.name) < 0; });
}

const Pattern* Library::getPattern (int index) const
{
    if (! juce::isPositiveAndBelow (index, (int) patterns.size()))
        return nullptr;

    return &patterns[(size_t) index];
}

} // namespace groove
