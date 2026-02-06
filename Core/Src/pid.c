/**
 * @file pid.c
 * @brief Implementation of gradient controller for kiln temperature rate control
 * @author Dennis Rathgeb
 * @date Apr 1, 2024
 *
 * Implements a PI gradient controller using Q16.16 fixed-point arithmetic.
 * Controls temperature rate of change (gradient) for ceramic firing profiles.
 */

#include "pid.h"
#include <stdio.h>

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

/*============================================================================*/
/* Temperature Controller (Outer P-Loop, Heating-Only)                         */
/*============================================================================*/

/**
 * @brief Initialize temperature controller with default parameters
 * @param[out] htc Pointer to temperature controller handle
 */
void TemperatureController_Init(TemperatureController_HandleTypeDef_t *htc)
{
    if (htc == NULL) {
        return;
    }
    htc->Kp_T = TC_DEFAULT_KP_T;
    htc->g_max = TC_DEFAULT_G_MAX;
    htc->T_band_mdeg = TC_DEFAULT_T_BAND_MDEG;
    TemperatureController_Reset(htc);
}

/**
 * @brief Set the temperature target and maximum gradient
 * @param[in,out] htc Pointer to temperature controller handle
 * @param[in] T_set_mdeg Temperature setpoint in milli-degrees C
 * @param[in] g_max_q16 Maximum gradient in °C/s as Q16.16
 * @param[in] is_cooling 1 for cooling step, 0 for heating step
 */
void TemperatureController_SetTarget(TemperatureController_HandleTypeDef_t *htc,
                                      int16_t T_set_mdeg, q16_t g_max_q16,
                                      uint8_t is_cooling)
{
    if (htc == NULL) {
        return;
    }
    htc->T_set_mdeg = T_set_mdeg;
    htc->g_max = g_max_q16;
    htc->is_cooling = is_cooling;
}

/**
 * @brief Update temperature controller with new measurement
 * @param[in,out] htc Pointer to temperature controller handle
 * @param[in] T_meas_mdeg Current temperature in milli-degrees C
 * @return Gradient setpoint in °C/s as Q16.16 for inner controller
 */
q16_t TemperatureController_Update(TemperatureController_HandleTypeDef_t *htc,
                                    int16_t T_meas_mdeg)
{
    if (htc == NULL || !htc->enabled) {
        return Q16_ZERO;
    }

    /* Cooling steps: just command zero gradient (natural cooling) */
    if (htc->is_cooling) {
        return Q16_ZERO;
    }

    int32_t e_T_mdeg = (int32_t)htc->T_set_mdeg - (int32_t)T_meas_mdeg;

    /* At or above setpoint: cannot cool */
    if (e_T_mdeg <= 0) {
        return Q16_ZERO;
    }

    /* Within deadband: hold */
    if (e_T_mdeg < (int32_t)htc->T_band_mdeg) {
        return Q16_ZERO;
    }

    /* P control: g_sp = Kp_T * e_T (milli-deg -> deg: /1000) */
    q16_t g_sp = (q16_t)(((int64_t)htc->Kp_T * e_T_mdeg) / 1000);

    /* Clamp to [0, g_max] */
    if (g_sp > htc->g_max) {
        g_sp = htc->g_max;
    }

    return g_sp;
}

/**
 * @brief Check if temperature is at target
 * @param[in] htc Pointer to temperature controller handle
 * @param[in] T_meas_mdeg Current temperature in milli-degrees C
 * @return 1 if at target, 0 otherwise
 */
uint8_t TemperatureController_AtTarget(TemperatureController_HandleTypeDef_t *htc,
                                        int16_t T_meas_mdeg)
{
    if (htc == NULL) {
        return 0;
    }

    int32_t e_T_mdeg = (int32_t)htc->T_set_mdeg - (int32_t)T_meas_mdeg;

    if (htc->is_cooling) {
        /* Cooling: at target when T <= T_set */
        return (e_T_mdeg >= 0);
    }
    /* Heating: within deadband */
    return (e_T_mdeg < (int32_t)htc->T_band_mdeg);
}

/**
 * @brief Reset temperature controller state
 * @param[in,out] htc Pointer to temperature controller handle
 */
void TemperatureController_Reset(TemperatureController_HandleTypeDef_t *htc)
{
    if (htc == NULL) {
        return;
    }
    htc->T_set_mdeg = 0;
    htc->is_cooling = 0;
    htc->enabled = 0;
}
