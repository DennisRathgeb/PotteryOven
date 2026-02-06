/**
 * @file pid.c
 * @brief Implementation of PID controller for temperature control
 * @author Dennis Rathgeb
 * @date Apr 1, 2024
 *
 * Implements a discrete PID controller with derivative filtering
 * and hysteresis-based bang-bang output for heater control.
 *
 * @note Internal state variables are static, preventing multiple
 *       independent PID controller instances.
 */

#include "pid.h"
#include <stdio.h>

/** @brief Accumulated integral term */
static float32_t integral = 0.0f;

/** @brief Previous error for derivative calculation */
static float32_t last_error = 0.0f;

/** @brief Previous filtered derivative value */
static float32_t last_derivative = 0.0f;

/**
 * @brief Initialize PID controller parameters
 * @param[out] hpid Pointer to PID handle to initialize
 * @param[in] k_p Proportional gain
 * @param[in] k_i Integral gain
 * @param[in] k_d Derivative gain
 * @param[in] hysteresis Hysteresis threshold for ON/OFF switching
 * @param[in] k_d_filter_coeff Derivative low-pass filter coefficient (0-1)
 *
 * Sets all PID tuning parameters. Does not reset internal state
 * (integral, last_error, last_derivative).
 */
void PID_Init(PID_HandletypeDef_t *hpid, float32_t k_p, float32_t k_i, float32_t k_d,
              float32_t hysteresis, float32_t k_d_filter_coeff) {
    hpid->k_proportional = k_p;
    hpid->k_integral = k_i;
    hpid->k_derivative = k_d;
    hpid->hysteresis = hysteresis;
    hpid->derivative_filter_coeff = k_d_filter_coeff;
}

/**
 * @brief Update PID controller gains at runtime
 * @param[in,out] hpid Pointer to PID handle
 * @param[in] k_p New proportional gain
 * @param[in] k_i New integral gain
 * @param[in] k_d New derivative gain
 * @param[in] hysteresis New hysteresis threshold
 *
 * Allows changing PID parameters during operation for tuning.
 * Internal state is preserved.
 */
void PID_UpdateParameters(PID_HandletypeDef_t *hpid, float32_t k_p, float32_t k_i,
                          float32_t k_d, float32_t hysteresis) {
    hpid->k_proportional = k_p;
    hpid->k_integral = k_i;
    hpid->k_derivative = k_d;
    hpid->hysteresis = hysteresis;
}

/**
 * @brief Calculate PID output with hysteresis
 * @param[in] hpid Pointer to PID handle
 * @param[in] current_temperature Current measured temperature in degrees C
 * @param[in] setpoint Target temperature in degrees C
 * @return PID_OUTPUT_ON if heater should activate, PID_OUTPUT_OFF otherwise
 *
 * PID Algorithm:
 * 1. Calculate error: error = setpoint - current_temperature
 * 2. Update integral: integral += error
 * 3. Calculate derivative: derivative = error - last_error
 * 4. Apply derivative filtering: filtered_derivative = (1-alpha)*last + alpha*current
 * 5. Calculate output: output = Kp*error + Ki*integral + Kd*derivative
 * 6. Apply hysteresis:
 *    - output > hysteresis: return ON
 *    - output < -hysteresis: return OFF
 *    - otherwise: return OFF (stay in current state)
 *
 * The derivative filtering reduces sensitivity to measurement noise.
 * Filter coefficient of 1.0 means no filtering, 0.0 means full filtering.
 */
