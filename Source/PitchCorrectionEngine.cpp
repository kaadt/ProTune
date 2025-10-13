#include "PitchCorrectionEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

namespace
{
constexpr float referenceFrequency = 440.0f;
constexpr int referenceMidiNote = 69;

juce::AudioBuffer<float> createHannWindow (int size)
{
    juce::AudioBuffer<float> buffer (1, size);
    auto* data = buffer.getWritePointer (0);
    for (int i = 0; i < size; ++i)
        data[i] = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * i / (float) (size - 1)));
    return buffer;
}

inline float wrapPhase (float value)
{
    return value - juce::MathConstants<float>::twoPi * std::floor ((value + juce::MathConstants<float>::pi)
                                                                   / juce::MathConstants<float>::twoPi);
}

inline int positiveModulo (int value, int modulo) noexcept
{
    auto remainder = value % modulo;
    return remainder < 0 ? remainder + modulo : remainder;
}

void designDecimationFilter (std::vector<float>& coefficients, int numTaps, int downsampleFactor)
{
    numTaps = juce::jmax (numTaps | 1, 3); // ensure odd length >= 3
    coefficients.resize ((size_t) numTaps);

    const auto m = (float) (numTaps - 1);
    const float cutoff = 1.0f / (2.0f * (float) downsampleFactor);

    for (int n = 0; n < numTaps; ++n)
    {
        const auto centred = (float) n - m * 0.5f;
        const auto window = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) n / m);

        float sinc;
        if (std::abs (centred) < 1.0e-6f)
            sinc = 2.0f * cutoff;
        else
            sinc = std::sin (juce::MathConstants<float>::twoPi * cutoff * centred)
                    / (juce::MathConstants<float>::pi * centred);

        coefficients[(size_t) n] = window * sinc;
    }

    const auto sum = std::accumulate (coefficients.begin(), coefficients.end(), 0.0f);
    if (std::abs (sum) > 1.0e-6f)
        for (auto& value : coefficients)
            value /= sum;
}

void applyDecimationFilter (const float* input, int inputSize,
                            const std::vector<float>& filter, int downsampleFactor,
                            std::vector<float>& filtered, std::vector<float>& downsampled)
{
    if (input == nullptr || inputSize <= 0)
    {
        downsampled.clear();
        return;
    }

    const auto filterSize = (int) filter.size();
    filtered.assign ((size_t) inputSize, 0.0f);

    if (filterSize == 0)
    {
        downsampled.assign (input, input + inputSize);
        return;
    }

    for (int n = 0; n < inputSize; ++n)
    {
        double acc = 0.0;
        for (int k = 0; k < filterSize; ++k)
        {
            const auto index = n - k;
            const auto sample = (index >= 0 && index < inputSize) ? input[index] : 0.0f;
            acc += (double) filter[(size_t) k] * (double) sample;
        }

        filtered[(size_t) n] = (float) acc;
    }

    const auto halfDelay = filterSize / 2;
    const auto firstSample = juce::jmin (halfDelay, inputSize - 1);

    const int estimatedSize = juce::jmax (1, (inputSize - firstSample + downsampleFactor - 1) / downsampleFactor);
    downsampled.resize ((size_t) estimatedSize);

    int outputIndex = 0;
    for (int n = firstSample; n < inputSize; n += downsampleFactor)
        downsampled[(size_t) outputIndex++] = filtered[(size_t) n];

    downsampled.resize ((size_t) outputIndex);
}
}

PitchCorrectionEngine::AllowedMask PitchCorrectionEngine::Parameters::ScaleSettings::patternToMask (int root,
                                                                                                    std::initializer_list<int> pattern) noexcept
{
    AllowedMask mask = 0;
    for (auto interval : pattern)
    {
        auto pitchClass = positiveModulo (root + interval, 12);
        mask |= (AllowedMask) (1u << pitchClass);
    }

    return mask;
}

