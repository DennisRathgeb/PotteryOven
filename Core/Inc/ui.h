/**
 * @file ui.h
 * @brief User interface module for menu-driven LCD control
 * @author Dennis Rathgeb
 * @date Apr 20, 2024
 *
 * Implements a state machine-based menu system for the kiln controller.
 * Uses a 16x2 LCD display and rotary encoder with buttons for navigation.
 *
 * @par Menu Structure
 * ```
 * PROGRAMS <-> SETPOINT <-> SETTINGS
 *     |                        |
 * PROGRAMS_OVERVIEW      SETTINGS_OVERVIEW
 *     |
 * PROGRAM_DETAILED / CREATE_PROGRAM
 *                           |
 *                   CREATE_PROGRAM_DETAILED
 * ```
 *
 * @par Navigation
 * - BUT1/ENC_DOWN: Navigate left/down
 * - BUT2/ENC_UP: Navigate right/up
 * - BUT3: Navigate back
 * - ENC_BUT: Enter/select
 *
 * @par Programs
 * Programs define firing profiles with gradient (degrees C/hour) and
 * target temperature sequences for ceramic firing.
 */

#ifndef INC_UI_H_
#define INC_UI_H_

#include <stdio.h>
#include <stdint.h>
#include "main.h"
#include "log.h"
#include <stm32f0xx_hal.h>
#include "lcd1602_rgb.h"
#include "arm_math.h"
#include "event.h"

/** @brief Enable debug logging for UI module */
#define UI_ENABLE_LOG

/** @brief LCD character buffer size (16 chars + null) */
#define UI_LCD_CHAR_SIZE 17

/** @brief Maximum allowed temperature in degrees C */
#define MAX_TEMPERATURE 1300

/** @brief Maximum allowed gradient in degrees C per hour (both positive and negative) */
#define MAX_GRADIENT 650

/** @brief Maximum allowed time setting value */
#define MAX_SETTING_TIME 20

/** @brief Maximum allowed setting value (both positive and negative) */
#define MAX_SETTING 20

/** @brief Increment step for button-based value changes */
#define BUTTON_INC 5

/** @brief Increment step for encoder-based value changes */
#define ENC_INC 20

/** @brief Increment for button-based float value changes (in millis, divide by 1000) */
#define BUTTON_INC_FLOAT_MILLIS 100

/** @brief Increment for encoder-based float value changes (in millis) */
#define ENC_INC_FLOAT_MILLIS 1000

/** @brief Minimum number of sequences in a program */
#define MIN_PROGRAM_SEQ_LENGTH 1

/** @brief Maximum number of sequences in a program */
#define MAX_PROGRAM_SEQ_LENGTH 10

/** @brief Maximum number of programs that can be stored */
#define MAX_PROGRAMS 10

/** @brief Maximum number of settings */
#define MAX_SETTINGS 8

/**
 * @brief Menu state enumeration for UI state machine
 *
 * Defines all possible menu points/screens in the UI.
 */
typedef enum
{
    NO_MENUPOINT,               /**< Initial/invalid state */
    SETTINGS,                   /**< Main settings menu */
    SETTINGS_OVERVIEW,          /**< Settings list view */
    SETPOINT,                   /**< Setpoint selection menu */
    PROGRAMS,                   /**< Main programs menu */
    PROGRAMS_OVERVIEW,          /**< Programs list view */
    PROGRAM_DETAILED,           /**< View program details */
    CREATE_PROGRAM,             /**< Create new program entry point */
    CREATE_PROGRAM_DETAILED     /**< Create program - enter values */
} ui_menupoint_t;

/**
 * @brief Structure for a firing program
 *
 * A program consists of a sequence of gradient/temperature pairs
 * defining the firing profile.
 */
typedef struct
{
    uint8_t length;                                 /**< Number of steps in program */
    uint16_t gradient[MAX_PROGRAM_SEQ_LENGTH];      /**< Temperature gradient for each step (degrees C/h) */
    uint16_t gradient_negative[MAX_PROGRAM_SEQ_LENGTH]; /**< Sign flag for gradient: 1=negative, 0=positive */
    uint16_t temperature[MAX_PROGRAM_SEQ_LENGTH];   /**< Target temperature for each step (degrees C) */
} ui_program_t;

/**
 * @brief Structure for storing multiple programs
 *
 * Contains an array of programs with current selection index.
 */
typedef struct
{
    uint8_t length;                         /**< Number of stored programs */
    ui_program_t program_list[MAX_PROGRAMS]; /**< Array of programs */
    uint8_t cur_index;                      /**< Currently selected program index */
} ui_programs_t;

/**
 * @brief Structure for a single setting
 *
 * Settings have a name displayed on LCD and a floating-point value.
 */
typedef struct
{
    char name[UI_LCD_CHAR_SIZE];    /**< Setting name for display */
    float32_t value;                /**< Setting value */
} ui_setting_t;

/**
 * @brief Structure for storing multiple settings
 *
 * Contains an array of settings with current selection index.
 */
typedef struct
{
    uint8_t length;                         /**< Number of settings */
    ui_setting_t setting_list[MAX_SETTINGS]; /**< Array of settings */
    uint8_t cur_index;                      /**< Currently selected setting index */
} ui_settings_t;

/**
 * @brief Handle structure for UI state
 *
 * Contains all state information for the menu system.
 */
typedef struct
{
    ui_menupoint_t state;           /**< Current menu state */
    ui_menupoint_t last_state;      /**< Previous menu state (for change detection) */
    ui_programs_t programs;         /**< Programs storage */
    ui_settings_t settings;         /**< Settings storage */
    LCD1602_RGB_HandleTypeDef_t *hlcd;  /**< Pointer to LCD handle */
    Event_Queue_HandleTypeDef_t *queue; /**< Pointer to event queue */
} Ui_HandleTypeDef_t;

/**
 * @brief Initialize the UI module
 * @param[out] ui Pointer to UI handle to initialize
 * @param[in] queue Pointer to event queue for input events
 * @param[in] hlcd Pointer to LCD handle for display output
 *
 * Initializes UI state machine, loads default programs and settings,
 * and configures custom LCD characters for degree symbol.
 */
void initUI(Ui_HandleTypeDef_t* ui, Event_Queue_HandleTypeDef_t *queue, LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Update UI state machine
 * @param[in,out] ui Pointer to UI handle
 * @return HAL_OK on success, HAL_ERROR on invalid event
 *
 * Main UI update function. Should be called in main loop.
 * Processes events from the event queue and updates display accordingly.
 */
HAL_StatusTypeDef ui_update(Ui_HandleTypeDef_t *ui);

/**
 * @brief Get next event from UI event queue
 * @param[in] ui Pointer to UI handle
 * @return Next event from queue, or NO_EVENT if queue is empty
 */
event_type_t ui_get_events(Ui_HandleTypeDef_t *ui);

#endif /* INC_UI_H_ */
