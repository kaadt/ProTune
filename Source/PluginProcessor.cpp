#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

ProTuneAudioProcessor::ProTuneAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "Parameters", createParameterLayout())
{
    // New Evo-style parameters
    inputTypeParam = parameters.getRawParameterValue ("inputType");
    retuneSpeedParam = parameters.getRawParameterValue ("retuneSpeed");
    trackingParam = parameters.getRawParameterValue ("tracking");
    humanizeParam = parameters.getRawParameterValue ("humanize");
    transposeParam = parameters.getRawParameterValue ("transpose");
    detuneParam = parameters.getRawParameterValue ("detune");
    bypassParam = parameters.getRawParameterValue ("bypass");

    // Core shared parameters
    keyParam = parameters.getRawParameterValue ("key");
    scaleModeParam = parameters.getRawParameterValue ("scaleMode");
    vibratoParam = parameters.getRawParameterValue ("vibrato");
    formantParam = parameters.getRawParameterValue ("formant");
    midiParam = parameters.getRawParameterValue ("midiEnabled");

    // Legacy parameters (for compatibility)
    speedParam = parameters.getRawParameterValue ("speed");
    transitionParam = parameters.getRawParameterValue ("transition");
    toleranceParam = parameters.getRawParameterValue ("tolerance");
    rangeLowParam = parameters.getRawParameterValue ("rangeLow");
    rangeHighParam = parameters.getRawParameterValue ("rangeHigh");
    scaleRootParam = parameters.getRawParameterValue ("scaleRoot");
    scaleMaskParam = parameters.getRawParameterValue ("scaleMask");
    enharmonicParam = parameters.getRawParameterValue ("enharmonicPref");
    forceCorrectionParam = parameters.getRawParameterValue ("forceCorrection");
}

void ProTuneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
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
    juce::ScopedNoDenormals noDenormals;

    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    updateEngineParameters();
    engine.pushMidi (midiMessages);
    engine.process (buffer);

    // Update telemetry for UI
    lastDetectedFrequency = engine.getLastDetectedFrequency();
    lastTargetFrequency = engine.getLastTargetFrequency();
    lastDetectionConfidence = engine.getLastDetectionConfidence();
    lastPitchRatio = engine.getLastPitchRatio();
}

juce::AudioProcessorEditor* ProTuneAudioProcessor::createEditor()
{
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
    if (tree.isValid())
    {
        parameters.replaceState (tree);
        updateEngineParameters();
    }
}

void ProTuneAudioProcessor::updateEngineParameters()
{
    // Input type (sets frequency range)
    if (inputTypeParam != nullptr)
    {
        int inputTypeIndex = juce::roundToInt (inputTypeParam->load());
        engineParameters.inputType = static_cast<PitchDetector::InputType> (
            juce::jlimit (0, 4, inputTypeIndex));
    }

    // Retune speed (prefer new param, fall back to legacy)
    if (retuneSpeedParam != nullptr)
        engineParameters.retuneSpeedMs = retuneSpeedParam->load();
    else if (speedParam != nullptr)
        engineParameters.retuneSpeedMs = speedParam->load();

    // Tracking
    if (trackingParam != nullptr)
        engineParameters.tracking = trackingParam->load() / 100.0f;

    // Humanize
    if (humanizeParam != nullptr)
        engineParameters.humanize = humanizeParam->load() / 100.0f;

    // Transpose
    if (transposeParam != nullptr)
        engineParameters.transpose = juce::roundToInt (transposeParam->load());

    // Detune
    if (detuneParam != nullptr)
        engineParameters.detune = detuneParam->load();

    // Bypass
    if (bypassParam != nullptr)
        engineParameters.bypass = bypassParam->load() > 0.5f;

    // Key/Root
    if (keyParam != nullptr)
        engineParameters.scale.root = juce::roundToInt (keyParam->load()) % 12;
    else if (scaleRootParam != nullptr)
        engineParameters.scale.root = juce::roundToInt (scaleRootParam->load()) % 12;

    // Scale mode
    if (scaleModeParam != nullptr)
    {
        int scaleModeIndex = juce::roundToInt (scaleModeParam->load());
        scaleModeIndex = juce::jlimit (0, (int) ScaleSettings::Type::Custom, scaleModeIndex);
        engineParameters.scale.type = static_cast<ScaleSettings::Type> (scaleModeIndex);
        engineParameters.scaleType = static_cast<ScaleMapper::ScaleType> (scaleModeIndex);
    }

    // Vibrato tracking
    if (vibratoParam != nullptr)
        engineParameters.vibratoTracking = vibratoParam->load();

    // Formant (PSOLA always preserves, but keep for UI display)
    if (formantParam != nullptr)
        engineParameters.formantPreserve = formantParam->load();

    // MIDI enable
    if (midiParam != nullptr)
        engineParameters.midiEnabled = midiParam->load() > 0.5f;

    // Note transition (legacy)
    if (transitionParam != nullptr)
        engineParameters.noteTransition = transitionParam->load();

    // Tolerance (legacy, deprecated)
    if (toleranceParam != nullptr)
        engineParameters.toleranceCents = toleranceParam->load();

    // Frequency range (legacy)
    if (rangeLowParam != nullptr)
        engineParameters.rangeLowHz = rangeLowParam->load();
    if (rangeHighParam != nullptr)
        engineParameters.rangeHighHz = rangeHighParam->load();

    // Custom scale mask
    if (scaleMaskParam != nullptr)
    {
        auto maskValue = (AllowedMask) juce::roundToInt (scaleMaskParam->load());
        engineParameters.customScaleMask = maskValue & 0x0FFFu;
        auto resolvedMask = ScaleSettings::maskForType (
            engineParameters.scale.type, engineParameters.scale.root, maskValue);
        engineParameters.scale.mask = resolvedMask != 0 ? resolvedMask : 0x0FFFu;
    }

    // Enharmonic preference
    if (enharmonicParam != nullptr)
    {
        int pref = juce::jlimit (0, 2, juce::roundToInt (enharmonicParam->load()));
        engineParameters.scale.enharmonicPreference =
            static_cast<ScaleSettings::EnharmonicPreference> (pref);
    }

    // Force correction (legacy)
    if (forceCorrectionParam != nullptr)
        engineParameters.forceCorrection = forceCorrectionParam->load() > 0.5f;

    // Ensure range is valid
    if (engineParameters.rangeLowHz > engineParameters.rangeHighHz)
        std::swap (engineParameters.rangeLowHz, engineParameters.rangeHighHz);

    engine.setParameters (engineParameters);
}

