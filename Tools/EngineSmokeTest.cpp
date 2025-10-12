#include "../Source/PitchCorrectionEngine.h"

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

    const double frequency = 430.0;
    double phase = 0.0;
    const double twoPiOverRate = juce::MathConstants<double>::twoPi / sampleRate;

    juce::AudioBuffer<float> buffer (1, blockSize);

    double dryEnergy = 0.0;
    double wetEnergy = 0.0;
    double diffEnergy = 0.0;

    juce::AudioBuffer<float> dryCopy (1, blockSize);

    for (int block = 0; block < 300; ++block)
    {
        auto* data = buffer.getWritePointer (0);
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

    }

    std::cout << "Detected: " << engine.getLastDetectedFrequency() << " Hz\n";
    std::cout << "Target:   " << engine.getLastTargetFrequency() << " Hz\n";
    std::cout << "Confidence: " << engine.getLastDetectionConfidence() << "\n";
    std::cout << "Dry RMS:    " << std::sqrt (dryEnergy / (blockSize * 300)) << "\n";
    std::cout << "Wet RMS:    " << std::sqrt (wetEnergy / (blockSize * 300)) << "\n";
    std::cout << "Diff RMS:   " << std::sqrt (diffEnergy / (blockSize * 300)) << "\n";
    return 0;
}
