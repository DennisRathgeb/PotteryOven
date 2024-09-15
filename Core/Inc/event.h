/*
 * events.h
 *
 *  Created on: Apr 26, 2024
 *      Author: Dennis Rathgeb
 */

#ifndef INC_EVENT_H_
#define INC_EVENT_H_

#include "stm32f0xx_hal.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>

//#define EVENT_ENABLE_LOG //enable debug output to uart


//Enum for all possible events
typedef enum
{
    NO_EVENT = 0,
    BUT1 = 1,
    BUT2 = 2,
    BUT3 = 3,
    BUT4 = 4,
    ENC_BUT = 5,
    ENC_UP = 6,
    ENC_DOWN = 7
}event_type_t;

// Define the node structure for the queue
typedef struct event_node_t {
    event_type_t data;         // Event data
    struct event_node_t* next;  // Pointer to the next node
} event_node_t;

// Define the queue structure
typedef struct {
    event_node_t* front;    // Pointer to the front of the queue
    event_node_t* rear;     // Pointer to the rear of the queue
} Event_Queue_HandleTypeDef_t;

HAL_StatusTypeDef initEvent(Event_Queue_HandleTypeDef_t* queue);
uint8_t event_isEmpty(Event_Queue_HandleTypeDef_t* queue);
event_node_t* event_createNode(event_type_t data);
void event_enqueue(Event_Queue_HandleTypeDef_t* queue, event_type_t event);
event_type_t event_dequeue(Event_Queue_HandleTypeDef_t* queue);
void event_displayQueue(Event_Queue_HandleTypeDef_t* queue);

#endif /* INC_EVENT_H_ */
