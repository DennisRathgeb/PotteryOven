/**
 * @file encoder.h
 * @brief Rotary encoder driver with interrupt-based rotation detection
 * @author Dennis Rathgeb
 * @date Apr 8, 2024
 *
 * Driver for rotary encoder with integrated push button.
 * The button and output A of the encoder are connected to GPIO interrupts.
 * In the interrupt callback, encoder_callback() is called with the pin
 * that triggered the interrupt so the encoder can update its state.
 *
 * When the button is pressed, position and direction are reset to zero.
 *
 * @note Uses TIM3 input capture for encoder rotation detection.
 */

#ifndef INC_ENCODER_H_
#define INC_ENCODER_H_

#include "main.h"
#include "stm32f0xx_hal.h"
#include "ui.h"
#include "event.h"

/**
 * @brief Handle structure for the rotary encoder
 *
 * Contains GPIO configuration, event queue reference, and state tracking.
 */
typedef struct
{
    /* Encoder Output A - Position signal */
    GPIO_TypeDef* port_a;   /**< GPIO port for encoder output A */
    uint16_t pin_a;         /**< GPIO pin for encoder output A */

    /* Encoder Output B - Direction signal */
    GPIO_TypeDef* port_b;   /**< GPIO port for encoder output B */
    uint16_t pin_b;         /**< GPIO pin for encoder output B */

    /* Encoder Button */
    GPIO_TypeDef* port_button;  /**< GPIO port for encoder push button */
    uint16_t pin_button;        /**< GPIO pin for encoder push button */

    Event_Queue_HandleTypeDef_t *queue; /**< Pointer to event queue for posting encoder events */
    uint8_t direction;  /**< Last rotation direction: 1 = clockwise, 0 = counter-clockwise */
    int position;       /**< Accumulated position counter (can be negative) */

} Encoder_HandleTypeDef_t;

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
 */
HAL_StatusTypeDef init_Encoder(Encoder_HandleTypeDef_t* hencoder, Event_Queue_HandleTypeDef_t *queue,
        GPIO_TypeDef* port_a, uint16_t pin_a,
        GPIO_TypeDef* port_b, uint16_t pin_b,
        GPIO_TypeDef* port_button, uint16_t pin_button);

/**
 * @brief Process encoder interrupt callback
 * @param[in,out] hencoder Pointer to encoder handle
 * @param[in] pin GPIO pin that triggered the interrupt (0xff for rotation, button pin for button)
 *
 * Call this function from HAL_GPIO_EXTI_Callback() or HAL_TIM_IC_CaptureCallback().
 * For rotation events, pass 0xff as the pin parameter.
 * For button press, pass the button pin value.
 */
void encoder_callback(Encoder_HandleTypeDef_t* hencoder, uint16_t pin);

/**
 * @brief Get both position and direction of the encoder
 * @param[in] hencoder Pointer to encoder handle
 * @param[out] position Pointer to store current position
 * @param[out] direction Pointer to store last direction (1=CW, 0=CCW)
 * @return HAL_OK on success, HAL_ERROR if handle is NULL
 */
HAL_StatusTypeDef encoder_get_state(Encoder_HandleTypeDef_t *hencoder, uint8_t *position, uint8_t *direction);

/**
 * @brief Get current encoder position
 * @param[in] hencoder Pointer to encoder handle
 * @param[out] position Pointer to store current position
 * @return HAL_OK on success, HAL_ERROR if handle is NULL
 * @note Position is truncated to uint8_t, may lose information for large values
 */
HAL_StatusTypeDef encoder_get_position(Encoder_HandleTypeDef_t *hencoder, uint8_t *position);

/**
 * @brief Get last rotation direction
 * @param[in] hencoder Pointer to encoder handle
 * @param[out] direction Pointer to store direction (1=clockwise, 0=counter-clockwise)
 * @return HAL_OK on success, HAL_ERROR if handle is NULL
 */
HAL_StatusTypeDef encoder_get_direction(Encoder_HandleTypeDef_t *hencoder, uint8_t *direction);

#endif /* INC_ENCODER_H_ */
