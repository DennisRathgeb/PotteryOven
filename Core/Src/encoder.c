/**
 * @file encoder.c
 * @brief Implementation of rotary encoder driver
 * @author Dennis Rathgeb
 * @date Apr 8, 2024
 *
 * Implements interrupt-driven rotary encoder with push button support.
 * Rotation events generate ENC_UP/ENC_DOWN events, button generates ENC_BUT.
 */

#include "encoder.h"

/**
 * @brief Update encoder position based on rotation direction
 * @param[in,out] hencoder Pointer to encoder handle
 * @param[in] direction Rotation direction: 1 = clockwise, 0 = counter-clockwise
 *
 * Increments or decrements position counter and posts appropriate event to queue.
 */
static void update_position(Encoder_HandleTypeDef_t *hencoder, uint8_t direction);

/**
 * @brief Initialize the encoder driver
 * @param[out] hencoder Pointer to encoder handle to initialize
 * @param[in] queue Pointer to event queue for posting events
 * @param[in] port_a GPIO port for encoder output A
 * @param[in] pin_a GPIO pin for encoder output A
 * @param[in] port_b GPIO port for encoder output B
 * @param[in] pin_b GPIO pin for encoder output B
 * @param[in] port_button GPIO port for encoder button
 * @param[in] pin_button GPIO pin for encoder button
 * @return HAL_OK on success
 *
 * Stores GPIO configuration and initializes position/direction to zero.
 */
HAL_StatusTypeDef init_Encoder(Encoder_HandleTypeDef_t* hencoder, Event_Queue_HandleTypeDef_t *queue,
        GPIO_TypeDef* port_a, uint16_t pin_a,
        GPIO_TypeDef* port_b, uint16_t pin_b,
        GPIO_TypeDef* port_button, uint16_t pin_button)
{
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

/**
 * @brief Process encoder interrupt callback
 * @param[in,out] hencoder Pointer to encoder handle
 * @param[in] pin GPIO pin that triggered the interrupt
 *
 * Called from interrupt context to update encoder state.
 * - If pin is 0xff: reads A and B pins to determine rotation direction
 * - If pin matches button pin: posts ENC_BUT event and resets position
 *
 * Direction is determined by comparing A and B states:
 * - Same state = clockwise (direction 0)
 * - Different state = counter-clockwise (direction 1)
 */
void encoder_callback(Encoder_HandleTypeDef_t* hencoder, uint16_t pin)
{
    if (pin == 0xff)
    {
        uint8_t state_a = HAL_GPIO_ReadPin(hencoder->port_a, hencoder->pin_a);
        uint8_t state_b = HAL_GPIO_ReadPin(hencoder->port_b, hencoder->pin_b);

        uint8_t direction = (state_a == state_b) ? 0 : 1;

        update_position(hencoder, direction);
    }
    else if (pin == hencoder->pin_button)
    {
        event_enqueue(hencoder->queue, ENC_BUT);
        hencoder->position = 0;
        hencoder->direction = 0;
    }
}

/**
 * @brief Update encoder position based on rotation direction
 * @param[in,out] hencoder Pointer to encoder handle
 * @param[in] direction Rotation direction: 1 = clockwise, 0 = counter-clockwise
 *
 * Updates position counter and direction field, then posts ENC_UP or ENC_DOWN
 * event to the event queue.
 */
static void update_position(Encoder_HandleTypeDef_t *hencoder, uint8_t direction)
{
    if (direction)
    {
        hencoder->position++;
        hencoder->direction = 1;
        event_enqueue(hencoder->queue, ENC_UP);
    }
    else
    {
        hencoder->position--;
        hencoder->direction = 0;
        event_enqueue(hencoder->queue, ENC_DOWN);
    }
}

/**
 * @brief Get both position and direction of the encoder
 * @param[in] hencoder Pointer to encoder handle
 * @param[out] position Pointer to store current position
 * @param[out] direction Pointer to store last direction (1=CW, 0=CCW)
 * @return HAL_OK on success, HAL_ERROR if any getter fails
 *
 * Convenience function that calls both encoder_get_position and encoder_get_direction.
 */
HAL_StatusTypeDef encoder_get_state(Encoder_HandleTypeDef_t* hencoder, uint8_t* position, uint8_t* direction)
{
    uint8_t status = HAL_OK;
    status = encoder_get_position(hencoder, position);
    if(HAL_OK != status)
    {
        return status;
    }
    status = encoder_get_direction(hencoder, direction);
    if(HAL_OK != status)
    {
        return status;
    }
    return HAL_OK;
}

/**
 * @brief Get current encoder position
 * @param[in] hencoder Pointer to encoder handle
 * @param[out] position Pointer to store current position (truncated to uint8_t)
 * @return HAL_OK on success, HAL_ERROR if handle is NULL
 * @warning Position is cast to uint8_t, values outside 0-255 will be truncated
 */
HAL_StatusTypeDef encoder_get_position(Encoder_HandleTypeDef_t* hencoder, uint8_t* position)
{
    if(NULL == hencoder)
    {
        return HAL_ERROR;
    }

    *position = hencoder->position;

    return HAL_OK;
}

/**
 * @brief Get last rotation direction
 * @param[in] hencoder Pointer to encoder handle
 * @param[out] direction Pointer to store direction (1=clockwise, 0=counter-clockwise)
 * @return HAL_OK on success, HAL_ERROR if handle is NULL
 */
HAL_StatusTypeDef encoder_get_direction(Encoder_HandleTypeDef_t* hencoder, uint8_t* direction)
{
    if(NULL == hencoder)
    {
        return HAL_ERROR;
    }

    *direction = hencoder->direction;

    return HAL_OK;
}
