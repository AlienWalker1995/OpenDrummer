#include "SampledKitPlayer.h"

#include <sfizz.hpp>

using midimap::Articulation;

namespace
{
    constexpr int kVoices = 256;             // a hit on a multi-mic kit is many voices
    constexpr int kHiHatPedalCC = midimap::kHiHatPedalCC;
}

SampledKitPlayer::SampledKitPlayer (const midimap::Mapping& mappingToUse)
    : mapping (mappingToUse)
{
    chokeArmed.fill (false);
    liveEvents.ensureSize (2048);
}

SampledKitPlayer::~SampledKitPlayer()
{
    // Stop the worker before tearing down anything it could be building.
    loader.removeAllJobs (true, 30000);
}

//==============================================================================
void SampledKitPlayer::prepare (double sampleRate, int blockSize)
{
    currentSampleRate.store (sampleRate > 0.0 ? sampleRate : 48000.0);

    // Drivers are allowed to deliver blocks larger than they promised, and
    // sfizz sizes its internal buffers from this, so leave generous headroom.
    maxBlockSize.store (juce::jmax (blockSize, 4096));

    collector.reset (currentSampleRate.load());
    monoScratch.setSize (2, maxBlockSize.load());

    const juce::SpinLock::ScopedLockType lock (synthLock);

    if (synth != nullptr)
    {
        synth->setSampleRate ((float) currentSampleRate.load());
        synth->setSamplesPerBlock (maxBlockSize.load());
    }
}

//==============================================================================
bool SampledKitPlayer::buildInstance (const sampledkits::InstalledKit& kit,
                                      std::unique_ptr<sfz::Sfizz>& out,
                                      juce::String& message, bool freewheel)
{
    juce::File resolvePath;
    juce::String text;

    if (! sampledkits::prepareSfz (kit, resolvePath, text, message))
        return false;

    auto fresh = std::make_unique<sfz::Sfizz>();
    builtAtRate = currentSampleRate.load();
    fresh->setSampleRate ((float) builtAtRate);
    fresh->setSamplesPerBlock (maxBlockSize.load());
    fresh->setNumVoices (kVoices);

    if (freewheel)
        fresh->enableFreeWheeling();

    if (! fresh->loadSfzString (resolvePath.getFullPathName().toStdString(), text.toStdString()))
    {
        message = "sfizz could not parse " + resolvePath.getFileName();
        return false;
    }

    if (fresh->getNumRegions() == 0)
    {
        message = "Loaded, but the kit has no playable regions";
        return false;
    }

    // Start with the hi-hat closed. Until the module sends its first pedal
    // position, sfizz would otherwise assume fully open and every closed hit
    // would ring. Roland modules send 127 for a closed pedal.
    const int closed = mapping.isPedalInverted() ? 0 : 127;
    fresh->cc (0, kHiHatPedalCC, closed);

    message = juce::String (fresh->getNumRegions()) + " regions";
    out = std::move (fresh);
    return true;
}

void SampledKitPlayer::installInstance (std::unique_ptr<sfz::Sfizz>& fresh,
                                        const sampledkits::KitDefinition* definition)
{
    {
        const juce::SpinLock::ScopedLockType lock (synthLock);
        std::swap (synth, fresh);
        activeKit = definition;

        // The device may have changed rate while this kit was loading. prepare()
        // could only update the instance that was live at the time, so bring
        // the new one into line before it plays a note.
        if (synth != nullptr && builtAtRate != currentSampleRate.load())
        {
            synth->setSampleRate ((float) currentSampleRate.load());
            synth->setSamplesPerBlock (maxBlockSize.load());
        }
    }

    chokeArmed.fill (false);
    loaded.store (synth != nullptr);

    if (definition != nullptr)
        kitGain.store (juce::Decibels::decibelsToGain (definition->gainDb));

    // `fresh` now owns the previous instance; the caller lets it die on
    // whichever non-audio thread it is running on.
}

