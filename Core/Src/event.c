/*
 * events.c
 *
 *  Created on: Apr 26, 2024
 *      Author: Dennis Rathgeb
 */
#include <event.h>
#include <stdio.h>
#include <stdlib.h>
/*
 * logs type to terminal if event calleback gets called
 */
static void event_diplay_type(event_type_t event){
#ifdef EVENT_ENABLE_LOG
    switch (event) {
        case BUT1:
            logMsg(LOG_INFO, "EVENT: BUT1 event detected");
            break;
        case BUT2:
            logMsg(LOG_INFO, "EVENT: BUT2 event detected");
            break;
        case BUT3:
            logMsg(LOG_INFO, "EVENT: BUT3 event detected");
            break;
        case BUT4:
            logMsg(LOG_INFO, "EVENT: BUT4 event detected");
            break;
        case ENC_BUT:
            logMsg(LOG_INFO, "EVENT: ENC_BUT event detected");
            break;
        case ENC_UP:
            logMsg(LOG_INFO, "EVENT: ENC_UP event detected");
            break;
        case ENC_DOWN:
            logMsg(LOG_INFO, "EVENT: ENC_DOWN event detected");
            break;
        default:
            logMsg(LOG_WARNING, "EVENT: unknown button event detected: %u", event);
            break;
    }
#endif
}
// Function to initialize the queue
HAL_StatusTypeDef initEvent(Event_Queue_HandleTypeDef_t* queue) {
    if (queue == NULL) {
#ifdef EVENT_ENABLE_LOG
        logMsg(LOG_ERROR, "EVENT: Init failed, queue is empty!\r\n");
#endif
        return HAL_ERROR;
    }
    queue->front = queue->rear = NULL;
    return HAL_OK; // Assuming HAL_OK is defined appropriately
}

// Function to check if the queue is empty
uint8_t event_isEmpty(Event_Queue_HandleTypeDef_t* queue) {
    return (queue->front == NULL);
}

// Function to create a new node
event_node_t* event_createNode(event_type_t data) {
    event_node_t* newNode = (event_node_t*)malloc(sizeof(event_node_t));
    if (newNode == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    newNode->data = data;
    newNode->next = NULL;
    return newNode;
}

// Function to enqueue an event
void event_enqueue(Event_Queue_HandleTypeDef_t* queue, event_type_t event) {
    event_diplay_type(event);
    event_node_t* newNode = event_createNode(event);
    if (event_isEmpty(queue)) {
        queue->front = queue->rear = newNode;
    } else {
        queue->rear->next = newNode;
        queue->rear = newNode;
    }
}

// Function to dequeue an event
event_type_t event_dequeue(Event_Queue_HandleTypeDef_t* queue) {
    if (event_isEmpty(queue)) {
        fprintf(stderr, "Queue is empty\n");
        exit(EXIT_FAILURE);
    }
    event_node_t* temp = queue->front;
    event_type_t data = temp->data;
    queue->front = queue->front->next;
    free(temp);
    if (queue->front == NULL) {
        queue->rear = NULL;
    }
    return data;
}

// Function to display the queue contents
void event_displayQueue(Event_Queue_HandleTypeDef_t* queue) {
    event_node_t* current = queue->front;
    printf("Queue: ");
    while (current != NULL) {
        printf("%d ", current->data);
        current = current->next;
    }
    printf("\r\n");
}





