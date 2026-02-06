/**
 * @file programs.h
 * @brief Firing programs management with flash persistence
 * @author Dennis Rathgeb
 * @date 2024
 *
 * Provides storage and management for kiln firing programs.
 * Programs are stored in flash memory and persist across power cycles.
 *
 * @par Usage
 * 1. Call programs_init() early in main() after settings_init()
 * 2. UI reads/modifies g_programs.programs[]
 * 3. Call programs_save() to persist changes to flash
 */

#ifndef INC_PROGRAMS_H_
#define INC_PROGRAMS_H_

#include "stm32f0xx_hal.h"
#include <stdint.h>

/*============================================================================*/
/* Program Constants                                                           */
/*============================================================================*/

/** @brief Maximum number of programs that can be stored */
#define MAX_PROGRAMS            10

/** @brief Maximum number of steps in a program */
#define MAX_PROGRAM_SEQ_LENGTH  10

/** @brief Minimum number of steps in a program */
#define MIN_PROGRAM_SEQ_LENGTH  1

/*============================================================================*/
/* Program Data Structures                                                     */
/*============================================================================*/

/**
 * @brief Structure for a single firing program
 *
 * A program consists of a sequence of gradient/temperature pairs
 * defining the firing profile. Each step specifies:
 * - gradient: Rate of temperature change in deg C/hour
 * - gradient_negative: 1 if cooling step, 0 if heating
 * - temperature: Target temperature for this step in deg C
 *
 * Size: 1 + (10 * 2) + (10 * 2) + (10 * 2) = 61 bytes per program
 */
typedef struct {
    uint8_t length;                                  /**< Number of steps in program (1-10) */
    uint16_t gradient[MAX_PROGRAM_SEQ_LENGTH];       /**< Temperature gradient for each step (deg C/h) */
    uint16_t gradient_negative[MAX_PROGRAM_SEQ_LENGTH]; /**< Sign flag: 1=negative (cooling), 0=positive */
    uint16_t temperature[MAX_PROGRAM_SEQ_LENGTH];    /**< Target temperature for each step (deg C) */
} program_t;

/**
 * @brief Structure for all programs stored in flash
 *
 * Contains header with magic number and count, plus array of programs.
 *
 * Size: 4 + 1 + 3 + (10 * 61) + 4 = 622 bytes
 * Fits in one 1KB flash page with room to spare.
 */
typedef struct {
    uint32_t magic;                      /**< Magic number: PROGRAMS_MAGIC = 0x50524731 */
    uint8_t count;                       /**< Number of stored programs (0-10) */
    uint8_t reserved[3];                 /**< Reserved for alignment */
    program_t programs[MAX_PROGRAMS];    /**< Array of programs */
    uint32_t crc32;                      /**< CRC32 for validation */
} programs_data_t;

/*============================================================================*/
/* Global Programs Instance                                                    */
/*============================================================================*/

/**
 * @brief Global programs instance (RAM working copy)
 *
 * This is the runtime copy of programs that UI and heater read from.
 * Modifications are made here, then persisted to flash via programs_save().
 */
extern programs_data_t g_programs;

/*============================================================================*/
/* Programs API                                                                */
/*============================================================================*/

/**
 * @brief Initialize programs module
 * @return HAL_OK on success, HAL_ERROR on failure
 *
 * Attempts to load programs from flash. If flash is empty or corrupt,
 * loads default programs instead. Should be called after settings_init().
 */
HAL_StatusTypeDef programs_init(void);

/**
 * @brief Load programs from flash
 * @return HAL_OK if valid programs loaded, HAL_ERROR if flash invalid
 *
 * Reads programs from flash and validates CRC. Does NOT fall back to
 * defaults on failure - use programs_init() for that behavior.
 */
HAL_StatusTypeDef programs_load(void);

/**
 * @brief Save programs to flash
 * @return HAL_OK on success, HAL_ERROR on failure
 *
 * Erases the programs flash page and writes current g_programs with
 * updated CRC. Call this after creating/modifying programs via UI.
 */
HAL_StatusTypeDef programs_save(void);

/**
 * @brief Reset programs to compiled defaults
 *
 * Overwrites g_programs with default example programs. Does NOT save
 * to flash - call programs_save() after if persistence is desired.
 */
void programs_reset_defaults(void);

/**
 * @brief Add a program to the list
 * @param[in] program Pointer to program to add
 * @return Index of added program, or -1 if list is full
 *
 * Copies program to next available slot. Does NOT save to flash.
 */
int8_t programs_add(const program_t* program);

/**
 * @brief Delete a program from the list
 * @param[in] index Index of program to delete (0-based)
 * @return HAL_OK on success, HAL_ERROR if index invalid
 *
 * Removes program and shifts remaining programs down. Does NOT save to flash.
 */
HAL_StatusTypeDef programs_delete(uint8_t index);

#endif /* INC_PROGRAMS_H_ */
