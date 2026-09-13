#include "DrumEngine.h"

using midimap::Articulation;

namespace
{
    // How fast a choked voice fades out. Long enough not to click, short
    // enough to feel like a real hand on a cymbal.
    constexpr float kChokeFadeSeconds = 0.045f;

    // Stepping on the hi-hat pedal closes the cymbals fast.
    constexpr float kPedalCloseFadeSeconds = 0.030f;

    constexpr size_t articulationIndex (Articulation a) noexcept
    {
        return (size_t) a;
    }
}

DrumEngine::DrumEngine()
{
    roundRobinCursor.fill (0);
}

void DrumEngine::prepare (double sampleRate, int /*blockSize*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    for (auto& voice : voices)
        voice = {};

    activeVoices.store (0);
    eventFifo.reset();
}

void DrumEngine::releaseResources()
{
    for (auto& voice : voices)
        voice = {};

    activeVoices.store (0);
}

//==============================================================================
void DrumEngine::setInstrument (Articulation articulation, kit::Instrument instrument)
{
    if (articulation == Articulation::None || articulation == Articulation::NumArticulations)
        return;

    const juce::ScopedLock sl (kitLock);
    instruments[articulationIndex (articulation)] = std::move (instrument);
}

void DrumEngine::clearKit()
{
    const juce::ScopedLock sl (kitLock);

    for (auto& instrument : instruments)
        instrument = {};
}

//==============================================================================
// MIDI thread. Push and return - never touch voices from here.

void DrumEngine::noteOn (int noteNumber, float velocity)
{
    int start1, size1, start2, size2;
    eventFifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 > 0)
    {
        eventBuffer[(size_t) start1] = { Event::Type::NoteOn, noteNumber, velocity };
        eventFifo.finishedWrite (1);
    }
    // If the queue is full we drop the hit. That only happens if the audio
    // thread has stalled, in which case a dropped note is the least of it.
}

void DrumEngine::controlChange (int controllerNumber, int value)
{
    if (controllerNumber != midimap::kHiHatPedalCC)
        return;

    int start1, size1, start2, size2;
    eventFifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 > 0)
    {
        eventBuffer[(size_t) start1] = { Event::Type::PedalCC, value, 0.0f };
        eventFifo.finishedWrite (1);
    }
}

void DrumEngine::choke (int noteNumber)
{
    int start1, size1, start2, size2;
    eventFifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 > 0)
    {
        eventBuffer[(size_t) start1] = { Event::Type::Choke, noteNumber, 0.0f };
        eventFifo.finishedWrite (1);
    }
}

//==============================================================================
// Audio thread from here down.

void DrumEngine::renderNextBlock (juce::AudioBuffer<float>& output,
                                  const juce::MidiBuffer& scheduled)
{
    const int numSamples = output.getNumSamples();
    const int numChannels = output.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    // The message thread only holds this while swapping a kit. Rendering
    // silence for one block is far better than blocking the audio thread.
    const juce::ScopedTryLock sl (kitLock);

    if (! sl.isLocked())
        return;

    // Drain everything the MIDI thread queued since the last block.
    int start1, size1, start2, size2;
    eventFifo.prepareToRead (eventFifo.getNumReady(), start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i)
        handleEvent (eventBuffer[(size_t) (start1 + i)]);

    for (int i = 0; i < size2; ++i)
        handleEvent (eventBuffer[(size_t) (start2 + i)]);

    eventFifo.finishedRead (size1 + size2);

    // Notes placed by the groove player, each keeping its offset in the block.
    for (const auto metadata : scheduled)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
            triggerNote (message.getNoteNumber(), message.getFloatVelocity(),
                         juce::jlimit (0, numSamples - 1, metadata.samplePosition));
    }

    // Mix every active voice.
    const float master = masterGain.load();
    const int outLeft = 0;
    const int outRight = numChannels > 1 ? 1 : 0;

    int voicesPlaying = 0;

    for (auto& voice : voices)
    {
        if (! voice.active)
            continue;

        ++voicesPlaying;

        const auto& buffer = voice.sample->audio;
        const juce::int64 sampleLength = buffer.getNumSamples();
        const int sampleChannels = buffer.getNumChannels();

        if (sampleChannels <= 0 || sampleLength <= 0)
        {
            voice.active = false;
            continue;
        }

        const float* srcL = buffer.getReadPointer (0);
        const float* srcR = buffer.getReadPointer (sampleChannels > 1 ? 1 : 0);

        float* dstL = output.getWritePointer (outLeft);
        float* dstR = output.getWritePointer (outRight);

        for (int i = juce::jlimit (0, numSamples, voice.startOffset); i < numSamples; ++i)
        {
            if (voice.position >= sampleLength)
            {
                voice.active = false;
                break;
            }

            const float env = voice.envelope;
            const auto pos = (int) voice.position;

            const float l = srcL[pos] * voice.gainLeft * env * master;
            const float r = srcR[pos] * voice.gainRight * env * master;

            dstL[i] += l;

            if (outRight != outLeft)
                dstR[i] += r;

            ++voice.position;

            if (voice.envelopeDelta != 0.0f)
            {
                voice.envelope += voice.envelopeDelta;

                if (voice.envelope <= 0.0f)
                {
                    voice.active = false;
                    break;
                }
            }
        }

        // The offset only delays the first block a voice appears in.
        voice.startOffset = 0;
    }

    activeVoices.store (voicesPlaying);

    // Track output peak for the meter.
    float peak = 0.0f;

    for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
        peak = juce::jmax (peak, output.getMagnitude (ch, 0, numSamples));

    float previous = peakLevel.load();
    peakLevel.store (juce::jmax (peak, previous * 0.85f));   // slow visual decay

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
        const float coefficient = 1.0f - std::exp (-(float) numSamples / (float) (currentSampleRate * 0.02));
        const float previousMeanSquare = meanSquare.load();
        meanSquare.store (previousMeanSquare + coefficient * (blockMeanSquare - previousMeanSquare));
    }

}

void DrumEngine::handleEvent (const Event& e)
{
    switch (e.type)
    {
        case Event::Type::PedalCC:
        {
            const float before = mapping.getOpenness();
            mapping.setPedalRaw (e.value);
            const float after = mapping.getOpenness();

            // Pedal has just been pressed shut - kill any ringing open hi-hat.
            if (before >= midimap::Mapping::kClosedThreshold
                && after < midimap::Mapping::kClosedThreshold)
            {
                chokeGroup (kit::kHiHat, kPedalCloseFadeSeconds);
            }

            break;
        }

        case Event::Type::Choke:
        {
            const auto articulation = mapping.articulationForNote (e.value);
            const auto& instrument = instruments[articulationIndex (articulation)];

            if (instrument.chokeGroup != kit::kNone)
                chokeGroup (instrument.chokeGroup, kChokeFadeSeconds);

            break;
        }

        case Event::Type::NoteOn:
            triggerNote (e.value, e.velocity, 0);
            break;
    }
}

void DrumEngine::triggerNote (int noteNumber, float velocity, int sampleOffset)
{
    const auto articulation = mapping.articulationForNote (noteNumber);

    if (articulation == Articulation::None)
        return;

    // A pedal-chick note also stops the open hi-hat ringing.
    if (articulation == Articulation::HiHatPedal)
        chokeGroup (kit::kHiHat, kPedalCloseFadeSeconds);

    const auto& instrument = instruments[articulationIndex (articulation)];

    if (instrument.layers.empty())
        return;

    startVoice (instrument, velocity, articulation, noteNumber, sampleOffset);
}

void DrumEngine::startVoice (const kit::Instrument& instrument, float velocity,
                             Articulation articulation, int noteNumber,
                             int sampleOffset)
{
    const float curve = velocityCurve.load();
    const float shaped = curve == 1.0f ? velocity : std::pow (velocity, curve);

    const auto* layer = instrument.layerForVelocity (shaped);

    if (layer == nullptr || layer->roundRobin.empty())
        return;

    // Work out which layer index this is, purely so the UI can show it.
    int layerIndex = 0;

    for (size_t i = 0; i < instrument.layers.size(); ++i)
        if (&instrument.layers[i] == layer)
            layerIndex = (int) i;

    // Round robin: step to the next alternate take for this articulation.
    auto& cursor = roundRobinCursor[articulationIndex (articulation)];
    const int rrIndex = cursor % (int) layer->roundRobin.size();
    cursor = (cursor + 1) % (int) layer->roundRobin.size();

    const auto& sample = layer->roundRobin[(size_t) rrIndex];

    if (sample == nullptr)
        return;

    // An open hi-hat must cut off the previous one, otherwise successive
    // open hits pile up into a wash.
    if (midimap::isOpenHiHat (articulation) || instrument.chokeGroup == kit::kHiHat)
        chokeGroup (kit::kHiHat, kChokeFadeSeconds);

    const int voiceIndex = findFreeVoice();

    if (voiceIndex < 0)
        return;

    auto& voice = voices[(size_t) voiceIndex];

    // Within a velocity layer, scale by where the hit sits inside that layer.
    // Without this you hear obvious steps at the layer boundaries.
    const float layerSpan = juce::jmax (0.0001f, layer->maxVelocity - layer->minVelocity);
    const float withinLayer = juce::jlimit (0.0f, 1.0f, (shaped - layer->minVelocity) / layerSpan);
    const float layerTrim = 0.80f + 0.20f * withinLayer;

    const float gain = instrument.gain * layerTrim;

    // Constant-power pan.
    const float panAngle = (instrument.pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;

    voice.sample = sample;
    voice.position = 0;
    voice.gainLeft = gain * std::cos (panAngle);
    voice.gainRight = gain * std::sin (panAngle);
    voice.chokeGroup = instrument.chokeGroup;
    voice.startOffset = juce::jmax (0, sampleOffset);
    voice.envelope = 1.0f;
    voice.envelopeDelta = 0.0f;
    voice.startOrder = ++voiceCounter;
    voice.active = true;

    // Record the hit for the UI, then publish by bumping the index last.
    const juce::uint64 writeIndex = hitWriteIndex.load();
    auto& entry = hitRing[(size_t) (writeIndex % kHitRingSize)];

    entry.index = writeIndex;
    entry.noteNumber = noteNumber;
    entry.velocity = velocity;
    entry.layerIndex = layerIndex;
    entry.roundRobinIndex = rrIndex;
    entry.articulation = articulation;

    hitWriteIndex.store (writeIndex + 1);
}

void DrumEngine::chokeGroup (int group, float fadeSeconds)
{
    if (group == kit::kNone)
        return;

    const float delta = -1.0f / juce::jmax (1.0f, (float) (fadeSeconds * currentSampleRate));

    for (auto& voice : voices)
        if (voice.active && voice.chokeGroup == group && voice.envelopeDelta == 0.0f)
            voice.envelopeDelta = delta;
}

int DrumEngine::findFreeVoice()
{
    for (size_t i = 0; i < voices.size(); ++i)
        if (! voices[i].active)
            return (int) i;

    // All busy - steal the one that has been going longest. Drum samples decay,
    // so the oldest voice is almost always the quietest.
    size_t oldest = 0;
    juce::uint64 oldestOrder = std::numeric_limits<juce::uint64>::max();

    for (size_t i = 0; i < voices.size(); ++i)
    {
        if (voices[i].startOrder < oldestOrder)
        {
            oldestOrder = voices[i].startOrder;
            oldest = i;
        }
    }

    return (int) oldest;
}

//==============================================================================
juce::uint64 DrumEngine::readHitsSince (juce::uint64 fromIndex, std::vector<HitInfo>& out) const
{
    const juce::uint64 writeIndex = hitWriteIndex.load();

    // If the UI stalled long enough for the ring to wrap, skip what was lost
    // rather than replaying stale entries.
    if (writeIndex > (juce::uint64) kHitRingSize
        && fromIndex < writeIndex - (juce::uint64) kHitRingSize)
    {
        fromIndex = writeIndex - (juce::uint64) kHitRingSize;
    }

    for (juce::uint64 i = fromIndex; i < writeIndex; ++i)
        out.push_back (hitRing[(size_t) (i % kHitRingSize)]);

    return writeIndex;
}

int DrumEngine::getActiveVoiceCount() const noexcept
{
    return activeVoices.load();
}

float DrumEngine::getPeakLevel() noexcept
{
    return peakLevel.load();
}
