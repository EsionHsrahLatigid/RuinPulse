#include "ruinpulse/RuinPulseEngine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

using ruinpulse::RuinPulseEngine;
using ruinpulse::RuinPulseParameters;

namespace
{
constexpr double sampleRate = 48000.0;

std::vector<float> renderNote (std::uint32_t seed, int note, float velocity, RuinPulseParameters params, int samples)
{
    RuinPulseEngine engine;
    engine.prepare (sampleRate);
    engine.setParameters (params);
    engine.reset (seed);
    engine.noteOn (note, velocity);

    std::vector<float> output;
    output.reserve (static_cast<std::size_t> (samples));
    for (int i = 0; i < samples; ++i)
        output.push_back (engine.processSample().left);

    return output;
}

float rms (const std::vector<float>& samples, std::size_t start = 0)
{
    double energy = 0.0;
    auto count = 0;
    for (auto i = start; i < samples.size(); ++i)
    {
        energy += static_cast<double> (samples[i]) * samples[i];
        ++count;
    }
    return count > 0 ? static_cast<float> (std::sqrt (energy / count)) : 0.0f;
}

float highBandEnergy (const std::vector<float>& samples)
{
    const auto coefficient = std::exp (-2.0f * 3.14159265358979323846f * 1800.0f / static_cast<float> (sampleRate));
    float low = 0.0f;
    float energy = 0.0f;

    for (const auto sample : samples)
    {
        low = (1.0f - coefficient) * sample + coefficient * low;
        const auto high = sample - low;
        energy += high * high;
    }

    return energy;
}

int transitionCount (const std::vector<float>& samples)
{
    int count = 0;
    auto previous = samples.empty() ? 0.0f : samples.front();
    for (const auto sample : samples)
    {
        if ((sample >= 0.0f) != (previous >= 0.0f) && std::fabs (sample - previous) > 0.02f)
            ++count;
        previous = sample;
    }
    return count;
}

float maxAbsDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    assert (a.size() == b.size());
    float result = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i)
        result = std::max (result, std::fabs (a[i] - b[i]));
    return result;
}

void testSilentBeforeTrigger()
{
    RuinPulseEngine engine;
    engine.prepare (sampleRate);
    engine.reset (123u);

    for (int i = 0; i < 2048; ++i)
    {
        const auto frame = engine.processSample();
        assert (std::fabs (frame.left) <= 1.0e-7f);
        assert (std::fabs (frame.right) <= 1.0e-7f);
    }
}

void testDefaultRendersAfterAttack()
{
    const auto samples = renderNote (7u, 48, 1.0f, {}, 4096);
    assert (rms (samples, 256) >= 1.0e-4f);
}

void testDeterministicSameEvents()
{
    RuinPulseParameters params;
    params.ruin = 0.74f;
    params.skew = 0.31f;
    params.drive = 0.63f;

    const auto a = renderNote (4242u, 55, 0.8f, params, 8192);
    const auto b = renderNote (4242u, 55, 0.8f, params, 8192);

    assert (maxAbsDiff (a, b) <= 1.0e-6f);
}

void testPulseSkewRuinChangeIdentityMetrics()
{
    RuinPulseParameters base;
    base.decaySeconds = 1.0f;
    base.outputGain = 0.85f;

    auto pulseWide = base;
    pulseWide.pulse = 0.85f;
    auto pulseNarrow = base;
    pulseNarrow.pulse = 0.15f;
    const auto wide = renderNote (9001u, 57, 1.0f, pulseWide, 12000);
    const auto narrow = renderNote (9001u, 57, 1.0f, pulseNarrow, 12000);
    assert (std::fabs (highBandEnergy (wide) - highBandEnergy (narrow)) > 5.0f);

    auto skewLow = base;
    skewLow.skew = 0.05f;
    auto skewHigh = base;
    skewHigh.skew = 0.95f;
    const auto lowSkew = renderNote (9001u, 57, 1.0f, skewLow, 12000);
    const auto highSkew = renderNote (9001u, 57, 1.0f, skewHigh, 12000);
    assert (std::abs (transitionCount (lowSkew) - transitionCount (highSkew)) >= 6
            || maxAbsDiff (lowSkew, highSkew) > 0.05f);

    auto clean = base;
    clean.ruin = 0.0f;
    auto ruined = base;
    ruined.ruin = 1.0f;
    const auto cleanSamples = renderNote (44u, 50, 1.0f, clean, 16000);
    const auto ruinedSamples = renderNote (44u, 50, 1.0f, ruined, 16000);
    assert (highBandEnergy (ruinedSamples) > highBandEnergy (cleanSamples) * 1.6f);
}

