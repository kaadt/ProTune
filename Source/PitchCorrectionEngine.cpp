#include "PitchCorrectionEngine.h"
#include <algorithm>
#include <cmath>

namespace
{
inline int positiveModulo (int value, int modulo) noexcept
{
    auto remainder = value % modulo;
    return remainder < 0 ? remainder + modulo : remainder;
}
}

// Legacy scale mask generation (kept for compatibility)
PitchCorrectionEngine::AllowedMask PitchCorrectionEngine::Parameters::ScaleSettings::patternToMask (
    int root, std::initializer_list<int> pattern) noexcept
{
    AllowedMask mask = 0;
    for (auto interval : pattern)
    {
        auto pitchClass = positiveModulo (root + interval, 12);
        mask |= (AllowedMask) (1u << pitchClass);
    }
    return mask;
}

PitchCorrectionEngine::AllowedMask PitchCorrectionEngine::Parameters::ScaleSettings::maskForType (
    Type type, int root, AllowedMask customMask) noexcept
{
    switch (type)
    {
        case Type::Chromatic:       return 0x0FFFu;
        case Type::Major:           return patternToMask (root, { 0, 2, 4, 5, 7, 9, 11 });
        case Type::NaturalMinor:    return patternToMask (root, { 0, 2, 3, 5, 7, 8, 10 });
        case Type::HarmonicMinor:   return patternToMask (root, { 0, 2, 3, 5, 7, 8, 11 });
        case Type::MelodicMinor:    return patternToMask (root, { 0, 2, 3, 5, 7, 9, 11 });
        case Type::Dorian:          return patternToMask (root, { 0, 2, 3, 5, 7, 9, 10 });
        case Type::Phrygian:        return patternToMask (root, { 0, 1, 3, 5, 7, 8, 10 });
        case Type::Lydian:          return patternToMask (root, { 0, 2, 4, 6, 7, 9, 11 });
        case Type::Mixolydian:      return patternToMask (root, { 0, 2, 4, 5, 7, 9, 10 });
        case Type::Locrian:         return patternToMask (root, { 0, 1, 3, 5, 6, 8, 10 });
        case Type::WholeTone:       return patternToMask (root, { 0, 2, 4, 6, 8, 10 });
        case Type::Blues:           return patternToMask (root, { 0, 3, 5, 6, 7, 10 });
        case Type::MajorPentatonic: return patternToMask (root, { 0, 2, 4, 7, 9 });
        case Type::MinorPentatonic: return patternToMask (root, { 0, 3, 5, 7, 10 });
        case Type::Diminished:      return patternToMask (root, { 0, 2, 3, 5, 6, 8, 9, 11 });
        case Type::Custom:
        default:                    return customMask & 0x0FFFu;
    }
}

PitchCorrectionEngine::PitchCorrectionEngine()
{
}

void PitchCorrectionEngine::prepare (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxBlockSize = samplesPerBlock;

    // Prepare all components
    detector.prepare (sampleRate, samplesPerBlock);
    retuneEngine.prepare (sampleRate);

    // Mono buffer for pitch detection (mix down stereo)
    monoBuffer.setSize (1, samplesPerBlock);

    // Prepare at least 2 shifters (stereo)
    ensureShifterChannels (2);

    updateComponentSettings();
}

void PitchCorrectionEngine::reset()
{
    detector.reset();
    retuneEngine.reset();

    for (auto& shifter : shifters)
        shifter.reset();

    lastDetectedFrequency = 0.0f;
    lastTargetFrequency = 0.0f;
    lastDetectionConfidence = 0.0f;
    lastPitchRatio = 1.0f;
    heldMidiNote = -1;
}

void PitchCorrectionEngine::setParameters (const Parameters& newParams)
{
    params = newParams;
    updateComponentSettings();
}

