/**
 * @file log.h
 * @brief Logging module with configurable log levels and UART output
 * @author Dennis Rathgeb
 * @date Nov 15, 2023
 *
 * This module implements a logging feature with multiple logging levels:
 * LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR
 *
 * Output is sent to UART (handle passed to initLog).
 * Printf is rerouted to UART if REROUTE_PRINTF is defined.
 *
 * @todo Implement logging feature to file
 */

#ifndef INC_LOG_H_
#define INC_LOG_H_

#include "stm32f0xx_hal.h"

/**
 * @defgroup LOG_LEVELS Logging Level Definitions
 * @brief Log level constants for message filtering
 * @{
 */
#define LOG_DEBUG   0  /**< Debug level - verbose debugging information */
#define LOG_INFO    1  /**< Info level - general information messages */
#define LOG_WARNING 2  /**< Warning level - potential issues */
#define LOG_ERROR   3  /**< Error level - error conditions */
/** @} */

/**
 * @brief Threshold for log message filtering
 *
 * Messages with log level below this threshold will not be displayed.
 * Set to LOG_DEBUG (0) to show all messages.
 */
#define LOG_LEVEL_THRESHOLD 0

/**
 * @brief Enable printf rerouting to UART
 *
 * When defined, printf output is redirected to the UART handle
 * passed to initLog().
 */
#define REROUTE_PRINTF

/**
 * @brief Initialize the logging module
 * @param[in] huart Pointer to UART handle for log output
 */
void initLog(UART_HandleTypeDef* huart);

/**
 * @brief Log a message with specified log level
 * @param[in] logLevel Log level (LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR)
 * @param[in] format Printf-style format string
 * @param[in] ... Variable arguments for format string
 *
 * Messages are only output if logLevel >= LOG_LEVEL_THRESHOLD
 */
void logMsg(int logLevel, const char* format, ...);

#ifdef REROUTE_PRINTF
#ifdef __GNUC__
/** @brief Prototype for putchar redirection (GCC) */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
/** @brief Prototype for getchar redirection (GCC) */
#define GETCHAR_PROTOTYPE int __io_getchar(void)
#else
/** @brief Prototype for putchar redirection (non-GCC) */
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
/** @brief Prototype for getchar redirection (non-GCC) */
#define GETCHAR_PROTOTYPE int getc(FILE *f)
#endif /* __GNUC__ */
#endif

#endif /* INC_LOG_H_ */