PID_Controller_Output_t PID_CalculateOutput(PID_HandletypeDef_t *hpid,
                                            float32_t current_temperature,
                                            float32_t setpoint) {
    float32_t error = setpoint - current_temperature;
    integral += error;
    float32_t derivative = error - last_error;

    /* Apply first-order low-pass filter to derivative term */
    float32_t filtered_derivative = (1.0f - hpid->derivative_filter_coeff) * last_derivative +
                                    hpid->derivative_filter_coeff * derivative;
    last_derivative = filtered_derivative;
    last_error = error;

    /* Calculate PID output */
    float32_t output = hpid->k_proportional * error +
                       hpid->k_integral * integral +
                       hpid->k_derivative * derivative;

    /* Apply hysteresis for bang-bang control */
    if (output > hpid->hysteresis) {
        return PID_OUTPUT_ON;
    } else if (output < -hpid->hysteresis) {
        return PID_OUTPUT_OFF;
    } else {
        return PID_OUTPUT_OFF;  /* Default: stay off */
    }
}

/*============================================================================*/
/* Gradient Controller Implementation (PI with Anti-Windup, Fixed-Point)       */
/*============================================================================*/

/**
 * @brief Initialize gradient controller with default parameters
 * @param[out] hgc Pointer to gradient controller handle
 */
void GradientController_Init(GradientController_HandleTypeDef_t *hgc)
{
    if (NULL == hgc) {
        return;
    }

    /* Set default controller tuning parameters */
    hgc->Kc = GC_DEFAULT_KC;
    hgc->Ti_inv_Ts = GC_DEFAULT_TI_INV_TS;
    hgc->Taw_inv_Ts = GC_DEFAULT_TAW_INV_TS;

    /* Set default gradient estimation parameters */
    hgc->alpha = GC_DEFAULT_ALPHA;
    hgc->one_minus_alpha = GC_DEFAULT_ONE_M_ALPHA;
    hgc->Ts_ms = GC_DEFAULT_TS_MS;

    /* Set default output limits */
    hgc->u_min = GC_DEFAULT_U_MIN;
    hgc->u_max = GC_DEFAULT_U_MAX;

    /* Reset internal state */
    GradientController_Reset(hgc);
}

/**
 * @brief Set the gradient setpoint
 * @param[in,out] hgc Pointer to gradient controller handle
 * @param[in] g_sp_deg_per_sec Gradient setpoint in °C/s as Q16.16
 */
void GradientController_SetSetpoint(GradientController_HandleTypeDef_t *hgc,
                                     q16_t g_sp_deg_per_sec)
{
    if (NULL == hgc) {
        return;
    }
    hgc->g_sp = g_sp_deg_per_sec;
}

/**
 * @brief Reset gradient controller internal state
 * @param[in,out] hgc Pointer to gradient controller handle
 */
void GradientController_Reset(GradientController_HandleTypeDef_t *hgc)
{
    if (NULL == hgc) {
        return;
    }

    hgc->T_prev_mdeg = 0;
    hgc->g_f_prev = Q16_ZERO;
    hgc->I = Q16_ZERO;
    hgc->g_sp = Q16_ZERO;
    hgc->initialized = 0;
}

/**
 * @brief Update gradient controller with new temperature measurement
 * @param[in,out] hgc Pointer to gradient controller handle
 * @param[in] T_current_mdeg Current temperature in milli-degrees C
 * @return Controller output as Q16.16 (0-65536 = 0.0-1.0)
 *
 * Fixed-point PI controller with anti-windup for gradient control.
 */
