/*
 * heater.h
 *
 *  Created on: Apr 5, 2024
 *      Author: burni
 */

#ifndef INC_HEATER_H_
#define INC_HEATER_H_

#define PWM_ON_SECONDS 2 //how long a coil stays on in PWM mode
#define INTERUPT_INTERVAL_SECONDS 1 //RTC intervall, needs to be lower than following two intervals
#define TEMPERATURE_SAMPLING_INTERVAL_SECONDS 1 //sampling intervall for temperature measurement
#define PID_CALC_INTERVAL_SECONDS 10 //intervall for calculation of new pid value

#include <stdio.h>
#include "main.h"
#include "log.h"
#include "stm32f0xx_hal.h"
#include "arm_math.h"
#include "log.h"
#include "MAX31855.h"

//enable printf logs for this file
#define HEATER_ENABLE_LOG


//max length for temperature measurement arrays
#define MAX_MEAS_AR_LENGTH 20

/*
 * Usage:
 * create a Heater_HandleTypedef and pass it to the init function
 *
 * set a level
 * set state will turn heater on to said level
 * set state needs to be called more frequent then PWM_ON_MSECONDS in order for pwm to update
 *
 * turn heater off or set it to level 0
 *
 * if door is open heater will set itself to level 0 and resume once flag is reset.
 * set_state needs to be called to update though
 */

/*
 * states a coil can have
 */
typedef enum
{
    COIL_OFF = 0,
    COIL_ON = 1,
    COIL_PWM = 2
}heater_coil_state_t;

/*
 * struct for individual coils
 */
typedef struct
{
    GPIO_TypeDef* port;
    uint16_t pin;
    heater_coil_state_t state;
    uint32_t time_pwm_last; //used to keep time in 50% PWM signal

}heater_coil_t;

/*
 * struct for all coils combined
 */
typedef struct
{
    heater_coil_t coil1;
    heater_coil_t coil2;
    heater_coil_t coil3;
}heater_coils_t;



typedef struct
{
    uint8_t flag_door_open;    //door is open flag
    uint8_t heater_level;      //current_heater_level
    uint8_t heater_level_prev; //safe for when door opens

    heater_coils_t coils;      //struct for coil states etc

    float32_t temperature[PID_CALC_INTERVAL_SECONDS / INTERUPT_INTERVAL_SECONDS]; //stores temparuter data
    MAX31855_HandleTypeDef_t* htemp;
    uint8_t time_counter;
}Heater_HandleTypeDef_t;



HAL_StatusTypeDef initHeater(Heater_HandleTypeDef_t* hheater,MAX31855_HandleTypeDef_t* htemp, GPIO_TypeDef* coil1_port, uint16_t coil1_pin,
        GPIO_TypeDef* coil2_port, uint16_t coil2_pin, GPIO_TypeDef* coil3_port, uint16_t coil3_pin);
HAL_StatusTypeDef heater_set_level(Heater_HandleTypeDef_t* hheater, uint8_t level);
HAL_StatusTypeDef heater_set_state(Heater_HandleTypeDef_t* hheater);
HAL_StatusTypeDef heater_turn_off(Heater_HandleTypeDef_t* hheater);
void heater_on_interupt(Heater_HandleTypeDef_t* hheater,RTC_HandleTypeDef *hrtc);

#endif /* INC_HEATER_H_ */
