/*
 * log.h
 *
 *  Created on: Nov 15, 2023
 *      Author: burni
 *
 *      implements a logging feature with logging levels:
 *      LOG_DEBUG,LOG_INFO,LOG_WARNING,LOG_ERROR
 *
 *      outputs it to uart(handle gets passed in initLog)
 *      also reroutes printf if REROUTE_PRINTF is defined
 *
 *
 *      TBD: logging feature to file
 */

#ifndef INC_LOG_H_
#define INC_LOG_H_


#include "stm32f0xx_hal.h"

#define LOG_DEBUG   0
#define LOG_INFO    1
#define LOG_WARNING 2
#define LOG_ERROR   3

//adjusts which messages get deisplayed
#define LOG_LEVEL_THRESHOLD 0



//if rerouting printf is not yet handled enable difine:
#define REROUTE_PRINTF
void initLog(UART_HandleTypeDef* huart);
void logMsg(int logLevel, const char* format, ...);

//Rerourung printf stuff
#ifdef REROUTE_PRINTF
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#define GETCHAR_PROTOTYPE int __io_getchar(void)
/* With GCC, small printf (option LD Linker->Libraries->Small printf
 set to 'Yes') calls __io_putchar() */
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#define GETCHAR_PROTOTYPE int getc(FILE *f)
#endif /* __GNUC__ */
#endif

#endif /* INC_LOG_H_ */