void SampledKitPlayer::loadAsync (const sampledkits::InstalledKit& kit, LoadCallback onDone)
{
    const auto generation = loadGeneration.fetch_add (1) + 1;
    pendingLoads.fetch_add (1);

    loader.addJob ([this, kit, onDone, generation]
    {
        const auto report = [onDone] (LoadResult result, juce::String message)
        {
            if (onDone)
                juce::MessageManager::callAsync ([onDone, result, message] { onDone (result, message); });
        };

        // Loads queue on one worker. If a newer kit was picked while this job
        // waited, skip it outright - otherwise clicking through kits makes the
        // one you wanted wait behind every kit you passed on the way.
        if (generation != loadGeneration.load())
        {
            pendingLoads.fetch_sub (1);
            report (LoadResult::Skipped, "superseded before loading");
            return;
        }

        std::unique_ptr<sfz::Sfizz> fresh;
        juce::String message;

        const bool ok = buildInstance (kit, fresh, message, false);

        // A newer request arrived while this one was loading: discard this
        // result rather than flashing an old kit in for a moment.
        const bool superseded = generation != loadGeneration.load();

        if (ok && ! superseded)
            installInstance (fresh, kit.definition);

        fresh.reset();   // old or discarded instance, destroyed off the audio thread
        pendingLoads.fetch_sub (1);

        // Every outcome is reported, including the skipped ones. A silent
        // return here is what let the app sit with no kit loaded and no sign
        // of it.
        report (superseded ? LoadResult::Skipped : ok ? LoadResult::Loaded : LoadResult::Failed,
                superseded ? "superseded while loading" : message);
    });
}

bool SampledKitPlayer::loadNow (const sampledkits::InstalledKit& kit, juce::String& error)
{
    std::unique_ptr<sfz::Sfizz> fresh;

    if (! buildInstance (kit, fresh, error, true))
        return false;

    installInstance (fresh, kit.definition);
    return true;
}

void SampledKitPlayer::allSoundOff()
{
    const juce::SpinLock::ScopedLockType lock (synthLock);

    if (synth != nullptr)
        synth->allSoundOff();
}

//==============================================================================
void SampledKitPlayer::handleMidi (const juce::MidiMessage& message)
{
    // Only what a drum kit sends. Anything else from the module - positional
    // sensing, stray controllers - would otherwise move the kit's own mic-mix
    // controls, which several of these kits map to CCs.
    if (message.isNoteOnOrOff() || message.isAftertouch()
        || (message.isController() && message.getControllerNumber() == kHiHatPedalCC))
    {
        collector.addMessageToQueue (message);
    }
}

void SampledKitPlayer::renderNextBlock (juce::AudioBuffer<float>& output, const juce::MidiBuffer& scheduled)
{
    const int numSamples = output.getNumSamples();
    const int numChannels = output.getNumChannels();

    liveEvents.clear();
    collector.removeNextBlockOfMessages (liveEvents, numSamples);

    output.clear();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const juce::SpinLock::ScopedTryLockType lock (synthLock);

    if (! lock.isLocked() || synth == nullptr)
        return;

    dispatch (liveEvents, numSamples);
    dispatch (scheduled, numSamples);

    // sfizz renders one stereo pair. Straight into the output when it has two
    // channels; through scratch when the device is mono.
    int done = 0;

    while (done < numSamples)
    {
        const int chunk = juce::jmin (numSamples - done, maxBlockSize.load());

        if (numChannels >= 2)
        {
            float* pair[2] = { output.getWritePointer (0, done), output.getWritePointer (1, done) };
            synth->renderBlock (pair, (size_t) chunk, 1);
        }
        else
        {
            float* pair[2] = { monoScratch.getWritePointer (0), monoScratch.getWritePointer (1) };
            synth->renderBlock (pair, (size_t) chunk, 1);

            auto* mono = output.getWritePointer (0, done);

            for (int i = 0; i < chunk; ++i)
                mono[i] = 0.5f * (pair[0][i] + pair[1][i]);
        }

        done += chunk;
    }

    output.applyGain (masterGain.load() * kitGain.load());

    float peak = 0.0f;

    for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
        peak = juce::jmax (peak, output.getMagnitude (ch, 0, numSamples));

    peakLevel.store (juce::jmax (peak, peakLevel.load() * 0.85f));
    activeVoices.store (synth->getNumActiveVoices());

    // Smoothed mean-square level for the VU meter. A VU responds to average
    // level, not peaks; a short one-pole here keeps the value steady between
    // the UI's polls, and the meter's own needle physics do the rest.
    {
        double sumSquares = 0.0;
        const int meterChannels = juce::jmin (2, numChannels);

        for (int ch = 0; ch < meterChannels; ++ch)
        {
            const auto* data = output.getReadPointer (ch);

            for (int i = 0; i < numSamples; ++i)
                sumSquares += (double) data[i] * (double) data[i];
        }

        const float blockMeanSquare = (float) (sumSquares / (double) juce::jmax (1, numSamples * meterChannels));
        const float coefficient = 1.0f - std::exp (-(float) numSamples / (float) (currentSampleRate.load() * 0.02));
        const float previousMeanSquare = meanSquare.load();
        meanSquare.store (previousMeanSquare + coefficient * (blockMeanSquare - previousMeanSquare));
    }

}

