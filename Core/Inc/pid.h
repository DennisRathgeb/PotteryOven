/*
 * pid.h
 *
 *  Created on: Apr 1, 2024
 *      Author: Dennis Rathgeb
 */

#ifndef INC_PID_H_
#define INC_PID_H_

#include "stm32f0xx_hal.h"
#include "arm_math.h"


/*
 * Type for discrete Output of PID Controller
 */
typedef enum
{
    PID_OUTPUT_ON = 1,
    PID_OUTPUT_OFF = 0
}PID_Controller_Output_t;

/*
 * HandleTypedef for PID controller
 */
typedef struct
{
    float32_t k_proportional;// Proportional gain
    float32_t k_integral; // Integral gain
    float32_t k_derivative; // Derivative gain

    float32_t derivative_filter_coeff; //low pass filter coef
    float32_t hysteresis; // Define temperature thresholds and hysteresis

}PID_HandletypeDef_t;

#endif /* INC_PID_H_ */
