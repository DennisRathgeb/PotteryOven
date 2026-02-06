# SSR Windowing (Time-Proportioning Control) for a Kiln  
## + Handling “Near 0 / Near 1” Duty with a Minimum Pulse Clamp (Standard Fix)

This note explains **SSR windowing** (also called **time-proportioning control**) and the standard practical fix for tiny ON/OFF blips when the duty cycle is close to 0 or 1.

---

## 1) Why SSR Windowing?

An SSR driving a heater can only be **ON or OFF**.  
But you want “30% power”, “72% power”, etc.

For a **slow thermal plant** (kiln), you don’t need fast switching. The kiln responds over **minutes**, so it mainly reacts to **average power**.

So instead of switching rapidly (PWM), you use **windows**:

- Choose a **window length** \(T_w\) (e.g. 20 s).
- Every window, decide how long the SSR should be ON.

This drastically reduces switching stress on SSRs and still gives good temperature/gradient control.

---

## 2) Core Windowing Math

Let the controller output be:

\[
u \in [0,1]
\]

Interpret \(u\) as the **requested average power fraction** over the next window.

For a window of length \(T_w\):

\[
t_{\text{on}} = u \, T_w
\]
\[
t_{\text{off}} = T_w - t_{\text{on}}
\]

Schedule:
- SSR ON for \(t_{\text{on}}\)
- SSR OFF for \(t_{\text{off}}\)

This produces an average power approximately \(u \cdot P_{\max}\) over that window.

---

## 3) The Edge Case: Tiny Blips Near 0 or 1

Example with \(T_w = 20\) s and \(u = 0.98\):

\[
t_{\text{on}} = 19.6\text{ s}, \quad t_{\text{off}} = 0.4\text{ s}
\]

That 0.4 s OFF blip:
- contributes almost nothing thermally
- causes an unnecessary SSR switching event (stress/EMI)

Similarly for \(u = 0.02\), you’d get a tiny ON blip.

---

## 4) Standard Fix: Minimum Pulse Clamp (“Minimum ON/OFF time”)

Choose a minimum meaningful switching time \(T_{\min}\) (SSR-friendly), e.g. **2–5 s**.

Define:

\[
u_{\min} = \frac{T_{\min}}{T_w}
\]

Clamp rules:

- If \(t_{\text{on}} < T_{\min}\) ⇒ treat as **fully OFF**: set \(u = 0\)
- If \(t_{\text{off}} < T_{\min}\) ⇒ treat as **fully ON**: set \(u = 1\)
- Otherwise ⇒ keep the computed duty

Equivalent in duty form:

- If \(u < u_{\min}\): set \(u = 0\)
- If \(u > 1-u_{\min}\): set \(u = 1\)
- Else: keep \(u\)

### Example
- \(T_w = 20\) s
- \(T_{\min} = 2\) s
- \(u_{\min} = 2/20 = 0.1\)

So:
- \(u < 0.1\) becomes 0
- \(u > 0.9\) becomes 1

No tiny pulses are generated.

---

## 5) Practical Execution Model

Use three “rates” (conceptually):

1. **Temperature sampling** (often 1 s): read sensor, estimate gradient  
2. **Controller update** (often 1–5 s): compute new \(u\)  
3. **SSR window execution** (e.g. 20 s): apply power as ON/OFF timing

Even if the controller computes \(u\) faster, you only **commit** a new window plan every \(T_w\).

---

## 6) Reference Implementation (C-style Pseudocode)

### Data structure

```c
typedef struct {
    // window configuration
    uint32_t Tw_s;        // window length in seconds (e.g. 20)
    uint32_t Tmin_s;      // minimum on/off time in seconds (e.g. 2)

    // current window state
    uint32_t win_start_s; // start time (seconds)
    uint32_t ton_s;       // ON duration for the current window (seconds)
    uint8_t  is_on;       // SSR state in this moment (0/1)

    // cached duty for debugging/logging
    float u_win;
} SSRWindow;
