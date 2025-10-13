# 🎧 Spectral Peak Vocoder  
### Real-Time Pitch & Harmonic Modulation Engine

Derived from advanced phase-vocoder concepts introduced by Jean Laroche & Mark Dolson (1999), this document outlines the design and math for a **real-time frequency-domain pitch shifting and harmonization engine**—the foundation for modern AutoTune-style systems.

---

## 🧠 Concept

Traditional phase vocoders shift pitch by:
1. **Time-scaling** by a factor β (changes duration, preserves pitch), then  
2. **Resampling** (restores duration, shifts pitch).

This two-stage approach is **computationally heavy** and limited to **linear pitch scaling** (same ratio for all frequencies).

The **Spectral Peak Vocoder** achieves pitch and frequency-domain manipulation **entirely in the FFT domain**, enabling:
- Constant computational cost (independent of β)
- Frequency-dependent (nonlinear) mappings
- Single-pass *chorusing*, *harmonizing*, and *inharmonic synthesis*

---

## ⚙️ Analysis–Synthesis Model

| Symbol | Meaning |
|---------|----------|
| `h(n)` | Analysis window |
| `w(n)` | Synthesis window |
| `R_i, R_s` | Input/output hop sizes |
| `N` | FFT size |
| `X(t_u, ω_k)` | STFT at frame `u`, bin `k` |
| `Y(t_u, ω_k)` | Modified STFT |
| `β` | Pitch-scaling ratio |

Short-Time Fourier Transform:
\[
X(t_u, ω_k) = \sum_{n=-N/2}^{N/2} x(n + t_u)h(n)e^{-j n ω_k}
\]

---

## 🔁 Standard Pitch-Scaling Cost

For a pitch ratio β:

\[
\begin{aligned}
C_1 &= \frac{1}{β}C_r + C_t \quad &(\text{resampling first}) \\
C_2 &= βC_t + C_r \quad &(\text{time-scaling first})
\end{aligned}
\]

Computation increases with β → non-optimal for real-time applications.

---

## 🚀 Spectral Peak-Based Method

### Core Operation

Each spectral peak is directly shifted by Δω:

\[
Y(t_u, ω_k) = X(t_u, ω_k - Δω_u)e^{jφ_u}
\]
\[
φ_u = φ_{u-1} + Δω_u R_0
\]

For constant Δω:

\[
y(n) = e^{j[(ω_0 + Δω)n + φ]}
\]

---

### Peak Detection

A simple yet effective approach:
- A channel is a *peak* if its amplitude exceeds its four nearest neighbors.

Quadratic refinement (optional):
\[
ω_\text{peak} = ω_k + \frac{A_{k-1} - A_{k+1}}{2(A_{k-1} - 2A_k + A_{k+1})}
\]

---

### Frequency Shift Mapping

Standard pitch ratio:
\[
Δω = βω_0 - ω_0
\]

Nonlinear mappings:
\[
β(ω) = β_0 + αω
\]
→ allows *partial stretching*, *frequency inversions*, or *custom harmonic maps*.

---

### Interpolation

If Δω is fractional:

\[
Y(t_u, ω_k) = 0.5[X(t_u, ω_k) + X(t_u, ω_{k+1})]
\]

| Overlap | Modulation Depth | Notes |
|----------|------------------|-------|
| 50% | ~−21 dB | Audible amplitude ripple |
| 75% | ~−51 dB | Near-transparent |

✅ Recommended: **75% overlap** or high-order **Lagrange/all-pass interpolation**.

---

### Phase Coherence

Perfect reconstruction condition:
\[
\sum_i g(n + iR_0)h(n + iR_0) = 1
\]

If hop size \( R_0 = N/m \) and \( Δω R_0 = n \frac{2π}{m} \), the rotation reduces to a simple sign change (no trig required).

Adjacent channels share the same phase rotation → inherent **identity phase-locking**, preventing “phasiness.”

---

## 🧮 Implementation Guide

1. **Windowing & STFT**  
   - Hanning window  
   - FFT size: 2048–4096  
   - Overlap: 75%

2. **Detect Peaks**  
   - Local maximum detection  
   - Optionally use parabolic refinement

3. **Apply Shifts**  
   - For each peak: compute Δω = βω₀ − ω₀  
   - Reassign frequency bins (interpolated or integer-shifted)

4. **Phase Correction**  
   - Update phase via \( e^{jΔωR_0} \)

5. **Reconstruction**  
   - Overlap-add with window pair satisfying reconstruction condition

---

## 🧪 Example Implementation (Python-like pseudocode)

```python
def spectral_peak_vocoder(x, beta, N=2048, overlap=0.75):
    hop = int(N * (1 - overlap))
    window = np.hanning(N)
    frames = frame_signal(x, N, hop)
    stft = np.fft.rfft(frames * window)

    phi = 0.0
    for u, X in enumerate(stft):
        peaks = detect_peaks(np.abs(X))
        Y = np.zeros_like(X)
        for k in peaks:
            omega = 2 * np.pi * k / N
            d_omega = beta * omega - omega
            phi += d_omega * hop
            shift_bins(Y, X, k, d_omega, phi)
        stft[u] = Y

    y = overlap_add(np.fft.irfft(stft), window, hop)
    return np.real(y)
🎶 Creative Applications
Effect	Description
Pitch-Shift	Uniform scaling β across all frequencies
Harmonizer	Duplicate peaks to multiple β ratios (e.g., 1.25, 1.5)
Chorus	Add small Δω fluctuations per frame
Partial Stretching	Frequency-dependent β(ω)
Spectral Warp	Arbitrary nonlinear mapping of ω
🧰 Mode Comparison
Mode	Overlap	Interp.	Cost	Quality
Integer Δω	50%	None	Very Low	Approximate pitch
Fractional Δω	75%	Linear	Low	High
Fractional Δω	50–75%	Lagrange / All-pass	Moderate	Near-transparent
🧾 Mathematical Proofs
Perfect Frequency Shift
If
Y
(
t
u
,
ω
)
=
X
(
t
u
,
ω
−
Δ
ω
)
e
j
Δ
ω
u
R
0
Y(t 
u
​	
 ,ω)=X(t 
u
​	
 ,ω−Δω)e 
jΔωuR 
0
​	
 
 
and windows satisfy the reconstruction condition:
∑
i
g
(
n
+
i
R
0
)
h
(
n
+
i
R
0
)
=
1
i
∑
​	
 g(n+iR 
0
​	
 )h(n+iR 
0
​	
 )=1
then:
y
(
n
)
=
e
j
[
(
ω
0
+
Δ
ω
)
n
+
φ
]
y(n)=e 
j[(ω 
0
​	
 +Δω)n+φ]
 
→ a perfectly shifted complex exponential.
Overlap-Add Modulation
For cosine-modulated windows:
w
(
n
)
=
h
(
n
)
cos
⁡
(
2
π
n
/
N
)
w(n)=h(n)cos(2πn/N)
the Fourier transform introduces sidebands at multiples of 
4
π
/
N
4π/N:
−21 dB (50 %), −51 dB (75 %).
→ explains smoother reconstruction at higher overlaps.
📚 Reference
Laroche, J. & Dolson, M. (1999).
New Phase-Vocoder Techniques for Real-Time Pitch Shifting, Chorusing, Harmonizing, and Other Exotic Audio Modifications.
Journal of the Audio Engineering Society, 47(11), 928–936.