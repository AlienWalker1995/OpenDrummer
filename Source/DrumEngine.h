#pragma once

#include "Kit.h"
#include "MidiMapping.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

/**
    The real-time sample playback engine.

    Threading model - the thing that matters most here:

      * MIDI arrives on a JUCE MIDI callback thread.
      * Audio is rendered on the driver's thread.
      * They must never block each other, so triggers cross between them
        through a lock-free FIFO and nothing in renderNextBlock allocates,
        locks or touches the filesystem.

    Trigger events are drained once per audio block, which quantises hits to
    the block boundary. At the 64-128 sample buffers an ASIO interface runs
    that is well under 3ms and not perceptible while playing.
*/
class DrumEngine
{
public:
    DrumEngine();

    void prepare (double sampleRate, int blockSize);
    void releaseResources();

    /** Installs a kit. Call from the message thread only - it takes the kit
        lock, which the audio thread only ever try-locks. */
    void setInstrument (midimap::Articulation articulation, kit::Instrument instrument);
    void clearKit();

    //==============================================================================
    // Called from the MIDI thread. All are lock-free and allocation-free.

    void noteOn (int noteNumber, float velocity);
    void controlChange (int controllerNumber, int value);
    void choke (int noteNumber);

    //==============================================================================
    // Called from the audio thread.

    /** Renders into a stereo (or wider) buffer, adding to whatever is there.

        `scheduled` carries notes placed by the groove player, each with a
        sample offset inside this block. Live MIDI arrives separately through
        the lock-free queue and always starts at the block boundary - there is
        no timestamp to preserve for a hit that just happened. */
    void renderNextBlock (juce::AudioBuffer<float>& output, const juce::MidiBuffer& scheduled);

    //==============================================================================
    midimap::Mapping& getMapping() noexcept { return mapping; }

    void  setMasterGain (float linearGain) noexcept { masterGain.store (linearGain); }
    float getMasterGain() const noexcept            { return masterGain.load(); }

    /** Velocity curve exponent. 1.0 is linear; below 1.0 makes soft hits
        louder, which most players want on mesh heads. */
    void  setVelocityCurve (float exponent) noexcept { velocityCurve.store (exponent); }
    float getVelocityCurve() const noexcept          { return velocityCurve.load(); }

    /** Snapshot of the most recent hit, for the UI. Lock-free and lossy by
        design - the UI only ever wants the latest value. */
    struct HitInfo
    {
        juce::uint64 index = 0;   // increments per hit, so the UI can spot new ones
        int   noteNumber = -1;
        float velocity   = 0.0f;
        int   layerIndex = -1;
        int   roundRobinIndex = -1;
        midimap::Articulation articulation = midimap::Articulation::None;
    };

    /** Appends every hit recorded since `fromIndex` to `out`, and returns the
        index to pass in next time. Polling for "the latest hit" would drop
        notes during a fast roll, so the engine keeps a ring instead. */
    juce::uint64 readHitsSince (juce::uint64 fromIndex, std::vector<HitInfo>& out) const;

    int getActiveVoiceCount() const noexcept;
    float getPeakLevel() noexcept;
    float getMeanSquare() const noexcept { return meanSquare.load(); }

private:
    //==============================================================================
    static constexpr int kMaxVoices = 64;
    static constexpr int kEventQueueSize = 256;

    struct Voice
    {
        kit::SamplePtr sample;          // shared_ptr keeps the audio alive while playing
        juce::int64 position = 0;
        float gainLeft = 0.0f;
        float gainRight = 0.0f;
        int   chokeGroup = kit::kNone;
        juce::uint64 startOrder = 0;    // for voice stealing

        // Samples to wait before this voice starts sounding, so a scheduled
        // note can land mid-block instead of being rounded to the boundary.
        int startOffset = 0;

        // Choke ramp. envelope 1.0 means "playing normally"; a choke starts
        // ramping it to zero so the sound stops without a click.
        float envelope = 1.0f;
        float envelopeDelta = 0.0f;

        bool active = false;
    };

    struct Event
    {
        enum class Type { NoteOn, Choke, PedalCC };
        Type  type = Type::NoteOn;
        int   value = 0;         // note number, or CC value for PedalCC
        float velocity = 0.0f;
    };

    //==============================================================================
    void handleEvent (const Event& e);
    void triggerNote (int noteNumber, float velocity, int sampleOffset);
    void startVoice (const kit::Instrument& instrument, float velocity,
                     midimap::Articulation articulation, int noteNumber,
                     int sampleOffset);
    void chokeGroup (int group, float fadeSeconds);
    int  findFreeVoice();

    //==============================================================================
    std::array<Voice, kMaxVoices> voices;
    juce::uint64 voiceCounter = 0;

    // Lock-free single-producer/single-consumer queue, MIDI thread -> audio thread.
    juce::AbstractFifo eventFifo { kEventQueueSize };
    std::array<Event, kEventQueueSize> eventBuffer;

    // The kit. Swapped on the message thread under this lock; the audio thread
    // only ever try-locks it and renders silence in the rare event it fails.
    juce::CriticalSection kitLock;
    std::array<kit::Instrument, (size_t) midimap::Articulation::NumArticulations> instruments;

    // Round-robin cursors, one per articulation.
    std::array<int, (size_t) midimap::Articulation::NumArticulations> roundRobinCursor {};

    midimap::Mapping mapping;

    double currentSampleRate = 44100.0;

    std::atomic<float> masterGain { 1.0f };
    std::atomic<float> velocityCurve { 1.0f };
    std::atomic<int>   activeVoices { 0 };
    std::atomic<float> peakLevel { 0.0f };
    std::atomic<float> meanSquare { 0.0f };

    // Hit telemetry for the UI. The audio thread writes into the ring and
    // bumps the index; the UI reads entries behind the write head. Plain
    // stores rather than atomics per field - the UI is always far enough
    // behind that tearing is not a practical concern, and this keeps the
    // audio thread free of synchronisation it does not need.
    static constexpr int kHitRingSize = 256;

    std::array<HitInfo, kHitRingSize> hitRing;
    std::atomic<juce::uint64> hitWriteIndex { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumEngine)
};