q16_t GradientController_Update(GradientController_HandleTypeDef_t *hgc,
                                 int16_t T_current_mdeg)
{
    if (NULL == hgc) {
        return Q16_ZERO;
    }

    /* First call: just store temperature and return 0 */
    if (!hgc->initialized) {
        hgc->T_prev_mdeg = T_current_mdeg;
        hgc->initialized = 1;
        return Q16_ZERO;
    }

    /*
     * 1. Gradient estimate (milli-degrees / ms → °C/s as Q16.16)
     *
     * dT_mdeg = T_current - T_prev (in milli-degrees)
     * g_hat [°C/s] = dT_mdeg [mdeg] / Ts_ms [ms]
     *              = dT_mdeg * 0.001 / (Ts_ms * 0.001)
     *              = dT_mdeg / Ts_ms [°C/s]
     *
     * To Q16.16: g_hat_q16 = (dT_mdeg << 16) / Ts_ms
     * But dT_mdeg is in milli-degrees, so scale by 0.001:
     * g_hat_q16 = ((dT_mdeg << 16) / 1000) / Ts_ms * 1000
     *           = (dT_mdeg << 16) / Ts_ms
     *
     * Wait - that's not right. Let me recalculate:
     * dT [°C] = dT_mdeg / 1000
     * dt [s] = Ts_ms / 1000
     * g [°C/s] = dT / dt = (dT_mdeg / 1000) / (Ts_ms / 1000) = dT_mdeg / Ts_ms
     *
     * So: g_hat_q16 = (dT_mdeg << 16) / Ts_ms
     *
     * For 1000ms sample time and 1000 mdeg change: g = 1 °C/s = 65536 Q16.16
     * Check: (1000 << 16) / 1000 = 65536 ✓
     */
    int32_t dT_mdeg = (int32_t)T_current_mdeg - (int32_t)hgc->T_prev_mdeg;
    q16_t g_hat = (q16_t)(((int64_t)dT_mdeg << 16) / (int32_t)hgc->Ts_ms);

    /*
     * 2. EMA filter: g_f = alpha * g_f_prev + (1-alpha) * g_hat
     */
    q16_t g_f = Q16_MUL(hgc->alpha, hgc->g_f_prev) +
                Q16_MUL(hgc->one_minus_alpha, g_hat);

    /*
     * 3. Error: e = g_sp - g_f
     */
    q16_t e = hgc->g_sp - g_f;

    /*
     * 4. PI output (unsaturated): u* = Kc * (e + I)
     */
    q16_t u_star = Q16_MUL(hgc->Kc, e + hgc->I);

    /*
     * 5. Saturation: u = clamp(u*, u_min, u_max)
     */
    q16_t u = u_star;
    if (u < hgc->u_min) {
        u = hgc->u_min;
    }
    if (u > hgc->u_max) {
        u = hgc->u_max;
    }

    /*
     * 6. Anti-windup integrator update (back-calculation method):
     *    I += (Ts/Ti) * e + (Ts/Taw) * (u - u*)
     *
     * The (u - u*) term only contributes when saturated, pulling
     * the integrator back to prevent excessive windup.
     */
    hgc->I += Q16_MUL(hgc->Ti_inv_Ts, e) +
              Q16_MUL(hgc->Taw_inv_Ts, u - u_star);

    /*
     * 7. Store state for next iteration
     */
    hgc->T_prev_mdeg = T_current_mdeg;
    hgc->g_f_prev = g_f;

    return u;
}

/**
 * @brief Convert controller output to heater level (0-6)
 * @param[in] u Controller output as Q16.16 (0-65536)
 * @return Heater level 0-6
 *
 * Uses rounding: level = (u * 6 + 0.5) truncated
 * In fixed-point: level = (u * 6 + 32768) >> 16
 */
uint8_t GradientController_GetHeaterLevel(q16_t u)
{
    /* Clamp input to valid range */
    if (u < 0) {
        u = 0;
    }
    if (u > Q16_ONE) {
        u = Q16_ONE;
    }

    /*
     * Map [0, 65536] to [0, 6] with rounding
     * level = (u * 6 + 32768) >> 16
     *
     * Thresholds (with rounding):
     * u < 10923  → level 0
     * u < 21845  → level 1
     * u < 32768  → level 2
     * u < 43691  → level 3
     * u < 54613  → level 4
     * u < 65536  → level 5
     * u = 65536  → level 6
     */
    int32_t level = ((int32_t)u * 6 + 32768) >> 16;

    if (level < 0) {
        level = 0;
    }
    if (level > 6) {
        level = 6;
    }

    return (uint8_t)level;
}
