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
 * The controller outputs discrete ON/OFF states suitable for
 * relay-based heating element control.
 */

#ifndef INC_PID_H_
#define INC_PID_H_

#include "stm32f0xx_hal.h"
#include "arm_math.h"

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

#endif /* INC_PID_H_ */