PitchCorrectionEngine::AllowedMask PitchCorrectionEngine::Parameters::ScaleSettings::maskForType (Type type,
                                                                                                   int root,
                                                                                                   AllowedMask customMask) noexcept
{
    switch (type)
    {
        case Type::Chromatic:
            return 0x0FFFu;
        case Type::Major:
            return patternToMask (root, { 0, 2, 4, 5, 7, 9, 11 });
        case Type::NaturalMinor:
            return patternToMask (root, { 0, 2, 3, 5, 7, 8, 10 });
        case Type::HarmonicMinor:
            return patternToMask (root, { 0, 2, 3, 5, 7, 8, 11 });
        case Type::MelodicMinor:
            return patternToMask (root, { 0, 2, 3, 5, 7, 9, 11 });
        case Type::Dorian:
            return patternToMask (root, { 0, 2, 3, 5, 7, 9, 10 });
        case Type::Phrygian:
            return patternToMask (root, { 0, 1, 3, 5, 7, 8, 10 });
        case Type::Lydian:
            return patternToMask (root, { 0, 2, 4, 6, 7, 9, 11 });
        case Type::Mixolydian:
            return patternToMask (root, { 0, 2, 4, 5, 7, 9, 10 });
        case Type::Locrian:
            return patternToMask (root, { 0, 1, 3, 5, 6, 8, 10 });
        case Type::WholeTone:
            return patternToMask (root, { 0, 2, 4, 6, 8, 10 });
        case Type::Blues:
            return patternToMask (root, { 0, 3, 5, 6, 7, 10 });
        case Type::MajorPentatonic:
            return patternToMask (root, { 0, 2, 4, 7, 9 });
        case Type::MinorPentatonic:
            return patternToMask (root, { 0, 3, 5, 7, 10 });
        case Type::Diminished:
            return patternToMask (root, { 0, 2, 3, 5, 6, 8, 9, 11 });
        case Type::Custom:
        default:
            return customMask & 0x0FFFu;
    }
}

void PitchCorrectionEngine::prepare (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxBlockSize = samplesPerBlock;

    updateAnalysisResources();

    baseRatioGlideTime = 0.01f;
    baseTargetTransitionTime = 0.01f;

    ratioSmoother.reset (sampleRate, baseRatioGlideTime);
    ratioSmoother.setCurrentAndTargetValue (1.0f);
    pitchSmoother.reset (sampleRate, baseTargetTransitionTime);
    pitchSmoother.setCurrentAndTargetValue (0.0f);
    detectionSmoother.reset (sampleRate, 0.05);
    detectionSmoother.setCurrentAndTargetValue (0.0f);

    dryBuffer.setSize (2, samplesPerBlock);

    updateResamplerResources();
    ensureCycleResamplers (2);
}

void PitchCorrectionEngine::reset()
{
    analysisBuffer.clear();
    analysisWritePosition = 0;
    ratioSmoother.setCurrentAndTargetValue (1.0f);
    pitchSmoother.setCurrentAndTargetValue (0.0f);
    detectionSmoother.setCurrentAndTargetValue (0.0f);
    lastDetectedFrequency = 0.0f;
    lastTargetFrequency = 0.0f;
    lastDetectionConfidence = 0.0f;
    lastLoggedDetected = 0.0f;
    lastLoggedTarget = 0.0f;
    heldMidiNote = std::numeric_limits<float>::quiet_NaN();
    activeTargetMidi = std::numeric_limits<float>::quiet_NaN();
    dryBuffer.clear();

    for (auto& channel : cycleResamplers)
        channel.reset();
}

