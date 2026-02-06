/**
 * @file settings.c
 * @brief Implementation of centralized settings management
 * @author Dennis Rathgeb
 * @date 2024
 *
 * Implements flash-based persistent storage for controller settings.
 * Provides load, save, and default initialization functionality.
 */

#include "settings.h"
#include "flash_storage.h"
#include <stdio.h>
#include <string.h>

/*============================================================================*/
/* Global Settings Instance                                                    */
/*============================================================================*/

/** @brief Global settings instance (RAM working copy) */
settings_data_t g_settings;

/*============================================================================*/
/* Settings Implementation                                                     */
/*============================================================================*/

/**
 * @brief Reset settings to compiled defaults
 */
void settings_reset_defaults(void)
{
    memset(&g_settings, 0, sizeof(g_settings));

    g_settings.magic = SETTINGS_MAGIC;

    /* Inner Loop (Gradient Controller) */
    g_settings.gc_Kc = DEFAULT_GC_KC;
    g_settings.gc_Ti_s = DEFAULT_GC_TI_S;
    g_settings.gc_Taw_s = DEFAULT_GC_TAW_S;
    g_settings.gc_alpha_x100 = DEFAULT_GC_ALPHA_X100;

    /* Outer Loop (Temperature Controller) */
    g_settings.tc_Kp = DEFAULT_TC_KP;
    g_settings.tc_T_band_deg = DEFAULT_TC_T_BAND_DEG;

    /* Cooling Brake */
    g_settings.cb_g_min_degph = DEFAULT_CB_G_MIN_DEGPH;
    g_settings.cb_hysteresis_degph = DEFAULT_CB_HYST_DEGPH;
    g_settings.cb_Kb = DEFAULT_CB_KB;
    g_settings.cb_u_brake_max_pct = DEFAULT_CB_U_MAX_PCT;

    /* SSR Timing */
    g_settings.ssr_window_seconds = DEFAULT_SSR_WINDOW;
    g_settings.ssr_min_switch = DEFAULT_SSR_MIN_SWITCH;

    /* CRC will be computed on save */
    g_settings.crc32 = 0;
}

/**
 * @brief Validate settings values are within bounds
 */
uint8_t settings_validate(void)
{
    /* Inner Loop bounds */
    if (g_settings.gc_Kc < 1 || g_settings.gc_Kc > 500) return 0;
    if (g_settings.gc_Ti_s < 10 || g_settings.gc_Ti_s > 300) return 0;
    if (g_settings.gc_Taw_s < 10 || g_settings.gc_Taw_s > 300) return 0;
    if (g_settings.gc_alpha_x100 < 50 || g_settings.gc_alpha_x100 > 99) return 0;

    /* Outer Loop bounds */
    if (g_settings.tc_Kp < 10 || g_settings.tc_Kp > 500) return 0;
    if (g_settings.tc_T_band_deg < 1 || g_settings.tc_T_band_deg > 20) return 0;

    /* Cooling Brake bounds */
    if (g_settings.cb_g_min_degph < 50 || g_settings.cb_g_min_degph > 300) return 0;
    if (g_settings.cb_hysteresis_degph < 1 || g_settings.cb_hysteresis_degph > 30) return 0;
    if (g_settings.cb_Kb < 100 || g_settings.cb_Kb > 10000) return 0;
    if (g_settings.cb_u_brake_max_pct < 1 || g_settings.cb_u_brake_max_pct > 50) return 0;

    /* SSR Timing bounds */
    if (g_settings.ssr_window_seconds < 10 || g_settings.ssr_window_seconds > 60) return 0;
    if (g_settings.ssr_min_switch < 1 || g_settings.ssr_min_switch > 15) return 0;

    return 1;
}

/**
 * @brief Load settings from flash
 */
HAL_StatusTypeDef settings_load(void)
{
    settings_data_t temp;
    uint32_t computed_crc;

    /* Read from flash */
    if (flash_read_data(FLASH_SETTINGS_ADDR, &temp, sizeof(temp)) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Check magic number */
    if (temp.magic != SETTINGS_MAGIC) {
        return HAL_ERROR;
    }

    /* Verify CRC (compute CRC with crc32 field set to 0) */
    uint32_t stored_crc = temp.crc32;
    temp.crc32 = 0;
    computed_crc = flash_compute_crc32(&temp, sizeof(temp) - sizeof(uint32_t));

    if (stored_crc != computed_crc) {
        return HAL_ERROR;
    }

    /* Restore CRC and copy to global */
    temp.crc32 = stored_crc;
    memcpy(&g_settings, &temp, sizeof(g_settings));

    /* Validate bounds */
    if (!settings_validate()) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

/**
 * @brief Save settings to flash
 */
HAL_StatusTypeDef settings_save(void)
{
    /* Ensure magic is set */
    g_settings.magic = SETTINGS_MAGIC;

    /* Compute CRC (with crc32 field as 0) */
    g_settings.crc32 = 0;
    g_settings.crc32 = flash_compute_crc32(&g_settings, sizeof(g_settings) - sizeof(uint32_t));

    /* Erase flash page */
    if (flash_erase_page(FLASH_SETTINGS_ADDR) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Write settings to flash */
    if (flash_write_data(FLASH_SETTINGS_ADDR, &g_settings, sizeof(g_settings)) != HAL_OK) {
        return HAL_ERROR;
    }

#ifdef SETTINGS_ENABLE_LOG
    printf("Settings saved to flash\r\n");
#endif

    return HAL_OK;
}

/**
 * @brief Initialize settings module
 */
HAL_StatusTypeDef settings_init(void)
{
    /* Try to load from flash */
    if (settings_load() == HAL_OK) {
#ifdef SETTINGS_ENABLE_LOG
        printf("Settings loaded from flash\r\n");
#endif
        return HAL_OK;
    }

    /* Flash empty or corrupt - use defaults */
#ifdef SETTINGS_ENABLE_LOG
    printf("Settings: using defaults (flash empty or invalid)\r\n");
#endif
    settings_reset_defaults();

    return HAL_OK;
}
