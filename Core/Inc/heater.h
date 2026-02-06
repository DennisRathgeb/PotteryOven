/**
 * @file heater.h
 * @brief Heater control module for kiln heating coils
 * @author Dennis Rathgeb
 * @date Apr 5, 2024
 *
 * Controls 3 heating coils with 7 power levels (0-6):
 * - Level 0: All coils OFF
 * - Level 1: Coil1 PWM (50% duty)
 * - Level 2: Coil1 ON
 * - Level 3: Coil1 ON, Coil2 PWM
 * - Level 4: Coil1+2 ON
 * - Level 5: Coil1+2 ON, Coil3 PWM
 * - Level 6: All coils ON
 *
 * PWM is implemented as manual toggle with PWM_ON_SECONDS period.
 *
 * @par Usage
 * 1. Create a Heater_HandleTypeDef_t and call initHeater()
 * 2. Set desired level with heater_set_level()
 * 3. Call heater_set_state() periodically (more often than PWM_ON_SECONDS)
 * 4. Use heater_turn_off() or set level 0 to stop heating
 *
 * @par Door Safety
 * If door is opened (flag_door_open set), heater automatically goes to level 0
 * and resumes previous level when door closes. heater_set_state() must be called
 * for this to take effect.
 */

#ifndef INC_HEATER_H_
#define INC_HEATER_H_

/** @brief PWM toggle period in seconds (coil stays on for this duration) */
#define PWM_ON_SECONDS 2

/** @brief RTC alarm interval in seconds (must be <= other intervals) */
#define INTERUPT_INTERVAL_SECONDS 1

/** @brief Temperature sampling interval in seconds */
#define TEMPERATURE_SAMPLING_INTERVAL_SECONDS 1

/** @brief PID calculation interval in seconds */
#define PID_CALC_INTERVAL_SECONDS 10

#include <stdio.h>
#include "main.h"
#include "log.h"
#include "stm32f0xx_hal.h"
#include "arm_math.h"
#include "log.h"
#include "MAX31855.h"
#include "pid.h"

/** @brief Enable printf logs for heater module */
#define HEATER_ENABLE_LOG

/** @brief Maximum length for temperature measurement arrays */
#define MAX_MEAS_AR_LENGTH 20

/**
 * @brief States that a heating coil can have
 */
typedef enum
{
    COIL_OFF = 0,   /**< Coil is off */
    COIL_ON = 1,    /**< Coil is continuously on */
    COIL_PWM = 2    /**< Coil is in PWM mode (50% duty cycle) */
} heater_coil_state_t;

/**
 * @brief Structure for individual heating coil
 *
 * Contains GPIO configuration and timing for PWM control.
 */
typedef struct
{
    GPIO_TypeDef* port;         /**< GPIO port for coil control */
    uint16_t pin;               /**< GPIO pin for coil control */
    heater_coil_state_t state;  /**< Current coil state (OFF, ON, PWM) */
    uint32_t time_pwm_last;     /**< Timestamp of last PWM toggle (for 50% duty) */
} heater_coil_t;

/**
 * @brief Structure containing all three heating coils
 */
typedef struct
{
    heater_coil_t coil1;    /**< First heating coil (lowest power) */
    heater_coil_t coil2;    /**< Second heating coil (medium power) */
    heater_coil_t coil3;    /**< Third heating coil (highest power) */
} heater_coils_t;

/**
 * @brief Handle structure for heater control
 *
 * Contains all state and configuration for the heater system.
 */
typedef struct
{
    uint8_t flag_door_open;     /**< Door open flag: 1=open, 0=closed */
    uint8_t heater_level;       /**< Current heater level (0-6) */
    uint8_t heater_level_prev;  /**< Previous level (0xff=none, used for door resume) */
    heater_coils_t coils;       /**< Structure containing all coil states */
    float32_t temperature[PID_CALC_INTERVAL_SECONDS / INTERUPT_INTERVAL_SECONDS]; /**< Temperature sample buffer */
    MAX31855_HandleTypeDef_t* htemp;    /**< Pointer to temperature sensor handle */
    uint8_t time_counter;       /**< Counter for interrupt timing */
    GradientController_HandleTypeDef_t* hgc;  /**< Pointer to gradient controller handle */
    uint8_t gradient_control_enabled;  /**< Flag to enable/disable gradient control */
} Heater_HandleTypeDef_t;

