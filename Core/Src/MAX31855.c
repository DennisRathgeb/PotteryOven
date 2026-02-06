/**
 * @file MAX31855.c
 * @brief Implementation of MAX31855 thermocouple-to-digital converter driver
 * @author Dennis Rathgeb
 * @date Mar 31, 2024
 *
 * Implements SPI communication with MAX31855 for temperature reading
 * and fault detection.
 */

#include "MAX31855.h"
#include "log.h"
#include <stdio.h>

/**
 * @brief Initialize the MAX31855 driver
 * @param[out] hmax31855 Pointer to MAX31855 handle to initialize
 * @param[in] hspi Pointer to SPI handle for communication
 * @return HAL_OK on success
 *
 * Stores SPI handle and sets chip select (NSS) pin high (inactive).
 */
HAL_StatusTypeDef max31855_init(MAX31855_HandleTypeDef_t* hmax31855, SPI_HandleTypeDef* hspi)
{
    hmax31855->hspi = hspi;
    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_SET);
    return HAL_OK;
}

/**
 * @brief Parse raw SPI data into payload structure fields
 * @param[in,out] hmax31855 Pointer to MAX31855 handle
 * @return HAL_OK on success, HAL_ERROR if handle is NULL
 *
 * Extracts all fields from the 32-bit raw data using bitwise operations.
 * The MAX31855 transmits MSB first (D31 first).
 */
static HAL_StatusTypeDef max31855_update_payload(MAX31855_HandleTypeDef_t *hmax31855) {
    if (NULL == hmax31855) {
        return HAL_ERROR;
    }

    /* Combine 4 bytes into 32-bit value (MSB first) */
    uint32_t raw_data = ((uint32_t)hmax31855->raw_payload[0] << 24) |
                        ((uint32_t)hmax31855->raw_payload[1] << 16) |
                        ((uint32_t)hmax31855->raw_payload[2] << 8) |
                        (uint32_t)hmax31855->raw_payload[3];

    /* Extract individual fields using bit masks and shifts */
    hmax31855->payload.therm_temp_sign = (raw_data >> 31) & 0x01;
    hmax31855->payload.therm_temp_value = (raw_data >> 20) & 0x7FF;
    hmax31855->payload.therm_temp_fractual_value = (raw_data >> 18) & 0x03;
    hmax31855->payload.fault = (raw_data >> 16) & 0x01;
    hmax31855->payload.int_temp_sign = (raw_data >> 15) & 0x01;
    hmax31855->payload.int_temp_value = (raw_data >> 8) & 0x7F;
    hmax31855->payload.int_temp_fractual_value = (raw_data >> 4) & 0x0F;
    hmax31855->payload.scv_fault = (raw_data >> 2) & 0x01;
    hmax31855->payload.scg_fault = (raw_data >> 1) & 0x01;
    hmax31855->payload.oc_fault = raw_data & 0x01;
    return HAL_OK;
}

/**
 * @brief Read data from MAX31855 via SPI
 * @param[in,out] hmax31855 Pointer to MAX31855 handle
 * @return HAL_OK on success, HAL_ERROR on failure
 *
 * Performs blocking SPI read:
 * 1. Asserts chip select (NSS low)
 * 2. Reads 4 bytes (32 bits) from MAX31855
 * 3. Deasserts chip select (NSS high)
 * 4. Parses raw data into payload structure
 *
 * @todo Implement non-blocking (DMA) version for better performance
 */
HAL_StatusTypeDef max31855_read_data(MAX31855_HandleTypeDef_t *hmax31855)
{
    if (NULL == hmax31855)
    {
        return HAL_ERROR;
    }

    if (NULL == hmax31855->hspi)
    {
        return HAL_ERROR;
    }

    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_Receive(hmax31855->hspi, hmax31855->raw_payload,
            MAX31855_PAYLOAD_LENGTH/8, MAX31855_TIMEOUT) != HAL_OK)
    {
        HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_SET);
        return HAL_ERROR;
    }
    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_SET);

    return max31855_update_payload(hmax31855);
}

/**
 * @brief Get thermocouple temperature sign
 * @param[in] hmax31855 Pointer to MAX31855 handle
 * @return 1 if temperature is negative, 0 if positive
 * @pre Call max31855_read_data() first to get current value
 */
uint16_t max31855_get_temp_sign(MAX31855_HandleTypeDef_t *hmax31855)
{
    return hmax31855->payload.therm_temp_sign;
}

/**
 * @brief Get thermocouple temperature integer value
 * @param[in] hmax31855 Pointer to MAX31855 handle
 * @return Unsigned temperature value
 * @pre Call max31855_read_data() first to get current value
 *
 * For negative temperatures, returns two's complement converted value.
 */
uint16_t max31855_get_temp_val(MAX31855_HandleTypeDef_t *hmax31855)
{
    if(0 == hmax31855->payload.therm_temp_sign)
    {
        return hmax31855->payload.therm_temp_value;
    }

    return (~(hmax31855->payload.therm_temp_value) + 1);
}

/**
 * @brief Get thermocouple temperature fractional value
 * @param[in] hmax31855 Pointer to MAX31855 handle
 * @return Fractional part as integer in 0.25C steps (0, 25, 50, or 75)
 * @pre Call max31855_read_data() first to get current value
 *
 * The fractional value is stored as 2 bits representing 0.25C increments.
 * This function returns the value multiplied by 25 for easier display.
 */
