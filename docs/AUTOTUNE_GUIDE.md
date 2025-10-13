# ProTune Auto-Tune Style Setup Guide

This guide walks through the practical steps required to get an audible "hard tune" effect similar to Antares Auto-Tune when using ProTune. It also highlights the code paths that drive each stage so you can debug or extend the behaviour if you still are not hearing the expected correction.

## 1. Confirm the audio path is active

1. Insert ProTune on a **mono vocal track** or route a stereo track that contains the same vocal on both sides. The engine now averages every available input channel before analysis, so mismatched stereo content (e.g. backing vocals on one side) can still confuse detection even though both channels contribute to the estimate.【F:Source/PitchCorrectionEngine.cpp†L361-L386】
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

1. Dial **Retune Speed** fully clockwise. This lowers the smoothing ramp in `ratioSmoother` to ~5 ms so ratios snap almost instantly.【F:Source/PitchCorrectionEngine.cpp†L45-L62】
2. Keep **Humanize** near zero for the robotic sound. Lower values minimise hysteresis so the `pitchSmoother` chases new targets immediately, while higher values add gentle lag for natural phrasing.【F:Source/PitchCorrectionEngine.cpp†L45-L62】
3. Set **Vibrato Tracking** near zero if you want vibrato flattened; higher values let the detector glide instead of locking to the scale.【F:Source/PitchCorrectionEngine.cpp†L45-L62】
4. Disable MIDI mode unless you are actively holding notes. When `params.midiEnabled` is true and a key is held, the engine locks to that MIDI note, so stray note-ons can prevent automatic snapping.【F:Source/PitchCorrectionEngine.cpp†L190-L218】 Use the **Force Correction** toggle if you want to hear instant correction regardless of the tolerance slider while debugging.【F:Source/PitchCorrectionEngine.cpp†L206-L229】【F:Source/PluginEditor.cpp†L24-L198】
5. The retune speed now adapts to how far you are from the snapped note: large errors trigger near-instant correction, while smaller deviations keep some vibrato for musicality, mirroring Hildebrandt’s patent guidance.【F:Source/PitchCorrectionEngine.cpp†L45-L229】

## 5. Snap to the desired scale

1. Set the **Scale** selector to **Chromatic** for hard Auto-Tune style correction. Choosing **Major** or **Minor** restricts snapping to the diatonic notes in the selected key.【F:Source/PitchCorrectionEngine.cpp†L200-L234】
2. Adjust the **Key** selector to match the song’s tonic so major/minor modes snap to the right accidentals. For more exotic modes, extend the interval tables in `snapNoteToScale` and surface them via additional scale choices.【F:Source/PitchCorrectionEngine.cpp†L200-L234】

## 6. Blend formants for realism

1. The `formantPreserve` control blends the corrected signal with the untouched dry buffer. Set it to 0 for the classic robotic sound or around 0.3–0.5 for a mix that keeps some natural timbre.【F:Source/PitchCorrectionEngine.cpp†L132-L154】
2. Extreme dry mix values (>0.7) will mask the effect because most of what you hear is the unprocessed voice.

## 7. Debugging when there is still “no effect”

1. Use logging or a debugger to inspect `lastDetectedFrequency` and `lastTargetFrequency` after each block. Both are exposed via getters and updated during `process`. Values near zero mean detection failed, so revisit the FFT size, range, or input level.【F:Source/PitchCorrectionEngine.cpp†L102-L151】
2. Confirm that `ratioValues` contains numbers other than 1.0. If the ratio never deviates, either detection equals the target (tolerance too high) or MIDI input pins the pitch.【F:Source/PitchCorrectionEngine.cpp†L114-L151】
3. Step through `PitchShiftChannel::processFrame` to ensure bins are remapped. If the `targetBin` calculation exceeds `spectrumSize`, the content will be dropped; check that `ratio` remains between 0.25 and 4.0 (already enforced by `juce::jlimit`).【F:Source/PitchCorrectionEngine.cpp†L243-L336】

## 8. Extending toward a commercial-grade effect

1. **Improve detection:** add autocorrelation, yin, or neural detectors and fuse results with the FFT estimate for better pitch locking on noisy vocals.【F:Source/PitchCorrectionEngine.cpp†L63-L151】
2. **Advanced scaling:** implement per-scale note allow-lists, scale degrees, and humanise curves to mimic Auto-Tune’s “Flex-Tune” and “Humanize” features.【F:Source/PitchCorrectionEngine.cpp†L206-L233】
3. **Robust pitch shifting:** replace the simple phase vocoder with a phase-locked vocoder or PSOLA to reduce transient smearing, especially when using high ratios.【F:Source/PitchCorrectionEngine.cpp†L243-L336】
4. **Formant preservation:** add an LPC-based envelope tracker and copy spectral envelopes from the dry path into the shifted bins before resynthesis for clearer consonants.【F:Source/PitchCorrectionEngine.cpp†L132-L154】【F:Source/PitchCorrectionEngine.cpp†L243-L336】

## 9. Verifying the autocorrelation detector

1. **Feed controlled test tones.** Create a sine sweep or fixed-pitch tones (e.g. 110 Hz, 220 Hz, 440 Hz) and route them through ProTune. Watch the debug output in your host: `PitchCorrectionEngine` logs whenever detection or targets change, so you can confirm the new decimated autocorrelation estimator locks to the right fundamental.【F:Source/PitchCorrectionEngine.cpp†L231-L274】【F:Source/PitchCorrectionEngine.cpp†L309-L371】
2. **Observe confidence swings.** The detector now calculates the patent-inspired error term `E(L) - 2H(L)` over the downsampled frame and rejects lags whose residual exceeds 18 % of the measured energy. Use a debugger to inspect `lastDetectionConfidence`; stable tones should report values near 1.0, while noise or harmonically ambiguous input falls toward zero.【F:Source/PitchCorrectionEngine.cpp†L329-L371】
3. **Check the decimator path.** The analysis frame is low-pass filtered with a 33-tap Hann-windowed sinc and decimated by 8× before searching lags 2–110. If you suspect aliasing, place a breakpoint in `designDecimationFilter` or `applyDecimationFilter` to visualise the filtered waveform and verify the effective sample rate (44.1 kHz / 8 ≈ 5512 Hz).【F:Source/PitchCorrectionEngine.cpp†L27-L76】【F:Source/PitchCorrectionEngine.cpp†L309-L342】
4. **Correlate to the GUI.** While stepping through the code, adjust the **Range** sliders: the detector clamps the evaluated lag window based on the user-selected bounds after decimation, so setting an unrealistically high `rangeLowHz` can clip potential periods. Confirm that UI changes propagate by checking `minLag`/`maxLag` in the debugger.【F:Source/PitchCorrectionEngine.cpp†L337-L356】【F:Source/PluginEditor.cpp†L24-L198】
5. **A/B Auto-Tune style settings.** Once pitch tracking responds to the test tones, switch back to vocal material and toggle the aggressive settings from sections 3–6. The new detector should provide a steady `detected` frequency that the smoothing stages can chase, yielding an audible “hard tune” effect without the comb-filter artifacts caused by mis-identified periods.【F:Source/PitchCorrectionEngine.cpp†L132-L212】【F:Source/PitchCorrectionEngine.cpp†L309-L371】

Following the checklist above should yield an obvious tuning effect—especially with Speed/Transition at maximum and Tolerance near zero. If you still do not hear correction, record a dry sample, run the plugin in a debugger, and inspect each stage as described in section 7 to locate the break in the pipeline.
