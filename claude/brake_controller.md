# Passive Cooling **Rate Limiter** (“Heater Brake”) for a Kiln  
## + Integration with Existing Inner Gradient PI + Outer Temperature Loop

This document adds a **cooling-rate limiter** for a kiln that has:
- an **inner PI** loop controlling the **temperature gradient** \(g = dT/dt\) by driving heater power \(u \ge 0\),
- an **outer temperature loop** that produces a gradient setpoint \(g_{\text{sp}}\) from temperature error,
- **no active cooling** (cooling is passive), but you still want **cooling ramps** like “cool at \(-1^\circ C/\text{min}\)”.

Key idea:  
**To limit cooling rate, you use the heater as a brake.**  
If natural cooling is “too fast” (too negative slope), apply a small amount of heat to reduce the magnitude of the negative slope.

---

## 1) Definitions and Units

- \(T(t)\): measured temperature \([^\circ C]\)
- \(g(t)=\frac{dT}{dt}\): true gradient \([^\circ C/s]\)
- \(\hat g_f[k]\): filtered gradient estimate (used for control) \([^\circ C/s]\)

- Heater command: \(u \in [0, u_{\max}]\) (e.g. 0..1 or 0..100%)

### Cooling ramp constraint
User specifies a maximum allowed cooling slope (negative):
\[
g_{\min} < 0 \quad \text{(e.g. } -1^\circ C/\text{min}\text{)}
\]

Constraint to enforce during cooling:
\[
\hat g_f(t) \ge g_{\min}
\]

Interpretation:
- Cooling slower than limit is OK: e.g. \(-0.3\) when limit is \(-1\)
- Cooling faster than limit is NOT OK: e.g. \(-3\) when limit is \(-1\)

---

## 2) Gradient Estimation (same as your inner loop)

With sample time \(T_s\):

Raw derivative:
\[
\hat g[k] = \frac{T[k]-T[k-1]}{T_s}
\]

EMA filter:
\[
\hat g_f[k] = \alpha \hat g_f[k-1] + (1-\alpha)\hat g[k]
\]

Approximate filter time constant:
\[
\tau_f \approx \frac{T_s \alpha}{1-\alpha}
\]

---

## 3) Cooling “Brake” Controller Concept

During cooling, heater is normally off: \(u=0\).

If cooling is **too fast**:
\[
\hat g_f < g_{\min}
\]
then enable “brake”: add heat until
\[
\hat g_f \approx g_{\min}
\]

This is a **one-sided constraint controller**:
- Only acts when the constraint is violated.
- Otherwise it outputs 0.

---

## 4) Cooling Brake Error Signal

Define cooling violation error:

\[
e_{\text{cool}} = g_{\min} - \hat g_f
\]

- If \(\hat g_f = -2\), \(g_{\min}=-1\): \(e_{\text{cool}} = +1\) → violation → apply brake
- If \(\hat g_f = -0.5\), \(g_{\min}=-1\): \(e_{\text{cool}} = -0.5\) → no violation → brake off

Brake should use only:
\[
e_{\text{cool}}^+ = \max(e_{\text{cool}}, 0)
\]

---

## 5) Two Brake Controller Options

### Option A (recommended start): P + hysteresis (simple, robust)

Define a hysteresis band \(\Delta g > 0\).

Enable brake when:
\[
\hat g_f < g_{\min} - \Delta g
\]

Disable brake when:
\[
\hat g_f > g_{\min} + \Delta g
\]

When enabled:
\[
u_{\text{brake}} = \mathrm{clip}\left(K_b \, (g_{\min} - \hat g_f),\;0,\;u_{\text{brake,max}}\right)
\]

When disabled:
\[
u_{\text{brake}} = 0
\]

**Notes**
- \(u_{\text{brake,max}}\) should be small (start 5–10% of full power)
- If \(\hat g_f > 0\) (warming), force brake off

---

### Option B: PI brake + anti-windup (tighter, use if needed)

Unsaturated brake:
\[
u^\* = K_b \left(e_{\text{cool}}^+ + I_b\right)
\]

Saturation:
\[
u = \mathrm{clip}(u^\*, 0, u_{\text{brake,max}})
\]

Integrator update with back-calculation anti-windup:
\[
I_b[k] = I_b[k-1]
+ \frac{T_s}{T_{i,b}} e_{\text{cool}}^+[k]
+ \frac{T_s}{T_{aw,b}} (u[k] - u^\*[k])
\]

Recommended:
\[
T_{aw,b} \approx T_{i,b}
\]

**Only integrate when braking is enabled** (or when \(e_{\text{cool}}^+>0\)).

---

## 6) Integrating with Existing Heating/Cooling Logic

You now have 3 operating regimes:

### Regime 1: Heating (active control)
Condition:
\[
g_{\text{sp}} \ge 0
\]
Action:
- Run your existing **inner gradient PI** to compute heater \(u\)
- Brake controller OFF (reset/freeze brake integrator)

