#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

/**
 * Retune Engine
 *
 * Handles pitch correction smoothing and humanization effects.
 * Inspired by Auto-Tune Evo's retune speed and humanize controls.
 *
 * Features:
 * - Retune Speed: Controls how fast pitch snaps to target (0-400ms)
 * - Vibrato Tracking: Preserve or flatten natural vibrato
 * - Humanize: Add subtle timing and pitch variations
 * - Note Transition: Smooth vs instant note changes
 */
class RetuneEngine
{
public:
    struct Settings
    {
        float retuneSpeedMs = 20.0f;    // 0 = instant, 400 = very slow
        float vibratoTracking = 0.5f;    // 0 = flatten, 1 = preserve
        float humanize = 0.0f;           // 0 = robotic, 1 = natural
        float noteTransition = 0.2f;     // 0 = instant, 1 = smooth portamento
    };

    RetuneEngine();
    ~RetuneEngine() = default;

    void prepare (double sampleRate);
    void reset();

    /**
     * Process pitch correction for one block.
     *
     * @param detectedFrequency Current detected pitch
     * @param targetFrequency Target pitch from scale mapper
     * @param numSamples Number of samples in block
     * @return Smoothed pitch ratio (output/input frequency ratio)
     */
    float process (float detectedFrequency, float targetFrequency, int numSamples);

    /**
     * Get the current smoothed pitch ratio for sample-by-sample processing.
     */
    float getNextRatio();

    void setSettings (const Settings& newSettings);
    const Settings& getSettings() const noexcept { return settings; }

private:
    Settings settings;
    double currentSampleRate = 44100.0;

    // Smoothing
    juce::SmoothedValue<float> ratioSmoother;
    juce::SmoothedValue<float> targetSmoother;

    // State tracking
    float lastDetectedFrequency = 0.0f;
    float lastTargetFrequency = 0.0f;
    float lastRatio = 1.0f;
    int lastTargetNote = -1;           // For note transition detection

    // Vibrato detection
    float vibratoPhase = 0.0f;
    float detectedVibratoRate = 0.0f;
    float detectedVibratoDepth = 0.0f;

    // Humanize
    float humanizePhase = 0.0f;
    juce::Random random;

    // Helpers
    float applyVibratoTracking (float detectedFreq, float targetFreq);
    float applyHumanize (float ratio);
    float detectNoteTransition (float targetFreq);
    float frequencyToMidi (float freq);
};
