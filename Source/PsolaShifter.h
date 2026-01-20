#pragma once

#include <juce_core/juce_core.h>
#include <deque>
#include <vector>

/**
 * PSOLA (Pitch Synchronous Overlap Add) Pitch Shifter
 *
 * This implementation uses time-domain pitch shifting that naturally preserves
 * formants without requiring spectral envelope manipulation. Unlike phase vocoders,
 * PSOLA works directly with waveform cycles, making it ideal for voice processing.
 *
 * Algorithm:
 * 1. Detect pitch marks at cycle boundaries using period information
 * 2. Extract pitch-synchronous grains (2 periods each, Hanning windowed)
 * 3. Shift pitch by adjusting grain spacing during overlap-add
 * 4. For pitch up: grains overlap more (duplicating some audio)
 * 5. For pitch down: grains overlap less (stretching audio)
 *
 * Key advantage: Formants are preserved because we're not modifying the
 * spectral content of each grain, just repositioning them in time.
 */
class PsolaShifter
{
public:
    PsolaShifter();
    ~PsolaShifter() = default;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    /**
     * Process audio with pitch shifting.
     *
     * @param input Input audio samples
     * @param output Output buffer for processed audio
     * @param numSamples Number of samples to process
     * @param pitchRatio Pitch shift ratio (e.g., 2.0 = octave up, 0.5 = octave down)
     * @param detectedPeriod Current detected pitch period in samples (0 if unvoiced)
     * @param confidence Detection confidence (0-1)
     */
    void process (const float* input, float* output, int numSamples,
                  float pitchRatio, float detectedPeriod, float confidence);

    int getLatencySamples() const noexcept { return latencySamples; }

private:
    // Grain extraction and synthesis
    struct Grain
    {
        std::vector<float> samples;
        std::vector<float> window;    // Window values for overlap normalization
        int centerPosition = 0;     // Position in input stream where grain was extracted
        int outputPosition = 0;     // Target position in output stream
        float period = 0.0f;        // Pitch period at extraction time
    };

    void extractGrain (int centerPos, float period);
    void synthesizeGrains (float* output, int numSamples);
    float getHannWindow (int index, int size) const;
    int alignToPeak (int center, int searchRadius, int minCenter, int maxCenter) const;

    // Input buffer for grain extraction (circular)
    std::vector<float> inputBuffer;
    int inputWritePos = 0;
    int inputReadPos = 0;

    // Output accumulator for overlap-add
    std::vector<float> outputAccum;
    int outputWritePos = 0;
    int outputReadPos = 0;

    // Active grains being synthesized
    std::deque<Grain> activeGrains;

    // Tracking
    double currentSampleRate = 44100.0;
    int latencySamples = 0;
    int maxPeriodSamples = 0;
    int minPeriodSamples = 0;

    float lastPeriod = 0.0f;
    float grainPhase = 0.0f;             // Phase accumulator for grain spawning (0-1)
    double inputReadPosition = 0.0;      // Current read position in input stream
    int totalInputSamples = 0;           // Total samples written to input buffer
    int totalOutputSamples = 0;          // Total samples output so far

    // Constants
    static constexpr int grainOverlapFactor = 2;  // Grains overlap by 50%
    static constexpr float unvoicedBlendTime = 0.01f; // 10ms crossfade for unvoiced
};
