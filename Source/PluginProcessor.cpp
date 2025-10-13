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
    scaleModeParam = parameters.getRawParameterValue ("scaleMode");
    scaleRootParam = parameters.getRawParameterValue ("scaleRoot");
    scaleMaskParam = parameters.getRawParameterValue ("scaleMask");
    enharmonicParam = parameters.getRawParameterValue ("enharmonicPref");
    midiParam = parameters.getRawParameterValue ("midiEnabled");
    forceCorrectionParam = parameters.getRawParameterValue ("forceCorrection");
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
    lastDetectionConfidence = engine.getLastDetectionConfidence();
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

    using ScaleSettings = PitchCorrectionEngine::Parameters::ScaleSettings;

    auto scaleModeIndex = juce::roundToInt (scaleModeParam->load());
    scaleModeIndex = juce::jlimit (0, (int) ScaleSettings::Type::Custom, scaleModeIndex);
    engineParameters.scale.type = static_cast<ScaleSettings::Type> (scaleModeIndex);

    auto rootIndex = juce::roundToInt (scaleRootParam->load());
    rootIndex = (rootIndex % 12 + 12) % 12;
    engineParameters.scale.root = rootIndex;

    auto maskValue = (AllowedMask) juce::roundToInt (scaleMaskParam->load());
    auto resolvedMask = ScaleSettings::maskForType (engineParameters.scale.type, engineParameters.scale.root, maskValue);
    if (resolvedMask == 0)
        resolvedMask = ScaleSettings::maskForType (ScaleSettings::Type::Chromatic, engineParameters.scale.root, 0x0FFFu);
    engineParameters.scale.mask = resolvedMask;

    auto enharmonicIndex = juce::roundToInt (enharmonicParam->load());
    enharmonicIndex = juce::jlimit (0, 2, enharmonicIndex);
    engineParameters.scale.enharmonicPreference = static_cast<ScaleSettings::EnharmonicPreference> (enharmonicIndex);

    engineParameters.midiEnabled = midiParam->load() > 0.5f;
    engineParameters.forceCorrection = forceCorrectionParam->load() > 0.5f;

    if (engineParameters.rangeLowHz > engineParameters.rangeHighHz)
        std::swap (engineParameters.rangeLowHz, engineParameters.rangeHighHz);

    engine.setParameters (engineParameters);
}

ProTuneAudioProcessor::ScaleSettings ProTuneAudioProcessor::getScaleSettings() const
{
    ScaleSettings settings;

    if (scaleModeParam != nullptr)
    {
        auto index = juce::roundToInt (scaleModeParam->load());
        index = juce::jlimit (0, (int) ScaleSettings::Type::Custom, index);
        settings.type = static_cast<ScaleSettings::Type> (index);
    }

    if (scaleRootParam != nullptr)
    {
        auto root = juce::roundToInt (scaleRootParam->load());
        settings.root = (root % 12 + 12) % 12;
    }

    if (scaleMaskParam != nullptr)
        settings.mask = (AllowedMask) juce::roundToInt (scaleMaskParam->load());

    settings.mask = ScaleSettings::maskForType (settings.type, settings.root, settings.mask);

    if (settings.mask == 0)
        settings.mask = ScaleSettings::maskForType (ScaleSettings::Type::Chromatic, settings.root, 0x0FFFu);

    if (enharmonicParam != nullptr)
    {
        auto pref = juce::roundToInt (enharmonicParam->load());
        pref = juce::jlimit (0, 2, pref);
        settings.enharmonicPreference = static_cast<ScaleSettings::EnharmonicPreference> (pref);
    }

    return settings;
}

ProTuneAudioProcessor::AllowedMask ProTuneAudioProcessor::getEffectiveScaleMask() const
{
    return getScaleSettings().mask;
}

ProTuneAudioProcessor::AllowedMask ProTuneAudioProcessor::getCustomScaleMask() const
{
    if (scaleMaskParam == nullptr)
        return 0x0FFFu;

    return (AllowedMask) juce::roundToInt (scaleMaskParam->load());
}

void ProTuneAudioProcessor::setScaleMaskFromUI (AllowedMask mask)
{
    mask &= 0x0FFFu;

    if (auto* param = dynamic_cast<juce::AudioParameterInt*> (parameters.getParameter ("scaleMask")))
    {
        auto normalised = param->convertTo0to1 ((int) mask);
        param->beginChangeGesture();
        param->setValueNotifyingHost (normalised);
        param->endChangeGesture();
    }
}

void ProTuneAudioProcessor::setScaleModeFromUI (ScaleSettings::Type type)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (parameters.getParameter ("scaleMode")))
    {
        auto index = (int) type;
        auto normalised = param->convertTo0to1 (index);
        param->beginChangeGesture();
        param->setValueNotifyingHost (normalised);
        param->endChangeGesture();
    }
}

ProTuneAudioProcessor::ScaleSettings::EnharmonicPreference ProTuneAudioProcessor::getEnharmonicPreference() const
{
    auto settings = getScaleSettings();
    return settings.enharmonicPreference;
}

bool ProTuneAudioProcessor::shouldUseFlatsForDisplay() const
{
    auto settings = getScaleSettings();

    if (settings.enharmonicPreference == ScaleSettings::EnharmonicPreference::Flats)
        return true;

    if (settings.enharmonicPreference == ScaleSettings::EnharmonicPreference::Sharps)
        return false;

    switch (settings.root)
    {
        case 5:  // F
        case 10: // Bb
        case 3:  // Eb
        case 8:  // Ab
        case 1:  // Db
        case 6:  // Gb
        case 11: // B/Cb
            return true;
        default:
            return false;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout ProTuneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("speed", "Speed",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f, 1.0f), 0.85f,
        juce::AudioParameterFloatAttributes().withLabel ("")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("transition", "Note Transition",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f, 1.0f), 0.2f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tolerance", "Tolerance (cents)",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f, 0.5f), 2.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("formant", "Formant Preserve",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f, 1.0f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("vibrato", "Vibrato Tracking",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f, 1.0f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("rangeLow", "Range Low (Hz)",
        juce::NormalisableRange<float> (40.0f, 500.0f, 0.01f, 0.5f), 80.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("rangeHigh", "Range High (Hz)",
        juce::NormalisableRange<float> (120.0f, 2000.0f, 0.01f, 0.5f), 800.0f));

    juce::StringArray scaleModes { "Chromatic", "Major", "Natural Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian", "Custom" };
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("scaleMode", "Scale", scaleModes, 0));

    juce::StringArray keyChoices { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("scaleRoot", "Key", keyChoices, 0));

    params.push_back (std::make_unique<juce::AudioParameterInt> ("scaleMask", "Scale Mask", 0, 0x0FFF, 0x0FFF));

    juce::StringArray enharmonicChoices { "Auto", "Sharps", "Flats" };
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("enharmonicPref", "Enharmonics", enharmonicChoices, 0));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("midiEnabled", "MIDI Control", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("forceCorrection", "Force Correction", true));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    DBG ("createPluginFilter invoked");
    return new ProTuneAudioProcessor();
}
