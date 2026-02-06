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
#include "pid.h"

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

/*============================================================================*/
/* Settings Categories and Limits                                              */
/*============================================================================*/

/** @brief Number of settings categories */
#define SETTINGS_NUM_CATEGORIES     5

/** @brief Maximum settings per category */
#define SETTINGS_MAX_PER_CATEGORY   4

/*--- Inner Loop (Gradient Controller) Settings ---*/
#define SETTINGS_KC_MIN             1
#define SETTINGS_KC_MAX             500
#define SETTINGS_KC_INC_BTN         1.0f
#define SETTINGS_KC_INC_ENC         10.0f

#define SETTINGS_TI_MIN             10
#define SETTINGS_TI_MAX             300
#define SETTINGS_TI_INC_BTN         1.0f
#define SETTINGS_TI_INC_ENC         10.0f

#define SETTINGS_TAW_MIN            10
#define SETTINGS_TAW_MAX            300
#define SETTINGS_TAW_INC_BTN        1.0f
#define SETTINGS_TAW_INC_ENC        10.0f

#define SETTINGS_ALPHA_MIN          0.50f
#define SETTINGS_ALPHA_MAX          0.99f
#define SETTINGS_ALPHA_INC_BTN      0.01f
#define SETTINGS_ALPHA_INC_ENC      0.05f

/*--- Outer Loop (Temperature Controller) Settings ---*/
#define SETTINGS_KPT_MIN            10
#define SETTINGS_KPT_MAX            500
#define SETTINGS_KPT_INC_BTN        1.0f
#define SETTINGS_KPT_INC_ENC        10.0f

#define SETTINGS_TBAND_MIN          1
#define SETTINGS_TBAND_MAX          20
#define SETTINGS_TBAND_INC_BTN      1.0f
#define SETTINGS_TBAND_INC_ENC      1.0f

/*--- Cooling Brake Settings ---*/
#define SETTINGS_GMIN_MIN           50      /* 50-300 °C/h (stored positive, displayed negative) */
#define SETTINGS_GMIN_MAX           300
#define SETTINGS_GMIN_INC_BTN       5.0f
#define SETTINGS_GMIN_INC_ENC       10.0f

#define SETTINGS_HYST_MIN           1
#define SETTINGS_HYST_MAX           30
#define SETTINGS_HYST_INC_BTN       1.0f
#define SETTINGS_HYST_INC_ENC       5.0f

#define SETTINGS_KB_MIN             100
#define SETTINGS_KB_MAX             10000
#define SETTINGS_KB_INC_BTN         100.0f
#define SETTINGS_KB_INC_ENC         500.0f

#define SETTINGS_UBRAKE_MIN         1
#define SETTINGS_UBRAKE_MAX         50
#define SETTINGS_UBRAKE_INC_BTN     1.0f
#define SETTINGS_UBRAKE_INC_ENC     5.0f

/*--- SSR Timing Settings ---*/
#define SETTINGS_SSRWIN_MIN         10
#define SETTINGS_SSRWIN_MAX         60
#define SETTINGS_SSRWIN_INC_BTN     1.0f
#define SETTINGS_SSRWIN_INC_ENC     5.0f

#define SETTINGS_SSRMIN_MIN         1
#define SETTINGS_SSRMIN_MAX         15
#define SETTINGS_SSRMIN_INC_BTN     1.0f
#define SETTINGS_SSRMIN_INC_ENC     1.0f

/**
 * @brief Menu state enumeration for UI state machine
 *
 * Defines all possible menu points/screens in the UI.
 */
