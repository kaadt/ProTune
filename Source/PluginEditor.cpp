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

static const juce::StringArray sharpNoteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const juce::StringArray flatNoteNames  { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
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

    addAndMakeVisible (midiButton);
    addAndMakeVisible (forceCorrectionButton);

    addAndMakeVisible (scaleSelector);
    addAndMakeVisible (keySelector);
    addAndMakeVisible (enharmonicSelector);

    for (size_t i = 0; i < noteButtons.size(); ++i)
    {
        auto& button = noteButtons[i];
        button.setClickingTogglesState (true);
        button.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
        button.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::dimgrey);
        button.onClick = [this, i]() { handleNoteToggle ((int) i); };
        addAndMakeVisible (button);
    }

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

    auto initialiseSelectionLabel = [] (juce::Label& label)
    {
        label.setJustificationType (juce::Justification::centredLeft);
        label.setColour (juce::Label::textColourId, juce::Colours::white);
        label.setFont (makeFont (12.0f, juce::Font::bold));
        label.setInterceptsMouseClicks (false, false);
    };

    addAndMakeVisible (scaleLabel);
    initialiseSelectionLabel (scaleLabel);

    addAndMakeVisible (keyLabel);
    initialiseSelectionLabel (keyLabel);

    addAndMakeVisible (enharmonicLabel);
    initialiseSelectionLabel (enharmonicLabel);

    scaleSelector.addItemList (juce::StringArray {
        "Chromatic",
        "Major",
        "Natural Minor",
        "Harmonic Minor",
        "Melodic Minor",
        "Dorian",
        "Phrygian",
        "Lydian",
        "Mixolydian",
        "Locrian",
        "Whole Tone",
        "Blues",
        "Major Pentatonic",
        "Minor Pentatonic",
        "Diminished",
        "Custom"
    }, 1);
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

    enharmonicSelector.addItemList (juce::StringArray { "Auto", "Sharps", "Flats" }, 1);
    enharmonicSelector.setJustificationType (juce::Justification::centredLeft);
    enharmonicSelector.setColour (juce::ComboBox::backgroundColourId, comboBackground);
    enharmonicSelector.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    enharmonicSelector.setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
    enharmonicSelector.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    enharmonicSelector.setTooltip ("Select whether to display sharps, flats, or automatic enharmonics");

    scaleSelector.onChange = [this] { handleScaleSelectorChanged(); };
    keySelector.onChange = [this] { handleKeySelectorChanged(); };
    enharmonicSelector.onChange = [this] { refreshScaleDisplay(); };

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
    enharmonicAttachment = std::make_unique<ComboBoxAttachment> (vts, "enharmonicPref", enharmonicSelector);
    midiAttachment = std::make_unique<ButtonAttachment> (vts, "midiEnabled", midiButton);
    forceCorrectionAttachment = std::make_unique<ButtonAttachment> (vts, "forceCorrection", forceCorrectionButton);

    scaleSelector.onChange();
    refreshScaleDisplay();
    updateNoteToggleLabels();
    updateKeySelectorLabels();

    setSize (820, 640);
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

    constexpr float selectionStripHeight = 72.0f;
    auto selectionStrip = meterBounds.removeFromTop (selectionStripHeight);
    auto selectionBackground = juce::Colour::fromRGB (16, 22, 32);
    g.setColour (selectionBackground);
    g.fillRoundedRectangle (selectionStrip, 6.0f);
    g.setColour (selectionBackground.brighter (0.1f));
    g.drawRoundedRectangle (selectionStrip, 6.0f, 1.2f);

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

    constexpr int selectionStripHeight = 72;
    auto selectionStrip = bounds.removeFromTop (selectionStripHeight);
    auto selectionContent = selectionStrip.reduced (16, 10);
    auto labelHeight = 18;
    auto comboHeight = 32;

    constexpr int minScaleWidth = 150;
    constexpr int minKeyWidth = 120;
    constexpr int minEnhWidth = 120;
    constexpr int minTotalWidth = minScaleWidth + minKeyWidth + minEnhWidth;

    const auto totalSelectionWidth = selectionContent.getWidth();
    int scaleWidth = 0;
    int keyWidth = 0;
    int enhWidth = 0;

    if (totalSelectionWidth >= minTotalWidth)
    {
        scaleWidth = juce::jmax (minScaleWidth, totalSelectionWidth / 3);
        scaleWidth = juce::jmin (scaleWidth, totalSelectionWidth - (minKeyWidth + minEnhWidth));

        auto remaining = totalSelectionWidth - scaleWidth;
        keyWidth = juce::jmax (minKeyWidth, remaining / 2);
        keyWidth = juce::jmin (keyWidth, remaining - minEnhWidth);
        enhWidth = remaining - keyWidth;
    }
    else
    {
        const auto compression = totalSelectionWidth / (float) minTotalWidth;
        scaleWidth = juce::roundToInt (minScaleWidth * compression);
        keyWidth = juce::roundToInt (minKeyWidth * compression);
        enhWidth = totalSelectionWidth - scaleWidth - keyWidth;

        if (enhWidth < 0)
        {
            enhWidth = 0;
            keyWidth = juce::jmax (0, totalSelectionWidth - scaleWidth);
        }
    }

    auto scaleColumn = selectionContent.removeFromLeft (scaleWidth).reduced (4, 0);
    auto keyColumn = selectionContent.removeFromLeft (keyWidth).reduced (4, 0);
    auto enhColumn = selectionContent.reduced (4, 0);

    scaleLabel.setBounds (scaleColumn.removeFromTop (labelHeight));
    scaleSelector.setBounds (scaleColumn.removeFromTop (comboHeight));

    keyLabel.setBounds (keyColumn.removeFromTop (labelHeight));
    keySelector.setBounds (keyColumn.removeFromTop (comboHeight));

    enharmonicLabel.setBounds (enhColumn.removeFromTop (labelHeight));
    enharmonicSelector.setBounds (enhColumn.removeFromTop (comboHeight));

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
    auto rightColumn = controlArea.removeFromRight (240);

    midiButton.setBounds (rightColumn.removeFromTop (32));
    forceCorrectionButton.setBounds (rightColumn.removeFromTop (32));

    auto noteArea = rightColumn.reduced (4);
    auto rowHeight = noteArea.getHeight() / 4;
    auto noteColumnWidth = noteArea.getWidth() / 3;

    for (int row = 0; row < 4; ++row)
    {
        auto rowBounds = noteArea.removeFromTop (rowHeight);
        auto rowSlice = rowBounds;

        for (int col = 0; col < 3; ++col)
        {
            auto index = row * 3 + col;
            if (index >= (int) noteButtons.size())
                break;

            auto cell = rowSlice.removeFromLeft (noteColumnWidth);
            noteButtons[(size_t) index].setBounds (cell.reduced (4));
        }
    }

    auto firstRowHeight = juce::jmax (80, juce::roundToInt (controlArea.getHeight() * 0.55f));
    auto firstRow = controlArea.removeFromTop (firstRowHeight);
    auto secondRow = controlArea;

    auto firstColumnWidth = firstRow.getWidth() / 4;
    speedSlider.setBounds (firstRow.removeFromLeft (firstColumnWidth).reduced (12, 10));
    transitionSlider.setBounds (firstRow.removeFromLeft (firstColumnWidth).reduced (12, 10));
    toleranceSlider.setBounds (firstRow.removeFromLeft (firstColumnWidth).reduced (12, 10));
    vibratoSlider.setBounds (firstRow.reduced (12, 10));

    auto secondColumnWidth = secondRow.getWidth() / 3;
    formantSlider.setBounds (secondRow.removeFromLeft (secondColumnWidth).reduced (12, 10));
    rangeLowSlider.setBounds (secondRow.removeFromLeft (secondColumnWidth).reduced (12, 10));
    rangeHighSlider.setBounds (secondRow.reduced (12, 10));
}

