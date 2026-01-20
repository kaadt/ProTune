#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

/**
 * Auto-Tune Evo Style Plugin Editor
 *
 * Layout (600x450):
 * - Header with title and bypass
 * - Left: Pitch meter with note display and cents deviation bar
 * - Right: Input type, key, scale selectors + retune speed knob
 * - Bottom: Control strip with tracking, humanize, vibrato, transpose, detune
 */
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
    void configureSlider (juce::Slider& slider, const juce::String& suffix = "");
    void configureLabel (juce::Label& label, float fontSize = 12.0f);
    juce::String frequencyToNoteName (float frequency) const;
    float frequencyToDeviation (float detected, float target) const;

    ProTuneAudioProcessor& processor;

    // Header
    juce::ToggleButton bypassButton { "Bypass" };

    // Pitch display
    juce::Label noteLabel;           // Large note name (e.g., "A4")
    juce::Label frequencyLabel;      // Frequency in Hz
    juce::Label inputPitchLabel;     // "Input: A4 (440.0 Hz)"

    // Selectors
    juce::ComboBox inputTypeSelector;
    juce::ComboBox keySelector;
    juce::ComboBox scaleSelector;
    juce::Label inputTypeLabel { {}, "Input Type" };
    juce::Label keyLabel { {}, "Key" };
    juce::Label scaleLabel { {}, "Scale" };

    // Main control - Retune Speed
    juce::Slider retuneSpeedSlider;
    juce::Label retuneSpeedLabel { {}, "Retune Speed" };

    // Control strip
    juce::Slider trackingSlider;
    juce::Slider humanizeSlider;
    juce::Slider vibratoSlider;
    juce::Slider transposeSlider;
    juce::Slider detuneSlider;

    juce::Label trackingLabel { {}, "Tracking" };
    juce::Label humanizeLabel { {}, "Humanize" };
    juce::Label vibratoLabel { {}, "Vibrato" };
    juce::Label transposeLabel { {}, "Transpose" };
    juce::Label detuneLabel { {}, "Detune" };

    // Parameter attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<ComboBoxAttachment> inputTypeAttachment;
    std::unique_ptr<ComboBoxAttachment> keyAttachment;
    std::unique_ptr<ComboBoxAttachment> scaleAttachment;
    std::unique_ptr<SliderAttachment> retuneSpeedAttachment;
    std::unique_ptr<SliderAttachment> trackingAttachment;
    std::unique_ptr<SliderAttachment> humanizeAttachment;
    std::unique_ptr<SliderAttachment> vibratoAttachment;
    std::unique_ptr<SliderAttachment> transposeAttachment;
    std::unique_ptr<SliderAttachment> detuneAttachment;

    // Smoothed display values
    float displayedDetectedHz = 0.0f;
    float displayedTargetHz = 0.0f;
    float displayedDeviation = 0.0f;

    // Colors
    juce::Colour bgColor { 18, 22, 30 };
    juce::Colour headerColor1 { 30, 80, 140 };
    juce::Colour headerColor2 { 20, 40, 70 };
    juce::Colour accentColor { 0, 180, 255 };
    juce::Colour meterBgColor { 25, 30, 40 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProTuneAudioProcessorEditor)
};