uint16_t max31855_get_temp_frac(MAX31855_HandleTypeDef_t *hmax31855)
{
    return hmax31855->payload.therm_temp_fractual_value * 25;
}

/**
 * @brief Get thermocouple temperature as float32
 * @param[in] hmax31855 Pointer to MAX31855 handle
 * @return Temperature in degrees Celsius as float32_t
 * @pre Call max31855_read_data() first to get current value
 *
 * Combines sign, integer, and fractional parts into a single float value.
 */
float32_t max31855_get_temp_f32(MAX31855_HandleTypeDef_t *hmax31855)
{
    uint16_t temp_sign = max31855_get_temp_sign(hmax31855);
    uint16_t temp_val = max31855_get_temp_val(hmax31855);
    float32_t temp_frac = hmax31855->payload.therm_temp_fractual_value * 0.25f;

    if (1 == temp_sign) {
        return -((float32_t)temp_val + temp_frac);
    } else {
        return (float32_t)temp_val + temp_frac;
    }
}

/*===========================================================================*/
/* DEBUG FUNCTIONS                                                           */
/*===========================================================================*/

/**
 * @brief Print a single byte in binary format
 * @param[in] byte Byte to print
 *
 * Outputs 8 bits from MSB to LSB.
 */
static void print_binary_2(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (byte >> i) & 0x01);
    }
}

/**
 * @brief Print raw payload bytes in binary format
 * @param[in] hmax31855 Pointer to MAX31855 handle
 *
 * Outputs all 4 bytes separated by spaces.
 */
static void print_raw_payload_in_binary(MAX31855_HandleTypeDef_t *hmax31855) {
    for (int i = 0; i < 4; i++) {
        print_binary_2(hmax31855->raw_payload[i]);
        printf(" ");
    }
    printf("\r\n");
}

/**
 * @brief Print payload values in human-readable format
 * @param[in] hmax31855 Pointer to MAX31855 handle
 *
 * Outputs internal and thermocouple temperatures plus fault status.
 */
void max_31855_print_payload(MAX31855_HandleTypeDef_t *hmax31855) {
    print_raw_payload_in_binary(hmax31855);

    uint16_t intTempSign = max31855_get_int_temp_sign(hmax31855);
    uint16_t intTempValue = max31855_get_int_temp_val(hmax31855);
    uint16_t intTempFrac = max31855_get_int_temp_frac(hmax31855);

    uint16_t thermTempSign = max31855_get_temp_sign(hmax31855);
    uint16_t thermTempValue = max31855_get_temp_val(hmax31855);
    uint16_t thermTempFrac = max31855_get_temp_frac(hmax31855);

    printf("Internal Temperature: %s%d.%dC\r\n",
           intTempSign ? "-" : "",
           intTempValue,
           intTempFrac);

    printf("Thermocouple Temperature: %s%d.%dC\r\n",
           thermTempSign ? "-" : "",
           thermTempValue,
           thermTempFrac);

    printf("Fault Status: %s\r\n",
           hmax31855->payload.fault ? "Fault Detected" : "No Fault");
    if (hmax31855->payload.fault) {
        printf("    OC Fault: %s\r\n", hmax31855->payload.oc_fault ? "Yes" : "No");
        printf("    SCG Fault: %s\r\n", hmax31855->payload.scg_fault ? "Yes" : "No");
        printf("    SCV Fault: %s\r\n", hmax31855->payload.scv_fault ? "Yes" : "No");
    }
}

/**
 * @brief Print a value in binary format with specified number of bits
 * @param[in] value Value to print
 * @param[in] num_bits Number of bits to display
 */
void print_binary(uint16_t value, int num_bits) {
    for (int i = num_bits - 1; i >= 0; i--) {
        printf("%d", (value >> i) & 0x01);
    }
}

/**
 * @brief Print all payload fields in binary format
 * @param[in] hmax31855 Pointer to MAX31855 handle
 *
 * Outputs each field of the parsed payload with its binary representation.
 * Useful for debugging bit extraction logic.
 */
void max_31855_print_max31855_payload_binary(MAX31855_HandleTypeDef_t* hmax31855) {
    max_31855_print_payload(hmax31855);
    print_raw_payload_in_binary(hmax31855);
    max31855_payload_t payload = hmax31855->payload;

    printf("therm_temp_sign = ");
    print_binary(payload.therm_temp_sign, 1);
    printf("\r\n");

    printf("therm_temp_value = ");
    print_binary(payload.therm_temp_value, 11);
    printf("\r\n");

    printf("therm_temp_fractual_value = ");
    print_binary(payload.therm_temp_fractual_value, 2);
    printf("\r\n");

    printf("reserved = ");
    print_binary(0, 1);
    printf("\r\n");

    printf("fault = ");
    print_binary(payload.fault, 1);
    printf("\r\n");

    printf("int_temp_sign = ");
    print_binary(payload.int_temp_sign, 1);
    printf("\r\n");

    printf("int_temp_value = ");
    print_binary(payload.int_temp_value, 7);
    printf("\r\n");

    printf("int_temp_fractual_value = ");
    print_binary(payload.int_temp_fractual_value, 4);
    printf("\r\n");

    printf("reserved = ");
    print_binary(0, 1);
    printf("\r\n");

    printf("scv_fault = ");
    print_binary(payload.scv_fault, 1);
    printf("\r\n");

    printf("scg_fault = ");
    print_binary(payload.scg_fault, 1);
    printf("\r\n");

    printf("oc_fault = ");
    print_binary(payload.oc_fault, 1);
    printf("\r\n");
}