//==============================================================================
void SampledKitPlayer::dispatch (const juce::MidiBuffer& events, int numSamples)
{
    for (const auto metadata : events)
    {
        const auto message = metadata.getMessage();
        const int delay = juce::jlimit (0, numSamples - 1, metadata.samplePosition);

        if (message.isNoteOn())
        {
            noteOn (delay, message.getNoteNumber(), message.getFloatVelocity());
        }
        else if (message.isNoteOff())
        {
            noteOff (delay, message.getNoteNumber());
        }
        else if (message.isController() && message.getControllerNumber() == kHiHatPedalCC)
        {
            const int raw = message.getControllerValue();
            const int value = mapping.isPedalInverted() ? 127 - raw : raw;

            openness.store (1.0f - (float) value / 127.0f);
            synth->cc (delay, kHiHatPedalCC, value);
        }
        else if (message.isAftertouch() && message.getAfterTouchValue() > 0 && activeKit != nullptr)
        {
            // Roland modules report a hand grabbing a cymbal as polyphonic
            // aftertouch on that cymbal's note.
            const auto articulation = mapping.articulationForNote (message.getNoteNumber());
            const auto index = (size_t) articulation;
            const int chokeKey = activeKit->chokeKeys[index];

            if (chokeKey >= 0 && chokeArmed[index])
            {
                chokeArmed[index] = false;
                synth->hdNoteOn (delay, chokeKey, 0.8f);
                synth->hdNoteOff (delay, chokeKey, 0.0f);
            }
        }
    }
}

void SampledKitPlayer::noteOn (int delay, int note, float velocity)
{
    if (activeKit == nullptr)
        return;

    const auto articulation = mapping.articulationForNote (note);

    // Notes outside the Roland map pass straight through, so a kit's extra
    // pieces are still reachable from a pad assigned to their key.
    const int key = articulation == Articulation::None ? note
                                                       : activeKit->keys[(size_t) articulation];

    if (key < 0)
        return;

    const float curve = velocityCurve.load();
    const float shaped = curve == 1.0f ? velocity : std::pow (velocity, curve);

    synth->hdNoteOn (delay, key, juce::jlimit (0.0f, 1.0f, shaped));

    if (articulation != Articulation::None)
        chokeArmed[(size_t) articulation] = true;

    recordHit (articulation, note, velocity);
}

void SampledKitPlayer::noteOff (int delay, int note)
{
    if (activeKit == nullptr)
        return;

    const auto articulation = mapping.articulationForNote (note);
    const int key = articulation == Articulation::None ? note
                                                       : activeKit->keys[(size_t) articulation];

    // Most drum regions are one-shot and ignore this, but hi-hats and a few
    // cymbal articulations do use it, so it has to arrive on the same key.
    if (key >= 0)
        synth->hdNoteOff (delay, key, 0.0f);
}

void SampledKitPlayer::recordHit (Articulation articulation, int note, float velocity)
{
    const auto writeIndex = hitWriteIndex.load();
    auto& entry = hitRing[(size_t) (writeIndex % kHitRingSize)];

    entry.index = writeIndex;
    entry.noteNumber = note;
    entry.velocity = velocity;
    entry.layerIndex = -1;          // sfizz picks layers internally
    entry.roundRobinIndex = -1;
    entry.articulation = articulation;

    hitWriteIndex.store (writeIndex + 1);
}

juce::uint64 SampledKitPlayer::readHitsSince (juce::uint64 fromIndex, std::vector<DrumEngine::HitInfo>& out) const
{
    const auto writeIndex = hitWriteIndex.load();

    if (writeIndex > (juce::uint64) kHitRingSize && fromIndex < writeIndex - (juce::uint64) kHitRingSize)
        fromIndex = writeIndex - (juce::uint64) kHitRingSize;

    for (auto i = fromIndex; i < writeIndex; ++i)
        out.push_back (hitRing[(size_t) (i % kHitRingSize)]);

    return writeIndex;
}
