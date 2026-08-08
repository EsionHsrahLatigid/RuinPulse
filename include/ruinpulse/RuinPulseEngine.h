#pragma once

#include "ruinpulse/RuinPulseDspPrimitives.h"

#include <cstdint>

namespace ruinpulse
{

/** Realtime-safe parameter set for the RuinPulse pulse-avalanche instrument.

    All values are sanitized by setParameters():
    pitchOffsetSemitones [-24, 24], pulse [0, 1], ruin [0, 1], skew [0, 1],
    decaySeconds [0.01, 4], drive [0, 1], outputGain [0, 1].
*/
struct RuinPulseParameters
{
    float pitchOffsetSemitones = 0.0f;
    float pulse = 0.48f;
    float ruin = 0.38f;
    float skew = 0.52f;
    float decaySeconds = 0.72f;
    float drive = 0.42f;
    float outputGain = 0.72f;
};

/** Monophonic MIDI-like phase-distorted pulse-avalanche synth.

    noteOn() retriggers a velocity-sensitive attack/hold envelope. noteOff()
    starts the parameterized decay tail. The MIDI note number tunes the
    oscillator and reseeds deterministic ruin bursts. processSample() and
    process() allocate no memory and always return finite, ceiling-bounded samples.
*/
class RuinPulseEngine
{
public:
    RuinPulseEngine();

    /** Sets the sample rate and rebuilds filters; invalid rates fall back to 44.1 kHz. */
    void prepare (double sampleRate) noexcept;

    /** Clears state and sets the deterministic base seed used by future noteOn calls. */
    void reset (std::uint32_t seed = 1u) noexcept;

    /** Applies sanitized parameters and updates filter/envelope coefficients. */
    void setParameters (const RuinPulseParameters& parameters) noexcept;

    /** Starts or retriggers the monophonic note, with velocity clamped to [0, 1]. */
    void noteOn (int noteNumber, float velocity) noexcept;

    /** Releases the current note; mismatched note numbers are ignored while another note is held. */
    void noteOff (int noteNumber) noexcept;

    /** Renders one stereo frame. Silent before noteOn and after envelope decay. */
    [[nodiscard]] StereoFrame processSample() noexcept;

    /** Renders numSamples into stereo buffers when both pointers are valid. */
    void process (float* left, float* right, int numSamples) noexcept;

private:
    enum class EnvelopeStage
    {
        idle,
        attack,
        hold,
        release
    };

    struct ClampedParameters
    {
        float pitchOffsetSemitones = 0.0f;
        float pulse = 0.48f;
        float ruin = 0.38f;
        float skew = 0.52f;
        float decaySeconds = 0.72f;
        float drive = 0.42f;
        float outputGain = 0.72f;
    };

    static std::uint32_t mixSeed (std::uint32_t value) noexcept;
    static int clampNote (int noteNumber) noexcept;

    void updateFilters() noexcept;
    void updateEnvelopeRates() noexcept;
    [[nodiscard]] float processEnvelope() noexcept;
    [[nodiscard]] float processOscillator() noexcept;
    [[nodiscard]] StereoFrame sanitizeFrame (float left, float right) const noexcept;

    ClampedParameters params;
    double sampleRate = 44100.0;
    std::uint32_t baseSeed = 1u;
    int currentNote = -1;

    DeterministicNoise noiseLeft;
    DeterministicNoise noiseRight;
    DeterministicNoise ruinNoise;
    Biquad toneLeft;
    Biquad toneRight;
    DcBlocker dcLeft;
    DcBlocker dcRight;

    EnvelopeStage envelopeStage = EnvelopeStage::idle;
    float envelope = 0.0f;
    float velocity = 0.0f;
    float attackStep = 1.0f;
    float releaseCoefficient = 0.997f;

    float phase = 0.0f;
    float phaseIncrement = 0.01f;
    float previousPulse = 0.0f;
    float avalancheState = 0.0f;
    float bodyState = 0.0f;
};

} // namespace ruinpulse