ProTuneAudioProcessor::ScaleSettings ProTuneAudioProcessor::getScaleSettings() const
{
    ScaleSettings settings;

    if (scaleModeParam != nullptr)
    {
        int index = juce::jlimit (0, (int) ScaleSettings::Type::Custom,
                                   juce::roundToInt (scaleModeParam->load()));
        settings.type = static_cast<ScaleSettings::Type> (index);
    }

    if (keyParam != nullptr)
        settings.root = juce::roundToInt (keyParam->load()) % 12;
    else if (scaleRootParam != nullptr)
        settings.root = juce::roundToInt (scaleRootParam->load()) % 12;

    if (scaleMaskParam != nullptr)
        settings.mask = (AllowedMask) juce::roundToInt (scaleMaskParam->load());

    settings.mask = ScaleSettings::maskForType (settings.type, settings.root, settings.mask);
    if (settings.mask == 0)
        settings.mask = 0x0FFFu;

    if (enharmonicParam != nullptr)
    {
        int pref = juce::jlimit (0, 2, juce::roundToInt (enharmonicParam->load()));
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
        int index = (int) type;
        auto normalised = param->convertTo0to1 (index);
        param->beginChangeGesture();
        param->setValueNotifyingHost (normalised);
        param->endChangeGesture();
    }
}

ProTuneAudioProcessor::ScaleSettings::EnharmonicPreference ProTuneAudioProcessor::getEnharmonicPreference() const
{
    return getScaleSettings().enharmonicPreference;
}

bool ProTuneAudioProcessor::shouldUseFlatsForDisplay() const
{
    auto settings = getScaleSettings();

    if (settings.enharmonicPreference == ScaleSettings::EnharmonicPreference::Flats)
        return true;
    if (settings.enharmonicPreference == ScaleSettings::EnharmonicPreference::Sharps)
        return false;

    // Auto: use flats for flat keys
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

    // === NEW AUTO-TUNE EVO STYLE PARAMETERS ===

    // Input Type
    juce::StringArray inputTypes { "Soprano", "Alto/Tenor", "Low Male", "Instrument", "Bass Inst." };
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "inputType", "Input Type", inputTypes, 1));  // Default: Alto/Tenor

    // Retune Speed (0-400ms, default 20ms like Auto-Tune)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "retuneSpeed", "Retune Speed",
        juce::NormalisableRange<float> (0.0f, 400.0f, 1.0f), 20.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    // Tracking (0-100%, higher = more relaxed detection)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "tracking", "Tracking",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Humanize (0-100%)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "humanize", "Humanize",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Transpose (-24 to +24 semitones)
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        "transpose", "Transpose", -24, 24, 0));

    // Detune (-100 to +100 cents)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "detune", "Detune",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("cents")));

    // Bypass
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "bypass", "Bypass", false));

    // Key (replaces scaleRoot for UI)
    juce::StringArray keyChoices { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "key", "Key", keyChoices, 0));

    // Scale Mode (16 scales + custom)
    juce::StringArray scaleModes {
        "Chromatic", "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor",
        "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian",
        "Whole Tone", "Blues", "Major Pentatonic", "Minor Pentatonic",
        "Diminished", "Custom"
    };
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "scaleMode", "Scale", scaleModes, 0));

    // Vibrato preservation (0-1)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "vibrato", "Vibrato",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    // Formant (kept for UI but PSOLA always preserves)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "formant", "Formant",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));  // Default ON!

    // MIDI Control
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "midiEnabled", "MIDI Control", false));

    // === LEGACY PARAMETERS (for preset compatibility) ===

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "speed", "Speed",
        juce::NormalisableRange<float> (0.0f, 400.0f, 0.1f), 20.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "transition", "Transition",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.2f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "tolerance", "Tolerance",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "rangeLow", "Range Low",
        juce::NormalisableRange<float> (40.0f, 500.0f, 1.0f), 80.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "rangeHigh", "Range High",
        juce::NormalisableRange<float> (120.0f, 2000.0f, 1.0f), 800.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "scaleRoot", "Scale Root", keyChoices, 0));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        "scaleMask", "Scale Mask", 0, 0x0FFF, 0x0FFF));

    juce::StringArray enharmonicChoices { "Auto", "Sharps", "Flats" };
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "enharmonicPref", "Enharmonics", enharmonicChoices, 0));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "forceCorrection", "Force Correction", true));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ProTuneAudioProcessor();
}
