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
    maxPeriodSamples = static_cast<int> (sampleRate / 50.0);   // ~50 Hz minimum
    minPeriodSamples = static_cast<int> (sampleRate / 1000.0); // ~1000 Hz maximum

    // Input buffer: need at least 4 periods worth for grain extraction
    int inputBufferSize = maxPeriodSamples * 8 + maxBlockSize;
    inputBuffer.assign (static_cast<size_t> (inputBufferSize), 0.0f);

    // Output accumulator: same size for overlap-add
    outputAccum.assign (static_cast<size_t> (inputBufferSize), 0.0f);

    // Latency is approximately 2 periods at minimum frequency
    latencySamples = maxPeriodSamples * 2;

    reset();
}

void PsolaShifter::reset()
{
    std::fill (inputBuffer.begin(), inputBuffer.end(), 0.0f);
    std::fill (outputAccum.begin(), outputAccum.end(), 0.0f);
    inputWritePos = 0;
    inputReadPos = 0;
    outputWritePos = 0;
    outputReadPos = 0;
    activeGrains.clear();
    lastPeriod = 0.0f;
    synthesisPosition = 0.0f;
    lastGrainInputCenter = -maxPeriodSamples * 4;  // Force first grain extraction
    totalInputSamples = 0;
}

void PsolaShifter::process (const float* input, float* output, int numSamples,
                            float pitchRatio, float detectedPeriod, float confidence)
{
    if (numSamples <= 0 || output == nullptr)
        return;

    // Clamp pitch ratio to reasonable range
    pitchRatio = juce::jlimit (0.5f, 2.0f, pitchRatio);

    // Handle unvoiced or no detection: pass through with slight smoothing
    if (detectedPeriod <= 0.0f || confidence < 0.1f)
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
        return;
    }

    // Clamp detected period to valid range
    float period = juce::jlimit (static_cast<float> (minPeriodSamples),
                                  static_cast<float> (maxPeriodSamples),
                                  detectedPeriod);

    // Smooth period transitions to avoid artifacts
    if (lastPeriod <= 0.0f)
        lastPeriod = period;
    else
        lastPeriod += (period - lastPeriod) * 0.3f;

    period = lastPeriod;

    int periodInt = static_cast<int> (period + 0.5f);
    int grainSize = periodInt * 2;  // Grain is 2 periods

    // Write input to circular buffer
    if (input != nullptr)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            inputBuffer[static_cast<size_t> (inputWritePos)] = input[i];
            inputWritePos = (inputWritePos + 1) % static_cast<int> (inputBuffer.size());
        }
    }
    totalInputSamples += numSamples;

    // Determine grain spacing for synthesis
    // For pitch up: grains are placed closer together (synthesis hop < analysis hop)
    // For pitch down: grains are placed further apart (synthesis hop > analysis hop)
    float analysisHop = period;  // Extract grains every period

    // Extract new grains as needed
    int availableInput = totalInputSamples - lastGrainInputCenter;

    while (availableInput >= static_cast<int> (analysisHop) + grainSize)
    {
        int grainCenter = lastGrainInputCenter + static_cast<int> (analysisHop);
        extractGrain (grainCenter, period);
        lastGrainInputCenter = grainCenter;
        availableInput = totalInputSamples - lastGrainInputCenter;
    }

    // Synthesize output via overlap-add
    synthesizeGrains (output, numSamples);
}

void PsolaShifter::extractGrain (int centerPos, float period)
{
    int periodInt = static_cast<int> (period + 0.5f);
    int grainSize = periodInt * 2;  // 2 periods per grain

    Grain grain;
    grain.samples.resize (static_cast<size_t> (grainSize));
    grain.centerPosition = centerPos;
    grain.period = period;

    int bufferSize = static_cast<int> (inputBuffer.size());
    int startPos = centerPos - periodInt;

    // Extract samples with Hanning window
    for (int i = 0; i < grainSize; ++i)
    {
        int bufIdx = ((startPos + i) % bufferSize + bufferSize) % bufferSize;
        float window = getHannWindow (i, grainSize);
        grain.samples[static_cast<size_t> (i)] = inputBuffer[static_cast<size_t> (bufIdx)] * window;
    }

    // Calculate output position based on synthesis timeline
    // Each grain is placed at synthesisPosition, then we advance by synthesisHop
    grain.outputPosition = static_cast<int> (synthesisPosition);

    activeGrains.push_back (std::move (grain));

    // Advance synthesis position by synthesis hop (period / pitchRatio already factored in)
    synthesisPosition += period;  // Will be adjusted in synthesize
}

void PsolaShifter::synthesizeGrains (float* output, int numSamples)
{
    // Clear output first
    std::fill (output, output + numSamples, 0.0f);

    // Process each active grain
    auto it = activeGrains.begin();
    while (it != activeGrains.end())
    {
        Grain& grain = *it;
        int grainSize = static_cast<int> (grain.samples.size());

        // Calculate where this grain contributes to current output
        int grainStart = grain.outputPosition - outputReadPos;
        int grainEnd = grainStart + grainSize;

        // Check if grain is completely past
        if (grainEnd < 0)
        {
            it = activeGrains.erase (it);
            continue;
        }

        // Check if grain hasn't started yet
        if (grainStart >= numSamples)
        {
            ++it;
            continue;
        }

        // Add grain contribution to output
        int outStart = std::max (0, grainStart);
        int outEnd = std::min (numSamples, grainEnd);

        for (int i = outStart; i < outEnd; ++i)
        {
            int grainIdx = i - grainStart;
            if (grainIdx >= 0 && grainIdx < grainSize)
            {
                output[i] += grain.samples[static_cast<size_t> (grainIdx)];
            }
        }

        // Remove if grain is complete
        if (grainEnd <= numSamples)
        {
            it = activeGrains.erase (it);
        }
        else
        {
            ++it;
        }
    }

    // Update read position for next block
    outputReadPos += numSamples;

    // Normalize output to prevent clipping from overlap-add
    // With 50% overlap and Hanning windows, the sum should be close to 1.0
    // but we add a safety limiter
    for (int i = 0; i < numSamples; ++i)
    {
        output[i] = juce::jlimit (-1.0f, 1.0f, output[i]);
    }
}

float PsolaShifter::getHannWindow (int index, int size) const
{
    if (size <= 1)
        return 1.0f;

    float phase = static_cast<float> (index) / static_cast<float> (size - 1);
    return 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * phase));
}