/**
 * @brief Initialize the heater module
 * @param[out] hheater Pointer to heater handle to initialize
 * @param[in] htemp Pointer to MAX31855 temperature sensor handle
 * @param[in] coil1_port GPIO port for coil 1
 * @param[in] coil1_pin GPIO pin for coil 1
 * @param[in] coil2_port GPIO port for coil 2
 * @param[in] coil2_pin GPIO pin for coil 2
 * @param[in] coil3_port GPIO port for coil 3
 * @param[in] coil3_pin GPIO pin for coil 3
 * @return HAL_OK on success, HAL_ERROR if hheater is NULL
 */
HAL_StatusTypeDef initHeater(Heater_HandleTypeDef_t* hheater, MAX31855_HandleTypeDef_t* htemp,
        GPIO_TypeDef* coil1_port, uint16_t coil1_pin,
        GPIO_TypeDef* coil2_port, uint16_t coil2_pin,
        GPIO_TypeDef* coil3_port, uint16_t coil3_pin);

/**
 * @brief Set the heater power level
 * @param[in,out] hheater Pointer to heater handle
 * @param[in] level Power level (0-6)
 * @return HAL_OK on success, HAL_ERROR for invalid level
 *
 * Configures coil states according to the level mapping:
 * - 0: All OFF
 * - 1: Coil1 PWM
 * - 2: Coil1 ON
 * - 3: Coil1 ON, Coil2 PWM
 * - 4: Coil1+2 ON
 * - 5: Coil1+2 ON, Coil3 PWM
 * - 6: All ON
 */
HAL_StatusTypeDef heater_set_level(Heater_HandleTypeDef_t* hheater, uint8_t level);

/**
 * @brief Apply the configured heater state to GPIO outputs
 * @param[in,out] hheater Pointer to heater handle
 * @return HAL_OK on success
 *
 * Must be called periodically (more often than PWM_ON_SECONDS) to:
 * - Update PWM toggling for coils in PWM mode
 * - Check door state and pause/resume heating
 */
HAL_StatusTypeDef heater_set_state(Heater_HandleTypeDef_t* hheater);

/**
 * @brief Turn off all heaters and reset to default state
 * @param[in,out] hheater Pointer to heater handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef heater_turn_off(Heater_HandleTypeDef_t* hheater);

/**
 * @brief RTC interrupt handler for temperature sampling and PID
 * @param[in,out] hheater Pointer to heater handle
 * @param[in] hrtc Pointer to RTC handle for timestamp
 *
 * Called from RTC alarm interrupt (every INTERUPT_INTERVAL_SECONDS).
 * Performs:
 * - Temperature sampling (every TEMPERATURE_SAMPLING_INTERVAL_SECONDS)
 * - Slope and mean calculation (every PID_CALC_INTERVAL_SECONDS)
 * - PID calculation (TODO: not yet implemented)
 */
void heater_on_interupt(Heater_HandleTypeDef_t* hheater, RTC_HandleTypeDef *hrtc);

/**
 * @brief Calculate temperature slope from sample buffer
 * @param[in] hheater Pointer to heater handle
 * @return Temperature slope in degrees C per second
 *
 * Uses linear regression to calculate the temperature rate of change
 * from the stored temperature samples.
 */
float32_t heater_calculate_slope(Heater_HandleTypeDef_t* hheater);

/**
 * @brief Calculate mean temperature from sample buffer
 * @param[in] hheater Pointer to heater handle
 * @return Mean temperature in degrees C
 */
float32_t heater_calculate_mean(Heater_HandleTypeDef_t* hheater);

/**
 * @brief Clear the temperature sample buffer
 * @param[in,out] hheater Pointer to heater handle
 *
 * Sets all elements in the temperature array to zero.
 */
void heater_set_temperature_zero(Heater_HandleTypeDef_t* hheater);

/**
 * @brief Print temperature with timestamp (debug function)
 * @param[in] hrtc Pointer to RTC handle for timestamp
 * @param[in] temperature Temperature value to print
 */
void heater_print_test(RTC_HandleTypeDef *hrtc, float32_t temperature);

#endif /* INC_HEATER_H_ */
