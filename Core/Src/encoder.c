/*
 * encoder.c
 *
 *  Created on: Apr 8, 2024
 *      Author: Dennis Rathgeb
 */


#include "encoder.h"

// Function prototypes
static void update_position(Encoder_HandleTypeDef_t *hencoder, uint8_t direction);

HAL_StatusTypeDef init_Encoder(Encoder_HandleTypeDef_t* hencoder, Event_Queue_HandleTypeDef_t *queue, GPIO_TypeDef* port_a, uint16_t pin_a,
                               GPIO_TypeDef* port_b, uint16_t pin_b, GPIO_TypeDef* port_button, uint16_t pin_button)
{
    // Initialize encoder struct
    hencoder->port_a = port_a;
    hencoder->pin_a = pin_a;
    hencoder->port_b = port_b;
    hencoder->pin_b = pin_b;
    hencoder->port_button = port_button;
    hencoder->pin_button = pin_button;

    hencoder->direction = 0;
    hencoder->position = 0;
    hencoder->queue = queue;


    return HAL_OK;
}
/*
 * gets called in callback for a,b and button, updates the position and direction of the encoder struct
 */
void encoder_callback(Encoder_HandleTypeDef_t* hencoder, uint16_t pin)
{
    // Determine which pin triggered the interrupt
    if (pin == 0xff)
    {
        // Read the state of both pins
        uint8_t state_a = HAL_GPIO_ReadPin(hencoder->port_a, hencoder->pin_a);
        uint8_t state_b = HAL_GPIO_ReadPin(hencoder->port_b, hencoder->pin_b);


        // Determine the direction of rotation based on the states of pins A and B
        uint8_t direction = (state_a == state_b) ? 0 : 1;

        // Update the position based on the direction of rotation
        update_position(hencoder, direction);
    }
    // Reset position and direction if the button is pressed
    else if (pin == hencoder->pin_button)
    {
        event_enqueue(hencoder->queue, ENC_BUT);
        hencoder->position = 0;
        hencoder->direction = 0;
        //printf("BUT\n\r");

    }
}


/*
 *  Increment or decrement position based on the direction of rotation
 */
static void update_position(Encoder_HandleTypeDef_t *hencoder, uint8_t direction)
{

    if (direction)
    {
        hencoder->position++;
        hencoder->direction = 1; // Clockwise
        event_enqueue(hencoder->queue, ENC_UP);
        //printf("CW\n\r");
    }
    else
    {
        hencoder->position--;
        hencoder->direction = 0; // Counter-clockwise
        event_enqueue(hencoder->queue, ENC_DOWN);
        //printf("CCW\n\r");
    }
}

HAL_StatusTypeDef encoder_get_state(Encoder_HandleTypeDef_t* hencoder, uint8_t* position, uint8_t* direction)
{
    uint8_t status = HAL_OK;
    status = encoder_get_position(hencoder, position);
    if(HAL_OK != status)
    {
        return status;
    }
    status = encoder_get_direction(hencoder,direction);
    if(HAL_OK != status)
    {
        return status;
    }
    return HAL_OK;
}

HAL_StatusTypeDef encoder_get_position(Encoder_HandleTypeDef_t* hencoder, uint8_t* position)
{
    if(NULL == hencoder)
    {
        return HAL_ERROR;
    }

    *position = hencoder->position;

    return HAL_OK;
}

HAL_StatusTypeDef encoder_get_direction(Encoder_HandleTypeDef_t* hencoder, uint8_t* direction)
{
    if(NULL == hencoder)
    {
        return HAL_ERROR;
    }

    *direction = hencoder->direction;

    return HAL_OK;
}