void PitchCorrectionEngine::setParameters (const Parameters& newParams)
{
    params = newParams;

    auto clampedSpeed = juce::jlimit (0.0f, 1.0f, params.speed);
    baseRatioGlideTime = juce::jmap (clampedSpeed, 0.0f, 1.0f, 0.3f, 0.003f);
    baseRatioGlideTime = juce::jlimit (0.001f, 0.35f, baseRatioGlideTime);
    ratioSmoother.reset (currentSampleRate, baseRatioGlideTime);

    baseTargetTransitionTime = juce::jmap (params.transition, 0.0f, 1.0f, 0.001f, 0.12f);
    pitchSmoother.reset (currentSampleRate, baseTargetTransitionTime);

    auto detectionTime = juce::jmap (params.vibratoTracking, 0.0f, 1.0f, 0.2f, 0.01f);
    detectionSmoother.reset (currentSampleRate, detectionTime);

    updateResamplerResources();
}

void PitchCorrectionEngine::setAnalysisWindowOrder (int newOrder)
{
    auto clampedOrder = juce::jlimit (minAnalysisFftOrder, maxAnalysisFftOrder, newOrder);
    if (clampedOrder == analysisFftOrder)
        return;

    analysisFftOrder = clampedOrder;
    updateAnalysisResources();
    reset();
}

void PitchCorrectionEngine::pushMidi (const juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        const auto& m = metadata.getMessage();
        if (m.isNoteOn())
            heldMidiNote = (float) m.getNoteNumber();
        else if (m.isNoteOff())
        {
            if (! std::isnan (heldMidiNote) && (int) heldMidiNote == m.getNoteNumber())
                heldMidiNote = std::numeric_limits<float>::quiet_NaN();
        }
    }
}

void PitchCorrectionEngine::process (juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() == 0)
        return;

    auto numSamples = buffer.getNumSamples();
    analyseBlock (buffer.getReadPointer (0), numSamples);

    auto detected = detectionSmoother.getCurrentValue();
    detectionSmoother.skip (numSamples);
    if (detected <= 0.0f)
        detected = lastDetectedFrequency;

    auto target = chooseTargetFrequency (detected);

    if (detected <= 0.0f || target <= 0.0f)
    {
        lastTargetFrequency = 0.0f;
        return;
    }

    auto ratio = target / juce::jmax (detected, 1.0f);
    ratioSmoother.reset (currentSampleRate, computeDynamicRatioTime (detected, target));
    ratioSmoother.setTargetValue (ratio);
    pitchSmoother.reset (currentSampleRate, computeDynamicTransitionTime (detected, target));
    pitchSmoother.setTargetValue (target);

    ensureCycleResamplers (buffer.getNumChannels());

    dryBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());

    juce::HeapBlock<float> ratioValues;
    juce::HeapBlock<float> periodValues;
    juce::HeapBlock<float> desiredPeriodValues;
    ratioValues.allocate ((size_t) numSamples, false);
    periodValues.allocate ((size_t) numSamples, false);
    desiredPeriodValues.allocate ((size_t) numSamples, false);

    float ratioValue = ratioSmoother.getCurrentValue();
    float targetValue = pitchSmoother.getCurrentValue();
    float finalTarget = targetValue;
    const float periodSamples = currentSampleRate / juce::jmax (detected, 1.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        auto clampedRatio = juce::jlimit (0.25f, 4.0f, ratioValue);
        ratioValues[i] = clampedRatio;
        periodValues[i] = periodSamples;
        desiredPeriodValues[i] = periodSamples / juce::jmax (0.25f, clampedRatio);

        finalTarget = targetValue;
        ratioValue = ratioSmoother.getNextValue();
        targetValue = pitchSmoother.getNextValue();
    }

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* dry = dryBuffer.getReadPointer (ch);
        auto* out = buffer.getWritePointer (ch);
        cycleResamplers[(size_t) ch].process (dry, out, numSamples,
                                              ratioValues.get(), periodValues.get(),
                                              desiredPeriodValues.get());
    }

    lastTargetFrequency = finalTarget;

    if (detected > 0.0f && finalTarget > 0.0f)
    {
        auto differenceDetected = std::abs (detected - lastLoggedDetected);
        auto differenceTarget = std::abs (finalTarget - lastLoggedTarget);
        constexpr float logThreshold = 0.5f; // Hz change before printing again

        if (differenceDetected >= logThreshold || differenceTarget >= logThreshold)
        {
            juce::String message ("PitchCorrectionEngine: detected "
                                  + juce::String (detected, 2) + " Hz -> target "
                                  + juce::String (finalTarget, 2) + " Hz");

            if (params.midiEnabled && ! std::isnan (heldMidiNote))
            {
                auto forcedFrequency = midiNoteToFrequency (heldMidiNote);
                message += " (MIDI override " + juce::String (forcedFrequency, 2) + " Hz)";
            }

            DBG (message);

            lastLoggedDetected = detected;
            lastLoggedTarget = finalTarget;
        }
    }

    if (params.formantPreserve > 0.0f)
    {
        auto dryAmount = juce::jlimit (0.0f, 1.0f, params.formantPreserve);

        // Equal-power crossfade keeps the perceived loudness stable while
        // allowing the user to re-introduce the dry timbre.
        auto wetGain = std::sqrt (juce::jlimit (0.0f, 1.0f, 1.0f - dryAmount));
        auto dryGain = std::sqrt (dryAmount);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            auto* dry = dryBuffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                auto mixed = wetGain * data[i] + dryGain * dry[i];
                data[i] = juce::jlimit (-1.0f, 1.0f, mixed);
            }
        }
    }
}

