# Inner Loop PI Controller for **Temperature Gradient Control** (Kiln / Pottery Oven)

This document explains and specifies an **inner-loop PI controller that directly controls the temperature gradient**  
\[
g(t) = \frac{dT}{dt}
\]
The absolute temperature setpoint will be handled later by an outer loop (cascade control).  
This is intentional and assumed.

The design uses:
- gradient as the controlled variable
- PI control with anti-windup
- lambda (IMC) tuning
- identification from a **limited power step (0 → 30%)**
- discrete-time implementation

---

## 1. Signals and Definitions

- Actuator input  
  \( u(t) \in [u_{\min}, u_{\max}] \)  
  Heater command (e.g. 0–1 or 0–100%)

- Measured temperature  
  \( T(t) \) [°C]

- Controlled variable (inner loop)  
  \[
  g(t) = \frac{dT(t)}{dt} \quad [^\circ C/s]
  \]

- Inner-loop setpoint  
  \( g_{\mathrm{sp}}(t) \) (desired ramp rate)

- Control error  
  \[
  e_g(t) = g_{\mathrm{sp}}(t) - \hat g(t)
  \]

---

## 2. Gradient Estimation (Required)

Direct differentiation amplifies noise. We therefore:
1. compute a discrete derivative
2. apply a first-order low-pass filter

### 2.1 Discrete derivative
With sampling time \( T_s \):
\[
\hat g[k] = \frac{T[k] - T[k-1]}{T_s}
\]

### 2.2 Low-pass filtering (EMA)
\[
\hat g_f[k] = \alpha \hat g_f[k-1] + (1-\alpha)\hat g[k]
\]

- \( 0 < \alpha < 1 \)
- larger \( \alpha \) → more smoothing → more delay

Approximate filter time constant:
\[
\tau_f \approx \frac{T_s \alpha}{1-\alpha}
\]

**Important:** This filter delay becomes part of the effective plant delay.

---

## 3. Identifying the Gradient Plant (Step Test)

Even though temperature is an integral of power, the **gradient behaves locally like a first-order system** due to losses and thermal inertia.

We model:
\[
G_g(s) = \frac{g(s)}{u(s)} = \frac{K_g}{\tau_g s + 1} e^{-L_g s}
\]

### 3.1 Step experiment
Apply a step:
\[
u: u_0 \rightarrow u_1,\quad \Delta u = u_1 - u_0
\]
(e.g. 0 → 0.30)

Record \( T(t) \), compute \( \hat g_f(t) \).

### 3.2 Gradient gain
Let:
- \( g_0 \) = mean gradient before step
- \( g_\infty \) = mean gradient after it settles

\[
K_g = \frac{g_\infty - g_0}{\Delta u}
\]

### 3.3 Time constant and delay (28% / 63% method)

Normalize gradient response:
\[
y_g(t) = \frac{\hat g_f(t) - g_0}{g_\infty - g_0}
\]

Find:
- \( t_{28} \): \( y_g = 0.283 \)
- \( t_{63} \): \( y_g = 0.632 \)

Compute:
\[
\tau_g = 1.5 (t_{63} - t_{28})
\]
\[
L_g = t_{63} - \tau_g
\]

\( L_g \) includes:
- thermal delay
- sensor delay
- gradient filter delay

This is desired.

---

## 4. PI Controller Structure (Inner Loop)

Continuous-time PI:
\[
C_g(s) = K_c \left( 1 + \frac{1}{T_i s} \right)
\]

- \( K_c \): proportional gain
- \( T_i \): integral time constant
- \( K_i = \frac{K_c}{T_i} \)

---

## 5. Lambda (IMC) Tuning for Gradient Loop

Choose desired closed-loop time constant \( \lambda_g \).

Recommended:
\[
\lambda_g = 3L_g \text{ to } 10L_g \quad (\text{start with } 5L_g)
\]

### PI tuning formulas (FOPDT)
\[
K_c = \frac{\tau_g}{K_g(\lambda_g + L_g)}
\]
\[
T_i = \tau_g
\]

