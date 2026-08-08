#include "ruinpulse/RuinPulseEngine.h"

#include <algorithm>
#include <cmath>

namespace ruinpulse
{

namespace
{
constexpr float ceiling = 0.98f;
constexpr float attackSeconds = 0.003f;

float midiNoteToFrequency (int noteNumber, float pitchOffsetSemitones) noexcept
{
    const auto semitone = static_cast<float> (noteNumber - 69) + pitchOffsetSemitones;
    return 440.0f * std::pow (2.0f, semitone / 12.0f);
}

float flushDenormal (float value) noexcept
{
    return std::fabs (value) < 1.0e-20f ? 0.0f : value;
}
}

RuinPulseEngine::RuinPulseEngine()
{
    prepare (44100.0);
    reset (1u);
}

void RuinPulseEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 1.0 ? newSampleRate : 44100.0;
    updateFilters();
    updateEnvelopeRates();
}

void RuinPulseEngine::reset (std::uint32_t seed) noexcept
{
    baseSeed = seed != 0u ? seed : 1u;
    currentNote = -1;
    envelopeStage = EnvelopeStage::idle;
    envelope = 0.0f;
    velocity = 0.0f;
    phase = 0.0f;
    previousPulse = 0.0f;
    avalancheState = 0.0f;
    bodyState = 0.0f;

    noiseLeft.reset (mixSeed (baseSeed ^ 0x2c1b3c6du));
    noiseRight.reset (mixSeed (baseSeed ^ 0x9e3779b9u));
    ruinNoise.reset (mixSeed (baseSeed ^ 0x85ebca6bu));
    toneLeft.reset();
    toneRight.reset();
    dcLeft.reset();
    dcRight.reset();
}

void RuinPulseEngine::setParameters (const RuinPulseParameters& parameters) noexcept
{
    params.pitchOffsetSemitones = clampFinite (parameters.pitchOffsetSemitones, -24.0f, 24.0f, RuinPulseParameters {}.pitchOffsetSemitones);
    params.pulse = clampFinite (parameters.pulse, 0.0f, 1.0f, RuinPulseParameters {}.pulse);
    params.ruin = clampFinite (parameters.ruin, 0.0f, 1.0f, RuinPulseParameters {}.ruin);
    params.skew = clampFinite (parameters.skew, 0.0f, 1.0f, RuinPulseParameters {}.skew);
    params.decaySeconds = clampFinite (parameters.decaySeconds, 0.01f, 4.0f, RuinPulseParameters {}.decaySeconds);
    params.drive = clampFinite (parameters.drive, 0.0f, 1.0f, RuinPulseParameters {}.drive);
    params.outputGain = clampFinite (parameters.outputGain, 0.0f, 1.0f, RuinPulseParameters {}.outputGain);

    updateFilters();
    updateEnvelopeRates();
}

void RuinPulseEngine::noteOn (int noteNumber, float newVelocity) noexcept
{
    const auto incomingNote = clampNote (noteNumber);
    const auto incomingVelocity = clampFinite (newVelocity, 0.0f, 1.0f, 1.0f);

    if (incomingVelocity <= 0.0f)
    {
        noteOff (incomingNote);
        return;
    }

    currentNote = incomingNote;
    velocity = incomingVelocity;

    const auto noteSeed = mixSeed (baseSeed ^ (static_cast<std::uint32_t> (currentNote) * 0x45d9f3bu));
    noiseLeft.reset (mixSeed (noteSeed ^ 0x3c6ef372u));
    noiseRight.reset (mixSeed (noteSeed ^ 0xbb67ae85u));
    ruinNoise.reset (mixSeed (noteSeed ^ 0xa54ff53au));

    phase = 0.0f;
    previousPulse = 0.0f;
    avalancheState = 0.0f;
    bodyState = 0.0f;
    phaseIncrement = std::clamp (midiNoteToFrequency (currentNote, params.pitchOffsetSemitones)
                                     / static_cast<float> (sampleRate),
                                 1.0e-6f,
                                 0.49f);

    envelope = 0.0f;
    envelopeStage = EnvelopeStage::attack;
    toneLeft.reset();
    toneRight.reset();
    dcLeft.reset();
    dcRight.reset();
}

void RuinPulseEngine::noteOff (int noteNumber) noexcept
{
    const auto safeNote = clampNote (noteNumber);
    if (currentNote == safeNote && envelopeStage != EnvelopeStage::idle)
        envelopeStage = EnvelopeStage::release;
}

StereoFrame RuinPulseEngine::processSample() noexcept
{
    const auto envelopeValue = processEnvelope();
    if (envelopeValue <= 0.0f)
        return {};

    phaseIncrement = std::clamp (midiNoteToFrequency (currentNote, params.pitchOffsetSemitones)
                                     / static_cast<float> (sampleRate),
                                 1.0e-6f,
                                 0.49f);

    const auto pulseSource = processOscillator();
    const auto noiseDepth = params.ruin * (0.015f + 0.09f * params.drive);
    const auto leftSource = pulseSource + noiseLeft.nextFloat() * noiseDepth;
    const auto rightSource = pulseSource + noiseRight.nextFloat() * noiseDepth;

    const auto left = dcLeft.process (toneLeft.process (leftSource)) * envelopeValue * params.outputGain;
    const auto right = dcRight.process (toneRight.process (rightSource)) * envelopeValue * params.outputGain;

    return sanitizeFrame (left, right);
}

void RuinPulseEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto frame = processSample();
        left[i] = frame.left;
        right[i] = frame.right;
    }
}

std::uint32_t RuinPulseEngine::mixSeed (std::uint32_t value) noexcept
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value != 0u ? value : 0x6d2b79f5u;
}

int RuinPulseEngine::clampNote (int noteNumber) noexcept
{
    return std::clamp (noteNumber, 0, 127);
}

void RuinPulseEngine::updateFilters() noexcept
{
    const auto cutoff = 1100.0f + params.ruin * 8200.0f + params.drive * 2600.0f;
    toneLeft.setLowPass (sampleRate, cutoff, 0.58f + params.skew * 0.42f);
    toneRight.setLowPass (sampleRate, cutoff * (1.0f + 0.015f * params.skew), 0.58f + params.skew * 0.42f);
    dcLeft.prepare (sampleRate, 8.0f);
    dcRight.prepare (sampleRate, 8.0f);
}

void RuinPulseEngine::updateEnvelopeRates() noexcept
{
    attackStep = 1.0f / static_cast<float> (std::max (1.0, sampleRate * static_cast<double> (attackSeconds)));
    releaseCoefficient = std::exp (-11.512925465f / static_cast<float> (std::max (1.0, sampleRate * static_cast<double> (params.decaySeconds))));
}

float RuinPulseEngine::processEnvelope() noexcept
{
    if (envelopeStage == EnvelopeStage::attack)
    {
        envelope += attackStep;
        if (envelope >= 1.0f)
        {
            envelope = 1.0f;
            envelopeStage = EnvelopeStage::hold;
        }
    }
    else if (envelopeStage == EnvelopeStage::release)
    {
        envelope *= releaseCoefficient;
        if (envelope < 1.0e-5f)
        {
            envelope = 0.0f;
            velocity = 0.0f;
            currentNote = -1;
            envelopeStage = EnvelopeStage::idle;
        }
    }

    return envelope * velocity;
}

float RuinPulseEngine::processOscillator() noexcept
{
    const auto skewCurve = 0.35f + params.skew * 1.85f;
    const auto warpedPhase = std::pow (phase, skewCurve);
    const auto width = 0.08f + params.pulse * 0.84f;
    const auto pulse = warpedPhase < width ? 1.0f : -1.0f;
    const auto transition = pulse - previousPulse;
    previousPulse = pulse;

    const auto avalancheProbability = params.ruin * (0.018f + 0.070f * params.skew);
    const auto randomUnit = static_cast<float> (ruinNoise.nextWord() >> 8u) * (1.0f / 16777216.0f);
    const auto avalancheHit = randomUnit < avalancheProbability ? ruinNoise.nextBinary() : 0.0f;
    avalancheState = flushDenormal (avalancheState * (0.88f - 0.30f * params.ruin)
                                    + transition * (0.22f + params.ruin * 0.42f)
                                    + avalancheHit * params.ruin * 0.7f);
    bodyState = flushDenormal (bodyState * (0.985f - 0.055f * params.ruin)
                               + pulse * (0.035f + params.pulse * 0.028f)
                               + avalancheState * 0.12f);

    phase += phaseIncrement * (1.0f + avalancheState * params.ruin * 0.015f);
    if (phase >= 1.0f)
        phase -= std::floor (phase);

    const auto asymmetry = (params.skew - 0.5f) * 0.18f;
    return pulse * (0.22f + params.pulse * 0.18f)
           + avalancheState * (0.36f + params.drive * 0.46f)
           + bodyState
           + asymmetry;
}

StereoFrame RuinPulseEngine::sanitizeFrame (float left, float right) const noexcept
{
    const auto driveAmount = 1.15f + params.drive * 7.5f + params.ruin * 2.0f;
    const auto drivenLeft = boundedDrive (left, driveAmount);
    const auto drivenRight = boundedDrive (right, driveAmount);
    const auto foldedLeft = boundedDrive (drivenLeft + boundedDrive (drivenLeft * (1.0f + params.ruin * 2.6f), 0.7f), 1.2f);
    const auto foldedRight = boundedDrive (drivenRight + boundedDrive (drivenRight * (1.0f + params.ruin * 2.6f), 0.7f), 1.2f);

    const auto safeLeft = flushDenormal (std::isfinite (foldedLeft) ? foldedLeft : 0.0f);
    const auto safeRight = flushDenormal (std::isfinite (foldedRight) ? foldedRight : 0.0f);
    return { std::clamp (safeLeft, -ceiling, ceiling),
             std::clamp (safeRight, -ceiling, ceiling) };
}

} // namespace ruinpulse
