/**
 * @file lcd1602_rgb.h
 * @brief LCD1602 RGB display driver with I2C interface
 * @author Dennis Rathgeb
 * @date Apr 6, 2024
 *
 * Driver for the Grove LCD1602 RGB backlight display.
 * The display has two I2C addresses:
 * - LCD controller at 0x7C
 * - RGB backlight controller at 0xC0
 *
 * Supports 4-bit mode operation with full control over display,
 * cursor, scrolling, and RGB backlight color.
 */

#ifndef SRC_LCD1602_RGB_H_
#define SRC_LCD1602_RGB_H_

#include <stdint.h>
#include <main.h>
#include "log.h"
#include <stdio.h>
#include "stm32f0xx_hal.h"

/** @brief Use FreeRTOS osDelay instead of HAL_Delay */
//#define LCD_USE_RTOS

/** @brief Use FreeRTOS osDelay for initialization only */
//#define LCD_USE_RTOS_INIT

/**
 * @defgroup LCD_I2C_ADDR I2C Device Addresses
 * @{
 */
#define LCD_ADDRESS     0x7c    /**< I2C address for LCD controller */
#define RGB_ADDRESS     0xc0    /**< I2C address for RGB backlight controller */
/** @} */

/**
 * @defgroup LCD_COLORS Color Definitions
 * @{
 */
#define WHITE           0   /**< White backlight */
#define RED             1   /**< Red backlight */
#define GREEN           2   /**< Green backlight */
#define BLUE            3   /**< Blue backlight */
/** @} */

/**
 * @defgroup RGB_REGISTERS RGB Controller Register Addresses
 * @{
 */
#define REG_RED         0x04    /**< Red PWM register (pwm2) */
#define REG_GREEN       0x03    /**< Green PWM register (pwm1) */
#define REG_BLUE        0x02    /**< Blue PWM register (pwm0) */
#define REG_MODE1       0x00    /**< Mode register 1 */
#define REG_MODE2       0x01    /**< Mode register 2 */
#define REG_OUTPUT      0x08    /**< Output state register */
/** @} */

/**
 * @defgroup LCD_COMMANDS LCD Command Bytes
 * @{
 */
#define LCD_CLEARDISPLAY    0x01    /**< Clear display and return cursor home */
#define LCD_RETURNHOME      0x02    /**< Return cursor to home position */
#define LCD_ENTRYMODESET    0x04    /**< Set entry mode */
#define LCD_DISPLAYCONTROL  0x08    /**< Display on/off control */
#define LCD_CURSORSHIFT     0x10    /**< Cursor/display shift */
#define LCD_FUNCTIONSET     0x20    /**< Function set */
#define LCD_SETCGRAMADDR    0x40    /**< Set CGRAM address */
#define LCD_SETDDRAMADDR    0x80    /**< Set DDRAM address */
/** @} */

/**
 * @defgroup LCD_ENTRY_FLAGS Entry Mode Flags
 * @{
 */
#define LCD_ENTRYRIGHT              0x00    /**< Decrement cursor position */
#define LCD_ENTRYLEFT               0x02    /**< Increment cursor position */
#define LCD_ENTRYSHIFTINCREMENT     0x01    /**< Shift display on write */
#define LCD_ENTRYSHIFTDECREMENT     0x00    /**< Don't shift display */
/** @} */

/**
 * @defgroup LCD_DISPLAY_FLAGS Display Control Flags
 * @{
 */
#define LCD_DISPLAYON   0x04    /**< Display on */
#define LCD_DISPLAYOFF  0x00    /**< Display off */
#define LCD_CURSORON    0x02    /**< Cursor visible */
#define LCD_CURSOROFF   0x00    /**< Cursor hidden */
#define LCD_BLINKON     0x01    /**< Cursor blink on */
#define LCD_BLINKOFF    0x00    /**< Cursor blink off */
/** @} */

/**
 * @defgroup LCD_SHIFT_FLAGS Cursor/Display Shift Flags
 * @{
 */
#define LCD_DISPLAYMOVE     0x08    /**< Shift entire display */
#define LCD_CURSORMOVE      0x00    /**< Move cursor only */
#define LCD_MOVERIGHT       0x04    /**< Shift/move right */
#define LCD_MOVELEFT        0x00    /**< Shift/move left */
/** @} */

/**
 * @defgroup LCD_FUNCTION_FLAGS Function Set Flags
 * @{
 */
