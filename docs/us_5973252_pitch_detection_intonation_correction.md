# US Patent US 5,973,252 — Pitch Detection and Intonation Correction

**Title:** Pitch Detection and Intonation Correction Apparatus and Method

**Inventor:** Harold A. Hildebrand

**Assignee:** Auburn Audio Technologies, Inc.

**Patent Date:** Oct. 26, 1999

---

## Abstract

A device and method to correct intonation errors and generate vibrato in solo instruments and vocal performances in real time. The device determines the pitch (period) of a musical note produced by voice or instrument and shifts the pitch of that note to produce a high-fidelity output. The system uses a pitch detector that recognizes pitch quickly (within a few cycles) and a pitch corrector that converts the detected pitch to a desired pitch. The method relies on computations derived from the autocorrelation function of the waveform and uses two functions, denoted `E` and `H`, to efficiently detect periodicity and track pitch.

---

## Key Concepts and Goals

- Determine instantaneous pitch (period) robustly and quickly (within a few cycles).
- Use autocorrelation-based methods to avoid weaknesses of zero-crossing and peak-based detectors.
- Make pitch correction by resampling (changing playback sample rate) with cycle-add/drop to preserve waveform shape and avoid artifacts.
- Provide vibrato modulation and smooth transitions to avoid instantaneous, unnatural pitch jumps.

---

## Definitions and Core Equations

Let sampled waveform be `{x}` where samples are indexed `j = 0, 1, 2, ..., i` and `i` is the current sample.

### Autocorrelation (discrete)

The (truncated/online) autocorrelation for lag `n` at time `i` is:

\[ R_i(n) = \sum_{j=0}^{i} x_j \; x_{j-n} \]

(Equation numbering in the patent: equation (1)).

### Energy and Cross Terms (E and H functions)

To make the computations efficient and suitable for realtime DSP, the patent defines two derived functions evaluated for a candidate period `L`:

- **E(L)** — accumulated energy over (approximately) two periods (used as a reference):

\[ E(L) = \sum_{j=0}^{i} x_j^2 \]

(This is a representation of the form given in the patent; E computed at zero-lag for blocks of length `2L`.)

- **H(L)** — autocorrelation at lag `L` (i.e., the cross-term between two adjacent cycles):

\[ H(L) = \sum_{j=0}^{i} x_j \; x_{j-L} \]

(These are implemented efficiently as running sums so they can be updated incrementally.)

The patent shows that for true periods `L` of the waveform, `E(L)` is nearly equal to `2 H(L)` (ignoring scale). Thus a test is constructed based on the difference

\[ V(L) = E(L) - 2 H(L) \]

A small value of `V(L)` (below a threshold) indicates a likely period. That threshold is defined relative to the signal energy using a parameter `\varepsilon` (eps).

The detection criterion (equation (6) in the patent) is of the form:

\[ \text{detect if } \; E(L) - 2 H(L) \le \varepsilon \cdot E(L) \]

or equivalently, the patent phrases it as searching for `L` such that `eps*Edown(L) > Edown(L) - 2*Hdown(L)` in the downsampled domain.

---

## Overall Processing Pipeline

1. **A/D conversion**: Input audio is sampled (preferably 44,100 Hz, 16-bit).
2. **Microprocessor & buffer**: Samples fed to DSP / microprocessor and stored in a circular buffer. Interrupts occur per sample.
3. **Two modes of operation**:
   - **Detection mode** — when pitch is unknown; downsample input 8:1 (with LPF) to reduce compute. Compute `E_down(L)` and `H_down(L)` for L = 2..110 on downsampled data (detects frequencies down to ~50 Hz and up to ~2756 Hz at 44.1 kHz input).
   - **Correction (tracking) mode** — once approximate period `Lmin` known, compute `E(L)` and `H(L)` over a narrow range around `Lmin` on full-rate data (L from 16..880 for 44.1 kHz) and track pitch changes.
4. **Period selection**: Find `Lmin` that minimizes `V(L) = E(L) - 2H(L)` (local minima satisfying the eps threshold). Handle the missing-fundamental case by checking both `Lmin` and `2*Lmin` (or comparing candidate minima).
5. **Interpolation for fractional period**: Fit a quadratic to `V` at `Lmin-1, Lmin, Lmin+1` and compute fractional minimum `Pmin` to produce accurate floating-point `cycle_period`.
6. **Desired period**: Compute `desired_cycle_period` from either MIDI input, the nearest note in a selected scale, or explicit user settings.
7. **Compute resample rate**:

\[ resample\_raw\_rate = \frac{cycle\_period}{desired\_cycle\_period} \]

8. **Smoothing**: Smooth `resample_raw_rate` using an exponential decay (`Decay` parameter) to produce `resample_rate1`:

\[ resample\_rate1 \leftarrow Decay \cdot resample\_rate1 + (1-Decay) \cdot resample\_raw\_rate \]

9. **Vibrato**: Optional multiplicative vibrato modulation applied to produce `resample_rate2`.

