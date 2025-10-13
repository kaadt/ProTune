# US Patent US 5,973,252 — Pitch Detection and Intonation Correction

**Title:** Pitch Detection and Intonation Correction Apparatus and Method  \
**Inventor:** Harold A. Hildebrand  \
**Assignee:** Auburn Audio Technologies, Inc.  \
**Patent Date:** Oct. 26, 1999

---

## 1. Executive Summary

The patent describes a real-time system that detects the pitch of monophonic audio (voice or solo instruments) and corrects the intonation by resampling the waveform. Detection is performed via autocorrelation-derived functions that react within just a few cycles, allowing tight tracking and optional vibrato generation. Pitch correction is achieved by smoothly warping the playback rate and adding or dropping whole cycles to avoid artifacts such as clicks or spectral distortion.

---

## 2. Mathematical Foundations

Let the sampled waveform be `{x}` with samples indexed `j = 0, 1, …, i`, where `i` is the current time sample. The detector evaluates candidate periods `L` using the following running sums:

| Symbol | Definition | Purpose |
| ------ | ----------- | ------- |
| `R_i(n)` | `\sum_{j=0}^{i} x_j x_{j-n}` | Autocorrelation at lag `n`. |
| `E(L)` | `\sum_{j=0}^{i} x_j^2` (accumulated energy over ~2 periods) | Reference energy. |
| `H(L)` | `\sum_{j=0}^{i} x_j x_{j-L}` | Cross-term between adjacent cycles. |

The decision statistic is:

\[ V(L) = E(L) - 2 H(L) \]

A valid period satisfies:

\[ V(L) \le \varepsilon \cdot E(L) \]

where `\varepsilon` (`eps` in code) is a tunable tolerance that controls how similar adjacent cycles must be.

Fractional periods are obtained by fitting a quadratic to `V(L-1)`, `V(L)`, and `V(L+1)`.

---

## 3. Processing Pipeline

### 3.1 Detection Mode (Coarse Search)

1. Low-pass filter and downsample the input by 8.  
2. Maintain `E_down(L)` and `H_down(L)` for `L = 2…110`.  
3. Compute `V_down(L) = E_down(L) - 2 H_down(L)` and locate local minima that pass the `\varepsilon` test.  
4. Expand the winning `L` back to the full-rate domain (`L * 8`). Handle missing fundamentals by also checking the second-best minimum.

### 3.2 Tracking Mode (Fine Search)

1. Around the coarse estimate `L_min`, track `N` candidates (preferred `N = 8`) at full sample rate (`L ≈ 16…880` for 44.1 kHz).  
2. Update `E(L)` and `H(L)` incrementally for each incoming sample.  
3. Every few samples, evaluate `V(L)` for all candidates, keep the minimum, and verify the `\varepsilon` condition.  
4. When the minimum shifts near the array boundary, roll the tracked window, recompute the vacated entries, and continue tracking.  
5. Derive the fractional period `P_min` via quadratic interpolation and convert it to frequency (`F0 = Fs / P_min`).

### 3.3 Pitch Correction and Output

1. Determine a `desired_cycle_period` from the closest musical scale step, MIDI command, or user-defined target.  
2. Compute the raw resampling ratio `cycle_period / desired_cycle_period`.  
3. Smooth the ratio using an exponential decay (`Decay`) to avoid abrupt jumps.  
4. Optionally apply vibrato modulation to the smoothed ratio.  
5. Resample the input via a fractional read pointer. When the pointer would overtake or lag behind the input pointer, add or drop exactly one period (`cycle_period`) to keep the buffer stable.  
6. Output interpolated samples (linear, cubic, or higher order depending on CPU budget).

---

## 4. Key Parameters

- `eps`: Detection threshold (`0.0…0.4`). Lower values demand closer cycle similarity.  
- `Decay`: Smoothing constant for the resampling ratio (0 → immediate, 1 → very sluggish).  
- `N`: Number of candidate periods tracked in the fine search (nominally 8).  
- Vibrato depth, rate, and delay.  
- Musical scale / MIDI mapping used to derive the desired pitch.

---

## 5. Connecting the Patent to the Python Prototype

The Python snippet below mirrors the patent’s `E/H/V` evaluation in a simplified form and can serve as a reference implementation. It measures the period by scanning possible lags (`pos`) and evaluating the same decision statistic:

```python
minimum = np.Inf
periodo = -1
for pos in range(minP, maxP):
    nolag = frame[0:(maxP * 2) - pos]
    onelag = frame[pos:maxP * 2]
    twolag = frame[np.round(np.arange(pos * 2, (maxP * 2) * 2, 2))]
    H = np.sum((nolag * onelag) - (onelag * twolag))
    E = np.sum((nolag ** 2) - (twolag ** 2))
    V = E - (2.0 * H)
    if V < minimum and V <= (np.finfo(np.float32).eps * E):
        minimum = V
        periodo = pos
```

Suggested enhancements inspired by the patent:

- **Incremental updates:** Instead of recomputing sums for each frame, maintain running sums (`E`, `H`) using circular buffers to minimize CPU usage.  
- **Coarse-to-fine search:** Start with a downsampled search to obtain a coarse period, then refine within a narrow range to reduce false detections.  
- **Fractional lag interpolation:** Apply quadratic interpolation to `V` around the detected minimum to obtain sub-sample precision.  
- **Cycle add/drop resampling:** When resynthesizing the corrected pitch, apply cycle repetition/removal as described to avoid audible artifacts.

---

## 6. Practical Implementation Tips

- Keep sufficient accumulator precision when squaring samples (`E`) or computing cross-products (`H`)—use 32-bit or 64-bit integers/floats as appropriate.  
- Carefully manage buffer offsets so the added/dropped cycles align with detected periods, preventing clicks.  
- Experiment with different interpolation schemes during resampling; linear is cheap but higher order filters give smoother results.  
- Test across a range of voices/instruments to tune `eps`, `Decay`, and vibrato parameters for musical feel.  
- Incorporate hysteresis or debounce logic when switching between detection and tracking modes to avoid rapid oscillation.

---

## 7. References

- Hildebrand, H. A., “Pitch Detection and Intonation Correction Apparatus and Method,” U.S. Patent 5,973,252, Oct. 26, 1999.
- Original Autotune product literature and marketing materials (for context on target behavior).  
- Internal experimentation and the Python prototype above for validating algorithmic choices.
