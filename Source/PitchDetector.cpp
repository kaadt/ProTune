#include "PitchDetector.h"
#include <cmath>
#include <algorithm>
#include <numeric>

PitchDetector::PitchDetector()
{
}

void PitchDetector::prepare (double sr, [[maybe_unused]] int maxBlockSize)
{
    sampleRate = sr;

    // Analysis window: 4 periods at minimum frequency
    int maxPeriod = static_cast<int> (sampleRate / minFreqHz);
    analysisWindowSize = maxPeriod * 4;

    // Input buffer: hold enough for analysis
    inputBuffer.assign (static_cast<size_t> (analysisWindowSize * 2), 0.0f);
    inputWritePos = 0;

    // Downsampled buffer
    int downsampledSize = analysisWindowSize / downsampleFactor + filterTaps;
    downsampledBuffer.assign (static_cast<size_t> (downsampledSize), 0.0f);

    // Design decimation filter (windowed sinc)
    decimationFilter.resize (filterTaps);
    float cutoff = 1.0f / (2.0f * downsampleFactor);
    int halfTaps = filterTaps / 2;

    for (int n = 0; n < filterTaps; ++n)
    {
        float x = static_cast<float> (n - halfTaps);
        float window = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi *
                                                 static_cast<float> (n) / static_cast<float> (filterTaps - 1));
        float sinc;
        if (std::abs (x) < 1e-6f)
            sinc = 2.0f * cutoff;
        else
            sinc = std::sin (juce::MathConstants<float>::twoPi * cutoff * x) /
                   (juce::MathConstants<float>::pi * x);
        decimationFilter[static_cast<size_t> (n)] = window * sinc;
    }

    // Normalize filter
    float sum = std::accumulate (decimationFilter.begin(), decimationFilter.end(), 0.0f);
    if (std::abs (sum) > 1e-6f)
    {
        for (auto& c : decimationFilter)
            c /= sum;
    }

    // Analysis window (Hanning)
    analysisWindow.resize (static_cast<size_t> (analysisWindowSize));
    for (int i = 0; i < analysisWindowSize; ++i)
    {
        analysisWindow[static_cast<size_t> (i)] = 0.5f *
            (1.0f - std::cos (juce::MathConstants<float>::twoPi *
                               static_cast<float> (i) / static_cast<float> (analysisWindowSize - 1)));
    }

    // Allocate score buffers
    int maxCoarseLag = 120;  // Cover down to ~50Hz at downsampled rate
    coarseScores.resize (static_cast<size_t> (maxCoarseLag));
    fineScores.resize (static_cast<size_t> (analysisWindowSize / 2));

    reset();
}

void PitchDetector::reset()
{
    std::fill (inputBuffer.begin(), inputBuffer.end(), 0.0f);
    inputWritePos = 0;
    lastPeriod = 0.0f;
    lastConfidence = 0.0f;
    stableFrameCount = 0;
}

