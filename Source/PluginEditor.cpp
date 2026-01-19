#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>

namespace
{
constexpr int windowWidth = 400;
constexpr int windowHeight = 500;
constexpr float meterDiameter = 200.0f;

static const juce::StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

ProTuneAudioProcessorEditor::ProTuneAudioProcessorEditor (ProTuneAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    configureSlider (speedSlider, "Speed");
    configureSlider (formantSlider, "Formant");

    auto setupLabel = [] (juce::Label& label)
    {
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, juce::Colours::white);
        label.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    };

    setupLabel (speedLabel);
    setupLabel (formantLabel);
    setupLabel (scaleLabel);
    setupLabel (keyLabel);

    addAndMakeVisible (speedLabel);
    addAndMakeVisible (formantLabel);
    addAndMakeVisible (scaleLabel);
    addAndMakeVisible (keyLabel);

    auto comboStyle = [this] (juce::ComboBox& combo)
    {
        combo.setColour (juce::ComboBox::backgroundColourId, juce::Colour::fromRGB (30, 35, 45));
        combo.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        combo.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (combo);
    };

    scaleSelector.addItemList ({
        "Chromatic", "Major", "Minor", "Dorian", "Mixolydian", "Blues", "Pentatonic"
    }, 1);
    comboStyle (scaleSelector);

    keySelector.addItemList (noteNames, 1);
    comboStyle (keySelector);

    centralNoteLabel.setJustificationType (juce::Justification::centred);
    centralNoteLabel.setFont (juce::Font (juce::FontOptions (48.0f, juce::Font::bold)));
    centralNoteLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (centralNoteLabel);

    centralFreqLabel.setJustificationType (juce::Justification::centred);
    centralFreqLabel.setFont (juce::Font (juce::FontOptions (16.0f)));
    centralFreqLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (centralFreqLabel);

    detectedLabel.setJustificationType (juce::Justification::centred);
    detectedLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    detectedLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (detectedLabel);

    auto& vts = processor.getValueTreeState();
    speedAttachment = std::make_unique<SliderAttachment> (vts, "speed", speedSlider);
    formantAttachment = std::make_unique<SliderAttachment> (vts, "formant", formantSlider);
    scaleAttachment = std::make_unique<ComboBoxAttachment> (vts, "scaleMode", scaleSelector);
    keyAttachment = std::make_unique<ComboBoxAttachment> (vts, "scaleRoot", keySelector);

    setSize (windowWidth, windowHeight);
    startTimerHz (30);
}

void ProTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (18, 22, 30));

    // Header
    juce::Rectangle<float> header (0, 0, (float) getWidth(), 50);
    g.setGradientFill (juce::ColourGradient (
        juce::Colour::fromRGB (30, 80, 140), header.getTopLeft(),
        juce::Colour::fromRGB (20, 40, 70), header.getBottomRight(), false));
    g.fillRect (header);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    g.drawText ("ProTune", header, juce::Justification::centred);

    // Pitch meter circle
    auto meterArea = getLocalBounds().toFloat()
                         .withTrimmedTop (140)
                         .withHeight (meterDiameter + 20)
                         .withSizeKeepingCentre (meterDiameter, meterDiameter);

    g.setColour (juce::Colour::fromRGB (25, 30, 40));
    g.fillEllipse (meterArea);

    auto accent = juce::Colour::fromRGB (0, 180, 255);
    g.setColour (accent);
    g.drawEllipse (meterArea.reduced (2), 3.0f);

    // Pitch deviation indicator
    if (displayedDetectedHz > 0.0f && displayedTargetHz > 0.0f)
    {
        auto cents = 1200.0f * std::log2 (displayedDetectedHz / displayedTargetHz);
        cents = juce::jlimit (-50.0f, 50.0f, cents);
        auto angle = juce::degreesToRadians (cents * 2.0f); // +-50 cents = +-100 degrees

        auto centre = meterArea.getCentre();
        auto radius = meterArea.getWidth() * 0.38f;
        auto tip = juce::Point<float> (
            centre.x + radius * std::sin (angle),
            centre.y - radius * std::cos (angle));

        g.setColour (juce::Colours::white);
        g.drawLine (centre.x, centre.y, tip.x, tip.y, 3.0f);

        // Center dot
        g.fillEllipse (centre.x - 4, centre.y - 4, 8, 8);
    }

    // Tick marks
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    auto centre = meterArea.getCentre();
    auto radius = meterArea.getWidth() * 0.46f;
    for (int tick = -2; tick <= 2; ++tick)
    {
        auto angle = juce::degreesToRadians (tick * 40.0f);
        auto outer = juce::Point<float> (centre.x + radius * std::sin (angle), centre.y - radius * std::cos (angle));
        auto inner = juce::Point<float> (centre.x + (radius - 10) * std::sin (angle), centre.y - (radius - 10) * std::cos (angle));
        g.drawLine (outer.x, outer.y, inner.x, inner.y, tick == 0 ? 2.0f : 1.0f);
    }
}

void ProTuneAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (60); // Header

    // Scale and Key selectors row
    auto selectorRow = bounds.removeFromTop (60).reduced (20, 10);
    auto halfWidth = selectorRow.getWidth() / 2;

    auto scaleArea = selectorRow.removeFromLeft (halfWidth).reduced (5, 0);
    scaleLabel.setBounds (scaleArea.removeFromTop (18));
    scaleSelector.setBounds (scaleArea);

    auto keyArea = selectorRow.reduced (5, 0);
    keyLabel.setBounds (keyArea.removeFromTop (18));
    keySelector.setBounds (keyArea);

    // Meter area (labels positioned inside)
    auto meterSection = bounds.removeFromTop (220);
    auto meterArea = meterSection.withSizeKeepingCentre ((int) meterDiameter, (int) meterDiameter);
    auto innerArea = meterArea.reduced (50);
    centralFreqLabel.setBounds (innerArea.removeFromBottom (24));
    centralNoteLabel.setBounds (innerArea);

    // Detected label below meter
    detectedLabel.setBounds (bounds.removeFromTop (30).reduced (20, 0));

    // Sliders row
    auto sliderRow = bounds.removeFromTop (120).reduced (20, 10);
    auto sliderWidth = sliderRow.getWidth() / 2;

    auto speedArea = sliderRow.removeFromLeft (sliderWidth).reduced (10, 0);
    speedLabel.setBounds (speedArea.removeFromTop (20));
    speedSlider.setBounds (speedArea);

    auto formantArea = sliderRow.reduced (10, 0);
    formantLabel.setBounds (formantArea.removeFromTop (20));
    formantSlider.setBounds (formantArea);
}

void ProTuneAudioProcessorEditor::timerCallback()
{
    auto detected = processor.getLastDetectedFrequency();
    auto target = processor.getLastTargetFrequency();

    constexpr float smoothing = 0.25f;
    displayedDetectedHz += (detected - displayedDetectedHz) * smoothing;
    displayedTargetHz += (target - displayedTargetHz) * smoothing;

    if (displayedTargetHz > 20.0f)
    {
        centralNoteLabel.setText (frequencyToNoteName (displayedTargetHz), juce::dontSendNotification);
        centralFreqLabel.setText (juce::String (displayedTargetHz, 1) + " Hz", juce::dontSendNotification);
    }
    else
    {
        centralNoteLabel.setText ("--", juce::dontSendNotification);
        centralFreqLabel.setText ("", juce::dontSendNotification);
    }

    if (displayedDetectedHz > 20.0f)
        detectedLabel.setText ("Input: " + frequencyToNoteName (displayedDetectedHz) + " (" + juce::String (displayedDetectedHz, 1) + " Hz)", juce::dontSendNotification);
    else
        detectedLabel.setText ("No pitch detected", juce::dontSendNotification);

    repaint();
}

juce::String ProTuneAudioProcessorEditor::frequencyToNoteName (float frequency) const
{
    if (frequency <= 0.0f)
        return "--";

    auto midi = 69.0f + 12.0f * std::log2 (frequency / 440.0f);
    auto rounded = (int) std::round (midi);
    auto noteIndex = ((rounded % 12) + 12) % 12;
    auto octave = (rounded / 12) - 1;
    return noteNames[noteIndex] + juce::String (octave);
}

void ProTuneAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGB (0, 180, 255));
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha (0.2f));
    slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setName (name);
    addAndMakeVisible (slider);
}
