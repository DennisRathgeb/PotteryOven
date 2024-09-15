/*
 * MAX31855_KASA_T.c
 *
 *  Created on: Mar 31, 2024
 *      Author: Dennis Rathgeb
 */
#include "MAX31855.h"
#include "log.h"
#include <stdio.h>



/*
 * init Function of handle for MAX31855
 */
HAL_StatusTypeDef max31855_init(MAX31855_HandleTypeDef_t* hmax31855, SPI_HandleTypeDef* hspi)
{
    hmax31855->hspi = hspi;
    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_SET);
    return HAL_OK;
}

/*
 * updates payload struct fields with values just read
 */
static HAL_StatusTypeDef max31855_update_payload(MAX31855_HandleTypeDef_t *hmax31855) {
    if (NULL == hmax31855) {
        return HAL_ERROR;
    }

    // Extract the raw 32-bit data from the 4-byte array
    uint32_t raw_data = ((uint32_t)hmax31855->raw_payload[0] << 24) |
                        ((uint32_t)hmax31855->raw_payload[1] << 16) |
                        ((uint32_t)hmax31855->raw_payload[2] << 8) |
                        (uint32_t)hmax31855->raw_payload[3];

    // Extract individual fields using bitwise AND and right shifts
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


/*
 * reads and updates data through SPi from MAX31855
 */
HAL_StatusTypeDef max31855_read_data(MAX31855_HandleTypeDef_t *hmax31855)
{
    if (NULL == hmax31855)
        {
            return HAL_ERROR;
        }

    if (NULL == &hmax31855->hspi)
        {
            return HAL_ERROR;
        }
    //TODO: non blocking implementation (DMA?)
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


/*
 * Returns sign of last read value from MAX31855.
 * 1 = negative, 0 = positive
 * Call  max31855_read_data() first to get an up to date value.
 */
uint16_t max31855_get_temp_sign(MAX31855_HandleTypeDef_t *hmax31855)
{
    return hmax31855->payload.therm_temp_sign;
}

/*
 * Returns unsigned value of last read value from MAX31855.
 * Check sign with max31855_get_temp_sign().
 * Call  max31855_read_data() first to get an up to date value.
 */
uint16_t max31855_get_temp_val(MAX31855_HandleTypeDef_t *hmax31855)
{
    if(0 == hmax31855->payload.therm_temp_sign)
    {
        return hmax31855->payload.therm_temp_value;
    }

    return (~(hmax31855->payload.therm_temp_value) + 1);
}

/*
 *  returns fractual value of last read value from MAX31855
 *  return format: after comma two digit number in .25 steps eg: 0.75 will return as 75
 *  Check sign with max31855_get_temp_sign().
 *  Call  max31855_read_data() first to get an up to date value.
 */
uint16_t max31855_get_temp_frac(MAX31855_HandleTypeDef_t *hmax31855)
{
    return hmax31855->payload.therm_temp_fractual_value * 25;
}


/*
 * Returns float32_t of last read value from MAX31855.
 * Call  max31855_read_data() first to get an up to date value.
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


//DEBUG STUFF PRINTS *******************************************************************//TODO:REMOVE
static void print_binary_2(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (byte >> i) & 0x01);
    }
}

static void print_raw_payload_in_binary(MAX31855_HandleTypeDef_t *hmax31855) {
    for (int i = 0; i < 4; i++) {
        print_binary_2(hmax31855->raw_payload[i]);
        printf(" "); // Add space between bytes for readability
    }
    printf("\r\n");
}
void max_31855_print_payload(MAX31855_HandleTypeDef_t *hmax31855) {
    print_raw_payload_in_binary(hmax31855);
    // Assuming your get functions are correctly implemented
    uint16_t intTempSign = max31855_get_int_temp_sign(hmax31855); // Update based on your actual implementation
    uint16_t intTempValue = max31855_get_int_temp_val(hmax31855); // This needs to be adapted if you implement it differently
    uint16_t intTempFrac = max31855_get_int_temp_frac(hmax31855); // Adapt this according to your actual function

    uint16_t thermTempSign = max31855_get_temp_sign(hmax31855); // Assuming same function can be used for thermocouple temp
    uint16_t thermTempValue = max31855_get_temp_val(hmax31855); // Adapt for thermocouple
    uint16_t thermTempFrac = max31855_get_temp_frac(hmax31855); // Adapt for thermocouple

    printf("Internal Temperature: %s%d.%dC\r\n",
           intTempSign ? "-" : "",
           intTempValue,
           intTempFrac);

    printf("Thermocouple Temperature: %s%d.%dC\r\n",
           thermTempSign ? "-" : "",
           thermTempValue,
           thermTempFrac);

    // Print fault status
    printf("Fault Status: %s\r\n",
           hmax31855->payload.fault ? "Fault Detected" : "No Fault");
    if (hmax31855->payload.fault) {
        printf("    OC Fault: %s\r\n", hmax31855->payload.oc_fault ? "Yes" : "No");
        printf("    SCG Fault: %s\r\n", hmax31855->payload.scg_fault ? "Yes" : "No");
        printf("    SCV Fault: %s\r\n", hmax31855->payload.scv_fault ? "Yes" : "No");
    }
}
void print_binary(uint16_t value, int num_bits) {
    for (int i = num_bits - 1; i >= 0; i--) {
        printf("%d", (value >> i) & 0x01);
    }
}


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
    print_binary(0, 1); // Assuming reserved field is always 0
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
    print_binary(0, 1); // Assuming reserved field is always 0
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







