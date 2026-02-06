/**
 * @file pid.h
 * @brief Gradient controller module for kiln temperature rate control
 * @author Dennis Rathgeb
 * @date Apr 1, 2024
 *
 * Implements a PI gradient controller for controlling temperature rate
 * of change (dT/dt) using Q16.16 fixed-point arithmetic optimized for
 * Cortex-M0 (no hardware FPU).
 *
 * Features:
 * - Gradient estimation via discrete derivative with EMA filtering
 * - PI control with back-calculation anti-windup
 * - Output saturation to [0, 1] mapped to heater levels 0-6
 * - Runtime tunable parameters (Kc, Ti, alpha)
 */

#ifndef INC_PID_H_
#define INC_PID_H_

#include "stm32f0xx_hal.h"
#include "arm_math.h"
#include <stdint.h>

/*============================================================================*/
/* Q16.16 Fixed-Point Arithmetic (No FPU on Cortex-M0)                        */
/*============================================================================*/

/** @brief Q16.16 fixed-point type (32-bit signed)
 *  Upper 16 bits: integer part, Lower 16 bits: fractional part
 *  Range: -32768.0 to +32767.99998, Precision: ~0.000015 (1/65536)
 */
typedef int32_t q16_t;

/** @brief Convert float to Q16.16 (use sparingly - involves float math) */
#define Q16_FROM_FLOAT(f)   ((q16_t)((f) * 65536.0f))

/** @brief Convert Q16.16 to float (for debugging/logging only) */
#define Q16_TO_FLOAT(q)     ((float)(q) / 65536.0f)

/** @brief Convert int to Q16.16 */
#define Q16_FROM_INT(i)     ((q16_t)((i) << 16))

/** @brief Q16.16 multiplication: (a * b) >> 16 using 64-bit intermediate */
#define Q16_MUL(a, b)       ((q16_t)(((int64_t)(a) * (int64_t)(b)) >> 16))

/** @brief Q16.16 division: (a << 16) / b using 64-bit intermediate */
#define Q16_DIV(a, b)       ((q16_t)(((int64_t)(a) << 16) / (b)))

/** @brief Q16.16 constant for 1.0 */
#define Q16_ONE             (65536)

/** @brief Q16.16 constant for 0.0 */
#define Q16_ZERO            (0)

/*============================================================================*/
/* Gradient Controller Default Constants                                       */
/*============================================================================*/

/**
 * Pre-computed controller parameters based on plant model:
 * - Kg = 0.01 °C/s per unit power (gradient gain)
 * - tau_g = 60 seconds (time constant)
 * - Lg = 10 seconds (delay)
 * - lambda_g = 50 seconds (5 * Lg, closed-loop time constant)
 *
 * Controller gains:
 * - Kc = tau_g / (Kg * (lambda_g + Lg)) = 60 / (0.01 * 60) = 100.0
 * - Ti = tau_g = 60s
 * - Taw = Ti = 60s (anti-windup time constant)
 */

/** @brief Proportional gain Kc = 100.0 as Q16.16 */
#define GC_DEFAULT_KC           Q16_FROM_INT(100)

/** @brief Pre-computed Ts/Ti = 1/60 ≈ 0.0167 as Q16.16 (for 1s sample time) */
#define GC_DEFAULT_TI_INV_TS    1092

/** @brief Pre-computed Ts/Taw = 1/60 ≈ 0.0167 as Q16.16 */
#define GC_DEFAULT_TAW_INV_TS   1092

/** @brief EMA filter coefficient alpha = 0.8 as Q16.16 */
#define GC_DEFAULT_ALPHA        52429

/** @brief Pre-computed (1 - alpha) = 0.2 as Q16.16 */
#define GC_DEFAULT_ONE_M_ALPHA  13107

/** @brief Default sample time in milliseconds */
#define GC_DEFAULT_TS_MS        1000

/** @brief Default output minimum (0.0 as Q16.16) */
#define GC_DEFAULT_U_MIN        Q16_ZERO

/** @brief Default output maximum (1.0 as Q16.16) */
#define GC_DEFAULT_U_MAX        Q16_ONE

/*============================================================================*/
/* Control Mode (Heating vs Cooling States)                                    */
/*============================================================================*/

/**
 * @brief Control mode for heater operation
 *
 * Determines which controller is active:
 * - HEAT: Inner gradient PI active, brake off
 * - COOL_PASSIVE: Heater off, cooling within limits
 * - COOL_BRAKE: Brake controller active, slowing cooling rate
 */
