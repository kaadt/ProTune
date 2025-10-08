#pragma once

#include <JuceHeader.h>
#include "PitchCorrectionEngine.h"

class ProTuneAudioProcessor : public juce::AudioProcessor
{
public:
    ProTuneAudioProcessor();
    ~ProTuneAudioProcessor() override = default;

    // AudioProcessor overrides
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return parameters; }

    float getLastDetectedFrequency() const noexcept { return lastDetectedFrequency; }
    float getLastTargetFrequency() const noexcept { return lastTargetFrequency; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void updateEngineParameters();

    PitchCorrectionEngine engine;
    PitchCorrectionEngine::Parameters engineParameters;

    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* speedParam = nullptr;
    std::atomic<float>* transitionParam = nullptr;
    std::atomic<float>* toleranceParam = nullptr;
    std::atomic<float>* formantParam = nullptr;
    std::atomic<float>* vibratoParam = nullptr;
    std::atomic<float>* rangeLowParam = nullptr;
    std::atomic<float>* rangeHighParam = nullptr;
    std::atomic<float>* chromaticParam = nullptr;
    std::atomic<float>* midiParam = nullptr;

    float lastDetectedFrequency = 0.0f;
    float lastTargetFrequency = 0.0f;
};

class ProTuneAudioProcessorEditor;

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
