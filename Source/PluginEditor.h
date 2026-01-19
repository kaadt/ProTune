#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class ProTuneAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit ProTuneAudioProcessorEditor (ProTuneAudioProcessor&);
    ~ProTuneAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void configureSlider (juce::Slider& slider, const juce::String& name);

    ProTuneAudioProcessor& processor;

    juce::Slider speedSlider;
    juce::Slider formantSlider;

    juce::ComboBox scaleSelector;
    juce::ComboBox keySelector;

    juce::Label speedLabel { {}, "Speed" };
    juce::Label formantLabel { {}, "Formant" };
    juce::Label scaleLabel { {}, "Scale" };
    juce::Label keyLabel { {}, "Key" };

    juce::Label centralNoteLabel;
    juce::Label centralFreqLabel;
    juce::Label detectedLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> speedAttachment;
    std::unique_ptr<SliderAttachment> formantAttachment;
    std::unique_ptr<ComboBoxAttachment> scaleAttachment;
    std::unique_ptr<ComboBoxAttachment> keyAttachment;

    float displayedDetectedHz = 0.0f;
    float displayedTargetHz = 0.0f;

    juce::String frequencyToNoteName (float frequency) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProTuneAudioProcessorEditor)
};
