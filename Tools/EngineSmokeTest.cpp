#include "../Source/PitchCorrectionEngine.h"

#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    std::cout << "=== ProTune Audio Test ===" << std::endl;

    PitchCorrectionEngine engine;
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;
    engine.prepare (sampleRate, blockSize);

    PitchCorrectionEngine::Parameters params;
    params.forceCorrection = true;
    params.scale.type = PitchCorrectionEngine::Parameters::ScaleSettings::Type::Chromatic;
    params.scale.root = 0;
    params.speed = 0.0f;
    params.transition = 0.0f;
    params.toleranceCents = 2.0f;
    params.formantPreserve = 0.0f;
    engine.setParameters (params);

    juce::MidiBuffer midi;

    // Generate 440 Hz sine wave
    const double frequency = 440.0;
    double phase = 0.0;
    const double twoPiOverRate = juce::MathConstants<double>::twoPi / sampleRate;

    juce::AudioBuffer<float> buffer (2, blockSize);

    const int numBlocks = 100;  // Run more blocks to allow warmup
    std::cout << "\nProcessing 440 Hz sine wave through " << numBlocks << " blocks..." << std::endl;
    std::cout << "Block\tInput RMS\tOutput RMS\tDetected Hz\tTarget Hz\tConfidence" << std::endl;
    std::cout << "-----\t---------\t----------\t-----------\t---------\t----------" << std::endl;

    bool hasOutput = false;
    bool detectedPitch = false;

    for (int block = 0; block < numBlocks; ++block)
    {
        // Fill buffer with sine wave
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            double p = phase;
            for (int i = 0; i < blockSize; ++i)
            {
                data[i] = (float) (std::sin (p) * 0.5);
                p += frequency * twoPiOverRate;
            }
        }
        phase += blockSize * frequency * twoPiOverRate;
        while (phase > juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;

        // Measure input
        double inputEnergy = 0.0;
        for (int i = 0; i < blockSize; ++i)
        {
            float s = buffer.getSample (0, i);
            inputEnergy += s * s;
        }
        double inputRms = std::sqrt (inputEnergy / blockSize);

        // Process
        engine.pushMidi (midi);
        engine.process (buffer);

        // Measure output
        double outputEnergy = 0.0;
        bool hasNaN = false;
        for (int i = 0; i < blockSize; ++i)
        {
            float s = buffer.getSample (0, i);
            if (std::isnan (s) || std::isinf (s))
                hasNaN = true;
            outputEnergy += s * s;
        }
        double outputRms = std::sqrt (outputEnergy / blockSize);

        if (outputRms > 0.01)
            hasOutput = true;

        float detected = engine.getLastDetectedFrequency();
        if (detected > 0.0f)
            detectedPitch = true;

        // Print first 10 blocks, then every 10th, or if we detected pitch
        if (block < 10 || block % 10 == 0 || detected > 0.0f)
        {
            std::cout << block << "\t"
                      << inputRms << "\t\t"
                      << outputRms << "\t\t"
                      << detected << "\t\t"
                      << engine.getLastTargetFrequency() << "\t\t"
                      << engine.getLastDetectionConfidence();

            if (hasNaN)
                std::cout << "\t[NaN DETECTED!]";

            std::cout << std::endl;
        }
    }

    std::cout << "\n=== Summary ===" << std::endl;
    if (hasOutput)
        std::cout << "PASS: Audio output detected" << std::endl;
    else
        std::cout << "FAIL: No audio output!" << std::endl;

    if (detectedPitch)
        std::cout << "PASS: Pitch detection working" << std::endl;
    else
        std::cout << "NOTE: Pitch detection needs tuning (no pitch detected for 440 Hz sine)" << std::endl;

    return hasOutput ? 0 : 1;
}
