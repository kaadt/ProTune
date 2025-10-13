#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <limits>
#include <memory>
#include <vector>
#include <cstdint>
#include <initializer_list>

class PitchCorrectionEngine
{
public:
    using AllowedMask = std::uint16_t;

    struct Parameters
    {
        float speed = 0.85f;
        float transition = 0.2f;
        float toleranceCents = 2.0f;
        float formantPreserve = 0.0f;
        float vibratoTracking = 0.5f;
        float rangeLowHz = 80.0f;
        float rangeHighHz = 1000.0f;
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
        bool midiEnabled = false;
        bool forceCorrection = true;
    };

    void prepare (double sampleRate, int samplesPerBlock);
    void reset();

    void setParameters (const Parameters& newParams);
    void pushMidi (const juce::MidiBuffer& midiMessages);

    void process (juce::AudioBuffer<float>& buffer);

    void setAnalysisWindowOrder (int newOrder);

    [[nodiscard]] float getLastDetectedFrequency() const noexcept { return lastDetectedFrequency; }
    [[nodiscard]] float getLastTargetFrequency() const noexcept { return lastTargetFrequency; }
    [[nodiscard]] float getLastDetectionConfidence() const noexcept { return lastDetectionConfidence; }

private:
    void analyseBlock (const float* samples, int numSamples);
    float estimatePitchFromAutocorrelation (const float* frame, int frameSize, float& confidenceOut);
    float chooseTargetFrequency (float detectedFrequency);
    void updateAnalysisResources();
    void updateDownsamplingResources();
    [[nodiscard]] float computeDynamicRatioTime (float detectedFrequency, float targetFrequency) const;
    [[nodiscard]] float computeDynamicTransitionTime (float detectedFrequency, float targetFrequency) const;
    [[nodiscard]] float applyTargetHysteresis (float candidateMidi, float rawMidi);
    [[nodiscard]] float clampMidiToRange (float midi) const noexcept;
    [[nodiscard]] float minimumConfidenceForLock() const noexcept;
    [[nodiscard]] int getAnalysisFftSize() const noexcept { return 1 << analysisFftOrder; }

    static float frequencyToMidiNote (float freq);
    static float midiNoteToFrequency (float midiNote);
    static float snapNoteToMask (float midiNote, AllowedMask mask);

    static constexpr int minAnalysisFftOrder = 9;
    static constexpr int maxAnalysisFftOrder = 12;
    static constexpr int defaultAnalysisFftOrder = 11;
    static constexpr int pitchDetectionDownsample = 8;

    double currentSampleRate = 44100.0;
    int maxBlockSize = 0;

    int analysisFftOrder = defaultAnalysisFftOrder;

    Parameters params;
    float baseRatioGlideTime = 0.01f;
    float baseTargetTransitionTime = 0.01f;

    juce::AudioBuffer<float> analysisBuffer;
    juce::AudioBuffer<float> windowedBuffer;
    juce::AudioBuffer<float> analysisFrame;
    std::vector<float> downsampledFrame;
    std::vector<float> filteredFrame;
    std::vector<float> decimationFilter;
    struct PeriodicitySample
    {
        double error = 0.0;
        double energy = 0.0;
        double correlation = 0.0;
    };
    std::vector<PeriodicitySample> periodicityScratch;
    int analysisWritePosition = 0;

    float lastDetectedFrequency = 0.0f;
    float lastTargetFrequency = 0.0f;
    float lastDetectionConfidence = 0.0f;

    float lastLoggedDetected = 0.0f;
    float lastLoggedTarget = 0.0f;

    juce::SmoothedValue<float> ratioSmoother;
    juce::SmoothedValue<float> pitchSmoother;

    juce::AudioBuffer<float> dryBuffer;

    float heldMidiNote = std::numeric_limits<float>::quiet_NaN();
    float activeTargetMidi = std::numeric_limits<float>::quiet_NaN();

    juce::LinearSmoothedValue<float> detectionSmoother { 0.0f };

    struct CycleResampler
    {
        void prepare (double sampleRateIn, int maxPeriodSamplesIn, int maxBlockSizeIn);
        void reset();
        void process (const float* input, float* output, int numSamples,
                      const float* ratioValues, const float* periodValues,
                      const float* desiredPeriodValues);

    private:
        double sampleRate = 44100.0;
        int maxPeriodSamples = 0;
        int maxBlockSize = 0;
        std::vector<float> buffer;
        int bufferSize = 0;
        double writeHead = 0.0;
        double readHead = 0.0;
    };

    void ensureCycleResamplers (int requiredChannels);
    void updateResamplerResources();

    std::vector<CycleResampler> cycleResamplers;
    int allocatedMaxPeriodSamples = 0;
};
