# Outer Temperature Loop (Heating-Only) for a Kiln with an Inner **Gradient PI**

You already have an **inner loop** that controls the temperature gradient

\[
g(t) = \frac{dT(t)}{dt}
\]

by driving the heater power \(u\) such that

\[
g(t) \approx g_{\mathrm{sp}}(t)
\]

This document defines the **outer loop** that controls **temperature** \(T(t)\) by generating the **gradient setpoint** \(g_{\mathrm{sp}}\).  
Cooling is **not available**, so negative gradients cannot be commanded.

---

## 1) Cascade structure and signals

### Variables
- \(T(t)\): measured kiln temperature \([^\circ C]\)
- \(T_{\mathrm{set}}(t)\): desired temperature setpoint \([^\circ C]\)
- \(g(t)=\frac{dT}{dt}\): measured/estimated temperature gradient \([^\circ C/s]\)
- \(g_{\mathrm{sp}}(t)\): gradient setpoint sent to the inner loop \([^\circ C/s]\)
- \(u(t)\): heater command (e.g. 0..1 or 0..100%)

### Errors
Outer loop temperature error:
\[
e_T(t) = T_{\mathrm{set}}(t) - T(t)
\]

Inner loop gradient error (already implemented):
\[
e_g(t) = g_{\mathrm{sp}}(t) - \hat g(t)
\]

---

## 2) What the outer loop must accomplish

You want to specify:
- a target temperature \(T_{\mathrm{set}}\)
- a maximum allowed ramp rate \(g_{\max}\) (user-defined “gradient”)

Desired behavior (heating-only):
- If far below setpoint: ramp at **exactly** \(g_{\max}\) (as much as inner loop can achieve)
- As you approach setpoint: reduce the ramp rate smoothly
- Near setpoint: command \(g_{\mathrm{sp}} = 0\) to avoid dithering/overshoot
- If above setpoint: command \(g_{\mathrm{sp}} = 0\) (cannot cool)

---

## 3) Outer-loop control law (recommended): **P → gradient command** with clamp + deadband

### 3.1 Proportional outer law
\[
g_{\mathrm{sp}}^\*(t) = K_{p,T}\, e_T(t)
\]

Where:
- \(K_{p,T}\) has units \(\frac{^\circ C/s}{^\circ C} = 1/s\) (or \(\frac{^\circ C/min}{^\circ C}=1/min\) if you work in minutes)

### 3.2 Heating-only clamp (no cooling)
Because you cannot command negative ramp:
\[
g_{\mathrm{sp}}(t) = \mathrm{clip}\big(g_{\mathrm{sp}}^\*(t),\,0,\,g_{\max}\big)
\]

### 3.3 Deadband near setpoint
To prevent dithering near target (sensor noise + thermal lag), enforce a stop band:

If
\[
0 \le e_T(t) < \Delta T_{\mathrm{band}}
\]
then set
\[
g_{\mathrm{sp}}(t)=0
\]

Also if \(e_T(t)\le 0\) (at/above setpoint):
\[
g_{\mathrm{sp}}(t)=0
\]

---

## 4) How to tune the outer-loop gain \(K_{p,T}\)

Pick a “slowdown distance” \(E_{\mathrm{sat}}\) in °C. This is the temperature error at which the outer loop should just reach the max ramp rate \(g_{\max}\).

We want:
\[
g_{\mathrm{sp}}^\* = g_{\max} \quad \text{when} \quad e_T = E_{\mathrm{sat}}
\]

So:
\[
K_{p,T} = \frac{g_{\max}}{E_{\mathrm{sat}}}
\]

**Interpretation**
- For \(e_T \ge E_{\mathrm{sat}}\), outer loop saturates at \(g_{\max}\) → constant ramp
- For \(0 < e_T < E_{\mathrm{sat}}\), ramp rate reduces linearly → smooth approach

