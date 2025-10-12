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
    configureSlider (rangeLowSlider, "Range Low", false);
    configureSlider (rangeHighSlider, "Range High", false);

    toleranceSlider.setTextValueSuffix (" cents");
    rangeLowSlider.setTextValueSuffix (" Hz");
    rangeHighSlider.setTextValueSuffix (" Hz");

    auto configureLabel = [] (juce::Label& label)
    {
        label.setJustificationType (juce::Justification::centredLeft);
        label.setFont (makeFont (13.0f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    };

    configureLabel (scaleLabel);
    addAndMakeVisible (scaleLabel);
    configureLabel (keyLabel);
    addAndMakeVisible (keyLabel);

    scaleSelector.addItemList ({ "Chromatic", "Major", "Minor" }, 1);
    scaleSelector.setJustificationType (juce::Justification::centred);
    scaleSelector.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    scaleSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colours::dimgrey.withAlpha (0.35f));
    scaleSelector.setSelectedId (1);
    addAndMakeVisible (scaleSelector);

    keySelector.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    keySelector.setJustificationType (juce::Justification::centred);
    keySelector.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    keySelector.setColour (juce::ComboBox::backgroundColourId, juce::Colours::dimgrey.withAlpha (0.35f));
    keySelector.setSelectedId (1);
    addAndMakeVisible (keySelector);

    addAndMakeVisible (midiButton);
    addAndMakeVisible (forceCorrectionButton);

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

    setSize (820, 480);
    startTimerHz (30);
}

void ProTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto background = juce::Colour::fromRGB (8, 12, 20);
    g.fillAll (background);

    constexpr float headerHeight = 70.0f;
    constexpr float margin = 24.0f;
    constexpr float meterColumnWidth = 300.0f;
    constexpr float meterSectionHeight = 280.0f;
    constexpr float meterDiameter = 240.0f;

    juce::Rectangle<float> header (0.0f, 0.0f, (float) getWidth(), headerHeight);
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

    auto content = getLocalBounds().toFloat().reduced (margin);
    content.removeFromTop (headerHeight);

    auto leftColumn = content.removeFromLeft (meterColumnWidth);
    auto meterArea = leftColumn.removeFromTop (meterSectionHeight).withSizeKeepingCentre (meterDiameter, meterDiameter);

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

    if (displayedDetectedHz > 0.0f && displayedTargetHz > 0.0f && displayedConfidence > 0.05f)
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
    constexpr int margin = 24;
    constexpr int headerHeight = 70;
    constexpr int meterColumnWidth = 300;
    constexpr int meterSectionHeight = 280;
    constexpr int meterDiameter = 240;

    auto bounds = getLocalBounds().reduced (margin);
    bounds.removeFromTop (headerHeight);

    auto leftColumn = bounds.removeFromLeft (meterColumnWidth);
    auto meterSection = leftColumn.removeFromTop (meterSectionHeight);
    auto meterArea = meterSection.withSizeKeepingCentre (meterDiameter, meterDiameter).toNearestInt();
    auto innerArea = meterArea.reduced (70);
    auto freqArea = innerArea.removeFromBottom (52);
    centralNoteLabel.setBounds (innerArea);
    centralFreqLabel.setBounds (freqArea);

    auto confidenceArea = leftColumn.removeFromTop (32);
    confidenceLabel.setBounds (confidenceArea.reduced (4, 0));

    auto readouts = leftColumn;
    auto detectedArea = readouts.removeFromTop (readouts.getHeight() / 2);
    detectedLabel.setBounds (detectedArea.reduced (6));
    targetLabel.setBounds (readouts.reduced (6));

    auto controlColumn = bounds;

    auto comboRow = controlColumn.removeFromTop (70);
    auto keyArea = comboRow.removeFromLeft (comboRow.getWidth() / 2);
    auto scaleArea = comboRow;

    auto keyLabelArea = keyArea.removeFromTop (24);
    keyLabel.setBounds (keyLabelArea);
    keySelector.setBounds (keyArea.reduced (8, 0));

    auto scaleLabelArea = scaleArea.removeFromTop (24);
    scaleLabel.setBounds (scaleLabelArea);
    scaleSelector.setBounds (scaleArea.reduced (8, 0));

    auto toggleArea = controlColumn.removeFromBottom (110).reduced (8, 4);
    midiButton.setBounds (toggleArea.removeFromTop (36));
    forceCorrectionButton.setBounds (toggleArea.removeFromTop (36));

    auto knobArea = controlColumn;
    auto firstRow = knobArea.removeFromTop (knobArea.getHeight() / 2);
    auto secondRow = knobArea;

    auto firstColumnWidth = firstRow.getWidth() / 4;
    speedSlider.setBounds (firstRow.removeFromLeft (firstColumnWidth).reduced (12));
    transitionSlider.setBounds (firstRow.removeFromLeft (firstColumnWidth).reduced (12));
    vibratoSlider.setBounds (firstRow.removeFromLeft (firstColumnWidth).reduced (12));
    formantSlider.setBounds (firstRow.reduced (12));

    auto secondColumnWidth = secondRow.getWidth() / 3;
    toleranceSlider.setBounds (secondRow.removeFromLeft (secondColumnWidth).reduced (12));
    rangeLowSlider.setBounds (secondRow.removeFromLeft (secondColumnWidth).reduced (12));
    rangeHighSlider.setBounds (secondRow.reduced (12));
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

    auto rawConfidence = juce::jlimit (0.0f, 1.0f, processor.getLastDetectionConfidence());
    displayedConfidence = smooth (displayedConfidence, rawConfidence, smoothing);

    const bool hasLock = rawConfidence > 0.05f;
    const float detectedForUi = hasLock ? juce::jmax (0.0f, detected) : 0.0f;
    const float targetForUi = hasLock ? juce::jmax (0.0f, target) : 0.0f;

    displayedDetectedHz = smooth (displayedDetectedHz, detectedForUi, smoothing);
    displayedTargetHz = smooth (displayedTargetHz, targetForUi, smoothing);

    auto makeReadout = [] (float frequency)
    {
        if (frequency <= 0.0f)
            return juce::String ("-- (--)");

        return juce::String (frequency, 2) + " Hz (" + frequencyToNoteName (frequency) + ")";
    };

    detectedLabel.setText ("Detected: " + makeReadout (displayedDetectedHz), juce::dontSendNotification);
    targetLabel.setText ("Target: " + makeReadout (displayedTargetHz), juce::dontSendNotification);

    if (displayedTargetHz > 0.0f && displayedConfidence > 0.05f)
    {
        centralNoteLabel.setText (frequencyToNoteName (displayedTargetHz), juce::dontSendNotification);
        centralFreqLabel.setText (juce::String (displayedTargetHz, 1) + " Hz", juce::dontSendNotification);
    }
    else
    {
        centralNoteLabel.setText ("--", juce::dontSendNotification);
        centralFreqLabel.setText ("-- Hz", juce::dontSendNotification);
    }

    if (displayedConfidence <= 0.05f)
        confidenceLabel.setText ("Confidence --", juce::dontSendNotification);
    else
        confidenceLabel.setText ("Confidence " + juce::String (displayedConfidence * 100.0f, 1) + "%",
                                 juce::dontSendNotification);

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
