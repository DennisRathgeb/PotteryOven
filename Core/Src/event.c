/**
 * @file event.c
 * @brief Implementation of event queue for UI event handling
 * @author Dennis Rathgeb
 * @date Apr 26, 2024
 *
 * Implements a linked-list FIFO queue for decoupling interrupt handlers
 * from UI processing. Events from buttons and encoder are queued here.
 */

#include <event.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Log event type to terminal when event callback is triggered
 * @param[in] event Event type to display
 *
 * Only outputs if EVENT_ENABLE_LOG is defined.
 */
static void event_diplay_type(event_type_t event) {
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

/**
 * @brief Initialize the event queue
 * @param[in,out] queue Pointer to queue handle to initialize
 * @return HAL_OK on success, HAL_ERROR if queue is NULL
 *
 * Sets front and rear pointers to NULL, indicating an empty queue.
 */
HAL_StatusTypeDef initEvent(Event_Queue_HandleTypeDef_t* queue) {
    if (queue == NULL) {
#ifdef EVENT_ENABLE_LOG
        logMsg(LOG_ERROR, "EVENT: Init failed, queue is empty!\r\n");
#endif
        return HAL_ERROR;
    }
    queue->front = queue->rear = NULL;
    return HAL_OK;
}

/**
 * @brief Check if the event queue is empty
 * @param[in] queue Pointer to queue handle
 * @return 1 if queue is empty (front is NULL), 0 otherwise
 */
uint8_t event_isEmpty(Event_Queue_HandleTypeDef_t* queue) {
    return (queue->front == NULL);
}

/**
 * @brief Create a new event node with dynamic memory allocation
 * @param[in] data Event type to store in the node
 * @return Pointer to newly allocated and initialized node
 * @warning Calls exit(EXIT_FAILURE) on malloc failure - inappropriate for embedded systems
 */
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

/**
 * @brief Add an event to the rear of the queue
 * @param[in,out] queue Pointer to queue handle
 * @param[in] event Event type to enqueue
 *
 * Creates a new node and adds it to the rear of the queue.
 * If queue is empty, the new node becomes both front and rear.
 * Logs the event type if EVENT_ENABLE_LOG is defined.
 */
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

/**
 * @brief Remove and return the event at the front of the queue
 * @param[in,out] queue Pointer to queue handle
 * @return The event type that was at the front of the queue
 * @warning Calls exit(EXIT_FAILURE) if queue is empty - check isEmpty() first
 *
 * Removes the front node, frees its memory, and returns the stored event.
 * Updates rear to NULL if queue becomes empty after dequeue.
 */
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

/**
 * @brief Display all events in the queue (debug function)
 * @param[in] queue Pointer to queue handle
 *
 * Iterates through the queue and prints each event's numeric value.
 * Does not modify the queue.
 */
void event_displayQueue(Event_Queue_HandleTypeDef_t* queue) {
    event_node_t* current = queue->front;
    printf("Queue: ");
    while (current != NULL) {
        printf("%d ", current->data);
        current = current->next;
    }
    printf("\r\n");
}
