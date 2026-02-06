/**
 * @file programs.c
 * @brief Implementation of firing programs management
 * @author Dennis Rathgeb
 * @date 2024
 *
 * Implements flash-based persistent storage for kiln firing programs.
 * Provides load, save, add, delete functionality.
 */

#include "programs.h"
#include "flash_storage.h"
#include <stdio.h>
#include <string.h>

/*============================================================================*/
/* Global Programs Instance                                                    */
/*============================================================================*/

/** @brief Global programs instance (RAM working copy) */
programs_data_t g_programs;

/*============================================================================*/
/* Default Programs                                                            */
/*============================================================================*/

/**
 * @brief Default program 1: Bisque firing (Schruehbrand)
 * 3 steps: heat to 200C, cool to 80C, cool to 120C
 */
static const program_t default_program_1 = {
    .length = 3,
    .gradient = {288, 300, 150},
    .gradient_negative = {0, 1, 1},
    .temperature = {200, 80, 120}
};

/**
 * @brief Default program 2: Glaze firing (Glasurbrand)
 * 5 steps: complex heating and cooling profile
 */
static const program_t default_program_2 = {
    .length = 5,
    .gradient = {80, 60, 150, 300, 80},
    .gradient_negative = {0, 1, 0, 0, 1},
    .temperature = {15, 80, 120, 300, 600}
};

/**
 * @brief Default program 3: Simple test firing
 * 2 steps: heat to 300C, then to 80C
 */
static const program_t default_program_3 = {
    .length = 2,
    .gradient = {300, 150},
    .gradient_negative = {0, 0},
    .temperature = {300, 80}
};

/*============================================================================*/
/* Programs Implementation                                                     */
/*============================================================================*/

/**
 * @brief Reset programs to compiled defaults
 */
void programs_reset_defaults(void)
{
    memset(&g_programs, 0, sizeof(g_programs));

    g_programs.magic = PROGRAMS_MAGIC;
    g_programs.count = 3;

    /* Copy default programs */
    memcpy(&g_programs.programs[0], &default_program_1, sizeof(program_t));
    memcpy(&g_programs.programs[1], &default_program_2, sizeof(program_t));
    memcpy(&g_programs.programs[2], &default_program_3, sizeof(program_t));

    /* CRC will be computed on save */
    g_programs.crc32 = 0;
}

/**
 * @brief Load programs from flash
 */
HAL_StatusTypeDef programs_load(void)
{
    programs_data_t temp;
    uint32_t computed_crc;

    /* Read from flash */
    if (flash_read_data(FLASH_PROGRAMS_ADDR, &temp, sizeof(temp)) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Check magic number */
    if (temp.magic != PROGRAMS_MAGIC) {
        return HAL_ERROR;
    }

    /* Verify CRC */
    uint32_t stored_crc = temp.crc32;
    temp.crc32 = 0;
    computed_crc = flash_compute_crc32(&temp, sizeof(temp) - sizeof(uint32_t));

    if (stored_crc != computed_crc) {
        return HAL_ERROR;
    }

    /* Validate count */
    if (temp.count > MAX_PROGRAMS) {
        return HAL_ERROR;
    }

    /* Restore CRC and copy to global */
    temp.crc32 = stored_crc;
    memcpy(&g_programs, &temp, sizeof(g_programs));

    return HAL_OK;
}

/**
 * @brief Save programs to flash
 */
HAL_StatusTypeDef programs_save(void)
{
    /* Ensure magic is set */
    g_programs.magic = PROGRAMS_MAGIC;

    /* Validate count */
    if (g_programs.count > MAX_PROGRAMS) {
        g_programs.count = MAX_PROGRAMS;
    }

    /* Compute CRC */
    g_programs.crc32 = 0;
    g_programs.crc32 = flash_compute_crc32(&g_programs, sizeof(g_programs) - sizeof(uint32_t));

    /* Erase flash page */
    if (flash_erase_page(FLASH_PROGRAMS_ADDR) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Write programs to flash */
    if (flash_write_data(FLASH_PROGRAMS_ADDR, &g_programs, sizeof(g_programs)) != HAL_OK) {
        return HAL_ERROR;
    }

#ifdef PROGRAMS_ENABLE_LOG
    printf("Programs saved to flash\r\n");
#endif

    return HAL_OK;
}

/**
 * @brief Initialize programs module
 */
HAL_StatusTypeDef programs_init(void)
{
    /* Try to load from flash */
    if (programs_load() == HAL_OK) {
#ifdef PROGRAMS_ENABLE_LOG
        printf("Programs loaded from flash (%d programs)\r\n", g_programs.count);
#endif
        return HAL_OK;
    }

    /* Flash empty or corrupt - use defaults */
#ifdef PROGRAMS_ENABLE_LOG
    printf("Programs: using defaults (flash empty or invalid)\r\n");
#endif
    programs_reset_defaults();

    return HAL_OK;
}

/**
 * @brief Add a program to the list
 */
int8_t programs_add(const program_t* program)
{
    if (program == NULL) {
        return -1;
    }

    if (g_programs.count >= MAX_PROGRAMS) {
        return -1;
    }

    /* Copy to next available slot */
    memcpy(&g_programs.programs[g_programs.count], program, sizeof(program_t));
    g_programs.count++;

    return (int8_t)(g_programs.count - 1);
}

/**
 * @brief Delete a program from the list
 */
HAL_StatusTypeDef programs_delete(uint8_t index)
{
    if (index >= g_programs.count) {
        return HAL_ERROR;
    }

    /* Shift remaining programs down */
    for (uint8_t i = index; i < g_programs.count - 1; i++) {
        memcpy(&g_programs.programs[i], &g_programs.programs[i + 1], sizeof(program_t));
    }

    /* Clear last slot */
    memset(&g_programs.programs[g_programs.count - 1], 0, sizeof(program_t));
    g_programs.count--;

    return HAL_OK;
}
