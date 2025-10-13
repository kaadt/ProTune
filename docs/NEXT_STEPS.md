# Next Steps Toward an Audible Auto-Tune Effect

This checklist prioritises the remaining engineering work required to turn the current
placeholder correction path into something closer to a commercial Auto-Tune style
experience. Each item calls out the relevant code and validation strategy so you can
iterate methodically.

## 1. Verify the detection-to-target pipeline
- **Log detected vs. target pitch** inside `PitchCorrectionEngine::process` to confirm
  that `lastDetectedFrequency` and `lastTargetFrequency` diverge when you sing off key.
- **Temporarily bypass the tolerance gate** in `chooseTargetFrequency` to ensure the
  shifter always has work to do while you debug. A large tolerance value (hundreds of
  cents) or a hard-coded return value is fine for testing.
- **Check MIDI override** by sending a held MIDI note and verifying the GUI/console
  reflects the forced target. This confirms the control path and smoothing behave
  correctly.

## 2. Tune the analysis window for responsiveness
- The 2048-sample Hann window (order 11) adds ~46 ms of look-ahead at 44.1 kHz. When
  chasing faster transients, experiment with order 10 (1024 samples) and confirm the
  coarse/fine autocorrelation search still locks without octave-hopping.
- After each change, log `confidenceOut` to make sure the tighter window has not
  increased the rejection rate for low-pitched material.

## 3. Harden the cycle-resampling shifter
- **Watch the read/write gap** inside `CycleResampler::process` and ensure the add/drop
  thresholds (`minLag`, `maxLag`) stay proportional to the detected period, especially
  when the ratio jumps abruptly.
- **Audit the interpolation**: linear blending is fast but may smear bright consonants.
  Swap in a short Lagrange or Hermite interpolator if aliasing becomes audible.
- **Stress-test at extremes** by driving ratios of 4×/0.25× and verifying the cycle
  adjustments keep the read head safely behind the write head without glitching.

## 4. Rebalance wet/dry mixing
- Currently the `formantPreserve` parameter crossfades back to the dry buffer. For
  maximum effect during testing, set it to 0.0 (full wet). Afterwards, revisit the
  blend curve so mid values still sound corrected rather than mostly dry.

## 5. Build host-facing diagnostics
- Add a JUCE logger toggle or GUI scope that plots detected vs. corrected pitch so you
  can confirm the DSP path is active without relying solely on your ears.
- Expose a “Force Correction” button in the GUI that temporarily disables tolerance and
  MIDI logic, helping new users hear the effect instantly.

## 6. Validate in multiple hosts
- Test in the JUCE Standalone and at least one VST3 host (FL Studio, Reaper). Compare
  results to rule out host-specific buffer sizing or latency compensation issues.
- Document any host quirks in `docs/AUTOTUNE_GUIDE.md` so others can reproduce your
  working configuration.

Tackle the list in order, committing after each logical milestone. Once the shifter
produces audible correction with the force-correction toggle, you can iterate on
musicality (vibrato tracking, range clamping, etc.) with confidence.
