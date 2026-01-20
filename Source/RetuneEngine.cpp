#include "RetuneEngine.h"
#include <cmath>

RetuneEngine::RetuneEngine()
{
}

void RetuneEngine::prepare (double sampleRate)
{
    currentSampleRate = sampleRate;

    // Initialize smoothers
    float retuneTimeSeconds = settings.retuneSpeedMs / 1000.0f;
    ratioSmoother.reset (sampleRate, juce::jmax (0.001, static_cast<double> (retuneTimeSeconds)));
    ratioSmoother.setCurrentAndTargetValue (1.0f);

    targetSmoother.reset (sampleRate, 0.02);  // 20ms for target frequency changes
    targetSmoother.setCurrentAndTargetValue (0.0f);

    reset();
}

void RetuneEngine::reset()
{
    ratioSmoother.setCurrentAndTargetValue (1.0f);
    targetSmoother.setCurrentAndTargetValue (0.0f);
    lastDetectedFrequency = 0.0f;
    lastTargetFrequency = 0.0f;
    lastRatio = 1.0f;
    lastTargetNote = -1;
    vibratoPhase = 0.0f;
    detectedVibratoRate = 0.0f;
    detectedVibratoDepth = 0.0f;
    humanizePhase = 0.0f;
}

void RetuneEngine::setSettings (const Settings& newSettings)
{
    settings = newSettings;

    // Update smoother time based on retune speed
    float retuneTimeSeconds = juce::jmax (0.001f, settings.retuneSpeedMs / 1000.0f);
    ratioSmoother.reset (currentSampleRate, static_cast<double> (retuneTimeSeconds));
}

float RetuneEngine::process (float detectedFrequency, float targetFrequency, int numSamples)
{
    if (detectedFrequency <= 0.0f || targetFrequency <= 0.0f)
    {
        // No valid pitch - maintain last ratio or reset
        if (numSamples > 0)
            ratioSmoother.skip (numSamples);
        return lastRatio;
    }

    // Detect note transitions
    float transitionFactor = detectNoteTransition (targetFrequency);

    // Apply vibrato tracking
    float adjustedTarget = applyVibratoTracking (detectedFrequency, targetFrequency);

    // Calculate base ratio
    float ratio = adjustedTarget / detectedFrequency;

    // Clamp ratio to reasonable range
    ratio = juce::jlimit (0.5f, 2.0f, ratio);

    // Apply humanize
    if (settings.humanize > 0.0f)
        ratio = applyHumanize (ratio);

    // Update smoother based on note transition
    if (transitionFactor > 0.5f)
    {
        // Smooth transition for note changes
        float transitionTime = juce::jmap (settings.noteTransition, 0.0f, 1.0f, 0.005f, 0.15f);
        ratioSmoother.reset (currentSampleRate, static_cast<double> (transitionTime));
    }
    else
    {
        // Normal retune speed for within-note correction
        float retuneTimeSeconds = juce::jmax (0.001f, settings.retuneSpeedMs / 1000.0f);
        ratioSmoother.reset (currentSampleRate, static_cast<double> (retuneTimeSeconds));
    }

    ratioSmoother.setTargetValue (ratio);

    // Skip samples and get final value
    ratioSmoother.skip (numSamples);
    lastRatio = ratioSmoother.getCurrentValue();

    // Update state
    lastDetectedFrequency = detectedFrequency;
    lastTargetFrequency = targetFrequency;

    return lastRatio;
}

float RetuneEngine::getNextRatio()
{
    return ratioSmoother.getNextValue();
}

float RetuneEngine::applyVibratoTracking (float detectedFreq, float targetFreq)
{
    // Vibrato tracking: blend between detected (with vibrato) and target (no vibrato)
    // vibratoTracking = 0: flatten all vibrato (use target)
    // vibratoTracking = 1: preserve vibrato (follow detected more closely)

    if (settings.vibratoTracking <= 0.01f)
        return targetFreq;

    if (settings.vibratoTracking >= 0.99f)
    {
        // Full preservation: only correct to nearest semitone
        float detectedMidi = frequencyToMidi (detectedFreq);
        float deviation = detectedMidi - std::round (detectedMidi);
        return targetFreq * std::pow (2.0f, deviation / 12.0f);
    }

    // Partial preservation: allow some vibrato through
    float detectedMidi = frequencyToMidi (detectedFreq);
    float deviation = detectedMidi - std::round (detectedMidi);

    // Scale deviation by tracking amount
    float scaledDeviation = deviation * settings.vibratoTracking;

    return targetFreq * std::pow (2.0f, scaledDeviation / 12.0f);
}

float RetuneEngine::applyHumanize (float ratio)
{
    if (settings.humanize <= 0.01f)
        return ratio;

    // Add subtle random micro-timing and pitch variations
    // Humanize adds natural imperfection to the robotic correction

    // Slow LFO for drift (0.5-2 Hz)
    humanizePhase += 1.5f / static_cast<float> (currentSampleRate) * 256.0f;
    if (humanizePhase > juce::MathConstants<float>::twoPi)
        humanizePhase -= juce::MathConstants<float>::twoPi;

    float lfo = std::sin (humanizePhase);

    // Random component (filtered noise)
    float noise = (random.nextFloat() - 0.5f) * 0.002f;

    // Combined modulation
    float modulation = (lfo * 0.005f + noise) * settings.humanize;

    // Apply as cents deviation
    float modRatio = std::pow (2.0f, modulation / 12.0f);

    return ratio * modRatio;
}

float RetuneEngine::detectNoteTransition (float targetFreq)
{
    if (targetFreq <= 0.0f)
        return 0.0f;

    float targetMidi = frequencyToMidi (targetFreq);
    int targetNote = static_cast<int> (std::round (targetMidi));

    if (lastTargetNote < 0)
    {
        lastTargetNote = targetNote;
        return 0.0f;
    }

    int noteDelta = std::abs (targetNote - lastTargetNote);
    lastTargetNote = targetNote;

    if (noteDelta >= 1)
        return 1.0f;  // Note change detected

    return 0.0f;
}

float RetuneEngine::frequencyToMidi (float freq)
{
    if (freq <= 0.0f)
        return 0.0f;

    return 69.0f + 12.0f * std::log2 (freq / 440.0f);
}
