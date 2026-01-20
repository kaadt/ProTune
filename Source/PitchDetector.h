#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>

/**
 * Cycle-Based Pitch Detector
 *
 * Implementation based on US Patent 5,973,252 by Harold A. Hildebrand.
 * Uses autocorrelation-derived decision statistics for fast, accurate pitch detection.
 *
 * Algorithm:
 * 1. Coarse search: Downsample by 8, evaluate V(L) = E(L) - 2H(L) for L=2..110
 * 2. Fine search: Track N=8 candidates at full rate around coarse estimate
 * 3. Quadratic interpolation for sub-sample precision
 * 4. Voicing decision based on periodicity threshold
 *
 * Key equations from patent:
 *   E(L) = sum of squared samples over 2 periods
 *   H(L) = cross-correlation between adjacent cycles
 *   V(L) = E(L) - 2*H(L) (decision statistic)
 *   Valid period: V(L) <= epsilon * E(L)
 */
class PitchDetector
{
public:
    struct Result
    {
        float frequency = 0.0f;     // Detected frequency in Hz (0 if unvoiced)
        float period = 0.0f;        // Period in samples
        float confidence = 0.0f;    // 0-1 confidence measure
        bool voiced = false;        // True if pitch detected
    };

    // Input type presets for frequency range optimization
    enum class InputType
    {
        Soprano,        // 200-1200 Hz
        AltoTenor,      // 100-600 Hz
        LowMale,        // 60-300 Hz
        Instrument,     // 80-2000 Hz
        BassInstrument  // 30-250 Hz
    };

    PitchDetector();
    ~PitchDetector() = default;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    /**
     * Process a block of audio and detect pitch.
     *
     * @param input Input audio samples (mono)
     * @param numSamples Number of samples in block
     * @return Pitch detection result
     */
    Result process (const float* input, int numSamples);

    // Configuration
    void setInputType (InputType type);
    void setFrequencyRange (float minHz, float maxHz);
    void setTracking (float tracking);  // 0-1: 0 = strict, 1 = relaxed

    InputType getInputType() const noexcept { return inputType; }
    float getMinFrequency() const noexcept { return minFreqHz; }
    float getMaxFrequency() const noexcept { return maxFreqHz; }

private:
    // Decision statistic computation
    struct PeriodScore
    {
        double E = 0.0;     // Energy
        double H = 0.0;     // Cross-correlation
        double V = 0.0;     // V = E - 2H
    };

    PeriodScore evaluatePeriod (const float* data, int dataSize, int lag);
    float refineWithQuadratic (int bestLag, const std::vector<PeriodScore>& scores);

    // Coarse search with downsampling
    int coarseSearch (const float* downsampledData, int downsampledSize);

    // Fine search around coarse estimate
    float fineSearch (const float* data, int dataSize, int coarseLag);

    // Downsampling
    void downsample (const float* input, int inputSize);

    // Input buffer (circular)
    std::vector<float> inputBuffer;
    int inputWritePos = 0;

    // Downsampled buffer for coarse search
    std::vector<float> downsampledBuffer;
    static constexpr int downsampleFactor = 8;

    // Decimation filter coefficients
    std::vector<float> decimationFilter;
    static constexpr int filterTaps = 33;

    // Tracking state
    float lastPeriod = 0.0f;
    float lastConfidence = 0.0f;
    int stableFrameCount = 0;

    // Configuration
    double sampleRate = 44100.0;
    InputType inputType = InputType::AltoTenor;
    float minFreqHz = 80.0f;
    float maxFreqHz = 800.0f;
    float epsilon = 0.15f;  // Tracking parameter (lower = stricter)

    // Analysis window
    int analysisWindowSize = 0;
    std::vector<float> analysisWindow;

    // Scratch buffers
    std::vector<PeriodScore> coarseScores;
    std::vector<PeriodScore> fineScores;
};
