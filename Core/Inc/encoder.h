/*
 * encoder.h
 *
 *  Created on: Apr 8, 2024
 *      Author: Dennis Rathgeb
 */

#ifndef INC_ENCODER_H_
#define INC_ENCODER_H_

#include "main.h"
#include "stm32f0xx_hal.h"
#include "ui.h"
#include "event.h"
/*
 * Driver for encoder, button, and output a of encoder are on interrupt,
 * in interrupt callback encoder_callback() with the pin of eighter button or A gets called so encoder can update
 * if button gets pressed position and directions get reset to zero;
 *
 *
 */
typedef struct
{
    //Encoder Output Ports and Pins
    //position
    GPIO_TypeDef* port_a;
    uint16_t pin_a;
    //direction
    GPIO_TypeDef* port_b;
    uint16_t pin_b;
    //Button Port and Pin
    GPIO_TypeDef* port_button;
    uint16_t pin_button;

    Event_Queue_HandleTypeDef_t *queue;
    uint8_t direction; //keeps track of position: 1 if CW 0 if CCW
    int position; //keeps track of position


}Encoder_HandleTypeDef_t;
//Init struct, sets all Ports/pins and inits all variables
HAL_StatusTypeDef init_Encoder(Encoder_HandleTypeDef_t* hencoder, Event_Queue_HandleTypeDef_t *queue, GPIO_TypeDef* port_a,
        uint16_t pin_a, GPIO_TypeDef* port_b, uint16_t pin_b, GPIO_TypeDef* port_button, uint16_t pin_button);
void encoder_callback(Encoder_HandleTypeDef_t* hencoder, uint16_t pin);

HAL_StatusTypeDef encoder_get_state(Encoder_HandleTypeDef_t *hencoder, uint8_t *position, uint8_t *direction);
HAL_StatusTypeDef encoder_get_position(Encoder_HandleTypeDef_t *hencoder, uint8_t *position);
HAL_StatusTypeDef encoder_get_direction(Encoder_HandleTypeDef_t *hencoder, uint8_t *direction);

#endif /* INC_ENCODER_H_ */
