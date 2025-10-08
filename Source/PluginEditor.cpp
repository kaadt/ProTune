#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>

namespace
{
inline int positiveModulo (int value, int modulo) noexcept
{
    auto result = value % modulo;
    return result < 0 ? result + modulo : result;
}

inline juce::Font makeFont (float height, int style = juce::Font::plain)
{
    return juce::Font (juce::FontOptions (height, style));
}

juce::String frequencyToNoteName (float frequency)
{
    if (frequency <= 0.0f)
        return "--";

    constexpr float referenceFrequency = 440.0f;
    constexpr int referenceMidi = 69;

    auto midi = referenceMidi + 12.0f * std::log2 (frequency / referenceFrequency);
    auto rounded = (int) std::round (midi);
    static const juce::StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    auto noteIndex = positiveModulo (rounded, 12);
    auto octave = (int) std::floor (rounded / 12.0f) - 1;
    auto name = noteNames[noteIndex];
    return juce::String (name + juce::String (octave));
}
}

ProTuneAudioProcessorEditor::ProTuneAudioProcessorEditor (ProTuneAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    configureSlider (speedSlider, "Speed");
    configureSlider (transitionSlider, "Note Transition");
    configureSlider (toleranceSlider, "Tolerance", false);
    configureSlider (formantSlider, "Formant", false);
    configureSlider (vibratoSlider, "Vibrato", false);
    configureSlider (rangeLowSlider, "Low Hz", false);
    configureSlider (rangeHighSlider, "High Hz", false);

    addAndMakeVisible (chromaticButton);
    addAndMakeVisible (midiButton);

    detectedLabel.setJustificationType (juce::Justification::centred);
    detectedLabel.setFont (makeFont (16.0f, juce::Font::bold));
    addAndMakeVisible (detectedLabel);

    targetLabel.setJustificationType (juce::Justification::centred);
    targetLabel.setFont (makeFont (16.0f, juce::Font::bold));
    addAndMakeVisible (targetLabel);

    auto& vts = processor.getValueTreeState();

    speedAttachment = std::make_unique<SliderAttachment> (vts, "speed", speedSlider);
    transitionAttachment = std::make_unique<SliderAttachment> (vts, "transition", transitionSlider);
    toleranceAttachment = std::make_unique<SliderAttachment> (vts, "tolerance", toleranceSlider);
    formantAttachment = std::make_unique<SliderAttachment> (vts, "formant", formantSlider);
    vibratoAttachment = std::make_unique<SliderAttachment> (vts, "vibrato", vibratoSlider);
    rangeLowAttachment = std::make_unique<SliderAttachment> (vts, "rangeLow", rangeLowSlider);
    rangeHighAttachment = std::make_unique<SliderAttachment> (vts, "rangeHigh", rangeHighSlider);

    chromaticAttachment = std::make_unique<ButtonAttachment> (vts, "chromatic", chromaticButton);
    midiAttachment = std::make_unique<ButtonAttachment> (vts, "midiEnabled", midiButton);

    setSize (680, 360);
    startTimerHz (30);
}

void ProTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.9f));

    juce::Rectangle<float> header (0.0f, 0.0f, (float) getWidth(), 60.0f);
    juce::ColourGradient gradient (juce::Colours::orange.brighter(), header.getTopLeft(),
                                   juce::Colours::purple.darker(), header.getBottomRight(), false);
    g.setGradientFill (gradient);
    g.fillRect (header);

    g.setColour (juce::Colours::white);
    g.setFont (makeFont (28.0f, juce::Font::bold));
    g.drawFittedText ("ProTune", header.toNearestInt(), juce::Justification::centredLeft, 1);
    g.setFont (makeFont (14.0f));
    g.drawFittedText ("Instant vocal tuning for studio and stage", header.toNearestInt().reduced (8, 0),
                      juce::Justification::centredRight, 1);
}

void ProTuneAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (16);
    auto header = bounds.removeFromTop (60);
    juce::ignoreUnused (header);

    auto detectionArea = bounds.removeFromTop (60);
    auto detectionWidth = detectionArea.getWidth() / 2;
    detectedLabel.setBounds (detectionArea.removeFromLeft (detectionWidth).reduced (8));
    targetLabel.setBounds (detectionArea.reduced (8));

    auto controlArea = bounds.removeFromTop (160);
    auto columnWidth = controlArea.getWidth() / 5;

    speedSlider.setBounds (controlArea.removeFromLeft (columnWidth).reduced (10));
    transitionSlider.setBounds (controlArea.removeFromLeft (columnWidth).reduced (10));
    toleranceSlider.setBounds (controlArea.removeFromLeft (columnWidth).reduced (10));
    formantSlider.setBounds (controlArea.removeFromLeft (columnWidth).reduced (10));
    vibratoSlider.setBounds (controlArea.reduced (10));

    auto bottomArea = bounds;
    auto toggleArea = bottomArea.removeFromRight (200);
    chromaticButton.setBounds (toggleArea.removeFromTop (30));
    midiButton.setBounds (toggleArea.removeFromTop (30));

    auto rangeArea = bottomArea;
    rangeLowSlider.setBounds (rangeArea.removeFromLeft (rangeArea.getWidth() / 2).reduced (10));
    rangeHighSlider.setBounds (rangeArea.reduced (10));
}

void ProTuneAudioProcessorEditor::timerCallback()
{
    auto detected = processor.getLastDetectedFrequency();
    auto target = processor.getLastTargetFrequency();

    auto detectedText = juce::String (detected > 0.0f ? juce::String (detected, 2) + " Hz" : "--")
                        + " (" + frequencyToNoteName (detected) + ")";
    auto targetText = juce::String (target > 0.0f ? juce::String (target, 2) + " Hz" : "--")
                      + " (" + frequencyToNoteName (target) + ")";

    detectedLabel.setText ("Detected: " + detectedText, juce::dontSendNotification);
    targetLabel.setText ("Target: " + targetText, juce::dontSendNotification);
}

void ProTuneAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& name, bool isVertical)
{
    slider.setSliderStyle (isVertical ? juce::Slider::RotaryVerticalDrag : juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha (0.3f));
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::orange);
    slider.setColour (juce::Slider::trackColourId, juce::Colours::orange);
    slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    slider.setName (name);
    slider.setTooltip (name);
    addAndMakeVisible (slider);
}
