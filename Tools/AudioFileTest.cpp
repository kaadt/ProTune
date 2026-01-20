#include "../Source/PitchCorrectionEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>

#include <iostream>
#include <fstream>

int main (int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout << "Usage: AudioFileTest <input.wav> <output.wav>" << std::endl;
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI init;

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];

    // Load input file
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (
        formatManager.createReaderFor (juce::File (inputPath)));

    if (reader == nullptr)
    {
        std::cout << "Failed to open input file: " << inputPath << std::endl;
        return 1;
    }

    std::cout << "Input file: " << inputPath << std::endl;
    std::cout << "Sample rate: " << reader->sampleRate << std::endl;
    std::cout << "Channels: " << reader->numChannels << std::endl;
    std::cout << "Length: " << reader->lengthInSamples << " samples ("
              << (reader->lengthInSamples / reader->sampleRate) << " seconds)" << std::endl;

    // Read entire file into buffer
    juce::AudioBuffer<float> inputBuffer (static_cast<int> (reader->numChannels),
                                           static_cast<int> (reader->lengthInSamples));
    reader->read (&inputBuffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);

    // Prepare engine
    PitchCorrectionEngine engine;
    constexpr int blockSize = 512;
    engine.prepare (reader->sampleRate, blockSize);

    // Set parameters for clear auto-tune effect
    PitchCorrectionEngine::Parameters params;
    params.retuneSpeedMs = 0.0f;  // Instant correction (T-Pain effect)
    params.tracking = 0.5f;
    params.humanize = 0.0f;
    params.transpose = 5;  // +5 semitones to make effect obvious
    params.detune = 0.0f;
    params.bypass = false;
    params.scale.type = PitchCorrectionEngine::Parameters::ScaleSettings::Type::Chromatic;
    params.scale.root = 0;  // C
    engine.setParameters (params);

    std::cout << "Testing with +5 semitone transpose to verify pitch shifting works" << std::endl;

    // Process in blocks
    juce::AudioBuffer<float> outputBuffer (inputBuffer.getNumChannels(),
                                            inputBuffer.getNumSamples());
    outputBuffer.clear();

    juce::MidiBuffer emptyMidi;
    int totalSamples = inputBuffer.getNumSamples();
    int processed = 0;
    int detectedCount = 0;
    int correctedCount = 0;

    std::cout << "\nProcessing..." << std::endl;

    while (processed < totalSamples)
    {
        int samplesThisBlock = std::min (blockSize, totalSamples - processed);

        // Copy input block
        juce::AudioBuffer<float> block (inputBuffer.getNumChannels(), samplesThisBlock);
        for (int ch = 0; ch < inputBuffer.getNumChannels(); ++ch)
        {
            block.copyFrom (ch, 0, inputBuffer, ch, processed, samplesThisBlock);
        }

        // Process
        engine.pushMidi (emptyMidi);
        engine.process (block);

        // Track stats
        if (engine.getLastDetectedFrequency() > 0)
        {
            detectedCount++;
            if (std::abs (engine.getLastPitchRatio() - 1.0f) > 0.001f)
                correctedCount++;
        }

        // Copy to output
        for (int ch = 0; ch < block.getNumChannels(); ++ch)
        {
            outputBuffer.copyFrom (ch, processed, block, ch, 0, samplesThisBlock);
        }

        processed += samplesThisBlock;

        // Progress
        if (processed % (blockSize * 100) == 0)
        {
            float progress = 100.0f * processed / totalSamples;
            std::cout << "\r  " << static_cast<int> (progress) << "% - "
                      << "Detected: " << engine.getLastDetectedFrequency() << " Hz, "
                      << "Target: " << engine.getLastTargetFrequency() << " Hz, "
                      << "Ratio: " << engine.getLastPitchRatio()
                      << "        " << std::flush;
        }
    }

    std::cout << "\n\nStats:" << std::endl;
    std::cout << "  Blocks with pitch detected: " << detectedCount << std::endl;
    std::cout << "  Blocks with correction applied: " << correctedCount << std::endl;

    // Write output file
    juce::File outFile (outputPath);
    outFile.deleteFile();

    std::unique_ptr<juce::AudioFormatWriter> writer (
        formatManager.findFormatForFileExtension ("wav")->createWriterFor (
            new juce::FileOutputStream (outFile),
            reader->sampleRate,
            static_cast<unsigned int> (outputBuffer.getNumChannels()),
            16, {}, 0));

    if (writer == nullptr)
    {
        std::cout << "Failed to create output file: " << outputPath << std::endl;
        return 1;
    }

    writer->writeFromAudioSampleBuffer (outputBuffer, 0, outputBuffer.getNumSamples());
    writer.reset();

    std::cout << "\nOutput written to: " << outputPath << std::endl;

    return 0;
}
