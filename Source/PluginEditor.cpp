#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <cmath>

namespace
{
constexpr int windowWidth = 600;
constexpr int windowHeight = 450;

static const juce::StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

ProTuneAudioProcessorEditor::ProTuneAudioProcessorEditor (ProTuneAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    // Bypass button
    bypassButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    bypassButton.setColour (juce::ToggleButton::tickColourId, accentColor);
    addAndMakeVisible (bypassButton);

    // Note display (large)
    noteLabel.setJustificationType (juce::Justification::centred);
    noteLabel.setFont (juce::Font (juce::FontOptions (48.0f, juce::Font::bold)));
    noteLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    noteLabel.setText ("--", juce::dontSendNotification);
    addAndMakeVisible (noteLabel);

    // Frequency display
    frequencyLabel.setJustificationType (juce::Justification::centred);
    frequencyLabel.setFont (juce::Font (juce::FontOptions (16.0f)));
    frequencyLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (frequencyLabel);

    // Input pitch label
    inputPitchLabel.setJustificationType (juce::Justification::centred);
    inputPitchLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    inputPitchLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    inputPitchLabel.setText ("No pitch detected", juce::dontSendNotification);
    addAndMakeVisible (inputPitchLabel);

    // Input Type selector
    inputTypeSelector.addItemList ({ "Soprano", "Alto/Tenor", "Low Male", "Instrument", "Bass Inst." }, 1);
    inputTypeSelector.setColour (juce::ComboBox::backgroundColourId, meterBgColor);
    inputTypeSelector.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    inputTypeSelector.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (inputTypeSelector);
    configureLabel (inputTypeLabel);
    addAndMakeVisible (inputTypeLabel);

    // Key selector
    keySelector.addItemList (noteNames, 1);
    keySelector.setColour (juce::ComboBox::backgroundColourId, meterBgColor);
    keySelector.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    keySelector.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (keySelector);
    configureLabel (keyLabel);
    addAndMakeVisible (keyLabel);

    // Scale selector
    scaleSelector.addItemList ({
        "Chromatic", "Major", "Minor", "Harm Minor", "Mel Minor",
        "Dorian", "Phrygian", "Lydian", "Mixolyd.", "Locrian",
        "Whole Tone", "Blues", "Maj Pent.", "Min Pent.", "Dim", "Custom"
    }, 1);
    scaleSelector.setColour (juce::ComboBox::backgroundColourId, meterBgColor);
    scaleSelector.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    scaleSelector.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (scaleSelector);
    configureLabel (scaleLabel);
    addAndMakeVisible (scaleLabel);

    // Retune Speed (main knob)
    configureSlider (retuneSpeedSlider, " ms");
    retuneSpeedSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    addAndMakeVisible (retuneSpeedSlider);
    configureLabel (retuneSpeedLabel, 14.0f);
    addAndMakeVisible (retuneSpeedLabel);

    // Control strip sliders
    configureSlider (trackingSlider, "%");
    configureSlider (humanizeSlider, "%");
    configureSlider (vibratoSlider);
    configureSlider (transposeSlider, " st");
    configureSlider (detuneSlider, " c");

    addAndMakeVisible (trackingSlider);
    addAndMakeVisible (humanizeSlider);
    addAndMakeVisible (vibratoSlider);
    addAndMakeVisible (transposeSlider);
    addAndMakeVisible (detuneSlider);

    configureLabel (trackingLabel);
    configureLabel (humanizeLabel);
    configureLabel (vibratoLabel);
    configureLabel (transposeLabel);
    configureLabel (detuneLabel);

    addAndMakeVisible (trackingLabel);
    addAndMakeVisible (humanizeLabel);
    addAndMakeVisible (vibratoLabel);
    addAndMakeVisible (transposeLabel);
    addAndMakeVisible (detuneLabel);

    // Parameter attachments
    auto& vts = processor.getValueTreeState();

    bypassAttachment = std::make_unique<ButtonAttachment> (vts, "bypass", bypassButton);
    inputTypeAttachment = std::make_unique<ComboBoxAttachment> (vts, "inputType", inputTypeSelector);
    keyAttachment = std::make_unique<ComboBoxAttachment> (vts, "key", keySelector);
    scaleAttachment = std::make_unique<ComboBoxAttachment> (vts, "scaleMode", scaleSelector);
    retuneSpeedAttachment = std::make_unique<SliderAttachment> (vts, "retuneSpeed", retuneSpeedSlider);
    trackingAttachment = std::make_unique<SliderAttachment> (vts, "tracking", trackingSlider);
    humanizeAttachment = std::make_unique<SliderAttachment> (vts, "humanize", humanizeSlider);
    vibratoAttachment = std::make_unique<SliderAttachment> (vts, "vibrato", vibratoSlider);
    transposeAttachment = std::make_unique<SliderAttachment> (vts, "transpose", transposeSlider);
    detuneAttachment = std::make_unique<SliderAttachment> (vts, "detune", detuneSlider);

    setSize (windowWidth, windowHeight);
    startTimerHz (30);
}

void ProTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background
    g.fillAll (bgColor);

