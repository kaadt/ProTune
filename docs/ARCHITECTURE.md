# ProTune Architecture Overview

## High-Level Structure
- **ProTuneAudioProcessor** orchestrates plugin lifecycle, buffer coordination, and parameter exposure.
- **PitchCorrectionEngine** owns the DSP pipeline, handling detection, target selection, and smoothing while remaining independent from host-facing concerns.
- **ProTuneAudioProcessorEditor** provides the GUI, binding controls to the processor via JUCE attachments and presenting live pitch feedback.

## I/O and Host Integration
- Registers stereo input/output buses and clears any surplus outputs each block.
- Executes the engine within `processBlock`, forwarding MIDI messages and caching the latest detected and target pitches for GUI consumption.
- Relies on `juce::AudioProcessorValueTreeState` for state management and automation, sharing the same state with GUI attachments.

## Supported Plugin Formats
- CMake currently configures a VST3 target plus a JUCE standalone build.
- Additional formats (e.g., AU, CLAP) can be enabled by extending the `FORMATS` list passed to `juce_add_plugin`.

## Real-Time Processing Model
- The engine preallocates FFT buffers, smoothing helpers, and a dry signal copy to avoid runtime allocations.
- GUI-controlled smoothing parameters are translated into coefficients outside the audio callback, keeping per-sample work lock-free and branch-light.

## Pitch Analysis Pipeline
1. Incoming audio fills a circular analysis buffer.
2. Blocks are windowed with a Hann function and transformed with a 4096-point FFT.
3. Peak bins are refined via parabolic interpolation, constrained to the configured vocal range, and smoothed for stability.
4. A `LinearSmoothedValue` reduces jitter before correction policies run.

## Correction Policy and Musical Mapping
- Converts detected pitch to MIDI space, optionally overridden by held MIDI notes.
- Applies chromatic, major, or minor snapping anchored to the selected key with tolerance gating so near-perfect intonation passes through unchanged.
- Clamps the final target to the configured range and converts it back to Hz for shifting.

## Pitch Shifting and Resynthesis
- Currently uses a placeholder dry-signal blend modulated by the formant-preserve parameter, identifying where a real-time shifter should integrate.
- The existing FFT infrastructure suits STFT-based phase vocoders or similar algorithms; third-party shifters could also consume the computed ratio.

## Smoothing and Artifact Mitigation
- Speed, transition, tolerance, formant mix, and vibrato tracking parameters map to `SmoothedValue` constants for attack/decay shaping without zipper noise.
- Maintaining a separate dry buffer enables crossfades for formant correction without allocations inside the callback.

## GUI and Parameter Management
- The editor exposes rotary controls for retune speed, humanize, tolerance, formant mix, vibrato tracking, and range along with key/scale selectors and MIDI/force-correction toggles.
- Labels display live detected and target pitches derived from processor caches, providing real-time feedback.

## Performance Considerations
- FFT plans, buffers, and smoothing objects are prepared per host configuration to minimize callback overhead.
- Further optimizations could leverage SIMD FFT implementations, refined range gating, or shorter analysis windows for lower latency.