**Typical choices**
- \(E_{\mathrm{sat}} \approx 10^\circ C\) to \(50^\circ C\), depending on kiln lag and how “no-overshoot” you want.
- Larger \(E_{\mathrm{sat}}\) = start slowing earlier = safer, less overshoot.
- Smaller \(E_{\mathrm{sat}}\) = later braking = tighter but riskier.

---

## 5) Unit consistency (important)

If your inner loop uses gradient in \(^\circ C/s\) but the user specifies \(^\circ C/min\):

Convert:
\[
g_{\max,s} = \frac{g_{\max,min}}{60}
\]

Then:
\[
K_{p,T,s} = \frac{g_{\max,s}}{E_{\mathrm{sat}}} = \frac{g_{\max,min}}{60\,E_{\mathrm{sat}}}
\]

(Or keep everything in \(^\circ C/min\) consistently.)

---

## 6) Discrete-time outer loop equations

Sampling time: \(T_s\)

At time step \(k\):

Temperature error:
\[
e_T[k]=T_{\mathrm{set}}[k]-T[k]
\]

Deadband + heating-only logic:
\[
g_{\mathrm{sp}}[k] =
\begin{cases}
0, & e_T[k]\le 0 \\
0, & 0 < e_T[k] < \Delta T_{\mathrm{band}} \\
\mathrm{clip}(K_{p,T}e_T[k], 0, g_{\max}), & e_T[k]\ge \Delta T_{\mathrm{band}}
\end{cases}
\]

Then send \(g_{\mathrm{sp}}[k]\) to the inner gradient PI controller.

---

## 7) Reference implementation (C-style)

```c
typedef struct {
    float Kp_T;     // outer P gain: (°C/s)/°C  (or (°C/min)/°C)
    float g_max;    // maximum ramp rate allowed (same units as inner g_sp)
    float T_band;   // deadband near setpoint [°C]
} OuterTempLoop;

static inline float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

// Heating-only outer loop: computes gradient setpoint for inner loop
float outer_temp_to_gradient_sp(const OuterTempLoop *ctl,
                                float T_set,
                                float T_meas)
{
    float eT = T_set - T_meas;     // [°C]

    // At or above setpoint -> cannot cool -> request zero gradient
    if (eT <= 0.0f) {
        return 0.0f;
    }

    // Deadband: close to target -> stop ramping
    if (eT < ctl->T_band) {
        return 0.0f;
    }

    // Proportional law -> gradient demand
    float g_sp_star = ctl->Kp_T * eT;

    // Enforce user ramp limit
    float g_sp = clampf(g_sp_star, 0.0f, ctl->g_max);

    return g_sp;
}

/*
Tuning:
Pick E_sat [°C] = error where you want full ramp (g_max) to begin braking.
Kp_T = g_max / E_sat

Example (working in °C/min):
  g_max = 2.0 °C/min
  E_sat = 20 °C
  Kp_T = 0.1 (°C/min)/°C

If inner loop uses °C/s:
  g_max_s = g_max_min / 60
  Kp_T_s  = Kp_T_min / 60
*/
) Practical notes

Start with outer P only. Outer PI is often unnecessary because the inner loop already removes most disturbances by regulating slope.

Overshoot usually comes from thermal lag. Fix it by:

increasing 
𝐸
s
a
t
E
sat
	​

 (earlier braking),

increasing 
Δ
𝑇
b
a
n
d
ΔT
band
	​

,

or reducing 
𝑔
max
⁡
g
max
	​

 near the top end of the kiln range (gain scheduling later).

For a heating-only kiln: if the process overshoots, the best the controller can do is command 
𝑔
s
p
=
0
g
sp
	​

=0 and wait for natural cooling.

Summary

Outer loop:

controls temperature by generating a gradient command

clamps gradient to 
[
0
,
𝑔
max
⁡
]
[0,g
max
	​

] (heating-only)

adds a deadband near target for stability

Inner loop (already implemented):

enforces 
𝑔
≈
𝑔
s
p
g≈g
sp
	​


Together:

ramps to 
𝑇
s
e
t
T
set
	​

 at user-defined slope,

automatically slows down,

stops and holds near setpoint without dithering.