    // Header gradient
    juce::Rectangle<float> header (0, 0, static_cast<float> (getWidth()), 45);
    g.setGradientFill (juce::ColourGradient (
        headerColor1, header.getTopLeft(),
        headerColor2, header.getBottomRight(), false));
    g.fillRect (header);

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    g.drawText ("ProTune", header.withTrimmedRight (100).withTrimmedLeft (20), juce::Justification::centredLeft);

    // Pitch meter area (left side)
    auto meterArea = juce::Rectangle<float> (20, 60, 260, 200);
    g.setColour (meterBgColor);
    g.fillRoundedRectangle (meterArea, 10.0f);
    g.setColour (accentColor.withAlpha (0.5f));
    g.drawRoundedRectangle (meterArea, 10.0f, 2.0f);

    // Cents deviation bar (horizontal)
    auto barArea = meterArea.reduced (20).removeFromBottom (40);
    g.setColour (juce::Colour::fromRGB (40, 45, 55));
    g.fillRoundedRectangle (barArea, 5.0f);

    // Draw tick marks
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    float barCenterX = barArea.getCentreX();
    float barY = barArea.getY();

    for (int tick = -2; tick <= 2; ++tick)
    {
        float x = barCenterX + tick * (barArea.getWidth() / 4.0f) * 0.5f;
        float tickHeight = (tick == 0) ? 15.0f : 10.0f;
        g.drawLine (x, barY + 5, x, barY + 5 + tickHeight, tick == 0 ? 2.0f : 1.0f);
    }

    // Deviation indicator
    if (displayedTargetHz > 0.0f)
    {
        float deviation = juce::jlimit (-50.0f, 50.0f, displayedDeviation);
        float indicatorX = barCenterX + (deviation / 50.0f) * (barArea.getWidth() * 0.4f);

        // Color based on deviation
        juce::Colour indicatorColor;
        float absDeviation = std::abs (deviation);
        if (absDeviation < 10.0f)
            indicatorColor = juce::Colour::fromRGB (0, 255, 100);  // Green
        else if (absDeviation < 25.0f)
            indicatorColor = juce::Colour::fromRGB (255, 255, 0);  // Yellow
        else
            indicatorColor = juce::Colour::fromRGB (255, 80, 80);  // Red

        g.setColour (indicatorColor);
        g.fillEllipse (indicatorX - 8, barArea.getCentreY() - 8, 16, 16);
        g.setColour (juce::Colours::white);
        g.drawEllipse (indicatorX - 8, barArea.getCentreY() - 8, 16, 16, 2.0f);
    }

    // Cents labels
    g.setColour (juce::Colours::grey);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("-50", barArea.withWidth (30), juce::Justification::centredLeft);
    g.drawText ("+50", barArea.withTrimmedLeft (barArea.getWidth() - 30), juce::Justification::centredRight);
    g.drawText ("0", juce::Rectangle<float> (barCenterX - 10, barArea.getBottom() + 2, 20, 14),
                juce::Justification::centred);

    // Control panel background (right side)
    auto controlArea = juce::Rectangle<float> (300, 60, 280, 200);
    g.setColour (meterBgColor.withAlpha (0.5f));
    g.fillRoundedRectangle (controlArea, 10.0f);

    // Bottom control strip background
    auto stripArea = juce::Rectangle<float> (20, 330, static_cast<float> (getWidth() - 40), 100);
    g.setColour (meterBgColor.withAlpha (0.3f));
    g.fillRoundedRectangle (stripArea, 10.0f);
}

void ProTuneAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Header
    auto headerArea = bounds.removeFromTop (45);
    bypassButton.setBounds (headerArea.removeFromRight (100).reduced (10, 8));

    bounds.removeFromTop (15);  // Spacing

    // Main area split: left (pitch meter), right (controls)
    auto mainArea = bounds.removeFromTop (200);
    auto leftArea = mainArea.removeFromLeft (280).reduced (20, 0);
    auto rightArea = mainArea.reduced (20, 0);

    // Pitch meter area - note and frequency displays
    auto noteArea = leftArea.removeFromTop (80);
    noteLabel.setBounds (noteArea);

    auto freqArea = leftArea.removeFromTop (25);
    frequencyLabel.setBounds (freqArea);

    leftArea.removeFromTop (10);
    auto inputArea = leftArea.removeFromTop (20);
    inputPitchLabel.setBounds (inputArea);

    // Right side controls
    auto selectorHeight = 28;
    auto labelHeight = 18;
    auto spacing = 8;

    // Input Type
    inputTypeLabel.setBounds (rightArea.removeFromTop (labelHeight));
    inputTypeSelector.setBounds (rightArea.removeFromTop (selectorHeight).reduced (0, 2));
    rightArea.removeFromTop (spacing);

    // Key and Scale on same row
    auto keyScaleRow = rightArea.removeFromTop (labelHeight + selectorHeight + 4);
    auto halfWidth = keyScaleRow.getWidth() / 2 - 5;

    auto keyArea = keyScaleRow.removeFromLeft (halfWidth);
    keyLabel.setBounds (keyArea.removeFromTop (labelHeight));
    keySelector.setBounds (keyArea.reduced (0, 2));

    keyScaleRow.removeFromLeft (10);
    scaleLabel.setBounds (keyScaleRow.removeFromTop (labelHeight));
    scaleSelector.setBounds (keyScaleRow.reduced (0, 2));

    rightArea.removeFromTop (spacing * 2);

    // Retune Speed knob
    retuneSpeedLabel.setBounds (rightArea.removeFromTop (20));
    retuneSpeedSlider.setBounds (rightArea.removeFromTop (80).withSizeKeepingCentre (100, 80));

    // Bottom control strip
    bounds.removeFromTop (30);
    auto stripArea = bounds.removeFromTop (100).reduced (20, 10);
    auto sliderWidth = stripArea.getWidth() / 5;

    auto makeSliderArea = [&] () -> juce::Rectangle<int>
    {
        return stripArea.removeFromLeft (sliderWidth).reduced (5, 0);
    };

    auto trackingArea = makeSliderArea();
    trackingLabel.setBounds (trackingArea.removeFromTop (18));
    trackingSlider.setBounds (trackingArea);

    auto humanizeArea = makeSliderArea();
    humanizeLabel.setBounds (humanizeArea.removeFromTop (18));
    humanizeSlider.setBounds (humanizeArea);

    auto vibratoArea = makeSliderArea();
    vibratoLabel.setBounds (vibratoArea.removeFromTop (18));
    vibratoSlider.setBounds (vibratoArea);

    auto transposeArea = makeSliderArea();
    transposeLabel.setBounds (transposeArea.removeFromTop (18));
    transposeSlider.setBounds (transposeArea);

    auto detuneArea = makeSliderArea();
    detuneLabel.setBounds (detuneArea.removeFromTop (18));
    detuneSlider.setBounds (detuneArea);
}

void ProTuneAudioProcessorEditor::timerCallback()
{
    auto detected = processor.getLastDetectedFrequency();
    auto target = processor.getLastTargetFrequency();

    // Smooth the display values
    constexpr float smoothing = 0.25f;
    displayedDetectedHz += (detected - displayedDetectedHz) * smoothing;
    displayedTargetHz += (target - displayedTargetHz) * smoothing;

    // Calculate deviation in cents
    if (displayedDetectedHz > 20.0f && displayedTargetHz > 20.0f)
    {
        float deviation = frequencyToDeviation (displayedDetectedHz, displayedTargetHz);
        displayedDeviation += (deviation - displayedDeviation) * smoothing;
    }
    else
    {
        displayedDeviation *= 0.9f;  // Fade out
    }

    // Update note display
    if (displayedTargetHz > 20.0f)
    {
        noteLabel.setText (frequencyToNoteName (displayedTargetHz), juce::dontSendNotification);
        frequencyLabel.setText (juce::String (displayedTargetHz, 1) + " Hz", juce::dontSendNotification);
    }
    else
    {
        noteLabel.setText ("--", juce::dontSendNotification);
        frequencyLabel.setText ("", juce::dontSendNotification);
    }

    // Update input pitch label
    if (displayedDetectedHz > 20.0f)
    {
        juce::String inputText = "Input: " + frequencyToNoteName (displayedDetectedHz) +
                                  " (" + juce::String (displayedDetectedHz, 1) + " Hz)";
        inputPitchLabel.setText (inputText, juce::dontSendNotification);
    }
    else
    {
        inputPitchLabel.setText ("No pitch detected", juce::dontSendNotification);
    }

    repaint();
}

void ProTuneAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColor);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha (0.2f));
    slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setTextValueSuffix (suffix);
}

void ProTuneAudioProcessorEditor::configureLabel (juce::Label& label, float fontSize)
{
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white);
    label.setFont (juce::Font (juce::FontOptions (fontSize, juce::Font::bold)));
}

juce::String ProTuneAudioProcessorEditor::frequencyToNoteName (float frequency) const
{
    if (frequency <= 0.0f)
        return "--";

    float midi = 69.0f + 12.0f * std::log2 (frequency / 440.0f);
    int rounded = static_cast<int> (std::round (midi));
    int noteIndex = ((rounded % 12) + 12) % 12;
    int octave = (rounded / 12) - 1;

    return noteNames[noteIndex] + juce::String (octave);
}

float ProTuneAudioProcessorEditor::frequencyToDeviation (float detected, float target) const
{
    if (detected <= 0.0f || target <= 0.0f)
        return 0.0f;

    return 1200.0f * std::log2 (detected / target);
}