typedef enum {
    CTRL_MODE_HEAT = 0,         /**< Heating: inner PI active, brake off */
    CTRL_MODE_COOL_PASSIVE,     /**< Passive cooling: heater off, within limits */
    CTRL_MODE_COOL_BRAKE        /**< Cooling too fast: brake active */
} ControlMode_t;

/*============================================================================*/
/* Temperature Controller Default Constants                                    */
/*============================================================================*/

/** Tuning: Kp_T = g_max / E_sat
 *  Default: g_max = 100°C/h = 0.0278°C/s, E_sat = 30°C
 *  Kp_T = 0.000926 /s ≈ 61 in Q16.16
 */
#define TC_DEFAULT_KP_T         61
#define TC_DEFAULT_G_MAX        1820    /* 100°C/h in Q16.16 °C/s */
#define TC_DEFAULT_T_BAND_MDEG  5000    /* 5°C deadband */
#define TC_DEFAULT_E_SAT_DEG    30      /* slowdown distance */

/*============================================================================*/
/* Cooling Brake Controller Default Constants                                  */
/*============================================================================*/

/** Default cooling limit: -100°C/h = -0.0278°C/s ≈ -1820 in Q16.16 */
#define CB_DEFAULT_G_MIN        (-1820)

/** Hysteresis band: 0.1°C/min = 0.00167°C/s ≈ 109 in Q16.16 */
#define CB_DEFAULT_DG_HYST      109

/** Brake gain Kb: u per (°C/s) - maps gradient error to 0-1 output */
#define CB_DEFAULT_KB           3000

/** Max brake power: 10% of full scale = 6554 in Q16.16 (0.1 * 65536) */
#define CB_DEFAULT_U_BRAKE_MAX  6554

/*============================================================================*/
/* Gradient Controller (PI with Anti-Windup, Fixed-Point)                      */
/*============================================================================*/

/**
 * @brief Handle structure for PI gradient controller
 *
 * Controls rate of temperature change (dT/dt) instead of absolute temperature.
 * Uses Q16.16 fixed-point arithmetic for efficient operation on Cortex-M0
 * (no hardware FPU).
 *
 * The controller implements:
 * - Gradient estimation via discrete derivative with EMA filtering
 * - PI control with back-calculation anti-windup
 * - Output saturation to [0, 1] mapped to heater levels 0-6
 */
typedef struct {
    /* Controller tuning parameters (all Q16.16 fixed-point) */
    q16_t Kc;               /**< Proportional gain */
    q16_t Ti_inv_Ts;        /**< Pre-computed: Ts / Ti (for integrator update) */
    q16_t Taw_inv_Ts;       /**< Pre-computed: Ts / Taw (for anti-windup) */

    /* Gradient estimation parameters */
    q16_t alpha;            /**< EMA filter coefficient (0-1 as Q16.16) */
    q16_t one_minus_alpha;  /**< Pre-computed: 1 - alpha */
    uint16_t Ts_ms;         /**< Sample time in milliseconds */

    /* Output limits (Q16.16) */
    q16_t u_min;            /**< Output minimum (typically 0 = 0.0) */
    q16_t u_max;            /**< Output maximum (typically 65536 = 1.0) */

    /* Internal state */
    int16_t T_prev_mdeg;    /**< Previous temperature in milli-degrees C */
    q16_t g_f_prev;         /**< Previous filtered gradient (°C/s as Q16.16) */
    q16_t I;                /**< Integrator state */
    q16_t g_sp;             /**< Gradient setpoint (°C/s as Q16.16) */

    /* Status flags */
    uint8_t initialized;    /**< Set to 1 after first temperature sample */
} GradientController_HandleTypeDef_t;

/*============================================================================*/
/* Temperature Controller (Outer P-Loop for Cascaded Control)                  */
/*============================================================================*/

/**
 * @brief Handle structure for outer temperature P controller
 *
 * Generates gradient setpoints for the inner gradient PI controller based on
 * temperature error. Implements heating-only behavior (cannot actively cool).
 *
 * Behavior:
 * - Far from target: ramp at g_max (maximum gradient from program)
 * - Approaching target: reduce gradient proportionally (g_sp = Kp_T * e_T)
 * - At target (within deadband): hold (g_sp = 0)
 * - Above target: wait for natural cooling (g_sp = 0, heating-only)
 */
