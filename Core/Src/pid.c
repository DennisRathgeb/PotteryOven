/*
 * pid.c
 *
 *  Created on: Apr 1, 2024
 *      Author: Dennis Rathgeb
 */


#include "pid.h"

#include <stdio.h>

#include "pid.h"

// Private variables
static float32_t integral = 0.0f;
static float32_t last_error = 0.0f;
static float32_t last_derivative = 0.0f;



// Function to initialize PID controller parameters
void PID_Init(PID_HandletypeDef_t *hpid, float32_t k_p, float32_t k_i, float32_t k_d,
            float32_t hysteresis, float32_t k_d_filter_coeff) {
    hpid->k_proportional = k_p;
    hpid->k_integral = k_i;
    hpid->k_derivative = k_d;
    hpid->hysteresis = hysteresis;
    hpid->derivative_filter_coeff = k_d_filter_coeff;
}

// Function to update PID controller parameters
void PID_UpdateParameters(PID_HandletypeDef_t *hpid, float32_t k_p, float32_t k_i, float32_t k_d, float32_t hysteresis) {
    hpid->k_proportional = k_p;
    hpid->k_integral = k_i;
    hpid->k_derivative = k_d;
    hpid->hysteresis = hysteresis;
}

// Function to calculate PID output with hysteresis
PID_Controller_Output_t PID_CalculateOutput(PID_HandletypeDef_t *hpid, float32_t current_temperature, float32_t setpoint) {
    float32_t error = setpoint - current_temperature;
    integral += error;
    float32_t derivative = error - last_error;
    // Apply derivative filtering
    float32_t filtered_derivative = (1.0f - hpid->derivative_filter_coeff) * last_derivative +
                                         hpid->derivative_filter_coeff * derivative;
    last_derivative = filtered_derivative;
    last_error = error;
    // Calculate PID output
    float32_t output = hpid->k_proportional * error +
                       hpid->k_integral * integral +
                       hpid->k_derivative * derivative;

    // Apply hysteresis
    if (output > hpid->hysteresis) {
        return PID_OUTPUT_ON;
    } else if (output < -hpid->hysteresis) {
        return PID_OUTPUT_OFF;
    } else {
        return PID_OUTPUT_OFF;  // Stay in the current state
    }
}






