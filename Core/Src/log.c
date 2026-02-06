/**
 * @file log.c
 * @brief Implementation of logging module with UART output
 * @author Dennis Rathgeb
 * @date Nov 15, 2023
 *
 * Implements a logging feature with logging levels:
 * LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR
 *
 * Outputs to UART (handle passed to initLog).
 * Also reroutes printf if REROUTE_PRINTF is defined.
 *
 * @todo Implement logging feature to file
 */

#include <log.h>
#include <stdio.h>
#include <stdarg.h>

/** @brief Static UART handle for log output */
static UART_HandleTypeDef* hlog_huart;

/**
 * @brief Initialize the logging module
 * @param[in] huart Pointer to UART handle for log output
 *
 * Stores the UART handle for use by logMsg and printf redirection.
 */
void initLog(UART_HandleTypeDef* huart) {
    hlog_huart = huart;
}

/**
 * @brief Log a message with specified log level
 * @param[in] logLevel Log level (LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR)
 * @param[in] format Printf-style format string
 * @param[in] ... Variable arguments for format string
 *
 * Messages are only output if logLevel >= LOG_LEVEL_THRESHOLD.
 * Output is sent to UART via printf (which is rerouted to UART).
 */
void logMsg(int logLevel, const char* format, ...) {
    if (logLevel >= LOG_LEVEL_THRESHOLD) {
        va_list args;
        va_start(args, format);

        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);

        printf("%s", buffer);
        printf("\r\n");

        va_end(args);
    }
}

#ifdef REROUTE_PRINTF
/**
 * @brief Redirect putchar to UART transmit
 * @param[in] ch Character to transmit
 * @return The character transmitted
 *
 * This function is called by printf to output each character.
 * Transmits the character via HAL_UART_Transmit with blocking.
 */
PUTCHAR_PROTOTYPE {
    HAL_UART_Transmit(hlog_huart, (uint8_t*) &ch, 1, HAL_MAX_DELAY);
    return ch;
}

/**
 * @brief Redirect getchar to UART receive
 * @return The character received
 *
 * This function blocks until a character is received via UART.
 */
GETCHAR_PROTOTYPE {
    int ch = 0;
    HAL_UART_Receive(hlog_huart, (uint8_t*) &ch, 1, HAL_MAX_DELAY);
    return ch;
}
#endif