void testRetriggerableDecay()
{
    RuinPulseParameters params;
    params.decaySeconds = 0.03f;
    params.outputGain = 0.9f;

    RuinPulseEngine engine;
    engine.prepare (sampleRate);
    engine.setParameters (params);
    engine.reset (77u);
    engine.noteOn (52, 1.0f);

    for (int i = 0; i < 1024; ++i)
        (void) engine.processSample();
    engine.noteOff (52);

    for (int i = 0; i < 9000; ++i)
        (void) engine.processSample();

    auto decayedPeak = 0.0f;
    for (int i = 0; i < 512; ++i)
    {
        const auto frame = engine.processSample();
        decayedPeak = std::max (decayedPeak, std::fabs (frame.left));
    }
    assert (decayedPeak < 1.0e-3f);

    engine.noteOn (52, 1.0f);
    auto retriggeredPeak = 0.0f;
    for (int i = 0; i < 1024; ++i)
    {
        const auto frame = engine.processSample();
        retriggeredPeak = std::max (retriggeredPeak, std::fabs (frame.left));
    }
    assert (retriggeredPeak > 0.02f);
}

void testNoteOffReleaseTailReachesSilence()
{
    RuinPulseParameters params;
    params.decaySeconds = 0.08f;
    params.outputGain = 0.85f;

    RuinPulseEngine engine;
    engine.prepare (sampleRate);
    engine.setParameters (params);
    engine.reset (777u);
    engine.noteOn (40, 1.0f);

    for (int i = 0; i < 1024; ++i)
        (void) engine.processSample();

    engine.noteOff (40);

    bool sawTail = false;
    for (int i = 0; i < 24000; ++i)
    {
        const auto frame = engine.processSample();
        sawTail = sawTail || std::fabs (frame.left) > 1.0e-4f;
    }
    assert (sawTail);

    for (int i = 0; i < 2048; ++i)
    {
        const auto frame = engine.processSample();
        assert (std::fabs (frame.left) <= 1.0e-5f);
        assert (std::fabs (frame.right) <= 1.0e-5f);
    }
}

void testFiniteBoundedExtremeParameters()
{
    RuinPulseParameters params;
    params.pitchOffsetSemitones = 1000.0f;
    params.pulse = 1000.0f;
    params.ruin = 1000.0f;
    params.skew = 1000.0f;
    params.decaySeconds = 1000.0f;
    params.drive = 1000.0f;
    params.outputGain = 1000.0f;

    RuinPulseEngine engine;
    engine.prepare (0.0);
    engine.setParameters (params);
    engine.reset (0u);
    engine.noteOn (999, 1000.0f);

    for (int i = 0; i < 16384; ++i)
    {
        const auto frame = engine.processSample();
        assert (std::isfinite (frame.left));
        assert (std::isfinite (frame.right));
        assert (frame.left >= -0.9801f && frame.left <= 0.9801f);
        assert (frame.right >= -0.9801f && frame.right <= 0.9801f);
    }
}

void testNonFiniteParametersFallbackSafely()
{
    RuinPulseParameters params;
    params.pitchOffsetSemitones = std::numeric_limits<float>::quiet_NaN();
    params.pulse = std::numeric_limits<float>::infinity();
    params.ruin = std::numeric_limits<float>::quiet_NaN();
    params.skew = std::numeric_limits<float>::infinity();
    params.decaySeconds = -std::numeric_limits<float>::infinity();
    params.drive = std::numeric_limits<float>::quiet_NaN();
    params.outputGain = std::numeric_limits<float>::quiet_NaN();

    RuinPulseEngine engine;
    engine.prepare (std::numeric_limits<double>::infinity());
    engine.setParameters (params);
    engine.reset (31337u);
    engine.noteOn (-100, std::numeric_limits<float>::infinity());

    bool sawEnergy = false;
    for (int i = 0; i < 4096; ++i)
    {
        const auto frame = engine.processSample();
        assert (std::isfinite (frame.left));
        assert (std::isfinite (frame.right));
        assert (frame.left >= -0.9801f && frame.left <= 0.9801f);
        assert (frame.right >= -0.9801f && frame.right <= 0.9801f);
        sawEnergy = sawEnergy || std::fabs (frame.left) > 1.0e-6f || std::fabs (frame.right) > 1.0e-6f;
    }

    assert (sawEnergy);
}

void testVelocityZeroActsSilent()
{
    RuinPulseEngine engine;
    engine.prepare (sampleRate);
    engine.reset (4u);
    engine.noteOn (44, 0.0f);

    for (int i = 0; i < 1024; ++i)
    {
        const auto frame = engine.processSample();
        assert (frame.left == 0.0f);
        assert (frame.right == 0.0f);
    }
}

} // namespace

int main()
{
    testSilentBeforeTrigger();
    testDefaultRendersAfterAttack();
    testDeterministicSameEvents();
    testPulseSkewRuinChangeIdentityMetrics();
    testRetriggerableDecay();
    testNoteOffReleaseTailReachesSilence();
    testFiniteBoundedExtremeParameters();
    testNonFiniteParametersFallbackSafely();
    testVelocityZeroActsSilent();

    std::cout << "RuinPulseEngineTests passed\n";
    return 0;
}
