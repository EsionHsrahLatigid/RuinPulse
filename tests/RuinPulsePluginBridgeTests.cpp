#include "RuinPulsePlugin.h"

#include <yup_audio_processors/yup_audio_processors.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace
{
constexpr int numChannels = 2;
constexpr int blockSamples = 4096;

class PluginHarness
{
public:
    PluginHarness()
        : audio (numChannels, blockSamples)
        , context { audio, midi, automation, nullptr, {}, {} }
    {
        const auto parameters = plugin.getParameters();
        parameters[4]->setValue (0.02f);
        plugin.prepareToPlay (yup::AudioSpec (48000.0f, blockSamples, numChannels));
    }

    float process()
    {
        audio.clear();
        plugin.processBlock (context);
        return peak();
    }

    std::array<float, blockSamples> processLeft()
    {
        audio.clear();
        plugin.processBlock (context);

        std::array<float, blockSamples> result {};
        const auto* samples = audio.getReadPointer (0);
        for (int sample = 0; sample < blockSamples; ++sample)
            result[static_cast<std::size_t> (sample)] = samples[sample];
        return result;
    }

    void addMidiNoteOn (int note, int sample)
    {
        midi.addEvent (yup::MidiMessage::noteOn (1, note, 1.0f), sample);
    }

    void addMidiNoteOff (int note, int sample)
    {
        midi.addEvent (yup::MidiMessage::noteOff (1, note), sample);
    }

    float peak() const
    {
        float result = 0.0f;
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        {
            const auto* samples = audio.getReadPointer (channel);
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                result = std::max (result, std::fabs (samples[sample]));
        }
        return result;
    }

    ruinpulse::plugin::RuinPulsePlugin plugin;

private:
    yup::AudioBuffer<float> audio;
    yup::MidiBuffer midi;
    yup::ParameterChangeBuffer automation;
    yup::AudioProcessContext<float> context;
};

void processUntilSilent (PluginHarness& harness)
{
    for (int i = 0; i < 80; ++i)
    {
        const auto peak = harness.process();
        if (peak < 1.0e-5f)
            return;
    }

    assert (false);
}

void testHeldSyntheticTriggerRendersAndMeters()
{
    PluginHarness harness;
    harness.plugin.setStandaloneTriggerGate (true);

    const auto peak = harness.process();

    assert (harness.plugin.isStandaloneTriggerGateRequested());
    assert (harness.plugin.getOutputPeakLevel() > 0.0f);
    assert (peak > 1.0e-5f);
}

void testRapidOnOffBeforeCallbackStillRendersRelease()
{
    PluginHarness harness;

    const auto startEdges = harness.plugin.getStandaloneTriggerEdgeCountForTests();
    harness.plugin.setStandaloneTriggerGate (true);
    harness.plugin.setStandaloneTriggerGate (false);
    assert (harness.plugin.getStandaloneTriggerEdgeCountForTests() == startEdges + 2u);
    assert (! harness.plugin.isStandaloneTriggerGateRequested());

    const auto peak = harness.process();
    assert (peak > 1.0e-5f);

    processUntilSilent (harness);
}

void testMidiNoteOffRestartsHeldStandaloneGate()
{
    PluginHarness harness;

    harness.plugin.setStandaloneTriggerGate (true);
    assert (harness.process() > 1.0e-5f);

    harness.addMidiNoteOn (60, 0);
    assert (harness.process() > 1.0e-5f);

    harness.addMidiNoteOff (60, 0);
    assert (harness.process() > 1.0e-5f);

    harness.plugin.setStandaloneTriggerGate (false);
    processUntilSilent (harness);
}

void testUiPressReleaseDoesNotInterruptHeldMidi()
{
    PluginHarness midiOnly;
    midiOnly.addMidiNoteOn (60, 0);
    const auto expected = midiOnly.processLeft();

    PluginHarness withUiEdges;
    withUiEdges.addMidiNoteOn (60, 0);
    withUiEdges.plugin.setStandaloneTriggerGate (true);
    withUiEdges.plugin.setStandaloneTriggerGate (false);
    const auto actual = withUiEdges.processLeft();

    assert (actual == expected);

    withUiEdges.addMidiNoteOff (60, 0);
    withUiEdges.process();
    processUntilSilent (withUiEdges);
}

void testHeldUiGateDoesNotInterruptMidiUntilMidiOff()
{
    PluginHarness midiOnly;
    midiOnly.addMidiNoteOn (60, 0);
    const auto expected = midiOnly.processLeft();

    PluginHarness withHeldUiGate;
    withHeldUiGate.addMidiNoteOn (60, 0);
    withHeldUiGate.plugin.setStandaloneTriggerGate (true);
    const auto actual = withHeldUiGate.processLeft();

    assert (actual == expected);
    assert (withHeldUiGate.plugin.isStandaloneTriggerGateRequested());

    withHeldUiGate.addMidiNoteOff (60, 0);
    assert (withHeldUiGate.process() > 1.0e-5f);

    withHeldUiGate.plugin.setStandaloneTriggerGate (false);
    processUntilSilent (withHeldUiGate);
}

void testFlushDoesNotSuppressHeldStandaloneGate()
{
    PluginHarness harness;

    harness.plugin.setStandaloneTriggerGate (true);
    assert (harness.process() > 1.0e-5f);

    harness.plugin.flush();
    assert (harness.plugin.isStandaloneTriggerGateRequested());
    assert (harness.process() > 1.0e-5f);

    harness.plugin.setStandaloneTriggerGate (false);
    processUntilSilent (harness);
}

void testReleaseGateDecaysToSilence()
{
    PluginHarness harness;

    harness.plugin.setStandaloneTriggerGate (true);
    assert (harness.process() > 1.0e-5f);

    harness.plugin.setStandaloneTriggerGate (false);
    assert (! harness.plugin.isStandaloneTriggerGateRequested());
    assert (harness.process() > 1.0e-5f);

    processUntilSilent (harness);
}

void testStateRoundTripUsesRuinPulseMagicAndStableParameterIDs()
{
    ruinpulse::plugin::RuinPulsePlugin source;
    const auto sourceParameters = source.getParameters();
    assert (sourceParameters.size() == 7u);

    source.setCurrentPreset (2);
    source.getParameterByID ("pitch_offset")->setValue (5.0f);
    source.getParameterByID ("pulse")->setValue (0.83f);
    source.getParameterByID ("ruin")->setValue (0.91f);
    source.getParameterByID ("skew")->setValue (0.22f);
    source.getParameterByID ("decay")->setValue (0.36f);
    source.getParameterByID ("drive")->setValue (0.77f);
    source.getParameterByID ("output")->setValue (0.64f);

    yup::MemoryBlock state;
    assert (source.saveStateIntoMemory (state).wasOk());
    assert (state.getSize() > 16u);
    const auto* bytes = static_cast<const char*> (state.getData());
    assert (bytes[0] == 'R' && bytes[1] == 'N' && bytes[2] == 'P' && bytes[3] == '1');

    ruinpulse::plugin::RuinPulsePlugin target;
    assert (target.loadStateFromMemory (state).wasOk());
    assert (target.getCurrentPreset() == 2);

    for (const auto& sourceParameter : sourceParameters)
    {
        const auto targetParameter = target.getParameterByID (sourceParameter->getID());
        assert (targetParameter != nullptr);
        assert (std::fabs (sourceParameter->getValue() - targetParameter->getValue()) <= 1.0e-6f);
    }

    yup::MemoryBlock invalidState (state);
    auto* invalidBytes = static_cast<char*> (invalidState.getData());
    invalidBytes[0] = 'S';
    assert (target.loadStateFromMemory (invalidState).failed());
}
} // namespace

int main()
{
    testHeldSyntheticTriggerRendersAndMeters();
    testRapidOnOffBeforeCallbackStillRendersRelease();
    testMidiNoteOffRestartsHeldStandaloneGate();
    testUiPressReleaseDoesNotInterruptHeldMidi();
    testHeldUiGateDoesNotInterruptMidiUntilMidiOff();
    testFlushDoesNotSuppressHeldStandaloneGate();
    testReleaseGateDecaysToSilence();
    testStateRoundTripUsesRuinPulseMagicAndStableParameterIDs();

    std::cout << "RuinPulsePluginBridgeTests passed\n";
    return 0;
}