PitchDetector::Result PitchDetector::process (const float* input, int numSamples)
{
    Result result;

    if (input == nullptr || numSamples <= 0)
        return result;

    // Accumulate input into circular buffer
    int bufferSize = static_cast<int> (inputBuffer.size());
    for (int i = 0; i < numSamples; ++i)
    {
        inputBuffer[static_cast<size_t> (inputWritePos)] = input[i];
        inputWritePos = (inputWritePos + 1) % bufferSize;
    }

    // Extract analysis frame (WITHOUT windowing - important for periodicity detection)
    std::vector<float> analysisFrame (static_cast<size_t> (analysisWindowSize));
    for (int i = 0; i < analysisWindowSize; ++i)
    {
        int idx = ((inputWritePos - analysisWindowSize + i) % bufferSize + bufferSize) % bufferSize;
        analysisFrame[static_cast<size_t> (i)] = inputBuffer[static_cast<size_t> (idx)];
    }

    // Remove DC offset
    float mean = std::accumulate (analysisFrame.begin(), analysisFrame.end(), 0.0f) /
                 static_cast<float> (analysisWindowSize);
    for (auto& s : analysisFrame)
        s -= mean;

    // Downsample for coarse search
    downsample (analysisFrame.data(), analysisWindowSize);

    // Coarse search in downsampled domain
    int downsampledSize = analysisWindowSize / downsampleFactor;
    int coarseLag = coarseSearch (downsampledBuffer.data(), downsampledSize);

    if (coarseLag <= 0)
        return result;  // No pitch detected

    // Fine search at full rate
    int fullRateLag = coarseLag * downsampleFactor;
    float refinedPeriod = fineSearch (analysisFrame.data(), analysisWindowSize, fullRateLag);

    if (refinedPeriod <= 0.0f)
        return result;

    // Convert to frequency
    float frequency = static_cast<float> (sampleRate) / refinedPeriod;

    // Check frequency range
    if (frequency < minFreqHz || frequency > maxFreqHz)
        return result;

    // Calculate confidence
    int periodInt = static_cast<int> (refinedPeriod + 0.5f);
    PeriodScore score = evaluatePeriod (analysisFrame.data(), analysisWindowSize, periodInt);

    if (score.E < 1e-9)
        return result;

    float normalizedError = static_cast<float> (score.V / score.E);
    float confidence = juce::jlimit (0.0f, 1.0f, 1.0f - normalizedError / epsilon);

    // Apply hysteresis for stability
    if (lastPeriod > 0.0f)
    {
        float periodRatio = refinedPeriod / lastPeriod;
        if (periodRatio > 0.95f && periodRatio < 1.05f)
        {
            stableFrameCount++;
            confidence = std::min (1.0f, confidence + 0.1f * static_cast<float> (stableFrameCount) / 10.0f);
        }
        else
        {
            stableFrameCount = 0;
        }
    }

    result.frequency = frequency;
    result.period = refinedPeriod;
    result.confidence = confidence;
    result.voiced = (confidence > 0.2f);  // Match PSOLA threshold

    lastPeriod = refinedPeriod;
    lastConfidence = confidence;

    return result;
}

void PitchDetector::setInputType (InputType type)
{
    inputType = type;

    switch (type)
    {
        case InputType::Soprano:
            setFrequencyRange (200.0f, 1200.0f);
            break;
        case InputType::AltoTenor:
            setFrequencyRange (100.0f, 600.0f);
            break;
        case InputType::LowMale:
            setFrequencyRange (60.0f, 300.0f);
            break;
        case InputType::Instrument:
            setFrequencyRange (80.0f, 2000.0f);
            break;
        case InputType::BassInstrument:
            setFrequencyRange (30.0f, 250.0f);
            break;
    }
}

void PitchDetector::setFrequencyRange (float minHz, float maxHz)
{
    minFreqHz = juce::jmax (20.0f, minHz);
    maxFreqHz = juce::jmin (2000.0f, maxHz);

    if (minFreqHz > maxFreqHz)
        std::swap (minFreqHz, maxFreqHz);
}

void PitchDetector::setTracking (float tracking)
{
    // tracking 0 = strict (low epsilon), 1 = relaxed (high epsilon)
    epsilon = juce::jmap (tracking, 0.0f, 1.0f, 0.08f, 0.35f);
}

PitchDetector::PeriodScore PitchDetector::evaluatePeriod (const float* data, int dataSize, int lag)
{
    PeriodScore score;

    if (lag <= 0 || lag * 2 >= dataSize)
        return score;

    // Patent's approach: evaluate over 2 periods ending at current position
    int windowEnd = dataSize;
    int windowStart = juce::jmax (0, windowEnd - lag * 2);

    double energy = 0.0;
    double correlation = 0.0;

    for (int n = windowStart; n < windowEnd; ++n)
    {
        double current = static_cast<double> (data[n]);
        energy += current * current;

        int prevIndex = n - lag;
        if (prevIndex >= windowStart)
            correlation += current * static_cast<double> (data[prevIndex]);
    }

    score.E = energy;
    score.H = correlation;
    score.V = energy - 2.0 * correlation;

    return score;
}

float PitchDetector::refineWithQuadratic (int bestLag, const std::vector<PeriodScore>& scores)
{
    if (bestLag <= 0 || bestLag >= static_cast<int> (scores.size()) - 1)
        return static_cast<float> (bestLag);

    double v1 = scores[static_cast<size_t> (bestLag - 1)].V;
    double v2 = scores[static_cast<size_t> (bestLag)].V;
    double v3 = scores[static_cast<size_t> (bestLag + 1)].V;

    double denom = v1 - 2.0 * v2 + v3;
    if (std::abs (denom) < 1e-9)
        return static_cast<float> (bestLag);

    double offset = 0.5 * (v1 - v3) / denom;
    return static_cast<float> (bestLag) + static_cast<float> (offset);
}

