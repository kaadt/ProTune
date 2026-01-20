#pragma once

#include <JuceHeader.h>
#include "PitchCorrectionEngine.h"

class ProTuneAudioProcessor : public juce::AudioProcessor
{
public:
    using ScaleSettings = PitchCorrectionEngine::Parameters::ScaleSettings;
    using AllowedMask = PitchCorrectionEngine::AllowedMask;

    ProTuneAudioProcessor();
    ~ProTuneAudioProcessor() override = default;

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

    // Telemetry for UI
    float getLastDetectedFrequency() const noexcept { return lastDetectedFrequency; }
    float getLastTargetFrequency() const noexcept { return lastTargetFrequency; }
    float getLastDetectionConfidence() const noexcept { return lastDetectionConfidence; }
    float getLastPitchRatio() const noexcept { return lastPitchRatio; }

    // Scale utilities
    ScaleSettings getScaleSettings() const;
    AllowedMask getEffectiveScaleMask() const;
    AllowedMask getCustomScaleMask() const;
    void setScaleMaskFromUI (AllowedMask mask);
    void setScaleModeFromUI (ScaleSettings::Type type);
    ScaleSettings::EnharmonicPreference getEnharmonicPreference() const;
    bool shouldUseFlatsForDisplay() const;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void updateEngineParameters();

    juce::AudioProcessorValueTreeState parameters;
    PitchCorrectionEngine engine;
    PitchCorrectionEngine::Parameters engineParameters;

    // New Auto-Tune Evo style parameters
    std::atomic<float>* inputTypeParam = nullptr;
    std::atomic<float>* retuneSpeedParam = nullptr;
    std::atomic<float>* trackingParam = nullptr;
    std::atomic<float>* humanizeParam = nullptr;
    std::atomic<float>* transposeParam = nullptr;
    std::atomic<float>* detuneParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    // Core parameters (shared)
    std::atomic<float>* keyParam = nullptr;
    std::atomic<float>* scaleModeParam = nullptr;
    std::atomic<float>* vibratoParam = nullptr;
    std::atomic<float>* formantParam = nullptr;
    std::atomic<float>* midiParam = nullptr;

    // Legacy parameters (for preset compatibility)
    std::atomic<float>* speedParam = nullptr;
    std::atomic<float>* transitionParam = nullptr;
    std::atomic<float>* toleranceParam = nullptr;
    std::atomic<float>* rangeLowParam = nullptr;
    std::atomic<float>* rangeHighParam = nullptr;
    std::atomic<float>* scaleRootParam = nullptr;
    std::atomic<float>* scaleMaskParam = nullptr;
    std::atomic<float>* enharmonicParam = nullptr;
    std::atomic<float>* forceCorrectionParam = nullptr;

    // Telemetry
    float lastDetectedFrequency = 0.0f;
    float lastTargetFrequency = 0.0f;
    float lastDetectionConfidence = 0.0f;
    float lastPitchRatio = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProTuneAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