typedef struct {
    q16_t Kp_T;             /**< Outer P gain (°C/s)/°C as Q16.16 */
    q16_t g_max;            /**< Max gradient °C/s as Q16.16 */
    int16_t T_band_mdeg;    /**< Deadband in milli-degrees */
    int16_t T_set_mdeg;     /**< Temperature setpoint milli-degrees */
    uint8_t is_cooling;     /**< 1 = cooling step, 0 = heating */
    uint8_t enabled;        /**< 1 = active */
} TemperatureController_HandleTypeDef_t;

/*============================================================================*/
/* Cooling Brake Controller (P + Hysteresis)                                   */
/*============================================================================*/

/**
 * @brief Handle structure for cooling brake controller
 *
 * Limits cooling rate by applying heat as a "brake" when natural cooling
 * is too fast. Uses P control with hysteresis for stability.
 *
 * Only activates when:
 * - Gradient setpoint is negative (cooling requested)
 * - Filtered gradient is more negative than g_min (cooling too fast)
 */
typedef struct {
    q16_t g_min;            /**< Allowed cooling limit (negative) °C/s as Q16.16 */
    q16_t dg_hyst;          /**< Hysteresis band (positive) °C/s as Q16.16 */
    q16_t Kb;               /**< Brake proportional gain */
    q16_t u_brake_max;      /**< Max brake power (0-65536 = 0-100%) */
    uint8_t brake_enabled;  /**< Hysteresis latch state */
} CoolingBrake_HandleTypeDef_t;

/**
 * @brief Initialize gradient controller with default parameters
 * @param[out] hgc Pointer to gradient controller handle
 *
 * Sets conservative default tuning values based on lambda tuning:
 * - Kc = 100.0 (proportional gain)
 * - Ti = 60s (integral time)
 * - Taw = 60s (anti-windup time constant)
 * - alpha = 0.8 (EMA filter coefficient)
 * - Ts = 1000ms (sample time)
 *
 * Resets all internal state to zero.
 */
void GradientController_Init(GradientController_HandleTypeDef_t *hgc);

/**
 * @brief Set the gradient setpoint
 * @param[in,out] hgc Pointer to gradient controller handle
 * @param[in] g_sp_deg_per_sec Gradient setpoint in °C/s as Q16.16
 *
 * @note UI programs use °C/h; convert before calling:
 *       g_sp_per_s = g_sp_per_h / 3600
 *       Q16: g_sp_q16 = (gradient_per_hour << 16) / 3600
 */
void GradientController_SetSetpoint(GradientController_HandleTypeDef_t *hgc,
                                     q16_t g_sp_deg_per_sec);

/**
 * @brief Reset gradient controller internal state
 * @param[in,out] hgc Pointer to gradient controller handle
 *
 * Clears integrator, gradient estimate, and previous temperature.
 * Call when starting a new firing program or after door open/close.
 */
void GradientController_Reset(GradientController_HandleTypeDef_t *hgc);

/**
 * @brief Update gradient controller with new temperature measurement
 * @param[in,out] hgc Pointer to gradient controller handle
 * @param[in] T_current_mdeg Current temperature in milli-degrees C (int16_t)
 * @return Controller output as Q16.16 (0.0-1.0 range, i.e., 0-65536)
 *
 * Call this function at the sample rate (default: every 1 second).
 *
 * Algorithm:
 * 1. Estimate gradient: g_hat = (T[k] - T[k-1]) / Ts
 * 2. EMA filter: g_f = alpha * g_f_prev + (1-alpha) * g_hat
 * 3. Error: e = g_sp - g_f
 * 4. PI output: u* = Kc * (e + I)
 * 5. Saturation: u = clamp(u*, u_min, u_max)
 * 6. Anti-windup: I += (Ts/Ti)*e + (Ts/Taw)*(u - u*)
 *
 * @note First call after init/reset returns 0 (no previous temperature).
 */
q16_t GradientController_Update(GradientController_HandleTypeDef_t *hgc,
                                 int16_t T_current_mdeg);

/**
 * @brief Convert controller output to heater level (0-6)
 * @param[in] u Controller output as Q16.16 (0-65536 = 0.0-1.0)
 * @return Heater level 0-6
 *
 * Mapping: level = round(u * 6)
 * - u = 0.0 (0)       → level 0
 * - u ≈ 0.17 (11141)  → level 1
 * - u ≈ 0.33 (21845)  → level 2
 * - u = 0.5 (32768)   → level 3
 * - u ≈ 0.67 (43691)  → level 4
 * - u ≈ 0.83 (54613)  → level 5
 * - u = 1.0 (65536)   → level 6
 */