typedef enum
{
    NO_MENUPOINT,               /**< Initial/invalid state */
    SETTINGS,                   /**< Main settings menu */
    SETTINGS_CATEGORIES,        /**< Settings category selection */
    SETTINGS_INNER_LOOP,        /**< Inner loop (gradient controller) settings */
    SETTINGS_OUTER_LOOP,        /**< Outer loop (temperature controller) settings */
    SETTINGS_COOLING_BRAKE,     /**< Cooling brake settings */
    SETTINGS_SSR_TIMING,        /**< SSR timing settings */
    SETTINGS_STATUS,            /**< Status display (read-only) */
    SETTINGS_OVERVIEW,          /**< Legacy - redirect to SETTINGS_CATEGORIES */
    SETPOINT,                   /**< Setpoint selection menu */
    PROGRAMS,                   /**< Main programs menu */
    PROGRAMS_OVERVIEW,          /**< Programs list view */
    PROGRAM_DETAILED,           /**< View program details */
    CREATE_PROGRAM,             /**< Create new program entry point */
    CREATE_PROGRAM_DETAILED     /**< Create program - enter values */
} ui_menupoint_t;

/**
 * @brief Settings category enumeration
 */
typedef enum
{
    SETTINGS_CAT_INNER_LOOP = 0,    /**< Gradient controller (Kc, Ti, Taw, alpha) */
    SETTINGS_CAT_OUTER_LOOP,        /**< Temperature controller (Kp_T, T_band) */
    SETTINGS_CAT_COOLING_BRAKE,     /**< Cooling brake (g_min, hyst, Kb, u_max) */
    SETTINGS_CAT_SSR_TIMING,        /**< SSR timing (window, min_switch) */
    SETTINGS_CAT_STATUS             /**< Status display (read-only) */
} ui_settings_category_t;

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
 * @brief Structure for a single setting parameter
 *
 * Settings have a name displayed on LCD, a value, and bounds.
 */
typedef struct
{
    char name[UI_LCD_CHAR_SIZE];    /**< Setting name for display */
    float32_t value;                /**< Current setting value */
    float32_t min_val;              /**< Minimum allowed value */
    float32_t max_val;              /**< Maximum allowed value */
    float32_t inc_btn;              /**< Increment for button press */
    float32_t inc_enc;              /**< Increment for encoder rotation */
    uint8_t decimals;               /**< Number of decimal places for display */
} ui_setting_param_t;

/**
 * @brief Structure for a settings category
 *
 * Contains settings for one category with selection tracking.
 */
typedef struct
{
    char name[UI_LCD_CHAR_SIZE];                    /**< Category name for display */
    uint8_t length;                                  /**< Number of settings in category */
    ui_setting_param_t params[SETTINGS_MAX_PER_CATEGORY]; /**< Settings in this category */
    uint8_t cur_index;                               /**< Currently selected setting */
} ui_settings_category_data_t;

/**
 * @brief Structure for all settings
 *
 * Contains all setting categories with navigation state.
 */
typedef struct
{
    ui_settings_category_data_t categories[SETTINGS_NUM_CATEGORIES]; /**< All categories */
    uint8_t cur_category;                           /**< Currently selected category */
    uint8_t edit_mode;                              /**< 0=navigate, 1=edit value */
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

/**
 * @brief Apply all UI settings to controllers
 * @param[in] ui Pointer to UI handle containing settings
 *
 * Transfers all settings from UI to the respective controllers:
 * - Inner loop: Kc, Ti, Taw, alpha to GradientController
 * - Outer loop: Kp_T, T_band to TemperatureController
 * - Cooling brake: g_min, hyst, Kb, u_brake_max to CoolingBrake
 * - SSR timing: window, min_switch to Heater SSR
 *
 * Call this when starting a program or after settings are modified.
 */
void ui_apply_all_settings(Ui_HandleTypeDef_t* ui);

/**
 * @brief Load current controller values into UI settings
 * @param[in,out] ui Pointer to UI handle to update
 *
 * Reads current values from all controllers and updates UI settings.
 * Call this at startup to sync UI with controller state.
 */
void ui_load_settings_from_controllers(Ui_HandleTypeDef_t* ui);

/**
 * @brief Legacy function - Apply UI settings to gradient controller
 * @param[in] ui Pointer to UI handle containing settings
 * @param[in,out] hgc Pointer to gradient controller handle to configure
 * @deprecated Use ui_apply_all_settings() instead
 */
void ui_apply_settings_to_controller(Ui_HandleTypeDef_t* ui, GradientController_HandleTypeDef_t* hgc);

#endif /* INC_UI_H_ */