void PitchCorrectionEngine::analyseBlock (const float* samples, int numSamples)
{
    auto fftSize = getAnalysisFftSize();
    auto* writePtr = analysisBuffer.getWritePointer (0);

    for (int i = 0; i < numSamples; ++i)
    {
        writePtr[analysisWritePosition] = samples[i];
        analysisWritePosition = (analysisWritePosition + 1) % fftSize;
    }

    analysisFrame.setSize (1, fftSize, false, false, true);
    auto* framePtr = analysisFrame.getWritePointer (0);
    for (int i = 0; i < fftSize; ++i)
    {
        auto index = (analysisWritePosition + i) % fftSize;
        framePtr[i] = writePtr[index] * windowedBuffer.getSample (0, i);
    }

    float mean = 0.0f;
    for (int i = 0; i < fftSize; ++i)
        mean += framePtr[i];
    mean /= (float) juce::jmax (fftSize, 1);

    for (int i = 0; i < fftSize; ++i)
        framePtr[i] -= mean;

    float confidence = 0.0f;
    lastDetectedFrequency = estimatePitchFromAutocorrelation (framePtr, fftSize, confidence);
    lastDetectionConfidence = confidence;

    if (lastDetectedFrequency > 0.0f)
    {
        if (confidence >= 0.05f)
            detectionSmoother.setTargetValue (lastDetectedFrequency);
        else
            detectionSmoother.setTargetValue (0.0f);
    }
    else
    {
        detectionSmoother.setTargetValue (0.0f);
    }
}