uint8_t GradientController_GetHeaterLevel(q16_t u);

/*============================================================================*/
/* Temperature Controller Functions                                            */
/*============================================================================*/

/**
 * @brief Initialize temperature controller with default parameters
 * @param[out] htc Pointer to temperature controller handle
 */
void TemperatureController_Init(TemperatureController_HandleTypeDef_t *htc);

/**
 * @brief Set the temperature target and maximum gradient
 * @param[in,out] htc Pointer to temperature controller handle
 * @param[in] T_set_mdeg Temperature setpoint in milli-degrees C
 * @param[in] g_max_q16 Maximum gradient in °C/s as Q16.16
 * @param[in] is_cooling 1 for cooling step, 0 for heating step
 */
void TemperatureController_SetTarget(TemperatureController_HandleTypeDef_t *htc,
                                      int16_t T_set_mdeg, q16_t g_max_q16,
                                      uint8_t is_cooling);

/**
 * @brief Update temperature controller with new measurement
 * @param[in,out] htc Pointer to temperature controller handle
 * @param[in] T_meas_mdeg Current temperature in milli-degrees C
 * @return Gradient setpoint in °C/s as Q16.16 for inner controller
 *
 * Computes gradient setpoint based on temperature error:
 * - Cooling step: returns 0 (wait for natural cooling)
 * - At/above setpoint: returns 0 (cannot actively cool)
 * - Within deadband: returns 0 (hold temperature)
 * - Below setpoint: returns min(Kp_T * error, g_max)
 */
q16_t TemperatureController_Update(TemperatureController_HandleTypeDef_t *htc,
                                    int16_t T_meas_mdeg);

/**
 * @brief Check if temperature is at target
 * @param[in] htc Pointer to temperature controller handle
 * @param[in] T_meas_mdeg Current temperature in milli-degrees C
 * @return 1 if at target, 0 otherwise
 *
 * For heating: at target when within deadband of setpoint
 * For cooling: at target when temperature <= setpoint
 */
uint8_t TemperatureController_AtTarget(TemperatureController_HandleTypeDef_t *htc,
                                        int16_t T_meas_mdeg);

/**
 * @brief Reset temperature controller state
 * @param[in,out] htc Pointer to temperature controller handle
 *
 * Clears setpoint and disables controller.
 */
void TemperatureController_Reset(TemperatureController_HandleTypeDef_t *htc);

/*============================================================================*/
/* Gradient Controller Additional Functions                                    */
/*============================================================================*/

/**
 * @brief Freeze gradient controller integrator (prevent windup during cooling)
 * @param[in,out] hgc Pointer to gradient controller handle
 *
 * Decays the integrator toward zero to prevent windup when the controller
 * output cannot affect the plant (e.g., during passive cooling).
 */
void GradientController_FreezeIntegrator(GradientController_HandleTypeDef_t *hgc);

/*============================================================================*/
/* Cooling Brake Controller Functions                                          */
/*============================================================================*/

/**
 * @brief Initialize cooling brake controller with default parameters
 * @param[out] hcb Pointer to cooling brake controller handle
 */
void CoolingBrake_Init(CoolingBrake_HandleTypeDef_t *hcb);

/**
 * @brief Set the cooling rate limit
 * @param[in,out] hcb Pointer to cooling brake controller handle
 * @param[in] g_min_q16 Allowed cooling limit (negative) in °C/s as Q16.16
 */
void CoolingBrake_SetLimit(CoolingBrake_HandleTypeDef_t *hcb, q16_t g_min_q16);

/**
 * @brief Update cooling brake controller
 * @param[in,out] hcb Pointer to cooling brake controller handle
 * @param[in] g_f Filtered gradient in °C/s as Q16.16
 * @return Brake power output as Q16.16 (0 if not braking)
 *
 * Uses P control with hysteresis:
 * - If g_f > 0 (warming): brake off
 * - If g_f < g_min - hysteresis: enable brake
 * - If g_f > g_min + hysteresis: disable brake
 * - When enabled: u = Kb * (g_min - g_f), clamped to u_brake_max
 */
q16_t CoolingBrake_Update(CoolingBrake_HandleTypeDef_t *hcb, q16_t g_f);

/**
 * @brief Reset cooling brake controller state
 * @param[in,out] hcb Pointer to cooling brake controller handle
 */
void CoolingBrake_Reset(CoolingBrake_HandleTypeDef_t *hcb);

#endif /* INC_PID_H_ */
