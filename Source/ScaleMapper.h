#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>

/**
 * Scale and Note Mapper
 *
 * Maps detected pitch to target note based on musical scale, key, and user settings.
 * Supports 16 preset scales plus custom scales via 12-bit bitmask.
 */
class ScaleMapper
{
public:
    using NoteMask = uint16_t;  // 12-bit mask for pitch classes

    enum class ScaleType
    {
        Chromatic,
        Major,
        NaturalMinor,
        HarmonicMinor,
        MelodicMinor,
        Dorian,
        Phrygian,
        Lydian,
        Mixolydian,
        Locrian,
        WholeTone,
        Blues,
        MajorPentatonic,
        MinorPentatonic,
        Diminished,
        Custom
    };

    struct Settings
    {
        ScaleType type = ScaleType::Chromatic;
        int root = 0;              // 0-11 (C=0, C#=1, ..., B=11)
        NoteMask customMask = 0x0FFF;  // For custom scale
        int transpose = 0;         // -24 to +24 semitones
        float detune = 0.0f;       // -100 to +100 cents
    };

    struct MapResult
    {
        float targetMidi = 0.0f;       // Target MIDI note with detune
        float targetFrequency = 0.0f;  // Target frequency in Hz
        int targetNoteNumber = 0;      // Integer MIDI note
        float deviationCents = 0.0f;   // How far input was from target
    };

    ScaleMapper();
    ~ScaleMapper() = default;

    /**
     * Map a detected frequency to the target note.
     *
     * @param detectedFrequency Input frequency in Hz
     * @param midiOverride Optional MIDI note override (-1 if not used)
     * @return Mapping result with target frequency
     */
    MapResult map (float detectedFrequency, int midiOverride = -1);

    void setSettings (const Settings& newSettings);
    const Settings& getSettings() const noexcept { return settings; }

    // Utilities
    static NoteMask getMaskForScale (ScaleType type, int root);
    static float midiToFrequency (float midiNote);
    static float frequencyToMidi (float frequency);
    static juce::String midiToNoteName (int midiNote, bool useFlats = false);

private:
    int snapToScale (float midiNote);
    NoteMask currentMask = 0x0FFF;
    Settings settings;

    // Reference tuning
    static constexpr float referenceA4 = 440.0f;
    static constexpr int referenceNote = 69;  // A4 = MIDI 69

    // Scale patterns (intervals from root)
    static const std::array<int, 7> majorPattern;
    static const std::array<int, 7> naturalMinorPattern;
    static const std::array<int, 7> harmonicMinorPattern;
    static const std::array<int, 7> melodicMinorPattern;
    static const std::array<int, 7> dorianPattern;
    static const std::array<int, 7> phrygianPattern;
    static const std::array<int, 7> lydianPattern;
    static const std::array<int, 7> mixolydianPattern;
    static const std::array<int, 7> locrianPattern;
    static const std::array<int, 6> wholeTonePattern;
    static const std::array<int, 6> bluesPattern;
    static const std::array<int, 5> majorPentatonicPattern;
    static const std::array<int, 5> minorPentatonicPattern;
    static const std::array<int, 8> diminishedPattern;
};