float PitchCorrectionEngine::estimatePitchFromAutocorrelation (const float* frame, int frameSize, float& confidenceOut)
{
    confidenceOut = 0.0f;

    if (frame == nullptr || frameSize <= 0)
        return 0.0f;

    applyDecimationFilter (frame, frameSize, decimationFilter, pitchDetectionDownsample,
                           filteredFrame, downsampledFrame);

    const auto downsampledSize = (int) downsampledFrame.size();
    if (downsampledSize <= 4)
        return 0.0f;

    const auto safeHigh = juce::jmax (params.rangeHighHz, 1.0f);
    const auto safeLow = juce::jmax (params.rangeLowHz, 1.0f);

    const double downsampledRate = currentSampleRate / (double) pitchDetectionDownsample;

    int minLag = (int) std::floor (downsampledRate / safeHigh);
    int maxLag = (int) std::ceil (downsampledRate / safeLow);

    minLag = juce::jlimit (2, juce::jmax (2, downsampledSize / 2 - 1), minLag);
    maxLag = juce::jlimit (juce::jmax (minLag + 1, 3), juce::jmin (110, downsampledSize - 2), maxLag);

    if (maxLag <= minLag)
        return 0.0f;

    auto evaluateWindow = [] (const float* data, int dataSize, int lag) -> PeriodicitySample
    {
        if (lag <= 1 || lag * 2 >= dataSize)
            return {};

        const int windowEnd = dataSize;
        const int windowStart = juce::jmax (0, windowEnd - lag * 2);

        double energy = 0.0;
        double correlation = 0.0;

        for (int n = windowStart; n < windowEnd; ++n)
        {
            const auto current = (double) data[n];
            energy += current * current;

            const int prevIndex = n - lag;
            if (prevIndex >= windowStart)
                correlation += current * (double) data[prevIndex];
        }

        const double error = energy - 2.0 * correlation;
        return { error, energy, correlation };
    };

    int coarseBestLag = -1;
    double coarseBestError = std::numeric_limits<double>::infinity();

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        auto sample = evaluateWindow (downsampledFrame.data(), downsampledSize, lag);
        if (sample.energy <= 1.0e-9)
            continue;

        if (sample.error < coarseBestError)
        {
            coarseBestError = sample.error;
            coarseBestLag = lag;
        }
    }

    if (coarseBestLag <= 0)
        return 0.0f;

    const double fullRate = currentSampleRate;
    int minFullLag = (int) std::floor (fullRate / safeHigh);
    int maxFullLag = (int) std::ceil (fullRate / safeLow);

    minFullLag = juce::jlimit (2, frameSize / 4, minFullLag);
    maxFullLag = juce::jlimit (juce::jmax (minFullLag + 1, 3), frameSize / 2, maxFullLag);

    const int coarseLagFull = juce::jlimit (minFullLag, maxFullLag, coarseBestLag * pitchDetectionDownsample);
    const int searchRadius = juce::jmax (pitchDetectionDownsample * 2, 6);

    int fineMin = juce::jlimit (minFullLag, maxFullLag - 1, coarseLagFull - searchRadius);
    int fineMax = juce::jlimit (juce::jmax (fineMin + 2, minFullLag + 2), maxFullLag, coarseLagFull + searchRadius);

    if (fineMax - fineMin < 2)
        fineMax = juce::jmin (maxFullLag, fineMin + 2);

    periodicityScratch.assign ((size_t) (fineMax + 2), {});

    double bestError = std::numeric_limits<double>::infinity();
    int bestLag = -1;
    int secondLag = -1;

    for (int lag = fineMin; lag <= fineMax; ++lag)
    {
        auto sample = evaluateWindow (frame, frameSize, lag);
        periodicityScratch[(size_t) lag] = sample;

        if (sample.energy <= 1.0e-9)
            continue;

        if (sample.error < bestError)
        {
            secondLag = bestLag;
            bestError = sample.error;
            bestLag = lag;
        }
        else if (secondLag < 0 || sample.error < periodicityScratch[(size_t) secondLag].error)
        {
            secondLag = lag;
        }
    }

    if (bestLag <= 0)
        return 0.0f;

    auto bestSample = periodicityScratch[(size_t) bestLag];
    double energy = juce::jmax (bestSample.energy, 1.0e-9);

    constexpr double periodicityEps = 0.18;

    // Handle missing fundamentals by checking whether the second candidate
    // represents a lower harmonic with substantially better periodicity.
    if (secondLag > 0 && bestLag >= 2 * secondLag - 2)
    {
        auto secondSample = periodicityScratch[(size_t) secondLag];
        double secondEnergy = juce::jmax (secondSample.energy, 1.0e-9);
        if (secondSample.error < bestSample.error * 1.15
            && secondSample.error < periodicityEps * secondEnergy)
        {
            bestLag = secondLag;
            bestSample = secondSample;
            energy = secondEnergy;
        }
    }

    if (bestSample.error > periodicityEps * energy)
        return 0.0f;

    confidenceOut = (float) juce::jlimit (0.0, 1.0, 1.0 - bestSample.error / energy);

    double refinedLag = (double) bestLag;

    if (bestLag > fineMin && bestLag < fineMax)
    {
        const auto& prev = periodicityScratch[(size_t) (bestLag - 1)];
        const auto& center = periodicityScratch[(size_t) bestLag];
        const auto& next = periodicityScratch[(size_t) (bestLag + 1)];

        if (prev.energy > 0.0 && center.energy > 0.0 && next.energy > 0.0)
        {
            const auto v1 = prev.error;
            const auto v2 = center.error;
            const auto v3 = next.error;
            const auto denominator = v1 - 2.0 * v2 + v3;

            if (std::abs (denominator) > 1.0e-9)
                refinedLag += 0.5 * (v1 - v3) / denominator;
        }
    }

    refinedLag = juce::jlimit ((double) fineMin, (double) fineMax, refinedLag);

    if (refinedLag <= 1.0)
        return 0.0f;

    const auto frequency = fullRate / refinedLag;
    return (float) frequency;
}

