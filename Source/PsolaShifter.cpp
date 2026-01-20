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

    // Input buffer: large enough to hold samples for look-back
    int inputBufferSize = maxPeriodSamples * 8 + maxBlockSize;
    inputBuffer.assign (static_cast<size_t> (inputBufferSize), 0.0f);

    // Latency
    latencySamples = maxPeriodSamples * 2;

    reset();
}

void PsolaShifter::reset()
{
    std::fill (inputBuffer.begin(), inputBuffer.end(), 0.0f);
    inputWritePos = 0;
    activeGrains.clear();
    lastPeriod = 0.0f;
    grainPhase = 0.0f;
    inputReadPosition = -1.0;
    totalInputSamples = 0;
    totalOutputSamples = 0;
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

    // Clear output
    std::fill (output, output + numSamples, 0.0f);

    // If unvoiced or no pitch detected, pass through
    if (detectedPeriod <= 0.0f || confidence < 0.2f)
    {
        if (input != nullptr)
        {
            for (int i = 0; i < numSamples; ++i)
                output[i] = input[i];
        }
        lastPeriod = 0.0f;
        grainPhase = 0.0f;
        inputReadPosition = -1.0;
        activeGrains.clear();
        totalOutputSamples += numSamples;
        return;
    }

    // Smooth period
    float period = juce::jlimit (static_cast<float> (minPeriodSamples),
                                  static_cast<float> (maxPeriodSamples),
                                  detectedPeriod);

    if (lastPeriod <= 0.0f)
        lastPeriod = period;
    else
        lastPeriod = lastPeriod * 0.9f + period * 0.1f;

    period = lastPeriod;
    int periodInt = juce::jmax (minPeriodSamples, static_cast<int> (period + 0.5f));
    int grainSize = periodInt * 2;

    // ============================================================================
    // FORMANT-PRESERVING PSOLA - Duration-Preserving Pitch Shift
    // ============================================================================
    //
    // For realtime pitch correction with duration preservation:
    //
    // Keep the analysis read position moving in real time (1 sample out per 1 sample in)
    // so grain contents are not time-scaled (formants stay put). Pitch shifting is
    // achieved only by changing the spacing of synthesis grains.
    //
    // For pitch UP (pitchRatio > 1):
    //   - Place grains closer together (period / pitchRatio)
    //   - This repeats cycles at a higher rate
    //
    // For pitch DOWN (pitchRatio < 1):
    //   - Place grains further apart
    //   - This skips cycles to lower the pitch
    // ============================================================================

    float outputHop = period / pitchRatio;  // Grain output spacing
    float phaseIncrement = 1.0f / outputHop;

    // Initialize input read position to current block start
    if (inputReadPosition < 0.0)
    {
        inputReadPosition = static_cast<double> (totalInputSamples - numSamples);
        grainPhase = 1.0f;
    }

    int outputBlockStart = totalOutputSamples;

    for (int outSample = 0; outSample < numSamples; ++outSample)
    {
        // Advance input read position in real time to preserve formants
        inputReadPosition += 1.0;

        // Grain spawn phase
        grainPhase += phaseIncrement;

        while (grainPhase >= 1.0f)
        {
            grainPhase -= 1.0f;

            int oldestAvailable = totalInputSamples - inputBufSize;

            int minCenter = oldestAvailable + grainSize / 2;
            int maxCenter = totalInputSamples - grainSize / 2;

            if (maxCenter < minCenter)
                continue;

            int inputCenter = static_cast<int> (inputReadPosition + 0.5);
            inputCenter = juce::jlimit (minCenter, maxCenter, inputCenter);
            inputCenter = alignToPeak (inputCenter, juce::jmax (1, periodInt / 2), minCenter, maxCenter);
            int inputStart = inputCenter - grainSize / 2;

            if (inputStart >= oldestAvailable)
            {
                Grain grain;
                grain.samples.resize (static_cast<size_t> (grainSize));
                grain.window.resize (static_cast<size_t> (grainSize));
                grain.outputPosition = outputBlockStart + outSample;
                grain.centerPosition = inputCenter;
                grain.period = period;

                for (int i = 0; i < grainSize; ++i)
                {
                    int bufIdx = (inputStart + i) % inputBufSize;
                    if (bufIdx < 0) bufIdx += inputBufSize;

                    float sample = inputBuffer[static_cast<size_t> (bufIdx)];
                    float window = getHannWindow (i, grainSize);
                    grain.window[static_cast<size_t> (i)] = window;
                    grain.samples[static_cast<size_t> (i)] = sample * window;
                }

                activeGrains.push_back (std::move (grain));
            }
        }

        // Overlap-add
        float outValue = 0.0f;
        float windowSum = 0.0f;
        int currentOutputIdx = outputBlockStart + outSample;

        for (auto& grain : activeGrains)
        {
            int grainLen = static_cast<int> (grain.samples.size());
            int grainStart = grain.outputPosition - grainLen / 2;
            int relIdx = currentOutputIdx - grainStart;

            if (relIdx >= 0 && relIdx < grainLen)
            {
                outValue += grain.samples[static_cast<size_t> (relIdx)];
                windowSum += grain.window[static_cast<size_t> (relIdx)];
            }
        }

        if (windowSum > 1.0e-6f)
            outValue /= windowSum;

        output[outSample] = outValue;
    }

    // Cleanup finished grains
    int blockEnd = totalOutputSamples + numSamples;
    while (!activeGrains.empty())
    {
        auto& front = activeGrains.front();
        int grainLen = static_cast<int> (front.samples.size());
        int grainEnd = front.outputPosition + grainLen / 2;
        if (grainEnd <= blockEnd)
            activeGrains.pop_front();
        else
            break;
    }

    totalOutputSamples += numSamples;
}

float PsolaShifter::getHannWindow (int index, int size) const
{
    if (size <= 1)
        return 1.0f;

    float phase = static_cast<float> (index) / static_cast<float> (size - 1);
    return 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * phase));
}

int PsolaShifter::alignToPeak (int center, int searchRadius, int minCenter, int maxCenter) const
{
    if (inputBuffer.empty())
        return center;

    int inputBufSize = static_cast<int> (inputBuffer.size());
    int start = juce::jmax (center - searchRadius, minCenter);
    int end = juce::jmin (center + searchRadius, maxCenter);

    int bestPos = center;
    float bestValue = -1.0f;

    for (int pos = start; pos <= end; ++pos)
    {
        int bufIdx = pos % inputBufSize;
        if (bufIdx < 0)
            bufIdx += inputBufSize;

        float sample = inputBuffer[static_cast<size_t> (bufIdx)];
        float magnitude = std::abs (sample);
        if (magnitude > bestValue)
        {
            bestValue = magnitude;
            bestPos = pos;
        }
    }

    return bestPos;
}