void PitchCorrectionEngine::updateComponentSettings()
{
    // Update pitch detector
    detector.setInputType (params.inputType);
    detector.setFrequencyRange (params.rangeLowHz, params.rangeHighHz);
    detector.setTracking (params.tracking);

    // Update scale mapper
    ScaleMapper::Settings scaleSettings;

    // Convert legacy scale type to new enum
    scaleSettings.type = static_cast<ScaleMapper::ScaleType> (static_cast<int> (params.scale.type));
    scaleSettings.root = params.scale.root;
    scaleSettings.customMask = params.customScaleMask;
    scaleSettings.transpose = params.transpose;
    scaleSettings.detune = params.detune;
    scaleMapper.setSettings (scaleSettings);

    // Update retune engine
    RetuneEngine::Settings retuneSettings;
    const Parameters defaultParams;
    float retuneSpeedMs = params.retuneSpeedMs;
    if (std::abs (retuneSpeedMs - defaultParams.retuneSpeedMs) < 1.0e-3f)
        retuneSpeedMs = params.speed;
    retuneSettings.retuneSpeedMs = retuneSpeedMs;
    retuneSettings.vibratoTracking = params.vibratoTracking;
    retuneSettings.humanize = params.humanize;
    float noteTransition = params.noteTransition;
    if (std::abs (noteTransition - defaultParams.noteTransition) < 1.0e-3f)
        noteTransition = params.transition;
    retuneSettings.noteTransition = noteTransition;
    retuneEngine.setSettings (retuneSettings);
}

void PitchCorrectionEngine::pushMidi (const juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        const auto& m = metadata.getMessage();
        if (m.isNoteOn())
            heldMidiNote = m.getNoteNumber();
        else if (m.isNoteOff())
        {
            if (heldMidiNote == m.getNoteNumber())
                heldMidiNote = -1;
        }
    }
}

void PitchCorrectionEngine::process (juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    // Bypass mode - just return input unchanged
    if (params.bypass)
        return;

    // Mix down to mono for pitch detection
    monoBuffer.setSize (1, numSamples, false, false, true);
    monoBuffer.clear();

    for (int ch = 0; ch < numChannels; ++ch)
        monoBuffer.addFrom (0, 0, buffer, ch, 0, numSamples, 1.0f / numChannels);

    // Detect pitch
    auto detectionResult = detector.process (monoBuffer.getReadPointer (0), numSamples);

    lastDetectedFrequency = detectionResult.frequency;
    lastDetectionConfidence = detectionResult.confidence;

    // Map to target note
    float targetFrequency = 0.0f;

    if (detectionResult.voiced || heldMidiNote >= 0)
    {
        // Use MIDI override if enabled and note held
        int midiOverride = (params.midiEnabled && heldMidiNote >= 0) ? heldMidiNote : -1;

        float inputFreq = (heldMidiNote >= 0 && params.midiEnabled)
            ? ScaleMapper::midiToFrequency (static_cast<float> (heldMidiNote))
            : detectionResult.frequency;

        if (inputFreq > 0.0f)
        {
            auto mapResult = scaleMapper.map (inputFreq, midiOverride);
            targetFrequency = mapResult.targetFrequency;
        }
    }

    lastTargetFrequency = targetFrequency;

    // Calculate pitch ratio with retune smoothing
    float pitchRatio = 1.0f;

    if (detectionResult.voiced && targetFrequency > 0.0f && detectionResult.frequency > 0.0f)
    {
        pitchRatio = retuneEngine.process (detectionResult.frequency, targetFrequency, numSamples);
    }

    lastPitchRatio = pitchRatio;

    // Ensure we have enough shifters
    ensureShifterChannels (numChannels);

    // Apply pitch shifting to each channel
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer (ch);
        shifters[static_cast<size_t> (ch)].process (
            channelData,
            channelData,
            numSamples,
            pitchRatio,
            detectionResult.period,
            detectionResult.confidence
        );
    }
}

void PitchCorrectionEngine::ensureShifterChannels (int numChannels)
{
    if (static_cast<int> (shifters.size()) < numChannels)
    {
        size_t oldSize = shifters.size();
        shifters.resize (static_cast<size_t> (numChannels));

        for (size_t i = oldSize; i < shifters.size(); ++i)
            shifters[i].prepare (currentSampleRate, maxBlockSize);
    }
}

int PitchCorrectionEngine::getLatencySamples() const noexcept
{
    if (shifters.empty())
        return 0;

    return shifters[0].getLatencySamples();
}
