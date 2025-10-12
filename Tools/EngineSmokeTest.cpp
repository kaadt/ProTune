#include "../Source/PitchCorrectionEngine.h"

#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    PitchCorrectionEngine engine;
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;
    engine.prepare (sampleRate, blockSize);

    PitchCorrectionEngine::Parameters params;
    params.forceCorrection = true;
    params.chromaticScale = true;
    params.speed = 1.0f;
    params.transition = 0.0f;
    engine.setParameters (params);

    juce::MidiBuffer midi;

    const double frequencyA = 430.0;
    const double frequencyB = 480.0;
    const double expectedLocked = 440.0;
    const double expectedJump = 493.883;

    double phase = 0.0;
    const double twoPiOverRate = juce::MathConstants<double>::twoPi / sampleRate;

    juce::AudioBuffer<float> buffer (1, blockSize);

    double dryEnergy = 0.0;
    double wetEnergy = 0.0;
    double diffEnergy = 0.0;

    juce::AudioBuffer<float> dryCopy (1, blockSize);

    const int totalBlocks = 400;
    const int switchBlock = totalBlocks / 2;

    double lockedTargetA = 0.0;

    for (int block = 0; block < totalBlocks; ++block)
    {
        auto* data = buffer.getWritePointer (0);
        auto frequency = block < switchBlock ? frequencyA : frequencyB;
        for (int i = 0; i < blockSize; ++i)
        {
            data[i] = (float) std::sin (phase);
            phase += frequency * twoPiOverRate;
            if (phase > juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
        }

        dryCopy.copyFrom (0, 0, buffer, 0, 0, blockSize);

        engine.pushMidi (midi);
        engine.process (buffer);

        auto* processed = buffer.getReadPointer (0);
        auto* dry = dryCopy.getReadPointer (0);

        for (int i = 0; i < blockSize; ++i)
        {
            dryEnergy += dry[i] * dry[i];
            wetEnergy += processed[i] * processed[i];
            auto diff = processed[i] - dry[i];
            diffEnergy += diff * diff;
        }

        if (block == switchBlock - 1)
            lockedTargetA = engine.getLastTargetFrequency();
    }

    auto finalTarget = engine.getLastTargetFrequency();

    if (std::abs (lockedTargetA - expectedLocked) > 2.0)
    {
        std::cerr << "Expected first target near " << expectedLocked << " Hz but received " << lockedTargetA << " Hz\n";
        return 1;
    }

    if (std::abs (finalTarget - expectedJump) > 3.0)
    {
        std::cerr << "Expected post-jump target near " << expectedJump << " Hz but received " << finalTarget << " Hz\n";
        return 1;
    }

    std::cout << "Detected: " << engine.getLastDetectedFrequency() << " Hz\n";
    std::cout << "Target:   " << finalTarget << " Hz\n";
    std::cout << "Confidence: " << engine.getLastDetectionConfidence() << "\n";
    std::cout << "Dry RMS:    " << std::sqrt (dryEnergy / (blockSize * totalBlocks)) << "\n";
    std::cout << "Wet RMS:    " << std::sqrt (wetEnergy / (blockSize * totalBlocks)) << "\n";
    std::cout << "Diff RMS:   " << std::sqrt (diffEnergy / (blockSize * totalBlocks)) << "\n";
    return 0;
}