void PitchCorrectionEngine::updateAnalysisResources()
{
    auto fftSize = getAnalysisFftSize();

    analysisBuffer.setSize (1, fftSize);
    analysisBuffer.clear();
    windowedBuffer = createHannWindow (fftSize);
    analysisFrame.setSize (1, fftSize);
    updateDownsamplingResources();
    analysisWritePosition = 0;
}

void PitchCorrectionEngine::updateDownsamplingResources()
{
    const auto fftSize = getAnalysisFftSize();
    filteredFrame.assign ((size_t) fftSize, 0.0f);
    downsampledFrame.assign ((size_t) juce::jmax (1, fftSize / pitchDetectionDownsample + 2), 0.0f);

    constexpr int decimationTaps = 33;
    designDecimationFilter (decimationFilter, decimationTaps, pitchDetectionDownsample);
}

void PitchCorrectionEngine::ensureCycleResamplers (int requiredChannels)
{
    if (requiredChannels <= 0)
        return;

    if ((int) cycleResamplers.size() < requiredChannels)
    {
        auto previousSize = cycleResamplers.size();
        cycleResamplers.resize ((size_t) requiredChannels);
        for (size_t i = previousSize; i < cycleResamplers.size(); ++i)
            cycleResamplers[i].prepare (currentSampleRate, allocatedMaxPeriodSamples, maxBlockSize);
    }
}

void PitchCorrectionEngine::updateResamplerResources()
{
    const float minimumFrequency = juce::jmax (params.rangeLowHz, 20.0f);
    allocatedMaxPeriodSamples = (int) std::ceil (currentSampleRate / juce::jmax (minimumFrequency, 1.0f));
    allocatedMaxPeriodSamples = juce::jlimit (32, 16384, allocatedMaxPeriodSamples);

    for (auto& resampler : cycleResamplers)
        resampler.prepare (currentSampleRate, allocatedMaxPeriodSamples, maxBlockSize);
}

void PitchCorrectionEngine::CycleResampler::prepare (double sampleRateIn, int maxPeriodSamplesIn, int maxBlockSizeIn)
{
    sampleRate = sampleRateIn;
    maxPeriodSamples = juce::jmax (maxPeriodSamplesIn, 1);
    maxBlockSize = juce::jmax (maxBlockSizeIn, 1);

    bufferSize = juce::jmax (maxBlockSize + maxPeriodSamples * 4, maxPeriodSamples * 6);
    buffer.assign ((size_t) bufferSize, 0.0f);
    reset();
}

