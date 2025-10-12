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
    configureSlider (speedSlider, "Retune Speed");
    configureSlider (transitionSlider, "Humanize");
    configureSlider (toleranceSlider, "Tolerance");
    configureSlider (formantSlider, "Formant");
    configureSlider (vibratoSlider, "Vibrato");
    configureSlider (rangeLowSlider, "Low Hz");
    configureSlider (rangeHighSlider, "High Hz");

    addAndMakeVisible (chromaticButton);
    addAndMakeVisible (midiButton);
    addAndMakeVisible (forceCorrectionButton);

    addAndMakeVisible (scaleSelector);
    addAndMakeVisible (keySelector);

    addAndMakeVisible (retuneLabel);
    retuneLabel.setJustificationType (juce::Justification::centred);
    retuneLabel.setFont (makeFont (14.0f, juce::Font::bold));
    retuneLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    retuneLabel.setInterceptsMouseClicks (false, false);
    retuneLabel.attachToComponent (&speedSlider, false);

    addAndMakeVisible (humanizeLabel);
    humanizeLabel.setJustificationType (juce::Justification::centred);
    humanizeLabel.setFont (makeFont (14.0f, juce::Font::bold));
    humanizeLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    humanizeLabel.setInterceptsMouseClicks (false, false);
    humanizeLabel.attachToComponent (&transitionSlider, false);

    addAndMakeVisible (scaleLabel);
    scaleLabel.setJustificationType (juce::Justification::centredLeft);
    scaleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    scaleLabel.setInterceptsMouseClicks (false, false);
    scaleLabel.attachToComponent (&scaleSelector, true);

    addAndMakeVisible (keyLabel);
    keyLabel.setJustificationType (juce::Justification::centredLeft);
    keyLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    keyLabel.setInterceptsMouseClicks (false, false);
    keyLabel.attachToComponent (&keySelector, true);

    scaleSelector.addItemList (juce::StringArray { "Chromatic", "Major", "Minor" }, 1);
    scaleSelector.setJustificationType (juce::Justification::centredLeft);
    auto comboBackground = juce::Colour::fromRGB (18, 24, 34);
    scaleSelector.setColour (juce::ComboBox::backgroundColourId, comboBackground);
    scaleSelector.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    scaleSelector.setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
    scaleSelector.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    scaleSelector.setTooltip ("Select the scale mode for automatic correction");

    keySelector.addItemList (juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    keySelector.setJustificationType (juce::Justification::centredLeft);
    keySelector.setColour (juce::ComboBox::backgroundColourId, comboBackground);
    keySelector.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    keySelector.setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
    keySelector.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    keySelector.setTooltip ("Choose the key signature root note");

    detectedLabel.setJustificationType (juce::Justification::centred);
    detectedLabel.setFont (makeFont (16.0f, juce::Font::bold));
    addAndMakeVisible (detectedLabel);

    targetLabel.setJustificationType (juce::Justification::centred);
    targetLabel.setFont (makeFont (16.0f, juce::Font::bold));
    addAndMakeVisible (targetLabel);

    centralNoteLabel.setJustificationType (juce::Justification::centred);
    centralNoteLabel.setFont (makeFont (54.0f, juce::Font::bold));
    centralNoteLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (centralNoteLabel);

    centralFreqLabel.setJustificationType (juce::Justification::centred);
    centralFreqLabel.setFont (makeFont (18.0f));
    centralFreqLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (centralFreqLabel);

    confidenceLabel.setJustificationType (juce::Justification::centred);
    confidenceLabel.setFont (makeFont (14.0f));
    confidenceLabel.setColour (juce::Label::textColourId, juce::Colours::lightsteelblue);
    addAndMakeVisible (confidenceLabel);

    auto& vts = processor.getValueTreeState();

    speedAttachment = std::make_unique<SliderAttachment> (vts, "speed", speedSlider);
    transitionAttachment = std::make_unique<SliderAttachment> (vts, "transition", transitionSlider);
    toleranceAttachment = std::make_unique<SliderAttachment> (vts, "tolerance", toleranceSlider);
    formantAttachment = std::make_unique<SliderAttachment> (vts, "formant", formantSlider);
    vibratoAttachment = std::make_unique<SliderAttachment> (vts, "vibrato", vibratoSlider);
    rangeLowAttachment = std::make_unique<SliderAttachment> (vts, "rangeLow", rangeLowSlider);
    rangeHighAttachment = std::make_unique<SliderAttachment> (vts, "rangeHigh", rangeHighSlider);

    scaleAttachment = std::make_unique<ComboBoxAttachment> (vts, "scaleMode", scaleSelector);
    keyAttachment = std::make_unique<ComboBoxAttachment> (vts, "scaleRoot", keySelector);
    midiAttachment = std::make_unique<ButtonAttachment> (vts, "midiEnabled", midiButton);
    forceCorrectionAttachment = std::make_unique<ButtonAttachment> (vts, "forceCorrection", forceCorrectionButton);

    setSize (820, 520);
    startTimerHz (30);
}

void ProTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto background = juce::Colour::fromRGB (8, 12, 20);
    g.fillAll (background);

    juce::Rectangle<float> header (0.0f, 0.0f, (float) getWidth(), 70.0f);
    juce::ColourGradient gradient (juce::Colour::fromRGB (6, 60, 120), header.getTopLeft(),
                                   juce::Colour::fromRGB (26, 30, 60), header.getBottomRight(), false);
    g.setGradientFill (gradient);
    g.fillRect (header);

    g.setColour (juce::Colours::white);
    g.setFont (makeFont (30.0f, juce::Font::bold));
    g.drawFittedText ("ProTune", header.toNearestInt().reduced (16, 0), juce::Justification::centredLeft, 1);
    g.setFont (makeFont (14.0f));
    g.drawFittedText ("Auto pitch correction inspired by Hildebrandt", header.toNearestInt().reduced (16, 0),
                      juce::Justification::centredRight, 1);

    auto meterBounds = getLocalBounds().toFloat().reduced (24.0f);
    meterBounds.removeFromTop (70.0f);
    auto meterArea = meterBounds.removeFromTop (240.0f).withSizeKeepingCentre (240.0f, 240.0f);

    auto rimColour = juce::Colour::fromRGB (12, 24, 40);
    g.setColour (rimColour);
    g.fillEllipse (meterArea);

    auto accent = juce::Colour::fromRGB (0, 162, 255);
    g.setColour (accent.withAlpha (0.9f));
    g.drawEllipse (meterArea, 4.0f);

    if (displayedConfidence > 0.0f)
    {
        auto confidenceWidth = juce::jmap (displayedConfidence, 0.0f, 1.0f, 2.0f, 9.0f);
        g.setColour (accent.withAlpha (juce::jlimit (0.2f, 0.8f, displayedConfidence + 0.2f)));
        g.drawEllipse (meterArea.reduced (6.0f), confidenceWidth);
    }

    if (displayedDetectedHz > 0.0f && displayedTargetHz > 0.0f)
    {
        auto deltaSemitones = 12.0f * std::log2 (displayedDetectedHz / displayedTargetHz);
        deltaSemitones = juce::jlimit (-2.0f, 2.0f, deltaSemitones);
        auto angle = juce::degreesToRadians (juce::jmap (deltaSemitones, -2.0f, 2.0f, -120.0f, 120.0f));
        auto centre = meterArea.getCentre();
        auto radius = meterArea.getWidth() * 0.42f;
        auto pointer = juce::Point<float> (centre.x + radius * std::sin (angle), centre.y - radius * std::cos (angle));
        g.setColour (juce::Colours::white);
        g.drawLine (centre.x, centre.y, pointer.x, pointer.y, 3.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.25f));
    auto centre = meterArea.getCentre();
    auto radius = meterArea.getWidth() * 0.48f;
    for (int tick = -2; tick <= 2; ++tick)
    {
        auto angle = juce::degreesToRadians ((float) juce::jmap (tick, -2, 2, -120, 120));
        auto outer = juce::Point<float> (centre.x + radius * std::sin (angle), centre.y - radius * std::cos (angle));
        auto inner = juce::Point<float> (centre.x + (radius - 12.0f) * std::sin (angle),
                                         centre.y - (radius - 12.0f) * std::cos (angle));
        g.drawLine (outer.x, outer.y, inner.x, inner.y, 1.2f);
    }
}

void ProTuneAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (24);
    bounds.removeFromTop (70);

    auto meterStrip = bounds.removeFromTop (240);
    auto meterArea = meterStrip.withSizeKeepingCentre (240, 240);
    auto innerArea = meterArea.reduced (70);
    auto freqArea = innerArea.removeFromBottom (48);
    centralNoteLabel.setBounds (innerArea);
    centralFreqLabel.setBounds (freqArea);

    auto confidenceArea = meterStrip.removeFromBottom (30);
    confidenceLabel.setBounds (confidenceArea);

    auto readouts = bounds.removeFromTop (70);
    detectedLabel.setBounds (readouts.removeFromLeft (readouts.getWidth() / 2).reduced (8));
    targetLabel.setBounds (readouts.reduced (8));

    auto controlArea = bounds;
    auto toggleArea = controlArea.removeFromRight (200);
    chromaticButton.setBounds (toggleArea.removeFromTop (36));
    midiButton.setBounds (toggleArea.removeFromTop (36));
    forceCorrectionButton.setBounds (toggleArea.removeFromTop (36));

    juce::FlexBox selectorBox;
    selectorBox.flexDirection = juce::FlexBox::Direction::column;
    selectorBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    selectorBox.alignContent = juce::FlexBox::AlignContent::stretch;

    auto comboMargin = juce::FlexItem::Margin (6.0f, 0.0f, 6.0f, 0.0f);
    selectorBox.items.add (juce::FlexItem (scaleSelector).withMinHeight (40.0f).withFlex (1.0f).withMargin (comboMargin));
    selectorBox.items.add (juce::FlexItem (keySelector).withMinHeight (40.0f).withFlex (1.0f).withMargin (comboMargin));

    auto comboArea = toggleArea.toFloat();
    selectorBox.performLayout (comboArea);

    auto firstRowHeight = juce::jmin (juce::roundToInt (juce::jmax (72.0f, controlArea.getHeight() / 2.0f)), controlArea.getHeight());
    auto firstRow = controlArea.removeFromTop (firstRowHeight);
    auto secondRow = controlArea;

    auto firstColumnWidth = firstRow.getWidth() / 4;
    speedSlider.setBounds (firstRow.removeFromLeft (firstColumnWidth).reduced (12, 6));
    transitionSlider.setBounds (firstRow.removeFromLeft (firstColumnWidth).reduced (12, 6));
    toleranceSlider.setBounds (firstRow.removeFromLeft (firstColumnWidth).reduced (12, 6));
    vibratoSlider.setBounds (firstRow.reduced (12, 6));

    auto secondColumnWidth = secondRow.getWidth() / 3;
    formantSlider.setBounds (secondRow.removeFromLeft (secondColumnWidth).reduced (12, 6));
    rangeLowSlider.setBounds (secondRow.removeFromLeft (secondColumnWidth).reduced (12, 6));
    rangeHighSlider.setBounds (secondRow.reduced (12, 6));
}

void ProTuneAudioProcessorEditor::timerCallback()
{
    auto detected = processor.getLastDetectedFrequency();
    auto target = processor.getLastTargetFrequency();

    const auto smooth = [] (float current, float targetValue, float factor)
    {
        return current + (targetValue - current) * factor;
    };

    constexpr float smoothing = 0.2f;

    displayedDetectedHz = smooth (displayedDetectedHz, juce::jmax (0.0f, detected), smoothing);
    displayedTargetHz = smooth (displayedTargetHz, juce::jmax (0.0f, target), smoothing);

    auto limitedConfidence = juce::jlimit (0.0f, 1.0f, processor.getLastDetectionConfidence());
    displayedConfidence = smooth (displayedConfidence, limitedConfidence, smoothing);

    auto detectedText = juce::String (displayedDetectedHz > 0.0f ? juce::String (displayedDetectedHz, 2) + " Hz" : "--")
                        + " (" + frequencyToNoteName (displayedDetectedHz) + ")";
    auto targetText = juce::String (displayedTargetHz > 0.0f ? juce::String (displayedTargetHz, 2) + " Hz" : "--")
                      + " (" + frequencyToNoteName (displayedTargetHz) + ")";

    detectedLabel.setText ("Detected: " + detectedText, juce::dontSendNotification);
    targetLabel.setText ("Target: " + targetText, juce::dontSendNotification);

    if (target > 0.0f)
    {
        centralNoteLabel.setText (frequencyToNoteName (target), juce::dontSendNotification);
        centralFreqLabel.setText (juce::String (target, 1) + " Hz", juce::dontSendNotification);
    }
    else
    {
        centralNoteLabel.setText ("--", juce::dontSendNotification);
        centralFreqLabel.setText ("-- Hz", juce::dontSendNotification);
    }

    confidenceLabel.setText ("Confidence " + juce::String (limitedConfidence * 100.0f, 1) + "%", juce::dontSendNotification);

    repaint();
}

void ProTuneAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& name, bool isVertical)
{
    slider.setSliderStyle (isVertical ? juce::Slider::RotaryVerticalDrag : juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha (0.18f));
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGB (0, 162, 255));
    slider.setColour (juce::Slider::trackColourId, juce::Colour::fromRGB (0, 140, 220));
    slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setName (name);
    slider.setTooltip (name);
    addAndMakeVisible (slider);
}
