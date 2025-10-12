# ProTune Auto-Tune Style Setup Guide

This guide walks through the practical steps required to get an audible "hard tune" effect similar to Antares Auto-Tune when using ProTune. It also highlights the code paths that drive each stage so you can debug or extend the behaviour if you still are not hearing the expected correction.

## 1. Confirm the audio path is active

1. Insert ProTune on a **mono vocal track** or ensure that both channels carry the same singer. The current engine analyses the left channel only before mirroring changes to every output channel during the pitch-shift stage.【F:Source/PitchCorrectionEngine.cpp†L70-L90】【F:Source/PitchCorrectionEngine.cpp†L114-L132】
2. Feed a dry vocal signal with **minimal effects before ProTune**. Compressors or modulation in front of the plugin can break pitch detection.
3. Verify the plugin meters in your host show activity and that the `process` method is called each block (e.g. use a debugger breakpoint around the dry-buffer copy section while hosting the plugin).【F:Source/PitchCorrectionEngine.cpp†L114-L132】

## 2. Match the analysis window to your song

1. The detection FFT now defaults to 2048 samples (~46 ms at 44.1 kHz), reducing response latency while keeping octave detection stable.【F:Source/PitchCorrectionEngine.cpp†L15-L108】
2. Call `PitchCorrectionEngine::setAnalysisWindowOrder` with values between 9 (512 samples) and 12 (4096 samples) to audition different trade-offs without touching internal constants.【F:Source/PitchCorrectionEngine.cpp†L72-L103】
3. After each change, rebuild the plugin, reload it in your host, and confirm that tuning still occurs.

## 3. Calibrate the pitch range and tolerance

1. Set the **Range Low/High** controls so that the singer’s entire register is inside `params.rangeLowHz` and `params.rangeHighHz`. Detection bins outside this window are ignored.【F:Source/PitchCorrectionEngine.cpp†L155-L197】
2. Keep **Tolerance** around `0`–`5` cents for a robotic sound. Inside `chooseTargetFrequency` the detected note blends toward the snapped pitch using a dead-band defined by the tolerance slider—smaller values mean the correction reaches the target almost instantly, while larger values preserve natural intonation.【F:Source/PitchCorrectionEngine.cpp†L206-L229】

## 4. Force fast, aggressive correction

1. Dial **Speed** fully clockwise. This lowers the smoothing ramp in `ratioSmoother` to ~5 ms so ratios snap almost instantly.【F:Source/PitchCorrectionEngine.cpp†L45-L62】
2. Increase **Transition** so the `pitchSmoother` follows targets quickly (1–5 ms).【F:Source/PitchCorrectionEngine.cpp†L45-L62】
3. Set **Vibrato Tracking** near zero if you want vibrato flattened; higher values let the detector glide instead of locking to the scale.【F:Source/PitchCorrectionEngine.cpp†L45-L62】
4. Disable MIDI mode unless you are actively holding notes. When `params.midiEnabled` is true and a key is held, the engine locks to that MIDI note, so stray note-ons can prevent automatic snapping.【F:Source/PitchCorrectionEngine.cpp†L190-L218】 Use the **Force Correction** toggle if you want to hear instant correction regardless of the tolerance slider while debugging.【F:Source/PitchCorrectionEngine.cpp†L206-L229】【F:Source/PluginEditor.cpp†L24-L92】

## 5. Snap to the desired scale

1. Leave **Chromatic** enabled for hard Auto-Tune style correction. Disabling it restricts snapping to the major scale defined in `snapNoteToScale`, which may skip accidentals you expect to hear.【F:Source/PitchCorrectionEngine.cpp†L224-L233】
2. If you need diatonic control in other modes, extend `majorScale` in `snapNoteToScale` or add alternate scales (minor, custom) and surface them as parameters.

## 6. Blend formants for realism

1. The `formantPreserve` control blends the corrected signal with the untouched dry buffer. Set it to 0 for the classic robotic sound or around 0.3–0.5 for a mix that keeps some natural timbre.【F:Source/PitchCorrectionEngine.cpp†L132-L154】
2. Extreme dry mix values (>0.7) will mask the effect because most of what you hear is the unprocessed voice.

## 7. Debugging when there is still “no effect”

1. Use logging or a debugger to inspect `lastDetectedFrequency` and `lastTargetFrequency` after each block. Both are exposed via getters and updated during `process`. Values near zero mean detection failed, so revisit the FFT size, range, or input level.【F:Source/PitchCorrectionEngine.cpp†L102-L151】
2. Confirm that `ratioValues` contains numbers other than 1.0. If the ratio never deviates, either detection equals the target (tolerance too high) or MIDI input pins the pitch.【F:Source/PitchCorrectionEngine.cpp†L114-L151】
3. Step through `PitchShiftChannel::processFrame` to ensure bins are remapped. If the `targetBin` calculation exceeds `spectrumSize`, the content will be dropped; check that `ratio` remains between 0.25 and 4.0 (already enforced by `juce::jlimit`).【F:Source/PitchCorrectionEngine.cpp†L243-L330】

## 8. Extending toward a commercial-grade effect

1. **Improve detection:** add autocorrelation, yin, or neural detectors and fuse results with the FFT estimate for better pitch locking on noisy vocals.【F:Source/PitchCorrectionEngine.cpp†L63-L151】
2. **Advanced scaling:** implement per-scale note allow-lists, scale degrees, and humanise curves to mimic Auto-Tune’s “Flex-Tune” and “Humanize” features.【F:Source/PitchCorrectionEngine.cpp†L206-L233】
3. **Robust pitch shifting:** replace the simple phase vocoder with a phase-locked vocoder or PSOLA to reduce transient smearing, especially when using high ratios.【F:Source/PitchCorrectionEngine.cpp†L243-L336】
4. **Formant preservation:** add an LPC-based envelope tracker and copy spectral envelopes from the dry path into the shifted bins before resynthesis for clearer consonants.【F:Source/PitchCorrectionEngine.cpp†L132-L154】【F:Source/PitchCorrectionEngine.cpp†L243-L336】

Following the checklist above should yield an obvious tuning effect—especially with Speed/Transition at maximum and Tolerance near zero. If you still do not hear correction, record a dry sample, run the plugin in a debugger, and inspect each stage as described in section 7 to locate the break in the pipeline.
