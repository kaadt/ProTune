#include "../Source/PitchCorrectionEngine.h"
#include <cmath>
#include <iostream>
#include <fstream>

// Simple WAV writer
void writeWav(const char* filename, const float* data, int numSamples, int sampleRate) {
    std::ofstream file(filename, std::ios::binary);
    
    // WAV header
    int dataSize = numSamples * 2;  // 16-bit samples
    int fileSize = 44 + dataSize - 8;
    
    file.write("RIFF", 4);
    file.write(reinterpret_cast<char*>(&fileSize), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    int fmtSize = 16;
    file.write(reinterpret_cast<char*>(&fmtSize), 4);
    short audioFormat = 1;  // PCM
    file.write(reinterpret_cast<char*>(&audioFormat), 2);
    short numChannels = 1;
    file.write(reinterpret_cast<char*>(&numChannels), 2);
    file.write(reinterpret_cast<char*>(&sampleRate), 4);
    int byteRate = sampleRate * 2;
    file.write(reinterpret_cast<char*>(&byteRate), 4);
    short blockAlign = 2;
    file.write(reinterpret_cast<char*>(&blockAlign), 2);
    short bitsPerSample = 16;
    file.write(reinterpret_cast<char*>(&bitsPerSample), 2);
    file.write("data", 4);
    file.write(reinterpret_cast<char*>(&dataSize), 4);
    
    // Write samples
    for (int i = 0; i < numSamples; i++) {
        short s = static_cast<short>(data[i] * 32767.0f);
        file.write(reinterpret_cast<char*>(&s), 2);
    }
}

int main() {
    const int sampleRate = 44100;
    const int duration = 2;  // seconds
    const int numSamples = sampleRate * duration;
    const float inputFreq = 220.0f;  // A3
    
    std::vector<float> input(numSamples);
    std::vector<float> output(numSamples);
    
    // Generate 220 Hz sine wave
    for (int i = 0; i < numSamples; i++) {
        input[i] = 0.5f * std::sin(2.0f * 3.14159265f * inputFreq * i / sampleRate);
    }
    
    // Write input for reference
    writeWav("sine_input.wav", input.data(), numSamples, sampleRate);
    std::cout << "Input: " << inputFreq << " Hz sine wave" << std::endl;
    
    // Process with +5 semitone shift (ratio ~1.335)
    PitchCorrectionEngine engine;
    engine.prepare(sampleRate, 512);
    
    PitchCorrectionEngine::Parameters params;
    params.retuneSpeedMs = 0.0f;  // Instant
    params.transpose = 5;  // +5 semitones
    params.bypass = false;
    params.scale.type = PitchCorrectionEngine::Parameters::ScaleSettings::Type::Chromatic;
    engine.setParameters(params);
    
    float expectedFreq = inputFreq * std::pow(2.0f, 5.0f / 12.0f);
    std::cout << "Expected output: " << expectedFreq << " Hz" << std::endl;
    
    // Process in blocks
    const int blockSize = 512;
    juce::MidiBuffer emptyMidi;
    
    for (int i = 0; i < numSamples; i += blockSize) {
        int samplesThisBlock = std::min(blockSize, numSamples - i);
        juce::AudioBuffer<float> block(1, samplesThisBlock);
        block.copyFrom(0, 0, input.data() + i, samplesThisBlock);
        
        engine.pushMidi(emptyMidi);
        engine.process(block);
        
        std::memcpy(output.data() + i, block.getReadPointer(0), samplesThisBlock * sizeof(float));
    }
    
    // Write output
    writeWav("sine_output.wav", output.data(), numSamples, sampleRate);
    
    // Analyze output: count zero crossings to estimate frequency
    int zeroCrossings = 0;
    for (int i = 1; i < numSamples; i++) {
        if ((output[i-1] >= 0 && output[i] < 0) || (output[i-1] < 0 && output[i] >= 0)) {
            zeroCrossings++;
        }
    }
    float estimatedFreq = (zeroCrossings / 2.0f) / duration;
    std::cout << "Measured output: " << estimatedFreq << " Hz (from zero crossings)" << std::endl;
    
    // Calculate RMS to verify signal exists
    float rms = 0;
    for (int i = 0; i < numSamples; i++) {
        rms += output[i] * output[i];
    }
    rms = std::sqrt(rms / numSamples);
    std::cout << "Output RMS: " << rms << std::endl;
    
    return 0;
}
