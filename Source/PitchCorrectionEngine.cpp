#include "PitchCorrectionEngine.h"

#include <cmath>

namespace
{
constexpr int fftOrder = 12;
constexpr int fftSize = 1 << fftOrder;
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
}

void PitchCorrectionEngine::prepare (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxBlockSize = samplesPerBlock;

    analysisBuffer.setSize (1, fftSize);
    analysisBuffer.clear();
    windowedBuffer = createHannWindow (fftSize);
    fftBuffer.allocate (fftSize);
    analysisWritePosition = 0;

    ratioSmoother.reset (sampleRate, 0.01);
    ratioSmoother.setCurrentAndTargetValue (1.0f);
    pitchSmoother.reset (sampleRate, 0.01);
    pitchSmoother.setCurrentAndTargetValue (0.0f);
    detectionSmoother.reset (sampleRate, 0.05);
    detectionSmoother.setCurrentAndTargetValue (0.0f);

    shifters.clear();
    shifters.resize (2);
    for (auto& shifter : shifters)
    {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
        shifter.prepare (spec);
        shifter.setWindowSize (fftSize);
        shifter.setHopSize (fftSize / 4);
    }

    dryBuffer.setSize (2, samplesPerBlock);
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
    heldMidiNote = std::numeric_limits<float>::quiet_NaN();
    for (auto& shifter : shifters)
        shifter.reset();
}

void PitchCorrectionEngine::setParameters (const Parameters& newParams)
{
    params = newParams;

    auto glideTime = juce::jmap (params.speed, 0.0f, 1.0f, 0.005f, 0.3f);
    ratioSmoother.reset (currentSampleRate, glideTime);

    auto transitionTime = juce::jmap (params.transition, 0.0f, 1.0f, 0.001f, 0.1f);
    pitchSmoother.reset (currentSampleRate, transitionTime);

    auto detectionTime = juce::jmap (params.vibratoTracking, 0.0f, 1.0f, 0.2f, 0.01f);
    detectionSmoother.reset (currentSampleRate, detectionTime);
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
        return;

    auto ratio = target / juce::jmax (detected, 1.0f);
    ratioSmoother.setTargetValue (ratio);
    pitchSmoother.setTargetValue (target);

    auto currentRatio = ratioSmoother.getCurrentValue();
    ratioSmoother.skip (numSamples);
    lastTargetFrequency = target;
    pitchSmoother.skip (numSamples);

    juce::dsp::AudioBlock<float> block (buffer);

    dryBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());

    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
    {
        if (ch >= shifters.size())
        {
            shifters.emplace_back();
            juce::dsp::ProcessSpec spec { currentSampleRate, (juce::uint32) maxBlockSize, 1 };
            shifters.back().prepare (spec);
            shifters.back().setWindowSize (fftSize);
            shifters.back().setHopSize (fftSize / 4);
        }

        auto& shifter = shifters[ch];
        shifter.setPitchRatio (currentRatio);
        shifter.process (juce::dsp::ProcessContextReplacing<float> (block.getSingleChannelBlock (ch)));
    }

    if (params.formantPreserve > 0.0f)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            auto* dry = dryBuffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                auto mixed = juce::jmap (params.formantPreserve, 0.0f, 1.0f, dry[i], data[i]);
                data[i] = juce::jlimit (-1.0f, 1.0f, mixed);
            }
        }
    }
}

void PitchCorrectionEngine::analyseBlock (const float* samples, int numSamples)
{
    auto* writePtr = analysisBuffer.getWritePointer (0);

    for (int i = 0; i < numSamples; ++i)
    {
        writePtr[analysisWritePosition] = samples[i];
        analysisWritePosition = (analysisWritePosition + 1) % fftSize;
    }

    juce::AudioBuffer<float> frame (1, fftSize);
    auto* framePtr = frame.getWritePointer (0);
    for (int i = 0; i < fftSize; ++i)
    {
        auto index = (analysisWritePosition + i) % fftSize;
        framePtr[i] = writePtr[index] * windowedBuffer.getSample (0, i);
    }

    auto* fftData = reinterpret_cast<float*> (fftBuffer.get());
    juce::FloatVectorOperations::copy (fftData, framePtr, fftSize);
    juce::FloatVectorOperations::clear (fftData + fftSize, fftSize);

    fft.performRealOnlyForwardTransform (fftData);

    lastDetectedFrequency = estimatePitchFromSpectrum();
    if (lastDetectedFrequency > 0.0f)
        detectionSmoother.setTargetValue (lastDetectedFrequency);
}

