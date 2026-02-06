/**
 * @file heater.h
 * @brief Heater control module for kiln heating coils
 * @author Dennis Rathgeb
 * @date Apr 5, 2024
 *
 * Controls 3 heating coils via SSR windowing (time-proportioning control).
 * Converts continuous duty cycle [0,1] to ON/OFF timing within a 20-second
 * window. Enforces 5-second minimum switching time to avoid tiny blips.
 *
 * @par Timing Hierarchy
 * - Sensor sampling: 1 second (temperature reading + gradient EMA update)
 * - Control loop: 1 second (PI update, outer loop update)
 * - SSR window: 20 seconds (power averaging, logging)
 * - Minimum switch: 5 seconds (smallest ON or OFF pulse)
 *
 * @par Usage
 * 1. Create a Heater_HandleTypeDef_t and call initHeater()
 * 2. Set gradient/temperature controller handles
 * 3. Start a program or set temperature target
 * 4. SSR windowing is handled automatically in heater_on_interupt()
 *
 * @par Door Safety
 * If door is opened (flag_door_open set), all coils are forced OFF.
 * Controller continues running so it resumes smoothly when door closes.
 */

#ifndef INC_HEATER_H_
#define INC_HEATER_H_

/*============================================================================*/
/* Timing Constants                                                            */
/*============================================================================*/

/** @brief RTC alarm interval in seconds (sensor & control rate) */
#define INTERUPT_INTERVAL_SECONDS               1

/** @brief Temperature sampling interval in seconds */
#define TEMPERATURE_SAMPLING_INTERVAL_SECONDS   1

/*============================================================================*/
/* SSR Windowing Constants                                                     */
/*============================================================================*/

/** @brief SSR window period in seconds */
#define SSR_WINDOW_SECONDS          20

/** @brief Minimum ON/OFF time in seconds (prevents tiny blips) */
#define SSR_MIN_SWITCH_SECONDS      5

/** @brief Minimum duty threshold: u_min = Tmin/Tw = 0.25 as Q16.16 */
#define SSR_DUTY_MIN_Q16            16384   /* 0.25 * 65536 */

/** @brief Maximum duty threshold: 1 - u_min = 0.75 as Q16.16 */
#define SSR_DUTY_MAX_Q16            49152   /* 0.75 * 65536 */

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

/**
 * @brief GPIO configuration for a heating coil
 */
typedef struct
{
    GPIO_TypeDef* port;  /**< GPIO port for coil control */
    uint16_t pin;        /**< GPIO pin for coil control */
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
 * @brief SSR windowing state for time-proportioning control
 *
 * Converts continuous duty cycle [0,1] to ON/OFF timing within
 * a configurable window. Enforces minimum switching time to avoid tiny blips.
 */
typedef struct {
    /* Runtime configuration (can be changed via UI) */
    uint8_t window_seconds;      /**< Window period in seconds (default 20) */
    uint8_t min_switch_seconds;  /**< Minimum ON/OFF time in seconds (default 5) */

    /* Runtime state */
    uint32_t window_start_tick;  /**< HAL_GetTick() at window start */
    uint32_t ton_ms;             /**< ON duration in ms for current window */
    uint8_t ssr_on;              /**< Current SSR state (0=OFF, 1=ON) */
    q16_t duty_current;          /**< Current window duty (for logging) */
} SSRWindow_HandleTypeDef_t;

/**
 * @brief Handle structure for heater control
 *
 * Contains all state and configuration for the heater system.
 */
typedef struct
{
    uint8_t flag_door_open;     /**< Door open flag: 1=open, 0=closed */
    heater_coils_t coils;       /**< Structure containing all coil GPIO config */
    SSRWindow_HandleTypeDef_t ssr;  /**< SSR windowing state */
    MAX31855_HandleTypeDef_t* htemp;    /**< Pointer to temperature sensor handle */
    GradientController_HandleTypeDef_t* hgc;  /**< Pointer to gradient controller handle */
    TemperatureController_HandleTypeDef_t* htc;  /**< Pointer to temperature controller (outer loop) */
    CoolingBrake_HandleTypeDef_t* hcb;           /**< Pointer to cooling brake controller */
    uint8_t gradient_control_enabled;  /**< Flag to enable/disable gradient control */
    ControlMode_t control_mode;        /**< Current control mode (heat/cool_passive/cool_brake) */

    /* Program execution state */
    void* active_program;       /**< Pointer to running ui_program_t (NULL if none) */
    uint8_t current_step;       /**< Current step index (0-based) in active program */
    uint16_t target_temperature; /**< Current step target temperature in degrees C */
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
 * @brief Turn off all heaters and reset to default state
 * @param[in,out] hheater Pointer to heater handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef heater_turn_off(Heater_HandleTypeDef_t* hheater);

/**
 * @brief RTC interrupt handler for temperature sampling and control
 * @param[in,out] hheater Pointer to heater handle
 * @param[in] hrtc Pointer to RTC handle for timestamp
 *
 * Called from RTC alarm interrupt (every INTERUPT_INTERVAL_SECONDS = 1s).
 *
 * Operation:
 * 1. Read temperature from MAX31855
 * 2. Update gradient estimate (EMA-filtered)
 * 3. Run outer temperature loop (if enabled)
 * 4. Run inner gradient PI controller
 * 5. Update SSR window (manages 20s ON/OFF timing)
 * 6. Log at start of each new SSR window
 */
void heater_on_interupt(Heater_HandleTypeDef_t* hheater, RTC_HandleTypeDef *hrtc);

/**
 * @brief Start executing a firing program
 * @param[in,out] hheater Pointer to heater handle
 * @param[in] program Pointer to ui_program_t to execute
 * @return HAL_OK on success, HAL_ERROR if invalid parameters
 *
 * Initializes program execution state, sets gradient setpoint for first step,
 * resets the gradient controller, and enables gradient control.
 */
HAL_StatusTypeDef heater_start_program(Heater_HandleTypeDef_t* hheater, void* program);

/**
 * @brief Stop executing the current program
 * @param[in,out] hheater Pointer to heater handle
 * @return HAL_OK on success, HAL_ERROR if hheater is NULL
 *
 * Disables gradient control, clears active program, and turns off heater.
 */
HAL_StatusTypeDef heater_stop_program(Heater_HandleTypeDef_t* hheater);

/**
 * @brief Set temperature target for cascaded control
 * @param[in,out] hheater Pointer to heater handle
 * @param[in] T_set_celsius Temperature setpoint in degrees C
 * @param[in] g_max_per_hour Maximum gradient in degrees C per hour
 * @return HAL_OK on success, HAL_ERROR if invalid parameters
 *
 * Sets up the outer temperature loop with the given target and maximum
 * gradient. Enables gradient control for the cascaded controller.
 */
HAL_StatusTypeDef heater_set_temperature_target(Heater_HandleTypeDef_t* hheater,
                                                 uint16_t T_set_celsius,
                                                 uint16_t g_max_per_hour);

#endif /* INC_HEATER_H_ */
