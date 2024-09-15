/*
 * MAX31855_KASA_T.h
 *
 *  Created on: Mar 31, 2024
 *      Author: Dennis Rathgeb
 */

#ifndef MAX31855_INC_MAX31855_H_
#define MAX31855_INC_MAX31855_H_

#include "stm32f0xx_hal.h"
#include "main.h"
#include "arm_math.h"

#define MAX31855_PAYLOAD_LENGTH  32
#define MAX31855_TIMEOUT  1000000
#include "../Inc/MAX31855.h"
/*
 * Raw payload bitfield
 */
//TODO: Test if bit order is correct. D31 should be first transmitted...
typedef struct __attribute__((packed))
{
    uint16_t therm_temp_sign :1;           //thermocouple temperature sign, 1= neg
    uint16_t therm_temp_value :11;         //thermocouple temperature value
    uint16_t therm_temp_fractual_value :2; // thermocouple temperature fractual part of value in 0.25C increments
    uint16_t :1;                           // reserved.
    uint16_t fault :1;                     // This bit reads at 1 when any of the SCV, SCG, or OC faults are active. Default value is 0.
    uint16_t int_temp_sign :1;             //reference junction temperaturesign sign, 1=negative
    uint16_t int_temp_value :7;            //reference junction temperature value
    uint16_t int_temp_fractual_value :4;   // reference junction temperature fractual part of value in 0.25C increments
    uint16_t :1;                           // reserved.
    uint16_t scv_fault :1;                 // This bit is a 1 when the thermocouple is short-circuited to VCC. Default value is 0.
    uint16_t scg_fault :1;                 // This bit is a 1 when the thermocouple is short-circuited to GND. Default value is 0.
    uint16_t oc_fault :1;                  // This bit is a 1 when the thermocouple is open (no connections). Default value is 0.

} max31855_payload_t;

//Strange stuff??? //TODO: find out unions
/*typedef struct {
    uint8_t therm_temp_sign;
    uint16_t therm_temp_value;
    uint8_t therm_temp_fractual_value;
    uint8_t reserved1;
    uint8_t fault;
    uint8_t int_temp_sign;
    uint8_t int_temp_value;
    uint8_t int_temp_fractual_value;
    uint8_t reserved2;
    uint8_t scv_fault;
    uint8_t scg_fault;
    uint8_t oc_fault;
} max31855_payload_t;
typedef union {
    uint8_t raw_payload[4];
    max31855_payload_t payload;
} max31855_data_union_t;*/

typedef struct
{
    SPI_HandleTypeDef* hspi;
    uint8_t raw_payload[4];
    max31855_payload_t payload;
    //max31855_data_union_t data;

}MAX31855_HandleTypeDef_t;

HAL_StatusTypeDef max31855_init(MAX31855_HandleTypeDef_t* hmax31855, SPI_HandleTypeDef* hspi);
void max_31855_print_max31855_payload_binary(MAX31855_HandleTypeDef_t* hmax31855);
void max_31855_print_payload(MAX31855_HandleTypeDef_t *hmax31855);
HAL_StatusTypeDef max31855_read_data(MAX31855_HandleTypeDef_t *hmax31855);
uint16_t max31855_get_temp_sign(MAX31855_HandleTypeDef_t *hmax31855);
uint16_t max31855_get_temp_val(MAX31855_HandleTypeDef_t *hmax31855);
uint16_t max31855_get_temp_frac(MAX31855_HandleTypeDef_t *hmax31855);
uint16_t max31855_get_int_temp_sign(MAX31855_HandleTypeDef_t *hmax31855);
uint16_t max31855_get_int_temp_val(MAX31855_HandleTypeDef_t *hmax31855);
uint16_t max31855_get_int_temp_frac(MAX31855_HandleTypeDef_t *hmax31855);
float32_t max31855_get_temp_f32(MAX31855_HandleTypeDef_t *hmax31855);

#endif /* MAX31855_INC_MAX31855_H_ */