**Interpretation**
- Larger \( \lambda_g \): smoother, more robust, slower
- Delay and noise directly reduce usable gain
- Integral action operates on natural gradient timescale

---

## 6. Discrete-Time PI with Anti-Windup

### 6.1 Error
\[
e[k] = g_{\mathrm{sp}}[k] - \hat g_f[k]
\]

### 6.2 Unsaturated controller output
\[
u^\*[k] = K_c \left( e[k] + I[k-1] \right)
\]

### 6.3 Actuator saturation
\[
u[k] = \mathrm{clip}(u^\*[k], u_{\min}, u_{\max})
\]

---

## 7. Anti-Windup (Back-Calculation)

Required because saturation is common during ramps.

Integrator update:
\[
I[k] = I[k-1]
      + \frac{T_s}{T_i} e[k]
      + \frac{T_s}{T_{aw}} (u[k] - u^\*[k])
\]

- \( T_{aw} \approx T_i \) (or \( T_i/2 \) for stronger unwinding)
- When not saturated: \( u = u^\* \Rightarrow \) normal PI
- When saturated: integrator is driven back toward consistency

---

## 8. Complete Discrete Algorithm (Reference Implementation)

```c
// 1) gradient estimate
g_hat = (T[k] - T[k-1]) / Ts;

// 2) gradient filter
g_f = alpha * g_f_prev + (1 - alpha) * g_hat;

// 3) error
e = g_sp - g_f;

// 4) PI (unsaturated)
u_star = Kc * (e + I);

// 5) saturation
u = clamp(u_star, u_min, u_max);

// 6) anti-windup integrator update
I += (Ts / Ti) * e + (Ts / T_aw) * (u - u_star);
# Inner Loop PI Controller for **Temperature Gradient Control** (Kiln / Pottery Oven)

This document explains and specifies an **inner-loop PI controller that directly controls the temperature gradient**  
\[
g(t) = \frac{dT}{dt}
\]
The absolute temperature setpoint will be handled later by an outer loop (cascade control).  
This is intentional and assumed.

The design uses:
- gradient as the controlled variable
- PI control with anti-windup
- lambda (IMC) tuning
- identification from a **limited power step (0 → 30%)**
- discrete-time implementation

---

## 1. Signals and Definitions

- Actuator input  
  \( u(t) \in [u_{\min}, u_{\max}] \)  
  Heater command (e.g. 0–1 or 0–100%)

- Measured temperature  
  \( T(t) \) [°C]

- Controlled variable (inner loop)  
  \[
  g(t) = \frac{dT(t)}{dt} \quad [^\circ C/s]
  \]

- Inner-loop setpoint  
  \( g_{\mathrm{sp}}(t) \) (desired ramp rate)

- Control error  
  \[
  e_g(t) = g_{\mathrm{sp}}(t) - \hat g(t)
  \]

---

## 2. Gradient Estimation (Required)

Direct differentiation amplifies noise. We therefore:
1. compute a discrete derivative
2. apply a first-order low-pass filter

### 2.1 Discrete derivative
With sampling time \( T_s \):
\[
\hat g[k] = \frac{T[k] - T[k-1]}{T_s}
\]

### 2.2 Low-pass filtering (EMA)
\[
\hat g_f[k] = \alpha \hat g_f[k-1] + (1-\alpha)\hat g[k]
\]

- \( 0 < \alpha < 1 \)
- larger \( \alpha \) → more smoothing → more delay

Approximate filter time constant:
\[
\tau_f \approx \frac{T_s \alpha}{1-\alpha}
\]

**Important:** This filter delay becomes part of the effective plant delay.

---

## 3. Identifying the Gradient Plant (Step Test)

Even though temperature is an integral of power, the **gradient behaves locally like a first-order system** due to losses and thermal inertia.

We model:
\[
G_g(s) = \frac{g(s)}{u(s)} = \frac{K_g}{\tau_g s + 1} e^{-L_g s}
\]

### 3.1 Step experiment
Apply a step:
\[
u: u_0 \rightarrow u_1,\quad \Delta u = u_1 - u_0
\]
(e.g. 0 → 0.30)

Record \( T(t) \), compute \( \hat g_f(t) \).

### 3.2 Gradient gain
Let:
- \( g_0 \) = mean gradient before step
- \( g_\infty \) = mean gradient after it settles

\[
K_g = \frac{g_\infty - g_0}{\Delta u}
\]

### 3.3 Time constant and delay (28% / 63% method)

Normalize gradient response:
\[
y_g(t) = \frac{\hat g_f(t) - g_0}{g_\infty - g_0}
\]

Find:
- \( t_{28} \): \( y_g = 0.283 \)
- \( t_{63} \): \( y_g = 0.632 \)

Compute:
\[
\tau_g = 1.5 (t_{63} - t_{28})
\]
\[
L_g = t_{63} - \tau_g
\]

\( L_g \) includes:
- thermal delay
- sensor delay
- gradient filter delay

This is desired.

---

## 4. PI Controller Structure (Inner Loop)

Continuous-time PI:
\[
C_g(s) = K_c \left( 1 + \frac{1}{T_i s} \right)
\]

- \( K_c \): proportional gain
- \( T_i \): integral time constant
- \( K_i = \frac{K_c}{T_i} \)

---

## 5. Lambda (IMC) Tuning for Gradient Loop

Choose desired closed-loop time constant \( \lambda_g \).

Recommended:
\[
\lambda_g = 3L_g \text{ to } 10L_g \quad (\text{start with } 5L_g)
\]

### PI tuning formulas (FOPDT)
\[
K_c = \frac{\tau_g}{K_g(\lambda_g + L_g)}
\]
\[
T_i = \tau_g
\]

**Interpretation**
- Larger \( \lambda_g \): smoother, more robust, slower
- Delay and noise directly reduce usable gain
- Integral action operates on natural gradient timescale

---

## 6. Discrete-Time PI with Anti-Windup

### 6.1 Error
\[
e[k] = g_{\mathrm{sp}}[k] - \hat g_f[k]
\]

### 6.2 Unsaturated controller output
\[
u^\*[k] = K_c \left( e[k] + I[k-1] \right)
\]

### 6.3 Actuator saturation
\[
u[k] = \mathrm{clip}(u^\*[k], u_{\min}, u_{\max})
\]

---

## 7. Anti-Windup (Back-Calculation)

Required because saturation is common during ramps.

Integrator update:
\[
I[k] = I[k-1]
      + \frac{T_s}{T_i} e[k]
      + \frac{T_s}{T_{aw}} (u[k] - u^\*[k])
\]

- \( T_{aw} \approx T_i \) (or \( T_i/2 \) for stronger unwinding)
- When not saturated: \( u = u^\* \Rightarrow \) normal PI
- When saturated: integrator is driven back toward consistency

---

## 8. Complete Discrete Algorithm (Reference Implementation)

```c
// 1) gradient estimate
g_hat = (T[k] - T[k-1]) / Ts;

// 2) gradient filter
g_f = alpha * g_f_prev + (1 - alpha) * g_hat;

// 3) error
e = g_sp - g_f;

// 4) PI (unsaturated)
u_star = Kc * (e + I);

// 5) saturation
u = clamp(u_star, u_min, u_max);

// 6) anti-windup integrator update
I += (Ts / Ti) * e + (Ts / T_aw) * (u - u_star);
9. Why This Architecture Is Correct

Inner loop regulates energy flow rate, not accumulated energy

Disturbances (load, airflow, losses) act directly on gradient and are rejected early

Outer loop later adjusts 
𝑔
s
p
g
sp
	​

 to meet absolute temperature targets

Lambda tuning provides predictable robustness despite slow, delayed dynamics

10. Notes on Limited Step Amplitude (0 → 30%)

Small steps are acceptable if the plant is locally linear

Identification must be done in the same operating region

Gradient-based identification avoids needing full thermal saturation

Conservative 
𝜆
𝑔
λ
g
	​

 increases robustness to modeling error

Summary

This controller is a true gradient controller, not a disguised temperature controller.
It forms a clean, robust inner loop suitable for cascade control and slow thermal systems.

Outer-loop temperature control can be added later without changing this design.