void ProTuneAudioProcessorEditor::timerCallback()
{
    refreshScaleDisplay();

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
                        + " (" + frequencyToDisplayName (displayedDetectedHz) + ")";
    auto targetText = juce::String (displayedTargetHz > 0.0f ? juce::String (displayedTargetHz, 2) + " Hz" : "--")
                      + " (" + frequencyToDisplayName (displayedTargetHz) + ")";

    detectedLabel.setText ("Detected: " + detectedText, juce::dontSendNotification);
    targetLabel.setText ("Target: " + targetText, juce::dontSendNotification);

    if (target > 0.0f)
    {
        centralNoteLabel.setText (frequencyToDisplayName (target), juce::dontSendNotification);
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

void ProTuneAudioProcessorEditor::refreshScaleDisplay()
{
    auto mask = processor.getEffectiveScaleMask();
    if (mask != lastDisplayedMask)
    {
        updateNoteToggleStates (mask);
        lastDisplayedMask = mask;
    }

    auto currentPref = processor.getEnharmonicPreference();
    auto preferFlats = processor.shouldUseFlatsForDisplay();
    if (currentPref != lastEnharmonicPref || preferFlats != lastPreferFlats)
    {
        lastEnharmonicPref = currentPref;
        lastPreferFlats = preferFlats;
        updateNoteToggleLabels();
        updateKeySelectorLabels();
    }
}

void ProTuneAudioProcessorEditor::updateNoteToggleStates (ProTuneAudioProcessor::AllowedMask mask)
{
    juce::ScopedValueSetter<bool> guard (isUpdatingNoteButtons, true);

    for (size_t i = 0; i < noteButtons.size(); ++i)
    {
        auto enabled = (mask & (ProTuneAudioProcessor::AllowedMask) (1u << (int) i)) != 0;
        noteButtons[i].setToggleState (enabled, juce::dontSendNotification);
    }
}

void ProTuneAudioProcessorEditor::updateNoteToggleLabels()
{
    for (size_t i = 0; i < noteButtons.size(); ++i)
    {
        auto name = pitchClassName ((int) i);
        noteButtons[i].setButtonText (name);
        noteButtons[i].setTooltip ("Allow note " + name);
    }
}

void ProTuneAudioProcessorEditor::updateKeySelectorLabels()
{
    const auto& names = processor.shouldUseFlatsForDisplay() ? flatNoteNames : sharpNoteNames;

    for (int i = 0; i < keySelector.getNumItems(); ++i)
        keySelector.changeItemText (i + 1, names[i]);
}

void ProTuneAudioProcessorEditor::handleScaleSelectorChanged()
{
    if (isUpdatingScaleControls)
        return;

    juce::ScopedValueSetter<bool> guard (isUpdatingScaleControls, true);

    auto settings = processor.getScaleSettings();
    if (settings.type != ProTuneAudioProcessor::ScaleSettings::Type::Custom)
        processor.setScaleMaskFromUI (processor.getEffectiveScaleMask());

    refreshScaleDisplay();
}

void ProTuneAudioProcessorEditor::handleKeySelectorChanged()
{
    if (isUpdatingScaleControls)
        return;

    juce::ScopedValueSetter<bool> guard (isUpdatingScaleControls, true);

    auto settings = processor.getScaleSettings();
    if (settings.type != ProTuneAudioProcessor::ScaleSettings::Type::Custom)
        processor.setScaleMaskFromUI (processor.getEffectiveScaleMask());

    refreshScaleDisplay();
}

void ProTuneAudioProcessorEditor::handleNoteToggle (int pitchClass)
{
    if (isUpdatingNoteButtons)
        return;

    auto mask = processor.getEffectiveScaleMask();
    auto bit = (ProTuneAudioProcessor::AllowedMask) (1u << positiveModulo (pitchClass, 12));

    if (noteButtons[(size_t) positiveModulo (pitchClass, 12)].getToggleState())
        mask |= bit;
    else
        mask &= ~bit;

    if (mask == 0)
    {
        mask |= bit;
        juce::ScopedValueSetter<bool> guardButtons (isUpdatingNoteButtons, true);
        noteButtons[(size_t) positiveModulo (pitchClass, 12)].setToggleState (true, juce::dontSendNotification);
    }

    processor.setScaleMaskFromUI (mask);

    auto settings = processor.getScaleSettings();
    if (settings.type != ProTuneAudioProcessor::ScaleSettings::Type::Custom)
        processor.setScaleModeFromUI (ProTuneAudioProcessor::ScaleSettings::Type::Custom);

    lastDisplayedMask = mask;
    refreshScaleDisplay();
}

juce::String ProTuneAudioProcessorEditor::pitchClassName (int pitchClass) const
{
    auto index = positiveModulo (pitchClass, 12);
    auto useFlats = processor.shouldUseFlatsForDisplay();
    return useFlats ? flatNoteNames[index] : sharpNoteNames[index];
}

juce::String ProTuneAudioProcessorEditor::frequencyToDisplayName (float frequency) const
{
    if (frequency <= 0.0f)
        return "--";

    constexpr float referenceFrequency = 440.0f;
    constexpr int referenceMidi = 69;

    auto midi = referenceMidi + 12.0f * std::log2 (frequency / referenceFrequency);
    auto rounded = (int) std::round (midi);
    auto noteIndex = positiveModulo (rounded, 12);
    auto octave = (int) std::floor (rounded / 12.0f) - 1;
    return pitchClassName (noteIndex) + juce::String (octave);
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
