#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <vector>

/**
    Data model for a drum kit.

    The shape here deliberately mirrors how commercial libraries (EZdrummer,
    Superior, DrumGizmo) actually store a kit: every instrument holds a set of
    velocity layers, and every layer holds several alternate recordings of the
    same hit. Playing back a different alternate each time is what stops
    repeated hits sounding like a machine gun.
*/
namespace kit
{

/** A single recorded hit. Multi-channel because real kits are multi-mic. */
struct Sample
{
    juce::AudioBuffer<float> audio;
};

using SamplePtr = std::shared_ptr<const Sample>;

/** All the alternate takes recorded at roughly one dynamic level. */
struct VelocityLayer
{
    float minVelocity = 0.0f;   // normalised, inclusive
    float maxVelocity = 1.0f;   // normalised, inclusive
    std::vector<SamplePtr> roundRobin;
};

/** Choke groups: voices in the same group cut each other off.
    Used for hi-hats (closing the pedal kills the open sound) and for
    hand-choking a cymbal. kNone means the instrument rings out freely. */
enum ChokeGroup
{
    kNone   = -1,
    kHiHat  = 0,
    kCrash1 = 1,
    kCrash2 = 2,
    kRide   = 3
};

struct Instrument
{
    juce::String name;
    std::vector<VelocityLayer> layers;

    int   chokeGroup = kNone;
    float gain       = 1.0f;
    float pan        = 0.0f;    // -1 hard left .. +1 hard right

    /** Picks the layer covering this velocity, falling back to the nearest
        one if the layers do not quite tile the whole 0..1 range. */
    const VelocityLayer* layerForVelocity (float velocity) const
    {
        if (layers.empty())
            return nullptr;

        for (const auto& layer : layers)
            if (velocity >= layer.minVelocity && velocity <= layer.maxVelocity)
                return &layer;

        // Velocity fell in a gap between layers - use whichever edge is closest.
        const VelocityLayer* best = &layers.front();
        float bestDistance = std::numeric_limits<float>::max();

        for (const auto& layer : layers)
        {
            const float distance = velocity < layer.minVelocity
                                 ? layer.minVelocity - velocity
                                 : velocity - layer.maxVelocity;

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = &layer;
            }
        }

        return best;
    }
};

} // namespace kit
