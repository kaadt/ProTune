#include "ScaleMapper.h"
#include <cmath>

// Scale patterns (intervals from root)
const std::array<int, 7> ScaleMapper::majorPattern = { 0, 2, 4, 5, 7, 9, 11 };
const std::array<int, 7> ScaleMapper::naturalMinorPattern = { 0, 2, 3, 5, 7, 8, 10 };
const std::array<int, 7> ScaleMapper::harmonicMinorPattern = { 0, 2, 3, 5, 7, 8, 11 };
const std::array<int, 7> ScaleMapper::melodicMinorPattern = { 0, 2, 3, 5, 7, 9, 11 };
const std::array<int, 7> ScaleMapper::dorianPattern = { 0, 2, 3, 5, 7, 9, 10 };
const std::array<int, 7> ScaleMapper::phrygianPattern = { 0, 1, 3, 5, 7, 8, 10 };
const std::array<int, 7> ScaleMapper::lydianPattern = { 0, 2, 4, 6, 7, 9, 11 };
const std::array<int, 7> ScaleMapper::mixolydianPattern = { 0, 2, 4, 5, 7, 9, 10 };
const std::array<int, 7> ScaleMapper::locrianPattern = { 0, 1, 3, 5, 6, 8, 10 };
const std::array<int, 6> ScaleMapper::wholeTonePattern = { 0, 2, 4, 6, 8, 10 };
const std::array<int, 6> ScaleMapper::bluesPattern = { 0, 3, 5, 6, 7, 10 };
const std::array<int, 5> ScaleMapper::majorPentatonicPattern = { 0, 2, 4, 7, 9 };
const std::array<int, 5> ScaleMapper::minorPentatonicPattern = { 0, 3, 5, 7, 10 };
const std::array<int, 8> ScaleMapper::diminishedPattern = { 0, 2, 3, 5, 6, 8, 9, 11 };

ScaleMapper::ScaleMapper()
{
    currentMask = getMaskForScale (settings.type, settings.root);
}

void ScaleMapper::setSettings (const Settings& newSettings)
{
    settings = newSettings;

    if (settings.type == ScaleType::Custom)
        currentMask = settings.customMask & 0x0FFF;
    else
        currentMask = getMaskForScale (settings.type, settings.root);

    // Ensure mask is valid
    if (currentMask == 0)
        currentMask = 0x0FFF;  // Fall back to chromatic
}

ScaleMapper::MapResult ScaleMapper::map (float detectedFrequency, int midiOverride)
{
    MapResult result;

    if (detectedFrequency <= 0.0f && midiOverride < 0)
        return result;

    float inputMidi;

    if (midiOverride >= 0)
    {
        // MIDI override takes precedence
        inputMidi = static_cast<float> (midiOverride);
    }
    else
    {
        inputMidi = frequencyToMidi (detectedFrequency);
    }

    // Apply transpose
    inputMidi += static_cast<float> (settings.transpose);

    // Snap to scale
    int snappedNote = snapToScale (inputMidi);
    result.targetNoteNumber = snappedNote;

    // Calculate deviation before snapping
    result.deviationCents = (inputMidi - static_cast<float> (snappedNote)) * 100.0f;

    // Apply detune
    result.targetMidi = static_cast<float> (snappedNote) + settings.detune / 100.0f;
    result.targetFrequency = midiToFrequency (result.targetMidi);

    return result;
}

int ScaleMapper::snapToScale (float midiNote)
{
    int rounded = static_cast<int> (std::round (midiNote));

    // Search for nearest note in scale
    int bestNote = rounded;
    float bestDistance = std::numeric_limits<float>::max();

    for (int delta = -12; delta <= 12; ++delta)
    {
        int candidate = rounded + delta;
        int pitchClass = ((candidate % 12) + 12) % 12;

        // Check if this pitch class is in the scale
        if ((currentMask & (1u << pitchClass)) != 0)
        {
            float distance = std::abs (static_cast<float> (candidate) - midiNote);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestNote = candidate;
            }
        }
    }

    return bestNote;
}

ScaleMapper::NoteMask ScaleMapper::getMaskForScale (ScaleType type, int root)
{
    NoteMask mask = 0;

    auto addPattern = [&mask, root] (const auto& pattern)
    {
        for (int interval : pattern)
        {
            int pitchClass = (root + interval) % 12;
            mask |= static_cast<NoteMask> (1u << pitchClass);
        }
    };

    switch (type)
    {
        case ScaleType::Chromatic:
            return 0x0FFF;  // All notes

        case ScaleType::Major:
            addPattern (majorPattern);
            break;

        case ScaleType::NaturalMinor:
            addPattern (naturalMinorPattern);
            break;

        case ScaleType::HarmonicMinor:
            addPattern (harmonicMinorPattern);
            break;

        case ScaleType::MelodicMinor:
            addPattern (melodicMinorPattern);
            break;

        case ScaleType::Dorian:
            addPattern (dorianPattern);
            break;

        case ScaleType::Phrygian:
            addPattern (phrygianPattern);
            break;

        case ScaleType::Lydian:
            addPattern (lydianPattern);
            break;

        case ScaleType::Mixolydian:
            addPattern (mixolydianPattern);
            break;

        case ScaleType::Locrian:
            addPattern (locrianPattern);
            break;

        case ScaleType::WholeTone:
            addPattern (wholeTonePattern);
            break;

        case ScaleType::Blues:
            addPattern (bluesPattern);
            break;

        case ScaleType::MajorPentatonic:
            addPattern (majorPentatonicPattern);
            break;

        case ScaleType::MinorPentatonic:
            addPattern (minorPentatonicPattern);
            break;

        case ScaleType::Diminished:
            addPattern (diminishedPattern);
            break;

        case ScaleType::Custom:
        default:
            return 0x0FFF;
    }

    return mask;
}

float ScaleMapper::midiToFrequency (float midiNote)
{
    return referenceA4 * std::pow (2.0f, (midiNote - static_cast<float> (referenceNote)) / 12.0f);
}

float ScaleMapper::frequencyToMidi (float frequency)
{
    if (frequency <= 0.0f)
        return 0.0f;

    return static_cast<float> (referenceNote) + 12.0f * std::log2 (frequency / referenceA4);
}

juce::String ScaleMapper::midiToNoteName (int midiNote, bool useFlats)
{
    static const char* sharpNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flatNames[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

    int pitchClass = ((midiNote % 12) + 12) % 12;
    int octave = (midiNote / 12) - 1;

    const char* name = useFlats ? flatNames[pitchClass] : sharpNames[pitchClass];
    return juce::String (name) + juce::String (octave);
}