10. **Resampling & cycle add/drop**: Use `resample_rate2` to step a floating `output_addr` pointer through the input buffer. When `output_addr` would overrun or underrun the `input_addr`, add or subtract exactly one `cycle_period` to repeat or drop a cycle — this preserves spectral envelope and avoids artifacts.

11. **Interpolation for output sample**: Interpolate from input buffer at fractional address `output_addr - 5` (the patent reserves a small lookahead to make interpolation safe) and output to D/A.

---

## Detection Algorithm (high-level)

- **Downsampled detection (8:1)**: Maintain `Edown(L)` and `Hdown(L)` arrays for `L=2..110`. Update these on each new downsampled value.
- **Find a local minimum `Limin1` in `Vdown(L) = Edown(L) - 2 Hdown(L)` satisfying the eps test**.
- **Check for missing fundamental**: If the fundamental frequency would be high enough in the full-rate data, also search for a second local minimum `Limin2` in the downsampled arrays and compare which candidate (after expanding by factor 8 back to full-rate) gives smaller `V(L)` in the full-rate domain.
- **Initialize full-rate `E()` and `H()` arrays around the chosen `Lmin` (range length `N`; preferred `N = 8`) and switch to tracking mode.**

---

## Tracking Algorithm (interrupt-time, optimized)

- Update `E(i)` and `H(i)` arrays for the `N` candidate `L` values (incrementally using running sums) every sample.
- Every 5th interrupt, calculate `V(i) = E(i) - 2H(i)` for i=1..N and find the minimum `Lmin` and `temp1` (the min value).
- Apply thresholds: if `temp1` fails the eps threshold, or pitch changes too fast, or energy too low, treat as tracking failure and switch back to detection mode.
- If `Lmin` has shifted outside the center region, shift (roll) the `E()` and `H()` arrays up or down by one index, adjust `EHOffset`, compute the new boundary `E(1)` or `E(N)` incrementally, and queue the expensive `H(1)`/`H(N)` computation for the non-interrupt poll loop (to be done gradually).
- Compute fractional `Pmin` by quadratic interpolation using `V(Lmin-1), V(Lmin), V(Lmin+1)` (3-point quadratic fit) and set `cycle_period = EHOffset - Pmin - 1`.
- Compute `resample_raw_rate`, smooth it to `resample_rate1`, apply vibrato modulation to get `resample_rate2`, and return to the interrupt output resampling code.

---

## Resampling & Cycle Add/Drop Details

- The output pointer `output_addr` is incremented by `resample_rate2` each sample; the input pointer `input_addr` increments by 1 each new input sample.
- If `output_addr` would move ahead of `input_addr` (overrun) then subtract `cycle_period` from `output_addr` to repeat one cycle (one-cycle repeat).
- If `output_addr` falls behind `input_addr` (underrun) then add `cycle_period` to `output_addr` to drop one cycle (drop one cycle).
- Use interpolation to read the fractional sample at `output_addr - 5` and write to the D/A.

---

## Parameters (user adjustable)

- `eps`: detection threshold (0.0 to 0.40). Smaller values are stricter (requires more similarity between cycles).
- `Decay`: smoothing factor for resample rate (0..1). 0 = instantaneous pitch changes; near 1 = heavy smoothing.
- `N`: number of L candidates tracked in full-rate arrays (preferred value 8).
- Vibrato depth, rate, and onset delay.
- MIDI/scale selection for `desired_cycle_period` (use MIDI note on messages and pitch bend as inputs).

---

## Claims (summary)

The patent claims methods and apparatus for:

- Determining period of a musical waveform by sampling and computing autocorrelation-derived functions `E(L)` and `H(L)` and selecting `L` that minimizes `E(L)-2H(L)`.
- Retuning (pitch-correcting) by resampling toward a desired period (from a musical scale or MIDI input) and doing so gradually via smoothing.
- Using an 8:1 downsampled detection stage and then full-rate tracking around the detected period.
- Using cycle add/drop in resampling to preserve natural waveform shape and avoid artifacts.

(Complete claim language and dependent claims are included in the original patent text.)

---

## Implementation Notes & Practical Tips (for reverse-engineering)

- Implement the downsampled detector first (8x LPF + pick every 8th sample) and compute `Edown(L)`/`Hdown(L)` for `L=2..110` to get a working detection front-end.
- Use running sums (circular buffer) to maintain `E` and `H` efficiently; update them incrementally per new sample to meet realtime constraints.
- Be careful about fixed-point vs floating-point: a DSP implementation traditionally uses fixed-point arithmetic; keep headroom for squared sums and cross-products (use 32-bit or 64-bit accumulators as needed).
- For fractional period estimation, implement a three-point quadratic interpolation to get `Pmin` precisely.
- For resampling, choose an interpolation method appropriate to CPU budget: linear, cubic, or windowed sinc. The patent leaves interpolation method open; higher-order interpolation gives fewer artifacts but costs CPU.
- When performing cycle-add/drop, ensure that the cycle boundaries line up with the measured period; otherwise you may introduce clicks. The proposed `output_addr +/- cycle_period` approach aligns cycle boundaries to the tracked period.

---

## References

The markdown file was created from the patent document **US 5,973,252** uploaded by the user.


---

*End of document.*

