# Automatic Tuning Procedure for Kiln Gradient Controller

This document describes the **automatic tuning process** for the kiln control system.
The goal is to identify the thermal dynamics of the kiln using a **controlled open-loop step test**
and compute **robust PI parameters** for the **inner gradient controller** using **lambda tuning**.

The design assumes:
- One SSR per heating coil
- All coils driven identically during tuning
- SSR power applied via **time windowing** (not fast PWM)
- Temperature sampling faster than actuation
- Heating only during tuning (no cooling control)

---

## 1. Control Context (What is Being Tuned)

The inner controller regulates **temperature gradient**:

\[
g(t) = \frac{dT}{dt} \quad [^\circ C/s]
\]

The plant seen by this controller is:

\[
u(t) \; \longrightarrow \; g(t)
\]

where:
- \(u(t)\) is heater duty (0…1)
- \(g(t)\) is temperature rate of change

The outer temperature loop and cooling logic are **not active during tuning**.

---

## 2. Identified Plant Model

The kiln is modeled as a **First-Order Plus Dead Time (FOPDT)** system:

\[
G_{gu}(s) = \frac{K_g \, e^{-L_g s}}{\tau_g s + 1}
\]

Parameters to identify:
- \(K_g\) — steady-state gain (°C/s per unit duty)
- \(L_g\) — dead time (s)
- \(\tau_g\) — time constant (s)

This model is sufficient for conservative and robust PI tuning.

---

## 3. Actuation Strategy During Tuning

### SSR Windowing
- Window length: \(T_w\) (e.g. 20 s)
- Duty \(u\) applied as:
  - ON for \(u \cdot T_w\)
  - OFF for \((1-u)\cdot T_w\)

During tuning:
- Duty is **constant** (0 or 0.30)
- No PI, no modulation inside the window
- All SSRs switch together

This ensures:
- Known input amplitude
- SSR-safe operation
- Clean identification

---

## 4. Measurement Strategy

### Temperature Sampling
- Sample period: \(T_s = 1\) s
- Sampling must be **much faster than SSR windowing**
- Used for:
  - Dead time detection
  - Gradient estimation
  - Noise estimation
  - Safety checks

### Gradient Estimation
For tuning, gradient is computed using **linear regression** over a sliding window:

\[
g(t) = \frac{\sum (t_i-\bar t)(T_i-\bar T)}{\sum (t_i-\bar t)^2}
\]

Recommended window:
- \(W = 30\)–\(120\) s (default: 60 s)

This avoids noisy differentiation and produces stable estimates.

---

## 5. Step Test Sequence

### Phase A — Baseline (Heater OFF)
- Apply \(u = 0\)
- Duration: 2–3 minutes
- Record temperature
- Compute:
  - Baseline gradient \(g_0\)
  - Gradient noise \(\sigma_g\)

This captures natural drift and sensor noise.

---

### Phase B — Step Input
- At time \(t_{step}\), apply:
  \[
  u = 0.30
  \]
- Keep duty constant
- Continue sampling temperature and computing gradient

---

### Phase C — Steady-State Detection
Gradient is considered settled when:

\[
|g(t) - \bar g_{N}| < \epsilon_g
\quad \text{for } N \text{ consecutive seconds}
\]

Typical values:
- \(N = 60\)–120 s
- \(\epsilon_g \approx 0.0002\;^\circ C/s\) (≈ 0.7 °C/h)

The steady-state gradient is:
\[
g_\infty = \bar g_N
\]

---

## 6. Parameter Identification

Let:
\[
\Delta u = 0.30
\]

### 6.1 Gain \(K_g\)

\[
\Delta g = g_\infty - g_0
\]

\[
K_g = \frac{\Delta g}{\Delta u}
\]

---

### 6.2 Dead Time \(L_g\)

Define detection threshold:
\[
g_{th} = g_0 + n \sigma_g
\quad (n = 4\text{–}5)
\]

Find first time \(t_{resp}\) after \(t_{step}\) where:
- \(g(t) > g_{th}\)
- Condition holds for ≥10–30 s

Then:
\[
L_g = t_{resp} - t_{step}
\]

---

### 6.3 Time Constant \(\tau_g\)

Target gradient level:
\[
g_{63} = g_0 + 0.632 \cdot \Delta g
\]

Find first time \(t_{63}\) where:
\[
g(t) \ge g_{63}
\]

Then:
\[
\tau_g = t_{63} - (t_{step} + L_g)
\]

---

## 7. PI Controller Design (Lambda Tuning)

### 7.1 Choose Closed-Loop Time Constant

A conservative and SSR-safe choice:

\[
\lambda_g = \max \left(
3L_g,\;
0.5\tau_g,\;
2T_w
\right)
\]

This ensures:
- Dead-time robustness
- No fighting actuator granularity
- Smooth gradient response

---

### 7.2 PI Parameters

#### Proportional Gain
\[
K_c = \frac{\tau_g}{K_g(\lambda_g + L_g)}
\]

#### Integral Time
\[
T_i = \tau_g
\]

#### Anti-Windup Time
\[
T_{aw} = T_i
\]

These parameters are loaded directly into the gradient PI controller.

---

## 8. Gradient Filter Parameter

To filter measurement noise without excessive lag:

Choose filter time constant:
\[
\tau_f = \frac{\tau_g}{5}
\]

EMA coefficient:
\[
\alpha = \frac{\tau_f}{\tau_f + T_s}
\]

Clamp to implementation limits (e.g. 0.5–0.95).

---

## 9. Actuator-Related Constraints

### SSR Window Constraint
To avoid control chatter:
\[
\lambda_g \ge 2T_w
\]

### Minimum Pulse Constraint
Ensure:
\[
u \cdot T_w \ge T_{min}
\]
for smallest nonzero duty used by the controller.

---

## 10. Parameters Produced by Autotune

The autotune process yields:

### Identified Plant
- \(K_g\)
- \(L_g\)
- \(\tau_g\)

### Controller Parameters
- \(K_c\)
- \(T_i\)
- \(T_{aw}\)
- \(\alpha\)

### Diagnostic Data
- \(g_0\) (baseline drift)
- \(\sigma_g\) (noise)
- \(\lambda_g\) (design choice)

These parameters are applied to the controller and may be stored or displayed via the UI.

---

## 11. Notes on Extensions

- Tuning may be repeated at different temperatures for gain scheduling
- Cooling dynamics are not tuned here (handled separately)
- Outer temperature loop does not affect identification
- The method is robust to quantized actuators and slow thermal plants

---

## 12. Summary

This tuning approach:
- Uses a **single safe step** (0 → 30%)
- Identifies a physically meaningful model
- Produces conservative, SSR-friendly PI gains
- Aligns directly with the implemented control architecture

It prioritizes **stability, hardware safety, and predictability** over aggressiveness.
