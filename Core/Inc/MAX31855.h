/**
 * @file MAX31855.h
 * @brief MAX31855 thermocouple-to-digital converter driver
 * @author Dennis Rathgeb
 * @date Mar 31, 2024
 *
 * Driver for the MAX31855 cold-junction compensated thermocouple-to-digital
 * converter. Communicates via SPI to read thermocouple and reference junction
 * temperatures with fault detection.
 *
 * @note The MAX31855 outputs 32 bits of data per read:
 *       - Bits 31-18: Thermocouple temperature (14-bit, signed, 0.25C resolution)
 *       - Bit 17: Reserved
 *       - Bit 16: Fault flag
 *       - Bits 15-4: Reference junction temperature (12-bit, signed, 0.0625C resolution)
 *       - Bit 3: Reserved
 *       - Bit 2: SCV fault (short to VCC)
 *       - Bit 1: SCG fault (short to GND)
 *       - Bit 0: OC fault (open circuit)
 *
 * @todo Verify bitfield byte order is correct (D31 should be first transmitted)
 */

#ifndef MAX31855_INC_MAX31855_H_
#define MAX31855_INC_MAX31855_H_

#include "stm32f0xx_hal.h"
#include "main.h"
#include "arm_math.h"

/** @brief Total payload length in bits from MAX31855 */
#define MAX31855_PAYLOAD_LENGTH  32

/** @brief SPI receive timeout in microseconds */
#define MAX31855_TIMEOUT  1000000

#include "../Inc/MAX31855.h"

/**
 * @brief Parsed payload structure for MAX31855 data
 *
 * Contains all fields extracted from the 32-bit SPI read.
 * Uses bitfields for compact storage.
 *
 * @note Packed attribute ensures no padding between fields.
 * @todo Verify bit order is correct for target architecture.
 */
typedef struct __attribute__((packed))
{
    uint16_t therm_temp_sign :1;           /**< Thermocouple temperature sign: 1=negative, 0=positive */
    uint16_t therm_temp_value :11;         /**< Thermocouple temperature integer value */
    uint16_t therm_temp_fractual_value :2; /**< Thermocouple temperature fractional value (0.25C steps) */
    uint16_t :1;                           /**< Reserved bit */
    uint16_t fault :1;                     /**< Fault flag: 1 when any fault (SCV, SCG, OC) is active */
    uint16_t int_temp_sign :1;             /**< Reference junction temperature sign: 1=negative */
    uint16_t int_temp_value :7;            /**< Reference junction temperature integer value */
    uint16_t int_temp_fractual_value :4;   /**< Reference junction temperature fractional (0.0625C steps) */
    uint16_t :1;                           /**< Reserved bit */
    uint16_t scv_fault :1;                 /**< Short to VCC fault: 1=thermocouple shorted to VCC */
    uint16_t scg_fault :1;                 /**< Short to GND fault: 1=thermocouple shorted to GND */
    uint16_t oc_fault :1;                  /**< Open circuit fault: 1=no thermocouple connection */

} max31855_payload_t;

/**
 * @brief Handle structure for MAX31855 device
 *
 * Contains SPI handle reference and storage for raw and parsed data.
 */
typedef struct
{
    SPI_HandleTypeDef* hspi;    /**< Pointer to SPI handle for communication */
    uint8_t raw_payload[4];     /**< Raw 32-bit data received from SPI */
    max31855_payload_t payload; /**< Parsed payload fields */

} MAX31855_HandleTypeDef_t;

/**
 * @brief Initialize the MAX31855 driver
 * @param[out] hmax31855 Pointer to MAX31855 handle to initialize
 * @param[in] hspi Pointer to SPI handle for communication
 * @return HAL_OK on success
 */
HAL_StatusTypeDef max31855_init(MAX31855_HandleTypeDef_t* hmax31855, SPI_HandleTypeDef* hspi);

/**
 * @brief Print parsed payload in binary format (debug function)
 * @param[in] hmax31855 Pointer to MAX31855 handle
 */
void max_31855_print_max31855_payload_binary(MAX31855_HandleTypeDef_t* hmax31855);

/**
 * @brief Print payload values in human-readable format (debug function)
 * @param[in] hmax31855 Pointer to MAX31855 handle
 */
void max_31855_print_payload(MAX31855_HandleTypeDef_t *hmax31855);

/**
 * @brief Read data from MAX31855 via SPI
 * @param[in,out] hmax31855 Pointer to MAX31855 handle
 * @return HAL_OK on success, HAL_ERROR on failure
 *
 * Performs blocking SPI read and updates both raw_payload and parsed payload fields.
 * @todo Implement non-blocking (DMA) version
 */
HAL_StatusTypeDef max31855_read_data(MAX31855_HandleTypeDef_t *hmax31855);

/**
 * @brief Get thermocouple temperature sign
 * @param[in] hmax31855 Pointer to MAX31855 handle
 * @return 1 if temperature is negative, 0 if positive
 * @pre Call max31855_read_data() first to get current value
 */
uint16_t max31855_get_temp_sign(MAX31855_HandleTypeDef_t *hmax31855);

/**
 * @brief Get thermocouple temperature integer value
 * @param[in] hmax31855 Pointer to MAX31855 handle
 * @return Unsigned temperature value (check sign separately)
 * @pre Call max31855_read_data() first to get current value
 */
uint16_t max31855_get_temp_val(MAX31855_HandleTypeDef_t *hmax31855);

/**
 * @brief Get thermocouple temperature fractional value
 * @param[in] hmax31855 Pointer to MAX31855 handle
 * @return Fractional value as integer (e.g., 75 for 0.75C)
 * @pre Call max31855_read_data() first to get current value
 */
uint16_t max31855_get_temp_frac(MAX31855_HandleTypeDef_t *hmax31855);

/**
 * @brief Get reference junction (internal) temperature sign
 * @param[in] hmax31855 Pointer to MAX31855 handle
 * @return 1 if temperature is negative, 0 if positive
 */
uint16_t max31855_get_int_temp_sign(MAX31855_HandleTypeDef_t *hmax31855);

/**
 * @brief Get reference junction temperature integer value
 * @param[in] hmax31855 Pointer to MAX31855 handle
 * @return Unsigned temperature value
 */
uint16_t max31855_get_int_temp_val(MAX31855_HandleTypeDef_t *hmax31855);

/**
 * @brief Get reference junction temperature fractional value
 * @param[in] hmax31855 Pointer to MAX31855 handle
 * @return Fractional value
 */
uint16_t max31855_get_int_temp_frac(MAX31855_HandleTypeDef_t *hmax31855);

/**
 * @brief Get thermocouple temperature as float32
 * @param[in] hmax31855 Pointer to MAX31855 handle
 * @return Temperature in degrees Celsius as float32_t
 * @pre Call max31855_read_data() first to get current value
 */
float32_t max31855_get_temp_f32(MAX31855_HandleTypeDef_t *hmax31855);

#endif /* MAX31855_INC_MAX31855_H_ */
