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
    void configureSlider (juce::Slider& slider, const juce::String& name, bool isVertical = true);

    ProTuneAudioProcessor& processor;

    juce::Slider speedSlider;
    juce::Slider transitionSlider;
    juce::Slider toleranceSlider;
    juce::Slider formantSlider;
    juce::Slider vibratoSlider;
    juce::Slider rangeLowSlider;
    juce::Slider rangeHighSlider;

    juce::ToggleButton chromaticButton { "Chromatic" };
    juce::ToggleButton midiButton { "MIDI Control" };

    juce::Label detectedLabel { {}, "Detected" };
    juce::Label targetLabel { {}, "Target" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> speedAttachment;
    std::unique_ptr<SliderAttachment> transitionAttachment;
    std::unique_ptr<SliderAttachment> toleranceAttachment;
    std::unique_ptr<SliderAttachment> formantAttachment;
    std::unique_ptr<SliderAttachment> vibratoAttachment;
    std::unique_ptr<SliderAttachment> rangeLowAttachment;
    std::unique_ptr<SliderAttachment> rangeHighAttachment;

    std::unique_ptr<ButtonAttachment> chromaticAttachment;
    std::unique_ptr<ButtonAttachment> midiAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProTuneAudioProcessorEditor)
};
