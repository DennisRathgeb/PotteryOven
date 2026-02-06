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
