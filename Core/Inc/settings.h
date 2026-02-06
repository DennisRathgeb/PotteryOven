/**
 * @file settings.h
 * @brief Centralized settings management with flash persistence
 * @author Dennis Rathgeb
 * @date 2024
 *
 * Provides a single source of truth for all controller parameters.
 * Settings are stored in flash memory and persist across power cycles.
 *
 * @par Usage
 * 1. Call settings_init() early in main() before controller init
 * 2. Controllers read from g_settings during their init
 * 3. UI modifies g_settings values
 * 4. Call settings_save() to persist changes to flash
 */

#ifndef INC_SETTINGS_H_
#define INC_SETTINGS_H_

#include "stm32f0xx_hal.h"
#include <stdint.h>

/*============================================================================*/
/* Default Values - Inner Loop (Gradient Controller)                           */
/*============================================================================*/

/** @brief Default proportional gain Kc (1-500) */
#define DEFAULT_GC_KC           100

/** @brief Default integral time Ti in seconds (10-300) */
#define DEFAULT_GC_TI_S         60

/** @brief Default anti-windup time Taw in seconds (10-300) */
#define DEFAULT_GC_TAW_S        60

/** @brief Default EMA filter coefficient alpha * 100 (50-99 for 0.50-0.99) */
#define DEFAULT_GC_ALPHA_X100   85

/*============================================================================*/
/* Default Values - Outer Loop (Temperature Controller)                        */
/*============================================================================*/

/** @brief Default outer P gain Kp_T (10-500) */
#define DEFAULT_TC_KP           61

/** @brief Default deadband in degrees C (1-20) */
#define DEFAULT_TC_T_BAND_DEG   5

/*============================================================================*/
/* Default Values - Cooling Brake                                              */
/*============================================================================*/

/** @brief Default min cooling gradient in deg C/h (50-300, stored positive) */
#define DEFAULT_CB_G_MIN_DEGPH  100

/** @brief Default hysteresis in deg C/h (1-30) */
#define DEFAULT_CB_HYST_DEGPH   6

/** @brief Default brake gain Kb (100-10000) */
#define DEFAULT_CB_KB           3000

/** @brief Default max brake output in percent (1-50) */
#define DEFAULT_CB_U_MAX_PCT    10

/*============================================================================*/
/* Default Values - SSR Timing                                                 */
/*============================================================================*/

/** @brief Default SSR window period in seconds (10-60) */
#define DEFAULT_SSR_WINDOW      20

/** @brief Default minimum switch time in seconds (1-15) */
#define DEFAULT_SSR_MIN_SWITCH  5

/*============================================================================*/
/* Settings Data Structure                                                     */
/*============================================================================*/

/**
 * @brief Settings data structure for flash storage
 *
 * All values stored in user-friendly units (integers).
 * Conversion to Q16.16 fixed-point happens at load time.
 *
 * Size: ~32 bytes (fits in one flash page with room to spare)
 */
typedef struct {
    uint32_t magic;              /**< Magic number: SETTINGS_MAGIC = 0x53455431 */

    /* Inner Loop (Gradient Controller) */
    uint16_t gc_Kc;              /**< Controller gain (1-500) */
    uint16_t gc_Ti_s;            /**< Integral time in seconds (10-300) */
    uint16_t gc_Taw_s;           /**< Anti-windup time in seconds (10-300) */
    uint16_t gc_alpha_x100;      /**< EMA coefficient * 100 (50-99) */

    /* Outer Loop (Temperature Controller) */
    uint16_t tc_Kp;              /**< P gain (10-500) */
    uint8_t tc_T_band_deg;       /**< Deadband in degrees C (1-20) */
    uint8_t reserved1;           /**< Reserved for alignment */

    /* Cooling Brake */
    uint16_t cb_g_min_degph;     /**< Min gradient in deg C/h (50-300, positive) */
    uint8_t cb_hysteresis_degph; /**< Hysteresis in deg C/h (1-30) */
    uint8_t cb_u_brake_max_pct;  /**< Max brake output % (1-50) */
    uint16_t cb_Kb;              /**< Brake gain (100-10000) */

    /* SSR Timing */
    uint8_t ssr_window_seconds;  /**< Window period (10-60) */
    uint8_t ssr_min_switch;      /**< Min switch time (1-15) */

    uint16_t reserved2;          /**< Reserved for future use */

    uint32_t crc32;              /**< CRC32 for validation */
} settings_data_t;

/*============================================================================*/
/* Global Settings Instance                                                    */
/*============================================================================*/

/**
 * @brief Global settings instance (RAM working copy)
 *
 * This is the runtime copy of settings that all modules should read from.
 * Modifications are made here, then persisted to flash via settings_save().
 */
extern settings_data_t g_settings;

/*============================================================================*/
/* Settings API                                                                */
/*============================================================================*/

/**
 * @brief Initialize settings module
 * @return HAL_OK on success, HAL_ERROR on failure
 *
 * Attempts to load settings from flash. If flash is empty or corrupt,
 * loads default values instead. Should be called early in main().
 */
HAL_StatusTypeDef settings_init(void);

/**
 * @brief Load settings from flash
 * @return HAL_OK if valid settings loaded, HAL_ERROR if flash invalid
 *
 * Reads settings from flash and validates CRC. Does NOT fall back to
 * defaults on failure - use settings_init() for that behavior.
 */
HAL_StatusTypeDef settings_load(void);

/**
 * @brief Save settings to flash
 * @return HAL_OK on success, HAL_ERROR on failure
 *
 * Erases the settings flash page and writes current g_settings with
 * updated CRC. Call this after modifying settings via UI.
 */
HAL_StatusTypeDef settings_save(void);

/**
 * @brief Reset settings to compiled defaults
 *
 * Overwrites g_settings with default values. Does NOT save to flash -
 * call settings_save() after if persistence is desired.
 */
void settings_reset_defaults(void);

/**
 * @brief Validate settings values are within bounds
 * @return 1 if all values valid, 0 if any value out of range
 *
 * Checks that all settings are within their defined min/max limits.
 * Called automatically by settings_load().
 */
uint8_t settings_validate(void);

#endif /* INC_SETTINGS_H_ */
