/*
 * lcd1602.h
 *
 *  Created on: Apr 6, 2024
 *      Author: burni
 */

#ifndef SRC_LCD1602_RGB_H_
#define SRC_LCD1602_RGB_H_

//#include "cmsis_os.h"
#include <stdint.h>
#include <main.h>
#include "log.h"
#include <stdio.h>
#include "stm32f0xx_hal.h"

//use osDelay instead of HAL_delay
//#define LCD_USE_RTOS
//use osDelay instead of HAL_delay for lcd_begin
//#define LCD_USE_RTOS_INIT
/*!
 *  Device I2C Address
 */
#define LCD_ADDRESS     0x7c
#define RGB_ADDRESS     0xc0

/*!
 *  Color define
 */
#define WHITE           0
#define RED             1
#define GREEN           2
#define BLUE            3

#define REG_RED         0x04        // pwm2
#define REG_GREEN       0x03        // pwm1
#define REG_BLUE        0x02        // pwm0

#define REG_MODE1       0x00
#define REG_MODE2       0x01
#define REG_OUTPUT      0x08

/*!
 *  Commands
 */
#define LCD_CLEARDISPLAY    0x01
#define LCD_RETURNHOME      0x02
#define LCD_ENTRYMODESET    0x04
#define LCD_DISPLAYCONTROL  0x08
#define LCD_CURSORSHIFT     0x10
#define LCD_FUNCTIONSET     0x20
#define LCD_SETCGRAMADDR    0x40
#define LCD_SETDDRAMADDR    0x80

/*!
 *  Flags for display entry mode
 */
#define LCD_ENTRYRIGHT              0x00
#define LCD_ENTRYLEFT               0x02
#define LCD_ENTRYSHIFTINCREMENT     0x01
#define LCD_ENTRYSHIFTDECREMENT     0x00

/*!
 *  Flags for display on/off control
 */
#define LCD_DISPLAYON   0x04
#define LCD_DISPLAYOFF  0x00
#define LCD_CURSORON    0x02
#define LCD_CURSOROFF   0x00
#define LCD_BLINKON     0x01
#define LCD_BLINKOFF    0x00

/*!
 *  Flags for display/cursor shift
 */
#define LCD_DISPLAYMOVE     0x08
#define LCD_CURSORMOVE      0x00
#define LCD_MOVERIGHT       0x04
#define LCD_MOVELEFT        0x00

/*!
 *  Flags for function set
 */
#define LCD_8BITMODE        0x10
#define LCD_4BITMODE        0x00
#define LCD_2LINE           0x08
#define LCD_1LINE           0x00
#define LCD_5x8DOTS         0x00

typedef struct {
    I2C_HandleTypeDef* hi2c;
    uint8_t lcd_cols;
    uint8_t lcd_rows;
    uint8_t showfunction;
    uint8_t showcontrol;
    uint8_t showmode;
    uint8_t initialized;
    uint8_t numlines;
    uint8_t currline;
    uint8_t lcdAddr;
    uint8_t RGBAddr;
    uint8_t cols;
    uint8_t rows;
    uint8_t backlightval;
} LCD1602_RGB_HandleTypeDef_t;

HAL_StatusTypeDef lcd1602_init(LCD1602_RGB_HandleTypeDef_t *hlcd, I2C_HandleTypeDef* hi2c,
        uint8_t cols, uint8_t rows);
HAL_StatusTypeDef  lcd1602_home(LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef lcd1602_display(LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef lcd1602_begin(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t cols, uint8_t lines);
HAL_StatusTypeDef lcd1602_command(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t value);
HAL_StatusTypeDef lcd1602_send(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t *data, uint8_t len);
HAL_StatusTypeDef lcd1602_setReg(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t addr, uint8_t data);
HAL_StatusTypeDef lcd1602_setRGB(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t r, uint8_t g, uint8_t b);
HAL_StatusTypeDef lcd1602_setCursor(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t col, uint8_t row);
HAL_StatusTypeDef lcd1602_clear(LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef lcd1602_BlinkLED(LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef lcd1602_noBlinkLED(LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef lcd1602_write_char(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t value);
HAL_StatusTypeDef lcd1602_send_string(LCD1602_RGB_HandleTypeDef_t *hlcd, const char *str);
HAL_StatusTypeDef lcd1602_stopBlink(LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef lcd1602_blink(LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef lcd1602_noCursor(LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef lcd1602_cursor(LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef lcd1602_scrollDisplayLeft(LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef lcd1602_scrollDisplayRight(LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef lcd1602_leftToRight(LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef lcd1602_rightToLeft(LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef lcd1602_noAutoscroll(LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef lcd1602_autoscroll(LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef lcd1602_customSymbol(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t location, uint8_t charmap[]);
HAL_StatusTypeDef lcd1602_setColorWhite(LCD1602_RGB_HandleTypeDef_t *hlcd);



#endif /* __LCD1602_RGB_H__ */