### Regime 2: Passive cooling (not too fast)
Condition:
\[
g_{\text{sp}} < 0 \quad \text{and} \quad \hat g_f \ge g_{\min}
\]
Action:
- Set heater command: \(u = 0\)
- Brake OFF (or armed but not enabled)
- Freeze/unwind the inner PI integrator so it doesn’t wind up at \(u=0\)

### Regime 3: Cooling too fast (apply brake)
Condition:
\[
g_{\text{sp}} < 0 \quad \text{and} \quad \hat g_f < g_{\min}
\]
Action:
- Set heater command: \(u = u_{\text{brake}}\)
- Inner gradient PI is **disabled** (or bypassed) in cooling mode
- Brake controller ON

**Why disable inner PI during cooling?**  
Because the plant cannot produce negative power; trying to track negative \(g_{\text{sp}}\) directly causes windup or nonphysical outputs.

---

## 7) Reference Implementation (C-style, P + hysteresis brake)

```c
typedef enum {
    MODE_HEAT = 0,
    MODE_COOL_PASSIVE,
    MODE_COOL_BRAKE
} ControlMode;

typedef struct {
    float g_min;          // allowed cooling limit (negative), e.g. -1 °C/min (or °C/s)
    float dg_hyst;        // hysteresis band, positive
    float Kb;             // brake proportional gain: u / (gradient units)
    float u_brake_max;    // cap brake power (e.g. 0.05..0.2 of full scale)
    int   brake_enabled;  // latch/hysteresis state
} CoolingBrake;

// clamp helper
static inline float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

// Updates brake state + output based on filtered gradient g_f
float cooling_brake_update(CoolingBrake *br, float g_f)
{
    // Safety: if we're already warming, brake must be off
    if (g_f > 0.0f) {
        br->brake_enabled = 0;
        return 0.0f;
    }

    // Hysteresis enable/disable
    if (!br->brake_enabled) {
        if (g_f < (br->g_min - br->dg_hyst)) {
            br->brake_enabled = 1;
        }
    } else {
        if (g_f > (br->g_min + br->dg_hyst)) {
            br->brake_enabled = 0;
        }
    }

    if (!br->brake_enabled) {
        return 0.0f;
    }

    // Violation amount (positive when cooling too fast)
    float e_cool = br->g_min - g_f;       // >0 => too negative slope
    if (e_cool < 0.0f) e_cool = 0.0f;

    // Brake power (heater as brake)
    float u_brake = br->Kb * e_cool;
    u_brake = clampf(u_brake, 0.0f, br->u_brake_max);

    return u_brake;
}

8) Full Integration Skeleton

Assume you already compute:

outer loop gradient request 
𝑔
sp
g
sp
	​

 (can be negative for cooling ramps)

filtered gradient measurement 
𝑔
^
𝑓
g
^
	​

f
	​


inner PI computes heating power when active


// Inputs each cycle: T_set, T_meas, g_sp (from outer loop), g_f (filtered gradient)
// Output: heater command u

if (g_sp >= 0.0f) {
    // ===== Heating mode =====
    mode = MODE_HEAT;

    // Brake off
    brake.brake_enabled = 0;

    // Inner gradient PI active:
    // u = inner_gradient_pi_update(g_sp, g_f, ...);
    u = inner_pi_output;

} else {
    // ===== Cooling requested =====
    // Determine if natural cooling is too fast vs allowed limit g_min
    // (g_min is negative, e.g. -1 °C/min)

    float u_brake = cooling_brake_update(&brake, g_f);

    if (u_brake > 0.0f) {
        mode = MODE_COOL_BRAKE;
        u = u_brake;
    } else {
        mode = MODE_COOL_PASSIVE;
        u = 0.0f;
    }

    // IMPORTANT: prevent inner PI windup in cooling modes.
    // Options:
    // 1) freeze inner integrator
    // 2) unwind toward zero slowly
    // 3) set inner PI to a known neutral state
}
9) Tuning Recommendations (starting points)
Hysteresis band 
Δ
𝑔
Δg

Choose based on gradient noise after filtering:

small noise: 
Δ
𝑔
≈
0.05
Δg≈0.05 to 0.2\,^\circ C/\text{min}

larger noise: increase 
Δ
𝑔
Δg

Brake max power 
𝑢
brake,max
u
brake,max
	​


Start small:

5%–10% of full heater power
Increase only if the brake cannot slow cooling enough.

Brake gain 
𝐾
𝑏
K
b
	​


Start low; increase until cooling rate is held near 
𝑔
min
⁡
g
min
	​

 without oscillation.
If you observe “toggle / chatter”, reduce 
𝐾
𝑏
K
b
	​

 and/or increase 
Δ
𝑔
Δg.

Critical: kiln delay + filtered derivative means you should tune conservatively.

10) Safety & Practical Constraints

Use time-proportioning (SSR window) appropriate for your hardware (e.g. 1–10 s).

Never allow braking to flip into heating:

if 
𝑔
^
𝑓
>
0
g
^
	​

f
	​

>0, brake off

Consider adding a temperature ceiling during braking (optional):

if 
𝑇
>
𝑇
set
+
Δ
𝑇
safe
T>T
set
	​

+ΔT
safe
	​

, brake off