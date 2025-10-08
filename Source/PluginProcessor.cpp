#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>

ProTuneAudioProcessor::ProTuneAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "Parameters", createParameterLayout())
{
    DBG ("ProTuneAudioProcessor constructed");
    speedParam = parameters.getRawParameterValue ("speed");
    transitionParam = parameters.getRawParameterValue ("transition");
    toleranceParam = parameters.getRawParameterValue ("tolerance");
    formantParam = parameters.getRawParameterValue ("formant");
    vibratoParam = parameters.getRawParameterValue ("vibrato");
    rangeLowParam = parameters.getRawParameterValue ("rangeLow");
    rangeHighParam = parameters.getRawParameterValue ("rangeHigh");
    chromaticParam = parameters.getRawParameterValue ("chromatic");
    midiParam = parameters.getRawParameterValue ("midiEnabled");
}

void ProTuneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    DBG ("prepareToPlay — sampleRate: " << sampleRate << ", blockSize: " << samplesPerBlock);
    engine.prepare (sampleRate, samplesPerBlock);
    updateEngineParameters();
}

void ProTuneAudioProcessor::releaseResources()
{
    engine.reset();
}

bool ProTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto input = layouts.getChannelSet (true, 0);
    auto output = layouts.getChannelSet (false, 0);

    if (input.isDisabled() || output.isDisabled())
        return false;

    if (input.size() != output.size())
        return false;

    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

void ProTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    DBG ("processBlock — channels: " << buffer.getNumChannels() << ", samples: " << buffer.getNumSamples());
    juce::ScopedNoDenormals noDenormals;

    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    updateEngineParameters();
    engine.pushMidi (midiMessages);
    engine.process (buffer);

    lastDetectedFrequency = engine.getLastDetectedFrequency();
    lastTargetFrequency = engine.getLastTargetFrequency();
}

juce::AudioProcessorEditor* ProTuneAudioProcessor::createEditor()
{
    DBG ("createEditor called");
    return new ProTuneAudioProcessorEditor (*this);
}

void ProTuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void ProTuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, sizeInBytes);
    if (! tree.isValid())
        return;

    parameters.replaceState (tree);
    updateEngineParameters();
}

void ProTuneAudioProcessor::updateEngineParameters()
{
    engineParameters.speed = speedParam->load();
    engineParameters.transition = transitionParam->load();
    engineParameters.toleranceCents = toleranceParam->load();
    engineParameters.formantPreserve = formantParam->load();
    engineParameters.vibratoTracking = vibratoParam->load();
    engineParameters.rangeLowHz = rangeLowParam->load();
    engineParameters.rangeHighHz = rangeHighParam->load();
    engineParameters.chromaticScale = chromaticParam->load() > 0.5f;
    engineParameters.midiEnabled = midiParam->load() > 0.5f;

    if (engineParameters.rangeLowHz > engineParameters.rangeHighHz)
        std::swap (engineParameters.rangeLowHz, engineParameters.rangeHighHz);

    engine.setParameters (engineParameters);
}

juce::AudioProcessorValueTreeState::ParameterLayout ProTuneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("speed", "Speed",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f, 1.0f), 0.5f,
        juce::AudioParameterFloatAttributes().withLabel ("")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("transition", "Note Transition",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f, 1.0f), 0.4f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tolerance", "Tolerance (cents)",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f, 0.5f), 10.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("formant", "Formant Preserve",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f, 1.0f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("vibrato", "Vibrato Tracking",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f, 1.0f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("rangeLow", "Range Low (Hz)",
        juce::NormalisableRange<float> (40.0f, 500.0f, 0.01f, 0.5f), 80.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("rangeHigh", "Range High (Hz)",
        juce::NormalisableRange<float> (120.0f, 2000.0f, 0.01f, 0.5f), 800.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> ("chromatic", "Chromatic Scale", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("midiEnabled", "MIDI Control", false));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    DBG ("createPluginFilter invoked");
    return new ProTuneAudioProcessor();
}
