#pragma once

#include <JuceHeader.h>
#include <limits>
#include <vector>

class PitchCorrectionEngine
{
public:
    struct Parameters
    {
        float speed = 0.5f;
        float transition = 0.5f;
        float toleranceCents = 5.0f;
        float formantPreserve = 0.5f;
        float vibratoTracking = 0.5f;
        float rangeLowHz = 80.0f;
        float rangeHighHz = 1000.0f;
        bool midiEnabled = false;
        bool chromaticScale = true;
    };

    void prepare (double sampleRate, int samplesPerBlock);
    void reset();

    void setParameters (const Parameters& newParams);
    void pushMidi (const juce::MidiBuffer& midiMessages);

    void process (juce::AudioBuffer<float>& buffer);

    [[nodiscard]] float getLastDetectedFrequency() const noexcept { return lastDetectedFrequency; }
    [[nodiscard]] float getLastTargetFrequency() const noexcept { return lastTargetFrequency; }

private:
    void analyseBlock (const float* samples, int numSamples);
    float estimatePitchFromSpectrum();
    float chooseTargetFrequency (float detectedFrequency);

    static float frequencyToMidiNote (float freq);
    static float midiNoteToFrequency (float midiNote);
    static float snapNoteToScale (float midiNote, bool chromatic);

    double currentSampleRate = 44100.0;
    int maxBlockSize = 0;

    Parameters params;

    juce::dsp::FFT fft { 12 }; // 4096 point FFT
    juce::AudioBuffer<float> analysisBuffer;
    juce::AudioBuffer<float> windowedBuffer;
    juce::HeapBlock<juce::dsp::Complex<float>> fftBuffer;
    int analysisWritePosition = 0;

    float lastDetectedFrequency = 0.0f;
    float lastTargetFrequency = 0.0f;

    juce::SmoothedValue<float> ratioSmoother;
    juce::SmoothedValue<float> pitchSmoother;

    juce::AudioBuffer<float> dryBuffer;

    float heldMidiNote = std::numeric_limits<float>::quiet_NaN();

    juce::LinearSmoothedValue<float> detectionSmoother { 0.0f };

    struct PitchShiftChannel
    {
        void prepare (int frameSizeIn, int oversamplingIn, double sampleRateIn);
        void reset();
        void processSamples (float* samples, int numSamples, const float* ratios, juce::dsp::FFT& fft);

    private:
        void processFrame (float ratio, juce::dsp::FFT& fft);

        int frameSize = 0;
        int oversampling = 0;
        int hopSize = 0;
        int spectrumSize = 0;
        juce::HeapBlock<float> inFifo;
        juce::HeapBlock<float> outFifo;
        juce::HeapBlock<float> window;
        juce::HeapBlock<float> analysisMag;
        juce::HeapBlock<float> analysisFreq;
        juce::HeapBlock<float> synthesisMag;
        juce::HeapBlock<float> synthesisFreq;
        juce::HeapBlock<float> lastPhase;
        juce::HeapBlock<float> sumPhase;
        juce::HeapBlock<float> fftBuffer;

        int inFifoIndex = 0;
        int outFifoIndex = 0;
        float ratioAccumulator = 0.0f;
        int ratioSampleCount = 0;
    };

    void ensurePitchShiftChannels (int requiredChannels);

    static constexpr int pitchFftOrder = 11; // 2048 frame
    static constexpr int pitchFftSize = 1 << pitchFftOrder;
    static constexpr int pitchOversampling = 4;

    juce::dsp::FFT pitchFft { pitchFftOrder };
    std::vector<PitchShiftChannel> pitchChannels;
};
