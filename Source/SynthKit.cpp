#include "SynthKit.h"

#include <cmath>

using midimap::Articulation;

namespace synthkit
{
namespace
{

constexpr int kNumLayers = 4;
constexpr int kRoundRobins = 3;

//==============================================================================
/** Everything that distinguishes one kit from another.

    Keeping the differences in data rather than in separate generators is what
    makes the kits comparable: a metal kick and a jazz kick run through the
    same code, so any difference you hear is a decision here, not an accident
    of two implementations drifting apart. */
struct Voicing
{
    // Kick
    float kickStart, kickEnd, kickPitchDecay, kickDecay, kickClick;

    // Snare
    float snareToneA, snareToneB, snareToneDecay, snareNoiseDecay;
    float snareNoiseMix, snareBrightness;

    // Toms
    float tomDecay, tomTuning;

    // Hi-hats
    float hatClosedDecay, hatOpenDecay, hatBrightness;

    // Cymbals
    float crashDecay, rideDecay, rideBell, cymbalBrightness;

    // Overall
    float level;

    /** Analog machines build voices from tuned oscillators rather than struck
        membranes: less noise in the attack, purer bodies, toms that are really
        the kick retuned. */
    bool analog;

    //--------------------------------------------------------------------------
    // Production character. These exist because genre is mostly decided after
    // the drum is hit - by how it is miked, compressed and triggered - and a
    // voicing that only describes the drum cannot tell punk from studio rock.

    /** Soft saturation. Doubles as heavy compression, since it lifts the tail
        relative to the peak. 0 is clean. */
    float drive;

    /** Centre of a tuned beater click, in Hz. 0 keeps the broadband tick the
        acoustic kits use; metal needs the click pitched where it cuts through
        guitars. */
    float kickClickFreq;

    /** How much successive takes differ. Triggered metal drums are nearly
        identical hit to hit; a punk drummer is not. 1 is a natural player. */
    float humanize;

    /** Early reflections from a small, live room. 0 is dry. */
    float room;

    /** How much of a china the crashes are: harsher, lower, more inharmonic. */
    float cymbalTrash;
};

const Voicing kVoicings[(size_t) Style::NumStyles] =
{
    // Studio - neutral acoustic reference
    { 125.0f, 44.0f, 0.035f, 0.42f, 0.35f,
      185.0f, 278.0f, 0.055f, 0.26f, 0.75f, 1.0f,
      0.62f, 1.0f,
      0.085f, 0.85f, 1.0f,
      2.4f, 1.6f, 0.35f, 1.0f,
      1.0f, false,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f },

    // Big Room - same kit, bigger space
    { 130.0f, 42.0f, 0.045f, 0.72f, 0.40f,
      180.0f, 270.0f, 0.075f, 0.46f, 0.85f, 0.92f,
      1.05f, 0.96f,
      0.10f, 1.25f, 0.95f,
      4.0f, 2.6f, 0.32f, 0.92f,
      1.0f, false,
      0.0f, 0.0f, 1.0f, 0.18f, 0.0f },

    // Jazz - small shells, sticks, quick decay
    { 145.0f, 56.0f, 0.026f, 0.28f, 0.28f,
      225.0f, 335.0f, 0.040f, 0.20f, 0.62f, 1.20f,
      0.42f, 1.18f,
      0.060f, 0.52f, 1.18f,
      1.7f, 1.9f, 0.42f, 1.15f,
      0.92f, false,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f },

    // 808
    {  92.0f, 40.0f, 0.055f, 1.25f, 0.10f,
      180.0f, 330.0f, 0.030f, 0.16f, 0.45f, 1.05f,
      0.95f, 0.88f,
      0.045f, 0.55f, 1.45f,
      3.0f, 1.4f, 0.20f, 1.35f,
      1.0f, true,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f },

    // 909
    { 112.0f, 48.0f, 0.030f, 0.52f, 0.45f,
      175.0f, 320.0f, 0.038f, 0.30f, 0.95f, 1.15f,
      0.48f, 1.02f,
      0.052f, 0.42f, 1.30f,
      2.1f, 1.5f, 0.28f, 1.22f,
      1.0f, true,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f },

    // CR-78
    { 100.0f, 55.0f, 0.022f, 0.30f, 0.06f,
      210.0f, 300.0f, 0.022f, 0.11f, 0.55f, 1.10f,
      0.30f, 1.10f,
      0.030f, 0.26f, 1.25f,
      1.2f, 0.9f, 0.18f, 1.20f,
      0.88f, true,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f },

    // Punk - undamped snare ring and loose wires, washy trashy cymbals, room
    // mics pushed hot, squashed and a little distorted.
    { 118.0f, 46.0f, 0.040f, 0.50f, 0.40f,
      200.0f, 300.0f, 0.110f, 0.36f, 0.95f, 1.25f,
      0.70f, 1.00f,
      0.090f, 1.00f, 0.95f,
      3.2f, 2.4f, 0.22f, 0.90f,
      0.95f, false,
      0.45f, 0.0f, 1.50f, 0.32f, 0.80f },

    // Metal - triggered, sample-tight kick with a 4 kHz beater click and a
    // deep fast sweep; high-tuned snare with the body cut and the crack
    // pushed; deep resonant toms; a china on the right.
    { 150.0f, 50.0f, 0.018f, 0.30f, 0.95f,
      // High tuning, but the shell ring is gated short: both tones sit inside
      // the 200-450 Hz body band, so letting them ring would add exactly the
      // weight metal production cuts. Measured, not guessed - see Renders.
      260.0f, 390.0f, 0.022f, 0.22f, 0.95f, 1.45f,
      0.85f, 0.82f,
      0.050f, 0.60f, 1.25f,
      2.2f, 1.8f, 0.50f, 1.25f,
      1.0f, false,
      0.35f, 4200.0f, 0.25f, 0.0f, 0.55f }
};

const Voicing& voicingFor (Style style)
{
    const auto index = (size_t) juce::jlimit (0, getNumStyles() - 1, (int) style);
    return kVoicings[index];
}

//==============================================================================
struct OnePoleHighPass
{
    float a = 0.0f, lastIn = 0.0f, lastOut = 0.0f;

