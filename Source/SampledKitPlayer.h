#pragma once

#include "DrumEngine.h"
#include "SampledKits.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>

namespace sfz { class Sfizz; }

/**
    Plays a recorded kit through sfizz.

    Threading, which is the only hard part:

      * Loading a kit reads thousands of files, so it runs on a worker thread
        into a brand-new sfizz instance that nothing else can see.
      * When it is ready, the finished instance is swapped in under a spin
        lock the audio thread only ever try-locks. The old instance is then
        destroyed on the worker, never on the audio thread.
      * MIDI from the drum module arrives on its own thread and is handed over
        through a MidiMessageCollector, which also keeps each hit's timing
        within the block.
*/
class SampledKitPlayer
{
public:
    explicit SampledKitPlayer (const midimap::Mapping& mappingToUse);
    ~SampledKitPlayer();

    //==============================================================================
    void prepare (double sampleRate, int blockSize);

    /** How a load ended. Skipped means a newer kit was picked before this one
        ran, so nothing was installed - the caller has to know that, or the app
        can end up with no kit loaded and nothing said about it. */
    enum class LoadResult { Loaded, Failed, Skipped };

    using LoadCallback = std::function<void (LoadResult result, const juce::String& message)>;

    /** Loads on a worker thread; `onDone` is called on the message thread. A
        newer request supersedes an older one still in flight. */
    void loadAsync (const sampledkits::InstalledKit& kit, LoadCallback onDone);

    /** Loads on the calling thread, with every sample read up front. For
        offline rendering, where nothing is waiting on real time. */
    bool loadNow (const sampledkits::InstalledKit& kit, juce::String& error);

    bool isLoaded() const noexcept  { return loaded.load(); }

    /** Silences every voice immediately. */
    void allSoundOff();
    bool isLoading() const noexcept { return pendingLoads.load() > 0; }

    //==============================================================================
    // MIDI thread.
    void handleMidi (const juce::MidiMessage& message);

    // Audio thread. Replaces the buffer contents.
    void renderNextBlock (juce::AudioBuffer<float>& output, const juce::MidiBuffer& scheduled);

    //==============================================================================
    void  setMasterGain (float linearGain) noexcept { masterGain.store (linearGain); }
    void  setVelocityCurve (float exponent) noexcept { velocityCurve.store (exponent); }

    juce::uint64 readHitsSince (juce::uint64 fromIndex, std::vector<DrumEngine::HitInfo>& out) const;
    float getPeakLevel() noexcept      { return peakLevel.load(); }
    float getMeanSquare() const noexcept { return meanSquare.load(); }
    int   getActiveVoiceCount() const noexcept { return activeVoices.load(); }
    float getOpenness() const noexcept { return openness.load(); }

private:
    //==============================================================================
    bool buildInstance (const sampledkits::InstalledKit& kit, std::unique_ptr<sfz::Sfizz>& out,
                        juce::String& message, bool freewheel);

    double builtAtRate = 0.0;   // worker thread only
    void installInstance (std::unique_ptr<sfz::Sfizz>& fresh, const sampledkits::KitDefinition* definition);

    void dispatch (const juce::MidiBuffer& events, int numSamples);
    void noteOn (int delay, int note, float velocity);
    void noteOff (int delay, int note);
    void recordHit (midimap::Articulation articulation, int note, float velocity);

    //==============================================================================
    const midimap::Mapping& mapping;

    juce::SpinLock synthLock;
    std::unique_ptr<sfz::Sfizz> synth;                          // guarded by synthLock
    const sampledkits::KitDefinition* activeKit = nullptr;      // guarded by synthLock

    juce::MidiMessageCollector collector;
    juce::MidiBuffer liveEvents;
    juce::AudioBuffer<float> monoScratch;

    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<int>    maxBlockSize { 4096 };

    std::atomic<bool>   loaded { false };
    std::atomic<int>    pendingLoads { 0 };
    std::atomic<juce::uint64> loadGeneration { 0 };

    std::atomic<float>  masterGain { 1.0f };
    std::atomic<float>  kitGain { 1.0f };
    std::atomic<float>  velocityCurve { 1.0f };
    std::atomic<float>  peakLevel { 0.0f };
    std::atomic<float>  meanSquare { 0.0f };
    std::atomic<int>    activeVoices { 0 };
    std::atomic<float>  openness { 0.0f };

    // A grabbed cymbal sends a stream of aftertouch; only the first one after
    // each hit should fire the kit's choke note.
    std::array<bool, (size_t) sampledkits::kNumArticulations> chokeArmed {};

    static constexpr int kHitRingSize = 256;
    std::array<DrumEngine::HitInfo, kHitRingSize> hitRing;
    std::atomic<juce::uint64> hitWriteIndex { 0 };

    // Declared last so it is destroyed first, joining the worker before the
    // instances it might still be touching go away.
    juce::ThreadPool loader { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampledKitPlayer)
};