int PitchDetector::coarseSearch (const float* downsampledData, int downsampledSize)
{
    // Determine lag range for coarse search
    double downsampledRate = sampleRate / downsampleFactor;
    int minLag = juce::jmax (2, static_cast<int> (downsampledRate / maxFreqHz));
    int maxLag = juce::jmin (110, static_cast<int> (downsampledRate / minFreqHz));

    if (maxLag <= minLag || maxLag >= downsampledSize / 2)
        return -1;

    int bestLag = -1;
    double bestRatio = 1.0;  // Track V/E ratio

    std::fill (coarseScores.begin(), coarseScores.end(), PeriodScore());

    for (int lag = minLag; lag <= maxLag && lag < static_cast<int> (coarseScores.size()); ++lag)
    {
        PeriodScore score = evaluatePeriod (downsampledData, downsampledSize, lag);
        coarseScores[static_cast<size_t> (lag)] = score;

        if (score.E < 1e-9)
            continue;

        double ratio = score.V / score.E;

        // Track the best ratio regardless of epsilon test
        if (ratio < bestRatio)
        {
            bestRatio = ratio;
            bestLag = lag;
        }
    }

    // Check if the best candidate passes the periodicity test
    // Use a more lenient threshold for coarse search
    if (bestLag <= 0 || bestRatio > 0.5)  // 0.5 is more lenient for coarse pass
        return -1;

    // Check for octave errors: prefer shorter periods (higher frequencies)
    // If half the period also passes the test, use the shorter period
    if (bestLag > 0)
    {
        int halfLag = bestLag / 2;
        if (halfLag >= minLag && halfLag < static_cast<int> (coarseScores.size()))
        {
            PeriodScore halfScore = coarseScores[static_cast<size_t> (halfLag)];
            if (halfScore.E > 1e-9)
            {
                double halfRatio = halfScore.V / halfScore.E;
                // Use the shorter period if it also has good periodicity
                // This prevents octave-down errors
                if (halfRatio < 0.5)  // Same threshold as coarse test
                {
                    bestLag = halfLag;
                    bestRatio = halfRatio;
                }
            }
        }
    }

    return bestLag;
}

float PitchDetector::fineSearch (const float* data, int dataSize, int coarseLag)
{
    // Search around coarse estimate with full sample resolution
    int searchRadius = downsampleFactor * 3;  // Search +/- 24 samples typically
    int minLag = juce::jmax (2, coarseLag - searchRadius);
    int maxLag = juce::jmin (dataSize / 2 - 1, coarseLag + searchRadius);

    if (maxLag <= minLag)
        return -1.0f;

    // Ensure fineScores is large enough
    if (static_cast<size_t> (maxLag + 1) > fineScores.size())
        fineScores.resize (static_cast<size_t> (maxLag + 2));

    int bestLag = -1;
    double bestRatio = 1.0;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        PeriodScore score = evaluatePeriod (data, dataSize, lag);
        fineScores[static_cast<size_t> (lag)] = score;

        if (score.E < 1e-9)
            continue;

        double ratio = score.V / score.E;
        if (ratio < bestRatio)
        {
            bestRatio = ratio;
            bestLag = lag;
        }
    }

    // Check if the best candidate passes the periodicity test
    if (bestLag <= 0 || bestRatio > epsilon)
        return -1.0f;

    // Refine with quadratic interpolation
    return refineWithQuadratic (bestLag, fineScores);
}

void PitchDetector::downsample (const float* input, int inputSize)
{
    int outputSize = inputSize / downsampleFactor;

    // Apply filter and decimate
    for (int n = 0; n < outputSize; ++n)
    {
        int inputIdx = n * downsampleFactor;
        double acc = 0.0;

        for (int k = 0; k < filterTaps; ++k)
        {
            int sampleIdx = inputIdx - filterTaps / 2 + k;
            float sample = (sampleIdx >= 0 && sampleIdx < inputSize) ? input[sampleIdx] : 0.0f;
            acc += sample * static_cast<double> (decimationFilter[static_cast<size_t> (k)]);
        }

        downsampledBuffer[static_cast<size_t> (n)] = static_cast<float> (acc);
    }
}
