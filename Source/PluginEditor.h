#pragma once

#include <JuceHeader.h>
#include <array>
#include <limits>
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

    juce::ComboBox scaleSelector;
    juce::ComboBox keySelector;
    juce::ComboBox enharmonicSelector;
    juce::Label retuneLabel { {}, "Retune Speed" };
    juce::Label humanizeLabel { {}, "Humanize" };
    juce::Label scaleLabel { {}, "Scale" };
    juce::Label keyLabel { {}, "Key" };
    juce::Label enharmonicLabel { {}, "Enharmonics" };
    juce::Label formantLabel { {}, "Formant" };
    juce::ToggleButton midiButton { "MIDI Control" };
    juce::ToggleButton forceCorrectionButton { "Force Correction" };

    std::array<juce::ToggleButton, 12> noteButtons;

    juce::Label detectedLabel { {}, "Detected" };
    juce::Label targetLabel { {}, "Target" };
    juce::Label centralNoteLabel;
    juce::Label centralFreqLabel;
    juce::Label confidenceLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> speedAttachment;
    std::unique_ptr<SliderAttachment> transitionAttachment;
    std::unique_ptr<SliderAttachment> toleranceAttachment;
    std::unique_ptr<SliderAttachment> formantAttachment;
    std::unique_ptr<SliderAttachment> vibratoAttachment;
    std::unique_ptr<SliderAttachment> rangeLowAttachment;
    std::unique_ptr<SliderAttachment> rangeHighAttachment;

    std::unique_ptr<ComboBoxAttachment> scaleAttachment;
    std::unique_ptr<ComboBoxAttachment> keyAttachment;
    std::unique_ptr<ComboBoxAttachment> enharmonicAttachment;
    std::unique_ptr<ButtonAttachment> midiAttachment;
    std::unique_ptr<ButtonAttachment> forceCorrectionAttachment;

    float displayedDetectedHz = 0.0f;
    float displayedTargetHz = 0.0f;
    float displayedConfidence = 0.0f;

    bool isUpdatingScaleControls = false;
    bool isUpdatingNoteButtons = false;
    ProTuneAudioProcessor::AllowedMask lastDisplayedMask = std::numeric_limits<ProTuneAudioProcessor::AllowedMask>::max();
    ProTuneAudioProcessor::ScaleSettings::EnharmonicPreference lastEnharmonicPref
        = ProTuneAudioProcessor::ScaleSettings::EnharmonicPreference::Auto;
    bool lastPreferFlats = true;

    void refreshScaleDisplay();
    void updateNoteToggleStates (ProTuneAudioProcessor::AllowedMask mask);
    void updateNoteToggleLabels();
    void updateKeySelectorLabels();
    void handleScaleSelectorChanged();
    void handleKeySelectorChanged();
    void handleNoteToggle (int pitchClass);
    juce::String pitchClassName (int pitchClass) const;
    juce::String frequencyToDisplayName (float frequency) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProTuneAudioProcessorEditor)
};