#define LCD_8BITMODE        0x10    /**< 8-bit data interface */
#define LCD_4BITMODE        0x00    /**< 4-bit data interface */
#define LCD_2LINE           0x08    /**< 2-line display */
#define LCD_1LINE           0x00    /**< 1-line display */
#define LCD_5x8DOTS         0x00    /**< 5x8 dot character font */
/** @} */

/**
 * @brief Handle structure for LCD1602 RGB display
 *
 * Contains I2C handle and display state information.
 */
typedef struct {
    I2C_HandleTypeDef* hi2c;    /**< Pointer to I2C handle for communication */
    uint8_t lcd_cols;           /**< Number of columns (usually 16) */
    uint8_t lcd_rows;           /**< Number of rows (usually 2) */
    uint8_t showfunction;       /**< Current function set flags */
    uint8_t showcontrol;        /**< Current display control flags */
    uint8_t showmode;           /**< Current entry mode flags */
    uint8_t initialized;        /**< Initialization complete flag */
    uint8_t numlines;           /**< Number of display lines */
    uint8_t currline;           /**< Current cursor line */
    uint8_t lcdAddr;            /**< LCD I2C address (unused, uses define) */
    uint8_t RGBAddr;            /**< RGB I2C address (unused, uses define) */
    uint8_t cols;               /**< Column count */
    uint8_t rows;               /**< Row count */
    uint8_t backlightval;       /**< Backlight value (unused) */
} LCD1602_RGB_HandleTypeDef_t;

/**
 * @brief Initialize the LCD display
 * @param[out] hlcd Pointer to LCD handle to initialize
 * @param[in] hi2c Pointer to I2C handle for communication
 * @param[in] cols Number of columns (typically 16)
 * @param[in] rows Number of rows (typically 2)
 * @return HAL_OK on success, error code on failure
 */
HAL_StatusTypeDef lcd1602_init(LCD1602_RGB_HandleTypeDef_t *hlcd, I2C_HandleTypeDef* hi2c,
        uint8_t cols, uint8_t rows);

/**
 * @brief Return cursor to home position (0,0)
 * @param[in] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_home(LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Turn on the display
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_display(LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Begin display operation (called by init)
 * @param[in,out] hlcd Pointer to LCD handle
 * @param[in] cols Number of columns
 * @param[in] lines Number of lines
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_begin(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t cols, uint8_t lines);

/**
 * @brief Send a command byte to the LCD
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] value Command byte to send
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_command(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t value);

/**
 * @brief Send raw data bytes to the LCD
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] data Pointer to data buffer
 * @param[in] len Number of bytes to send
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_send(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t *data, uint8_t len);

/**
 * @brief Write a value to an RGB controller register
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] addr Register address
 * @param[in] data Value to write
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_setReg(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t addr, uint8_t data);

/**
 * @brief Set RGB backlight color
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] r Red intensity (0-255)
 * @param[in] g Green intensity (0-255)
 * @param[in] b Blue intensity (0-255)
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_setRGB(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Set cursor position
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] col Column (0 to cols-1)
 * @param[in] row Row (0 or 1)
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_setCursor(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t col, uint8_t row);

/**
 * @brief Clear the display
 * @param[in] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_clear(LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Enable LED blinking
 * @param[in] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_BlinkLED(LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Disable LED blinking
 * @param[in] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_noBlinkLED(LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Write a single character to the display
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] value ASCII character to write
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_write_char(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t value);

/**
 * @brief Write a null-terminated string to the display
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] str Null-terminated string to write
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_send_string(LCD1602_RGB_HandleTypeDef_t *hlcd, const char *str);

/**
 * @brief Stop cursor blinking
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_stopBlink(LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Enable cursor blinking
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_blink(LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Hide the cursor
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_noCursor(LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Show the cursor
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_cursor(LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Scroll display content left
 * @param[in] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_scrollDisplayLeft(LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Scroll display content right
 * @param[in] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_scrollDisplayRight(LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Set text direction left-to-right
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_leftToRight(LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Set text direction right-to-left
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_rightToLeft(LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Disable autoscroll mode
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_noAutoscroll(LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Enable autoscroll mode
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_autoscroll(LCD1602_RGB_HandleTypeDef_t *hlcd);

/**
 * @brief Create a custom character in CGRAM
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] location CGRAM location (0-7)
 * @param[in] charmap Array of 8 bytes defining character pattern
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_customSymbol(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t location, uint8_t charmap[]);

/**
 * @brief Set backlight to white (255, 255, 255)
 * @param[in] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_setColorWhite(LCD1602_RGB_HandleTypeDef_t *hlcd);

#endif /* __LCD1602_RGB_H__ */
