/*
 * log.c
 *
 *  Created on: Nov 15, 2023
 *      Author: Dennis Rathgeb
 *       *      implements a logging feature with logging levels:
 *      LOG_DEBUG,LOG_INFO,LOG_WARNING,LOG_ERROR
 *
 *      outputs it to uart(handle gets passed in initLog)
 *      also reroutes printf if REROUTE_PRINTF is defined
 *
 *
 *      TBD: logging feature to file
 *
 *      */

#include <log.h>
#include <stdio.h>
#include <stdarg.h>


static UART_HandleTypeDef* hlog_huart; // Store the UART handle

/**
 * @brief initializes all needed log params
 * @param uart handle to output printf
 * @return none
 */
void initLog(UART_HandleTypeDef* huart) {
    hlog_huart = huart; // Store the UART handle for later use
}

/**
 * @brief logging feature: log levels and mesage
 * @param variatic params
 * @return none
 */

void logMsg(int logLevel, const char* format, ...) {
    if (logLevel >= LOG_LEVEL_THRESHOLD) {
        va_list args;
        va_start(args, format);

        char buffer[256]; // Adjust the buffer size as needed
        vsnprintf(buffer, sizeof(buffer), format, args);

        // Output log message through UART using printf
        printf("%s", buffer);
        printf("\r\n");

        va_end(args);
    }
}


//REROUTING PRINTF STUFF
#ifdef REROUTE_PRINTF
PUTCHAR_PROTOTYPE {

    HAL_UART_Transmit(hlog_huart, (uint8_t*) &ch, 1, HAL_MAX_DELAY);

    return ch;
}
GETCHAR_PROTOTYPE {

    int ch = 0;
    HAL_UART_Receive(hlog_huart, (uint8_t*) &ch, 1, HAL_MAX_DELAY);
    return ch;
}
#endif