    void setCutoff (float hz, double sampleRate)
    {
        const float rc = 1.0f / (juce::MathConstants<float>::twoPi * juce::jmax (1.0f, hz));
        const float dt = 1.0f / (float) sampleRate;
        a = rc / (rc + dt);
    }

    float process (float x)
    {
        const float y = a * (lastOut + x - lastIn);
        lastIn = x;
        lastOut = y;
        return y;
    }
};

struct OnePoleLowPass
{
    float a = 0.0f, lastOut = 0.0f;

    void setCutoff (float hz, double sampleRate)
    {
        const float rc = 1.0f / (juce::MathConstants<float>::twoPi * juce::jmax (1.0f, hz));
        const float dt = 1.0f / (float) sampleRate;
        a = dt / (rc + dt);
    }

    float process (float x)
    {
        lastOut += a * (x - lastOut);
        return lastOut;
    }
};

/** Returns a mutable handle - the generators write into it. It converts to the
    const kit::SamplePtr on return. */
std::shared_ptr<kit::Sample> makeBuffer (double sampleRate, float seconds)
{
    auto sample = std::make_shared<kit::Sample>();
    const int length = juce::jmax (16, (int) (sampleRate * seconds));
    sample->audio.setSize (1, length);
    sample->audio.clear();
    return sample;
}

/** The "after the drum is hit" stage: room, then saturation, then a tail fade.
    Order matters - saturating after the room is what a hot room mic through a
    compressor actually sounds like, and the fade has to come last so neither
    stage can leave a discontinuity at the end. */
void finishSample (juce::AudioBuffer<float>& buffer, double sr, const Voicing& v)
{
    if (v.room > 0.0f)
    {
        // Grow the buffer so the reflections have somewhere to land.
        const int dryLength = buffer.getNumSamples();
        buffer.setSize (1, dryLength + (int) (sr * 0.06), true, true);

        const juce::AudioBuffer<float> dry (buffer);
        const auto* in = dry.getReadPointer (0);
        auto* out = buffer.getWritePointer (0);

        // A handful of early reflections from a small live room. Walls soak up
        // the top end, so what comes back is darker than what went out.
        const float delays[] = { 0.011f, 0.017f, 0.023f, 0.029f, 0.037f, 0.047f };
        const float gains[]  = { 0.55f,  -0.45f, 0.40f,  -0.32f, 0.26f,  -0.20f };

        OnePoleLowPass absorb;
        absorb.setCutoff (4500.0f, sr);

        std::vector<float> darkened ((size_t) dryLength);

        for (int i = 0; i < dryLength; ++i)
            darkened[(size_t) i] = absorb.process (in[i]);

        for (size_t tap = 0; tap < std::size (delays); ++tap)
        {
            const int offset = (int) (sr * delays[tap]);
            const float gain = gains[tap] * v.room;

            for (int i = 0; i < dryLength; ++i)
            {
                const int target = i + offset;

                if (target < buffer.getNumSamples())
                    out[target] += darkened[(size_t) i] * gain;
            }
        }
    }

    if (v.drive > 0.0f)
    {
        // tanh(k x) / tanh(k): unity at full scale, but everything below it is
        // lifted - saturation and compression in one move.
        const float k = 1.0f + v.drive * 4.0f;
        const float norm = 1.0f / std::tanh (k);
        auto* data = buffer.getWritePointer (0);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            data[i] = std::tanh (data[i] * k) * norm;
    }

    const int fadeLength = juce::jmin (buffer.getNumSamples(), (int) (sr * 0.005));

    if (fadeLength > 1)
        buffer.applyGainRamp (buffer.getNumSamples() - fadeLength, fadeLength, 1.0f, 0.0f);
}

//==============================================================================
kit::SamplePtr makeKick (double sr, const Voicing& v, float intensity, juce::Random& rng)
{
    const float decay = v.kickDecay * (0.88f + 0.22f * intensity);
    auto sample = makeBuffer (sr, decay + 0.05f);
    auto* data = sample->audio.getWritePointer (0);
    const int length = sample->audio.getNumSamples();

    const float jitter = v.humanize;
    const float startFreq = v.kickStart + rng.nextFloat() * (v.analog ? 4.0f : 12.0f) * jitter;
    const float endFreq = v.kickEnd + rng.nextFloat() * (v.analog ? 0.8f : 3.0f) * jitter;

    OnePoleHighPass clickFilter;

    if (v.kickClickFreq > 0.0f)
        clickFilter.setCutoff (v.kickClickFreq * 0.7f, sr);

    float phase = 0.0f;

    for (int i = 0; i < length; ++i)
    {
        const float t = (float) i / (float) sr;
        const float freq = endFreq + (startFreq - endFreq) * std::exp (-t / v.kickPitchDecay);

        phase += juce::MathConstants<float>::twoPi * freq / (float) sr;

        const float bodyDecay = v.analog ? decay * 0.55f : decay * 0.35f;
        const float body = std::sin (phase) * std::exp (-t / bodyDecay);

        float click;

        if (v.kickClickFreq > 0.0f)
        {
            // A pitched beater click: filtered noise plus a short tone at the
            // click frequency. This is the "tick" that lets double-kick parts
            // stay audible under distorted guitars.
            const float noise = clickFilter.process (rng.nextFloat() * 2.0f - 1.0f);
            const float tone = std::sin (juce::MathConstants<float>::twoPi * v.kickClickFreq * t);
            click = (noise * 0.7f + tone * 0.5f) * std::exp (-t / 0.0045f) * v.kickClick;
        }
        else
        {
            click = (rng.nextFloat() * 2.0f - 1.0f)
                  * std::exp (-t / 0.0025f) * v.kickClick * intensity;
        }

        data[i] = (body * 0.92f + click) * (0.45f + 0.55f * intensity) * v.level;
    }

    finishSample (sample->audio, sr, v);
    return sample;
}

kit::SamplePtr makeSnare (double sr, const Voicing& v, float intensity,
                          juce::Random& rng, bool rimshot)
{
    const float decay = v.snareNoiseDecay * (rimshot ? 1.3f : 1.0f);
    auto sample = makeBuffer (sr, decay + 0.08f);
    auto* data = sample->audio.getWritePointer (0);
    const int length = sample->audio.getNumSamples();

    const float toneA = v.snareToneA * (rimshot ? 1.8f : 1.0f) + rng.nextFloat() * 8.0f * v.humanize;
    const float toneB = v.snareToneB * (rimshot ? 1.8f : 1.0f) + rng.nextFloat() * 10.0f * v.humanize;

    OnePoleHighPass hp;
    hp.setCutoff ((1200.0f + 2200.0f * intensity) * v.snareBrightness, sr);

    float phaseA = 0.0f, phaseB = 0.0f;

    for (int i = 0; i < length; ++i)
    {
        const float t = (float) i / (float) sr;

        phaseA += juce::MathConstants<float>::twoPi * toneA / (float) sr;
        phaseB += juce::MathConstants<float>::twoPi * toneB / (float) sr;

        const float shell = (std::sin (phaseA) * 0.6f + std::sin (phaseB) * 0.4f)
                          * std::exp (-t / v.snareToneDecay);

        const float wires = hp.process (rng.nextFloat() * 2.0f - 1.0f)
                          * std::exp (-t / (decay * 0.42f));

        const float crack = rimshot
                          ? (rng.nextFloat() * 2.0f - 1.0f) * std::exp (-t / 0.0018f) * 0.6f
                          : 0.0f;

        data[i] = (shell * (1.0f - v.snareNoiseMix * 0.55f)
                     + wires * v.snareNoiseMix
                     + crack)
                * (0.35f + 0.65f * intensity) * v.level;
    }

    finishSample (sample->audio, sr, v);
    return sample;
}

kit::SamplePtr makeCrossStick (double sr, const Voicing& v, float intensity, juce::Random& rng)
{
    auto sample = makeBuffer (sr, 0.14f);
    auto* data = sample->audio.getWritePointer (0);
    const int length = sample->audio.getNumSamples();

    const float tone = 520.0f * v.snareBrightness + rng.nextFloat() * 40.0f * v.humanize;
    float phase = 0.0f;

    for (int i = 0; i < length; ++i)
    {
        const float t = (float) i / (float) sr;
        phase += juce::MathConstants<float>::twoPi * tone / (float) sr;

        const float knock = std::sin (phase) * std::exp (-t / 0.018f);
        const float tick = (rng.nextFloat() * 2.0f - 1.0f) * std::exp (-t / 0.0012f) * 0.5f;

        data[i] = (knock * 0.7f + tick) * (0.4f + 0.6f * intensity) * v.level;
    }

    finishSample (sample->audio, sr, v);
    return sample;
}

kit::SamplePtr makeTom (double sr, const Voicing& v, float intensity, juce::Random& rng,
                        float startFreq, float endFreq, bool rim)
{
    const float decay = v.tomDecay * (rim ? 0.5f : 1.0f);
    auto sample = makeBuffer (sr, decay + 0.08f);
    auto* data = sample->audio.getWritePointer (0);
    const int length = sample->audio.getNumSamples();

    const float detune = 1.0f + (rng.nextFloat() - 0.5f) * (v.analog ? 0.012f : 0.04f) * v.humanize;
    float phase = 0.0f;

    for (int i = 0; i < length; ++i)
    {
        const float t = (float) i / (float) sr;
        const float freq = (endFreq + (startFreq - endFreq) * std::exp (-t / 0.09f)) * detune;

        phase += juce::MathConstants<float>::twoPi * freq / (float) sr;

        const float body = std::sin (phase) * std::exp (-t / (decay * 0.4f));

        const float attack = (rng.nextFloat() * 2.0f - 1.0f)
                           * std::exp (-t / 0.003f) * (v.analog ? 0.05f : 0.30f) * intensity;

        data[i] = (body * 0.85f + attack) * (0.4f + 0.6f * intensity) * v.level;
    }

    finishSample (sample->audio, sr, v);
    return sample;
}

kit::SamplePtr makeHiHat (double sr, const Voicing& v, float intensity, juce::Random& rng,
                          float decay, bool edge)
{
    auto sample = makeBuffer (sr, decay + 0.06f);
    auto* data = sample->audio.getWritePointer (0);
    const int length = sample->audio.getNumSamples();

    OnePoleHighPass hp;
    hp.setCutoff ((edge ? 5500.0f : 7200.0f) * v.hatBrightness, sr);

    OnePoleLowPass lp;
    lp.setCutoff ((9000.0f + 4000.0f * intensity) * v.hatBrightness, sr);

    // Analog hats are six square oscillators through a high pass, not noise.
    // That metallic ring is the whole character of an 808 hat.
    constexpr int kRingMods = 6;
    float phases[kRingMods] = {};
    const float ringFreqs[kRingMods] = { 3011.0f, 4109.0f, 5501.0f, 6301.0f, 7411.0f, 8707.0f };

    for (int i = 0; i < length; ++i)
    {
        const float t = (float) i / (float) sr;

        float source;

        if (v.analog)
        {
            float metal = 0.0f;

            for (int p = 0; p < kRingMods; ++p)
            {
                phases[p] += juce::MathConstants<float>::twoPi * ringFreqs[p] * v.hatBrightness / (float) sr;
                metal += std::sin (phases[p]) > 0.0f ? 1.0f : -1.0f;
            }

            source = metal / (float) kRingMods;
        }
        else
        {
            source = rng.nextFloat() * 2.0f - 1.0f;
        }

        const float shaped = lp.process (hp.process (source));

        data[i] = shaped * std::exp (-t / (decay * 0.38f))
                * (0.3f + 0.7f * intensity) * 0.8f * v.level;
    }

    finishSample (sample->audio, sr, v);
    return sample;
}

kit::SamplePtr makeCymbal (double sr, const Voicing& v, float intensity, juce::Random& rng,
                           float decay, float bellAmount, float trash)
{
    auto sample = makeBuffer (sr, decay + 0.1f);
    auto* data = sample->audio.getWritePointer (0);
    const int length = sample->audio.getNumSamples();

    // A china sits lower and rougher than a crash, so trash pulls the filters
    // down as well as adding its own partials.
    OnePoleHighPass hp;
    hp.setCutoff (2800.0f * v.cymbalBrightness * (1.0f - trash * 0.45f), sr);

    OnePoleLowPass lp;
    lp.setCutoff ((7000.0f + 6000.0f * intensity) * v.cymbalBrightness, sr);

    constexpr int kPartials = 6;
    float phases[kPartials] = {};
    float trashPhases[kPartials] = {};
    float freqs[kPartials];
    float trashFreqs[kPartials];

    for (int p = 0; p < kPartials; ++p)
    {
        freqs[p] = 420.0f * (1.0f + (float) p * 1.37f) + rng.nextFloat() * 60.0f;
        trashFreqs[p] = 310.0f * (1.0f + (float) p * 0.83f) + rng.nextFloat() * 90.0f;
    }

    for (int i = 0; i < length; ++i)
    {
        const float t = (float) i / (float) sr;

        const float noise = lp.process (hp.process (rng.nextFloat() * 2.0f - 1.0f));

        float metal = 0.0f;
        float rough = 0.0f;

        for (int p = 0; p < kPartials; ++p)
        {
            phases[p] += juce::MathConstants<float>::twoPi * freqs[p] / (float) sr;
            metal += std::sin (phases[p]);

            // Clipped partials: the buzzy, inharmonic edge of a china.
            trashPhases[p] += juce::MathConstants<float>::twoPi * trashFreqs[p] / (float) sr;
            rough += juce::jlimit (-0.4f, 0.4f, std::sin (trashPhases[p]));
        }

        metal /= (float) kPartials;
        rough /= (float) kPartials * 0.4f;

        const float attack = 1.0f - std::exp (-t / 0.004f);

        // Chinas bloom fast and die quicker than crashes of the same size.
        const float envelope = attack * std::exp (-t / (decay * 0.45f * (1.0f - trash * 0.3f)));

        const float voice = noise * (1.0f - bellAmount) + metal * bellAmount;

        data[i] = (voice * (1.0f - trash * 0.45f) + rough * trash * 0.55f)
                * envelope * (0.28f + 0.72f * intensity) * 0.75f * v.level;
    }

    finishSample (sample->audio, sr, v);
    return sample;
}

//==============================================================================
template <typename GeneratorFn>
kit::Instrument buildInstrument (const juce::String& name, int chokeGroup,
                                 float gain, float pan, int seed, GeneratorFn&& generate)
{
    kit::Instrument instrument;
    instrument.name = name;
    instrument.chokeGroup = chokeGroup;
    instrument.gain = gain;
    instrument.pan = pan;

    for (int layer = 0; layer < kNumLayers; ++layer)
    {
        kit::VelocityLayer velocityLayer;
        velocityLayer.minVelocity = (float) layer / (float) kNumLayers;
        velocityLayer.maxVelocity = (float) (layer + 1) / (float) kNumLayers;

        const float intensity = (velocityLayer.minVelocity + velocityLayer.maxVelocity) * 0.5f;

        for (int rr = 0; rr < kRoundRobins; ++rr)
        {
            // A distinct seed per take is what makes the alternates differ.
            juce::Random rng (seed * 7919 + layer * 131 + rr * 17 + 1);
            velocityLayer.roundRobin.push_back (generate (intensity, rng));
        }

        instrument.layers.push_back (std::move (velocityLayer));
    }

    return instrument;
}

} // anonymous namespace

//==============================================================================
juce::String getStyleName (Style style)
{
    switch (style)
    {
        case Style::Studio:      return "Studio";
        case Style::BigRoom:     return "Big Room";
        case Style::Jazz:        return "Jazz";
        case Style::Eight08:     return "808";
        case Style::Nine09:      return "909";
        case Style::CR78:        return "CR-78";
        case Style::Punk:        return "Punk";
        case Style::Metal:       return "Metal";
        case Style::NumStyles:
        default:                 return "Studio";
    }
}

juce::String getStyleDescription (Style style)
{
    switch (style)
    {
        case Style::Studio:  return "Neutral acoustic kit";
        case Style::BigRoom: return "Long decays, plenty of air";
        case Style::Jazz:    return "Small shells, quick and bright";
        case Style::Eight08: return "Long tuned kick, ticking snare";
        case Style::Nine09:  return "Punchy and bright";
        case Style::CR78:    return "Tiny and polite, 1978";
        case Style::Punk:    return "Loose, loud and trashy, in a small room";
        case Style::Metal:   return "Triggered kick click, cracking snare, deep toms";
        case Style::NumStyles:
        default:             return {};
    }
}

//==============================================================================
void build (DrumEngine& engine, double sampleRate, Style style)
{
    const auto sr = sampleRate;
    const auto& v = voicingFor (style);

    engine.setInstrument (Articulation::Kick,
        buildInstrument ("Kick", kit::kNone, 1.0f, 0.0f, 1,
            [sr, &v] (float i, juce::Random& r) { return makeKick (sr, v, i, r); }));

    engine.setInstrument (Articulation::SnareHead,
        buildInstrument ("Snare", kit::kNone, 0.9f, -0.08f, 2,
            [sr, &v] (float i, juce::Random& r) { return makeSnare (sr, v, i, r, false); }));

    engine.setInstrument (Articulation::SnareRimshot,
        buildInstrument ("Snare Rimshot", kit::kNone, 0.95f, -0.08f, 3,
            [sr, &v] (float i, juce::Random& r) { return makeSnare (sr, v, i, r, true); }));

    engine.setInstrument (Articulation::SnareCrossStick,
        buildInstrument ("Cross Stick", kit::kNone, 0.8f, -0.08f, 4,
            [sr, &v] (float i, juce::Random& r) { return makeCrossStick (sr, v, i, r); }));

    // Toms sweep left to right across the kit, high to low.
    struct TomSpec { Articulation head, rim; float start, end, pan; int seed; };

    const TomSpec toms[] =
    {
        { Articulation::Tom1, Articulation::Tom1Rim, 240.0f, 165.0f, -0.35f, 5 },
        { Articulation::Tom2, Articulation::Tom2Rim, 190.0f, 130.0f, -0.12f, 6 },
        { Articulation::Tom3, Articulation::Tom3Rim, 145.0f, 100.0f,  0.22f, 7 },
        { Articulation::Tom4, Articulation::Tom4Rim, 110.0f,  76.0f,  0.45f, 8 }
    };

    for (const auto& tom : toms)
    {
        const float start = tom.start * v.tomTuning;
        const float end = tom.end * v.tomTuning;

        engine.setInstrument (tom.head,
            buildInstrument ("Tom", kit::kNone, 0.85f, tom.pan, tom.seed,
                [sr, &v, start, end] (float i, juce::Random& r)
                { return makeTom (sr, v, i, r, start, end, false); }));

        engine.setInstrument (tom.rim,
            buildInstrument ("Tom Rim", kit::kNone, 0.7f, tom.pan, tom.seed + 40,
                [sr, &v, start, end] (float i, juce::Random& r)
                { return makeTom (sr, v, i, r, start * 1.6f, end * 1.6f, true); }));
    }

    // Hi-hats share a choke group so they cut each other off.
    engine.setInstrument (Articulation::HiHatClosedBow,
        buildInstrument ("HH Closed", kit::kHiHat, 0.65f, -0.42f, 10,
            [sr, &v] (float i, juce::Random& r) { return makeHiHat (sr, v, i, r, v.hatClosedDecay, false); }));

    engine.setInstrument (Articulation::HiHatClosedEdge,
        buildInstrument ("HH Closed Edge", kit::kHiHat, 0.7f, -0.42f, 11,
            [sr, &v] (float i, juce::Random& r) { return makeHiHat (sr, v, i, r, v.hatClosedDecay * 1.4f, true); }));

    engine.setInstrument (Articulation::HiHatOpenBow,
        buildInstrument ("HH Open", kit::kHiHat, 0.65f, -0.42f, 12,
            [sr, &v] (float i, juce::Random& r) { return makeHiHat (sr, v, i, r, v.hatOpenDecay, false); }));

    engine.setInstrument (Articulation::HiHatOpenEdge,
        buildInstrument ("HH Open Edge", kit::kHiHat, 0.7f, -0.42f, 13,
            [sr, &v] (float i, juce::Random& r) { return makeHiHat (sr, v, i, r, v.hatOpenDecay * 1.25f, true); }));

    engine.setInstrument (Articulation::HiHatPedal,
        buildInstrument ("HH Pedal", kit::kHiHat, 0.55f, -0.42f, 14,
            [sr, &v] (float i, juce::Random& r) { return makeHiHat (sr, v, i, r, v.hatClosedDecay * 0.65f, false); }));

    // Crash 1 carries a little of the trash; crash 2 is where the china lives.
    const float trash1 = v.cymbalTrash * 0.35f;
    const float trash2 = v.cymbalTrash;

    engine.setInstrument (Articulation::Crash1Bow,
        buildInstrument ("Crash 1", kit::kCrash1, 0.7f, -0.55f, 20,
            [sr, &v, trash1] (float i, juce::Random& r) { return makeCymbal (sr, v, i, r, v.crashDecay, 0.12f, trash1); }));

    engine.setInstrument (Articulation::Crash1Edge,
        buildInstrument ("Crash 1 Edge", kit::kCrash1, 0.75f, -0.55f, 21,
            [sr, &v, trash1] (float i, juce::Random& r) { return makeCymbal (sr, v, i, r, v.crashDecay * 1.16f, 0.08f, trash1); }));

    engine.setInstrument (Articulation::Crash2Bow,
        buildInstrument ("Crash 2", kit::kCrash2, 0.7f, 0.58f, 22,
            [sr, &v, trash2] (float i, juce::Random& r) { return makeCymbal (sr, v, i, r, v.crashDecay * 0.92f, 0.12f, trash2); }));

    engine.setInstrument (Articulation::Crash2Edge,
        buildInstrument ("Crash 2 Edge", kit::kCrash2, 0.75f, 0.58f, 23,
            [sr, &v, trash2] (float i, juce::Random& r) { return makeCymbal (sr, v, i, r, v.crashDecay * 1.08f, 0.08f, trash2); }));

    engine.setInstrument (Articulation::RideBow,
        buildInstrument ("Ride", kit::kRide, 0.62f, 0.4f, 24,
            [sr, &v] (float i, juce::Random& r) { return makeCymbal (sr, v, i, r, v.rideDecay, v.rideBell, 0.0f); }));

    engine.setInstrument (Articulation::RideEdge,
        buildInstrument ("Ride Edge", kit::kRide, 0.68f, 0.4f, 25,
            [sr, &v] (float i, juce::Random& r) { return makeCymbal (sr, v, i, r, v.rideDecay * 1.35f, v.rideBell * 0.45f, 0.0f); }));

    engine.setInstrument (Articulation::RideBell,
        buildInstrument ("Ride Bell", kit::kRide, 0.72f, 0.4f, 26,
            [sr, &v] (float i, juce::Random& r) { return makeCymbal (sr, v, i, r, v.rideDecay * 1.18f, juce::jmin (0.85f, v.rideBell * 2.1f), 0.0f); }));
}

} // namespace synthkit