void PitchCorrectionEngine::CycleResampler::reset()
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writeHead = 0.0;
    readHead = - (double) juce::jmax (maxPeriodSamples, 1);
}

void PitchCorrectionEngine::CycleResampler::process (const float* input, float* output, int numSamples,
                                                     const float* ratioValues, const float* periodValues,
                                                     const float* desiredPeriodValues)
{
    if (input == nullptr || output == nullptr || numSamples <= 0 || bufferSize <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto writeIndex = positiveModulo ((int) std::floor (writeHead), bufferSize);
        buffer[(size_t) writeIndex] = input[i];
        writeHead += 1.0;

        const double period = juce::jmax (1.0, (double) periodValues[i]);
        const double desiredPeriod = juce::jmax (1.0, (double) desiredPeriodValues[i]);
        const double ratio = juce::jmax (0.25, (double) ratioValues[i]);

        if (period <= 1.0 || desiredPeriod <= 1.0 || ! std::isfinite (ratio))
        {
            output[i] = input[i];
            continue;
        }

        if (readHead < -0.5)
            readHead = writeHead - period;

        const double increment = period / desiredPeriod;
        readHead += increment;

        const double lag = writeHead - readHead;
        const double minLag = juce::jmax (period * 0.75, desiredPeriod * 0.65);
        const double maxLag = juce::jmax (period * 1.35, desiredPeriod * 1.6);

        if (lag < minLag)
            readHead -= period;
        else if (lag > maxLag)
            readHead += period;

        double readIndex = std::fmod (readHead, (double) bufferSize);
        if (readIndex < 0.0)
            readIndex += bufferSize;

        const int index0 = (int) std::floor (readIndex);
        const int index1 = (index0 + 1) % bufferSize;
        const float frac = (float) (readIndex - (double) index0);
        const float s0 = buffer[(size_t) index0];
        const float s1 = buffer[(size_t) index1];

        output[i] = juce::jlimit (-1.0f, 1.0f, s0 + frac * (s1 - s0));
    }
}

float PitchCorrectionEngine::chooseTargetFrequency (float detectedFrequency)
{
    if (detectedFrequency <= 0.0f)
        return 0.0f;

    auto rawMidi = frequencyToMidiNote (detectedFrequency);
    const bool usingMidi = params.midiEnabled && ! std::isnan (heldMidiNote);

    auto candidateMidi = usingMidi ? heldMidiNote : snapNoteToMask (rawMidi, params.scale.mask);
    candidateMidi = clampMidiToRange (candidateMidi);

    if (usingMidi)
    {
        activeTargetMidi = candidateMidi;
        return midiNoteToFrequency (activeTargetMidi);
    }

    if (lastDetectionConfidence < minimumConfidenceForLock())
    {
        if (lastTargetFrequency > 0.0f)
            return lastTargetFrequency;

        activeTargetMidi = candidateMidi;
        return midiNoteToFrequency (activeTargetMidi);
    }

    auto snappedMidi = applyTargetHysteresis (candidateMidi, rawMidi);

    float finalMidi = snappedMidi;

    if (! params.forceCorrection)
    {
        auto toleranceSemitones = params.toleranceCents / 100.0f;
        if (toleranceSemitones > 0.0f)
        {
            auto deltaSemitones = snappedMidi - rawMidi;
            auto correctionMix = juce::jlimit (0.0f, 1.0f, std::abs (deltaSemitones) / toleranceSemitones);
            finalMidi = rawMidi + deltaSemitones * correctionMix;
        }
    }

    return midiNoteToFrequency (finalMidi);
}

float PitchCorrectionEngine::frequencyToMidiNote (float freq)
{
    return referenceMidiNote + 12.0f * std::log2 (freq / referenceFrequency);
}

float PitchCorrectionEngine::midiNoteToFrequency (float midiNote)
{
    return referenceFrequency * std::pow (2.0f, (midiNote - referenceMidiNote) / 12.0f);
}

