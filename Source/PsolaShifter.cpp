#include "PsolaShifter.h"
#include <cmath>
#include <algorithm>

PsolaShifter::PsolaShifter()
{
}

void PsolaShifter::prepare (double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;

    // Period range for typical voice: 50 Hz - 1000 Hz
    maxPeriodSamples = static_cast<int> (sampleRate / 50.0);
    minPeriodSamples = static_cast<int> (sampleRate / 1000.0);

    // Input buffer: large enough for several periods plus block
    int inputBufferSize = maxPeriodSamples * 8 + maxBlockSize;
    inputBuffer.assign (static_cast<size_t> (inputBufferSize), 0.0f);

    // Latency for look-ahead
    latencySamples = maxPeriodSamples * 2;

    reset();
}

void PsolaShifter::reset()
{
    std::fill (inputBuffer.begin(), inputBuffer.end(), 0.0f);
    inputWritePos = 0;
    outputWritePos = 0;
    outputReadPos = 0;
    activeGrains.clear();
    lastPeriod = 0.0f;
    synthesisPosition = 0.0f;
    lastGrainInputCenter = 0;
    totalInputSamples = 0;
}

void PsolaShifter::process (const float* input, float* output, int numSamples,
                            float pitchRatio, float detectedPeriod, float confidence)
{
    if (numSamples <= 0 || output == nullptr)
        return;

    // Clamp pitch ratio
    pitchRatio = juce::jlimit (0.5f, 2.0f, pitchRatio);

    int inputBufSize = static_cast<int> (inputBuffer.size());

    // Write input to circular buffer
    if (input != nullptr)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            inputBuffer[static_cast<size_t> (inputWritePos)] = input[i];
            inputWritePos = (inputWritePos + 1) % inputBufSize;
        }
        totalInputSamples += numSamples;
    }

    // If unvoiced or no pitch detected, pass through
    if (detectedPeriod <= 0.0f || confidence < 0.2f)
    {
        if (input != nullptr)
        {
            for (int i = 0; i < numSamples; ++i)
                output[i] = input[i];
        }
        else
        {
            std::fill (output, output + numSamples, 0.0f);
        }
        lastPeriod = 0.0f;
        synthesisPosition = 0.0f;
        activeGrains.clear();
        return;
    }

    // Clamp and smooth period
    float period = juce::jlimit (static_cast<float> (minPeriodSamples),
                                  static_cast<float> (maxPeriodSamples),
                                  detectedPeriod);

    if (lastPeriod <= 0.0f)
        lastPeriod = period;
    else
        lastPeriod = lastPeriod * 0.9f + period * 0.1f;

    period = lastPeriod;
    int periodInt = juce::jmax (minPeriodSamples, static_cast<int> (period + 0.5f));

    // Pitch-synchronous OLA with per-grain resampling:
    //
    // The KEY to pitch shifting is that each grain represents TWO pitch cycles.
    // By resampling the grain, we change the apparent frequency.
    //
    // Original: grain of 2*period samples = 2 cycles at input frequency
    // Resampled: same grain compressed/stretched to 2*period samples
    //            but containing pitchRatio more/fewer waveform cycles
    //
    // For pitch UP: compress waveform within grain (more cycles per grain)
    //   -> Read from input at rate pitchRatio (faster)
    //   -> Each output sample reads from input[i * pitchRatio]
    //
    // We keep grain SIZE the same (2*period samples) to maintain constant overlap
    // but change the CONTENT to have more/fewer waveform cycles.

    int grainSize = periodInt * 2;  // Fixed size for consistent overlap
    float hop = period;  // Extract and place grains every period samples

    // Clear output buffer for this block
    std::fill (output, output + numSamples, 0.0f);

    // Process: spawn grains at regular intervals
    for (int outSample = 0; outSample < numSamples; ++outSample)
    {
        // Check if we need to spawn a new grain
        while (synthesisPosition <= static_cast<float> (outSample))
        {
            // Calculate input position for this grain
            float inputPos = static_cast<float> (lastGrainInputCenter) + hop;

            // For pitch up, we need more input content per grain
            // So we read from a larger region of input (grainSize * pitchRatio samples)
            int inputReadSize = static_cast<int> (static_cast<float> (grainSize) * pitchRatio + 0.5f);

            // Check if we have enough input data
            if (static_cast<int> (inputPos) + inputReadSize / 2 > totalInputSamples)
                break;

            // Create grain
            Grain grain;
            grain.samples.resize (static_cast<size_t> (grainSize));
            grain.period = period;
            grain.centerPosition = static_cast<int> (inputPos);
            grain.outputPosition = outSample;

            // Extract and resample: read inputReadSize samples, output grainSize samples
            int inputStart = static_cast<int> (inputPos) - inputReadSize / 2;

            for (int i = 0; i < grainSize; ++i)
            {
                // Map output index to input index
                // For pitch UP: read MORE input samples per output sample
                float inputIdx = static_cast<float> (i) * pitchRatio;

                int idx0 = static_cast<int> (inputIdx);
                int idx1 = idx0 + 1;
                float frac = inputIdx - static_cast<float> (idx0);

                // Clamp to input bounds
                if (idx0 >= inputReadSize) idx0 = inputReadSize - 1;
                if (idx1 >= inputReadSize) idx1 = inputReadSize - 1;

                // Get input samples
                int bufIdx0 = ((inputStart + idx0) % inputBufSize + inputBufSize) % inputBufSize;
                int bufIdx1 = ((inputStart + idx1) % inputBufSize + inputBufSize) % inputBufSize;

                float s0 = inputBuffer[static_cast<size_t> (bufIdx0)];
                float s1 = inputBuffer[static_cast<size_t> (bufIdx1)];

                // Linear interpolation
                float sample = s0 + frac * (s1 - s0);

                // Apply Hanning window
                float window = getHannWindow (i, grainSize);
                grain.samples[static_cast<size_t> (i)] = sample * window;
            }

            activeGrains.push_back (std::move (grain));

            // Update positions
            lastGrainInputCenter = static_cast<int> (inputPos);
            synthesisPosition += hop;
        }

        // Synthesize: overlap-add all active grains
        for (auto it = activeGrains.begin(); it != activeGrains.end();)
        {
            Grain& grain = *it;
            int grainIdx = outSample - grain.outputPosition;
            int grainLen = static_cast<int> (grain.samples.size());

            if (grainIdx >= 0 && grainIdx < grainLen)
            {
                output[outSample] += grain.samples[static_cast<size_t> (grainIdx)];
            }

            if (grainIdx >= grainLen)
            {
                it = activeGrains.erase (it);
            }
            else
            {
                ++it;
            }
        }
    }

    // Adjust for next block
    synthesisPosition -= static_cast<float> (numSamples);

    for (auto& grain : activeGrains)
    {
        grain.outputPosition -= numSamples;
    }
}

float PsolaShifter::getHannWindow (int index, int size) const
{
    if (size <= 1)
        return 1.0f;

    float phase = static_cast<float> (index) / static_cast<float> (size - 1);
    return 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * phase));
}