float PitchCorrectionEngine::estimatePitchFromSpectrum()
{
    auto* fftData = reinterpret_cast<float*> (fftBuffer.get());

    int binLow = (int) std::floor (params.rangeLowHz * fftSize / currentSampleRate);
    int binHigh = (int) std::ceil (params.rangeHighHz * fftSize / currentSampleRate);
    binLow = juce::jlimit (1, fftSize / 2, binLow);
    binHigh = juce::jlimit (binLow + 1, fftSize / 2, binHigh);

    float bestMagnitude = 0.0f;
    int bestBin = binLow;

    for (int bin = binLow; bin < binHigh; ++bin)
    {
        auto real = fftData[bin * 2];
        auto imag = fftData[bin * 2 + 1];
        auto mag = real * real + imag * imag;
        if (mag > bestMagnitude)
        {
            bestMagnitude = mag;
            bestBin = bin;
        }
    }

    if (bestMagnitude <= 0.0f)
        return 0.0f;

    if (bestBin > 0 && bestBin < fftSize / 2 - 1)
    {
        auto magnitude = [fftData] (int binIndex)
        {
            auto real = fftData[binIndex * 2];
            auto imag = fftData[binIndex * 2 + 1];
            return juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, real * real + imag * imag));
        };

        auto left = magnitude (bestBin - 1);
        auto center = magnitude (bestBin);
        auto right = magnitude (bestBin + 1);
        auto denominator = left - 2.0f * center + right;
        if (std::abs (denominator) > 1.0e-6f)
        {
            auto delta = 0.5f * (left - right) / denominator;
            bestBin += juce::jlimit (-1.0f, 1.0f, delta);
        }
    }

    auto frequency = (bestBin * currentSampleRate) / (float) fftSize;
    lastDetectedFrequency = frequency;
    return frequency;
}

float PitchCorrectionEngine::chooseTargetFrequency (float detectedFrequency)
{
    if (detectedFrequency <= 0.0f)
        return 0.0f;

    auto midiNote = frequencyToMidiNote (detectedFrequency);

    if (! std::isnan (heldMidiNote) && params.midiEnabled)
        midiNote = heldMidiNote;
    else
        midiNote = snapNoteToScale (midiNote, params.chromaticScale);

    auto deltaCents = 100.0f * (midiNote - frequencyToMidiNote (detectedFrequency));
    if (std::abs (deltaCents) < params.toleranceCents)
        return detectedFrequency;

    auto constrainedMidi = juce::jlimit (frequencyToMidiNote (params.rangeLowHz),
                                         frequencyToMidiNote (params.rangeHighHz),
                                         midiNote);

    return midiNoteToFrequency (constrainedMidi);
}

float PitchCorrectionEngine::frequencyToMidiNote (float freq)
{
    return referenceMidiNote + 12.0f * std::log2 (freq / referenceFrequency);
}

float PitchCorrectionEngine::midiNoteToFrequency (float midiNote)
{
    return referenceFrequency * std::pow (2.0f, (midiNote - referenceMidiNote) / 12.0f);
}

float PitchCorrectionEngine::snapNoteToScale (float midiNote, bool chromatic)
{
    if (chromatic)
        return std::round (midiNote);

    static constexpr int majorScale[] = { 0, 2, 4, 5, 7, 9, 11 };
    auto octave = std::floor (midiNote / 12.0f);
    auto noteInOctave = midiNote - octave * 12.0f;

    int closest = majorScale[0];
    float bestDistance = std::numeric_limits<float>::max();
    for (auto note : majorScale)
    {
        auto distance = std::abs (noteInOctave - (float) note);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            closest = note;
        }
    }

    return octave * 12.0f + (float) closest;
}