float PitchCorrectionEngine::snapNoteToMask (float midiNote, AllowedMask mask)
{
    if (mask == 0)
        return std::round (midiNote);

    auto searchCenter = (int) std::floor (midiNote + 0.5f);
    float bestMidi = (float) searchCenter;
    float bestDistance = std::numeric_limits<float>::max();

    for (int delta = -24; delta <= 24; ++delta)
    {
        auto candidate = searchCenter + delta;
        auto pitchClass = positiveModulo (candidate, 12);
        if ((mask & (AllowedMask) (1u << pitchClass)) == 0)
            continue;

        auto distance = std::abs ((float) candidate - midiNote);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestMidi = (float) candidate;
        }
    }

    if (bestDistance == std::numeric_limits<float>::max())
        return std::round (midiNote);

    return bestMidi;
}


float PitchCorrectionEngine::computeDynamicRatioTime (float detectedFrequency, float targetFrequency) const
{
    auto time = baseRatioGlideTime;
    if (detectedFrequency <= 0.0f || targetFrequency <= 0.0f)
        return juce::jlimit (0.001f, 0.3f, time);

    auto detectedMidi = frequencyToMidiNote (detectedFrequency);
    auto targetMidi = frequencyToMidiNote (targetFrequency);
    auto semitoneDelta = std::abs (targetMidi - detectedMidi);

    auto vibratoFactor = juce::jmap (juce::jlimit (0.0f, 1.0f, params.vibratoTracking), 0.0f, 1.0f, 0.55f, 1.35f);
    auto limitedDelta = juce::jlimit (0.0f, 4.0f, semitoneDelta);
    auto distanceFactor = juce::jmap (limitedDelta, 0.0f, 4.0f, 0.5f, 1.35f);
    auto combined = juce::jlimit (0.25f, 1.5f, vibratoFactor * distanceFactor);

    return juce::jlimit (0.001f, 0.3f, time * combined);
}

float PitchCorrectionEngine::computeDynamicTransitionTime (float detectedFrequency, float targetFrequency) const
{
    juce::ignoreUnused (detectedFrequency);

    auto time = baseTargetTransitionTime;
    if (targetFrequency <= 0.0f)
        return juce::jlimit (0.001f, 0.4f, time);

    auto previousTarget = lastTargetFrequency > 0.0f ? lastTargetFrequency : targetFrequency;
    auto semitoneJump = std::abs (frequencyToMidiNote (targetFrequency) - frequencyToMidiNote (previousTarget));

    auto limitedJump = juce::jlimit (0.0f, 7.0f, semitoneJump);
    auto distanceFactor = juce::jmap (limitedJump, 0.0f, 7.0f, 0.45f, 1.35f);
    return juce::jlimit (0.001f, 0.4f, time * juce::jlimit (0.35f, 1.5f, distanceFactor));
}

float PitchCorrectionEngine::applyTargetHysteresis (float candidateMidi, float rawMidi)
{
    auto hysteresis = juce::jmap (params.transition, 0.0f, 1.0f, 0.05f, 0.45f);

    if (std::isnan (activeTargetMidi))
    {
        activeTargetMidi = candidateMidi;
        return activeTargetMidi;
    }

    auto deltaFromActive = std::abs (rawMidi - activeTargetMidi);
    if (deltaFromActive < hysteresis)
        return activeTargetMidi;

    activeTargetMidi = candidateMidi;
    return activeTargetMidi;
}

float PitchCorrectionEngine::clampMidiToRange (float midi) const noexcept
{
    return juce::jlimit (frequencyToMidiNote (params.rangeLowHz), frequencyToMidiNote (params.rangeHighHz), midi);
}

float PitchCorrectionEngine::minimumConfidenceForLock() const noexcept
{
    return juce::jmap (params.vibratoTracking, 0.0f, 1.0f, 0.18f, 0.06f);
}
