#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <limits>
#include <memory>
#include <vector>
#include <cstdint>

#include "PitchDetector.h"
#include "ScaleMapper.h"
#include "RetuneEngine.h"
#include "PsolaShifter.h"

/**
 * Main Pitch Correction Engine
 *
 * Orchestrates pitch detection, scale mapping, retune smoothing, and pitch shifting.
 * Redesigned to match Auto-Tune Evo's functionality with formant-preserving PSOLA.
 *
 * Processing flow:
 * 1. PitchDetector: Cycle-based pitch detection (patent US 5,973,252)
 * 2. ScaleMapper: Map to target note based on key/scale/MIDI
 * 3. RetuneEngine: Apply retune speed and humanization
 * 4. PsolaShifter: Pitch shift with natural formant preservation
 */
class PitchCorrectionEngine
{
public:
    using AllowedMask = std::uint16_t;

    struct Parameters
    {
        // Input type for frequency range optimization
        PitchDetector::InputType inputType = PitchDetector::InputType::AltoTenor;

        // Scale settings
        ScaleMapper::ScaleType scaleType = ScaleMapper::ScaleType::Chromatic;
        int scaleRoot = 0;                          // 0-11 (C=0)
        AllowedMask customScaleMask = 0x0FFF;       // For custom scale
        int transpose = 0;                          // -24 to +24 semitones
        float detune = 0.0f;                        // -100 to +100 cents

        // Retune settings
        float retuneSpeedMs = 20.0f;                // 0 = instant, 400 = slow
        float tracking = 0.5f;                      // Pitch detection sensitivity
        float humanize = 0.0f;                      // Natural variation
        float vibratoTracking = 0.5f;               // Vibrato preservation
        float noteTransition = 0.2f;                // Note transition smoothness

        // Legacy compatibility (kept for existing presets)
        float speed = 20.0f;                        // Alias for retuneSpeedMs
        float transition = 0.2f;                    // Alias for noteTransition
        float toleranceCents = 0.0f;                // Deprecated
        float formantPreserve = 1.0f;               // PSOLA always preserves, but kept for UI
        float rangeLowHz = 80.0f;
        float rangeHighHz = 1000.0f;

        // Global
        bool bypass = false;
        bool midiEnabled = false;                   // Use MIDI notes as target
        bool forceCorrection = true;

        // Scale settings struct for legacy compatibility
        struct ScaleSettings
        {
            enum class Type
            {
                Chromatic = 0,
                Major,
                NaturalMinor,
                HarmonicMinor,
                MelodicMinor,
                Dorian,
                Phrygian,
                Lydian,
                Mixolydian,
                Locrian,
                WholeTone,
                Blues,
                MajorPentatonic,
                MinorPentatonic,
                Diminished,
                Custom
            };

            enum class EnharmonicPreference
            {
                Auto = 0,
                Sharps,
                Flats
            };

            Type type = Type::Chromatic;
            int root = 0;
            AllowedMask mask = 0x0FFFu;
            EnharmonicPreference enharmonicPreference = EnharmonicPreference::Auto;

            static AllowedMask patternToMask (int root, std::initializer_list<int> pattern) noexcept;
            static AllowedMask maskForType (Type type, int root, AllowedMask customMask) noexcept;
        };

        ScaleSettings scale;
    };

    PitchCorrectionEngine();
    ~PitchCorrectionEngine() = default;

    void prepare (double sampleRate, int samplesPerBlock);
    void reset();

    void setParameters (const Parameters& newParams);
    void pushMidi (const juce::MidiBuffer& midiMessages);

    void process (juce::AudioBuffer<float>& buffer);

    [[nodiscard]] float getLastDetectedFrequency() const noexcept { return lastDetectedFrequency; }
    [[nodiscard]] float getLastTargetFrequency() const noexcept { return lastTargetFrequency; }
    [[nodiscard]] float getLastDetectionConfidence() const noexcept { return lastDetectionConfidence; }
    [[nodiscard]] float getLastPitchRatio() const noexcept { return lastPitchRatio; }

    [[nodiscard]] int getLatencySamples() const noexcept;

private:
    void updateComponentSettings();

    // New modular components
    PitchDetector detector;
    ScaleMapper scaleMapper;
    RetuneEngine retuneEngine;
    std::vector<PsolaShifter> shifters;  // One per channel

    // Parameters
    Parameters params;
    double currentSampleRate = 44100.0;
    int maxBlockSize = 0;

    // MIDI state
    int heldMidiNote = -1;

    // Telemetry
    float lastDetectedFrequency = 0.0f;
    float lastTargetFrequency = 0.0f;
    float lastDetectionConfidence = 0.0f;
    float lastPitchRatio = 1.0f;

    // Analysis buffer for mono mixdown
    juce::AudioBuffer<float> monoBuffer;

    // Ensure enough shifters for channels
    void ensureShifterChannels (int numChannels);
};
