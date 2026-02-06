/**
 * @file flash_storage.h
 * @brief Flash memory storage API for persistent settings
 * @author Dennis Rathgeb
 * @date 2024
 *
 * Provides functions to read, write, and erase flash memory pages
 * for persistent storage of settings and programs on STM32F030C8.
 *
 * @note STM32F030 has 64KB flash with 1KB page size.
 *       Flash writes must be half-word (16-bit) aligned.
 *       Page erase is required before writing (sets all bits to 1).
 */

#ifndef INC_FLASH_STORAGE_H_
#define INC_FLASH_STORAGE_H_

#include "stm32f0xx_hal.h"
#include <stdint.h>

/*============================================================================*/
/* Flash Memory Layout                                                         */
/*============================================================================*/

/**
 * Flash memory addresses for persistent storage
 * Located at end of 64KB flash to avoid code region
 *
 * Flash Layout:
 * 0x08000000 - 0x0800F7FF: Code + Constants (~62KB)
 * 0x0800F800 - 0x0800FBFF: Settings (1KB page)
 * 0x0800FC00 - 0x0800FFFF: Programs (1KB page)
 */
#define FLASH_SETTINGS_ADDR     0x0800F800
#define FLASH_PROGRAMS_ADDR     0x0800FC00
#define FLASH_PAGE_SIZE         1024

/*============================================================================*/
/* Magic Numbers for Validation                                                */
/*============================================================================*/

/** @brief Magic number for settings: "SET1" in ASCII */
#define SETTINGS_MAGIC          0x53455431

/** @brief Magic number for programs: "PRG1" in ASCII */
#define PROGRAMS_MAGIC          0x50524731

/*============================================================================*/
/* Flash Storage API                                                           */
/*============================================================================*/

/**
 * @brief Erase a flash page
 * @param[in] page_addr Page-aligned address to erase (must be 1KB aligned)
 * @return HAL_OK on success, HAL_ERROR on failure
 *
 * @note Erasing sets all bytes in the page to 0xFF
 * @note Must be page-aligned (multiple of FLASH_PAGE_SIZE)
 */
HAL_StatusTypeDef flash_erase_page(uint32_t page_addr);

/**
 * @brief Write data to flash
 * @param[in] addr Starting address to write (must be half-word aligned)
 * @param[in] data Pointer to data buffer
 * @param[in] size Number of bytes to write (will be rounded up to half-word)
 * @return HAL_OK on success, HAL_ERROR on failure
 *
 * @note Caller must erase the page first if writing to previously written area
 * @note Address must be half-word (2-byte) aligned
 * @note Data is written in half-word (16-bit) chunks
 */
HAL_StatusTypeDef flash_write_data(uint32_t addr, const void* data, uint32_t size);

/**
 * @brief Read data from flash
 * @param[in] addr Starting address to read
 * @param[out] data Pointer to destination buffer
 * @param[in] size Number of bytes to read
 * @return HAL_OK on success, HAL_ERROR on failure
 *
 * @note Flash can be read directly via pointer, but this function
 *       provides a consistent API and error checking
 */
HAL_StatusTypeDef flash_read_data(uint32_t addr, void* data, uint32_t size);

/**
 * @brief Compute CRC32 checksum
 * @param[in] data Pointer to data buffer
 * @param[in] size Number of bytes to checksum
 * @return CRC32 checksum value
 *
 * Uses a simple polynomial-based CRC32 implementation.
 * The CRC field in the data structure should be set to 0 before computing.
 */
uint32_t flash_compute_crc32(const void* data, uint32_t size);

/**
 * @brief Check if flash region contains erased (0xFF) data
 * @param[in] addr Starting address to check
 * @param[in] size Number of bytes to check
 * @return 1 if all bytes are 0xFF, 0 otherwise
 *
 * Useful for detecting first boot (fresh flash).
 */
uint8_t flash_is_erased(uint32_t addr, uint32_t size);

#endif /* INC_FLASH_STORAGE_H_ */
