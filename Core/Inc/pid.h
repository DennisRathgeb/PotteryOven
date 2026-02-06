/**
 * @file pid.h
 * @brief PID controller module for temperature control
 * @author Dennis Rathgeb
 * @date Apr 1, 2024
 *
 * Implements a discrete PID controller with:
 * - Proportional, Integral, and Derivative terms
 * - Derivative filtering (low-pass) to reduce noise sensitivity
 * - Hysteresis for bang-bang style output (ON/OFF)
 *
 * Also implements a PI gradient controller for controlling
 * temperature rate of change (dT/dt) using fixed-point arithmetic.
 *
 * The controller outputs discrete ON/OFF states suitable for
 * relay-based heating element control.
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

/**
 * @brief Discrete output states from PID controller
 *
 * Used for bang-bang style control where the heater is either
 * fully on or fully off.
 */
typedef enum
{
    PID_OUTPUT_ON = 1,   /**< Heater should be turned on */
    PID_OUTPUT_OFF = 0   /**< Heater should be turned off */
} PID_Controller_Output_t;

/**
 * @brief Handle structure for PID controller parameters
 *
 * Contains all tuning parameters for the PID controller.
 * Internal state (integral, last_error, last_derivative) is
 * stored in static variables within pid.c.
 */
typedef struct
{
    float32_t k_proportional;       /**< Proportional gain (Kp) */
    float32_t k_integral;           /**< Integral gain (Ki) */
    float32_t k_derivative;         /**< Derivative gain (Kd) */
    float32_t derivative_filter_coeff; /**< Low-pass filter coefficient for derivative (0-1) */
    float32_t hysteresis;           /**< Hysteresis threshold for output switching */
} PID_HandletypeDef_t;

/**
 * @brief Initialize PID controller parameters
 * @param[out] hpid Pointer to PID handle to initialize
 * @param[in] k_p Proportional gain
 * @param[in] k_i Integral gain
 * @param[in] k_d Derivative gain
 * @param[in] hysteresis Hysteresis threshold for ON/OFF output
 * @param[in] k_d_filter_coeff Derivative filter coefficient (0-1, higher = less filtering)
 */
void PID_Init(PID_HandletypeDef_t *hpid, float32_t k_p, float32_t k_i, float32_t k_d,
              float32_t hysteresis, float32_t k_d_filter_coeff);

/**
 * @brief Update PID controller gains at runtime
 * @param[in,out] hpid Pointer to PID handle
 * @param[in] k_p New proportional gain
 * @param[in] k_i New integral gain
 * @param[in] k_d New derivative gain
 * @param[in] hysteresis New hysteresis threshold
 *
 * Allows real-time tuning of PID parameters without resetting internal state.
 */
void PID_UpdateParameters(PID_HandletypeDef_t *hpid, float32_t k_p, float32_t k_i,
                          float32_t k_d, float32_t hysteresis);

/**
 * @brief Calculate PID output based on current temperature and setpoint
 * @param[in] hpid Pointer to PID handle
 * @param[in] current_temperature Current measured temperature
 * @param[in] setpoint Target temperature
 * @return PID_OUTPUT_ON if heater should be on, PID_OUTPUT_OFF otherwise
 *
 * Implements the PID algorithm:
 * - error = setpoint - current_temperature
 * - integral += error
 * - derivative = error - last_error (filtered)
 * - output = Kp*error + Ki*integral + Kd*derivative
 * - Apply hysteresis to determine ON/OFF state
 */
PID_Controller_Output_t PID_CalculateOutput(PID_HandletypeDef_t *hpid,
                                            float32_t current_temperature,
                                            float32_t setpoint);

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

#endif /* INC_PID_H_ */
