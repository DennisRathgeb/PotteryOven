/**
 * @file lcd1602_rgb.c
 * @brief Implementation of LCD1602 RGB display driver
 * @author Dennis Rathgeb
 * @date Apr 6, 2024
 *
 * Implements I2C communication with Grove LCD1602 RGB backlight display.
 * Handles both the LCD controller and RGB backlight controller.
 */

#include "lcd1602_rgb.h"

/**
 * @brief Initialize the LCD display
 * @param[out] hlcd Pointer to LCD handle to initialize
 * @param[in] hi2c Pointer to I2C handle for communication
 * @param[in] cols Number of columns (typically 16)
 * @param[in] rows Number of rows (typically 2)
 * @return HAL_OK on success, error code on failure
 *
 * Stores configuration and calls lcd1602_begin for hardware initialization.
 */
HAL_StatusTypeDef lcd1602_init(LCD1602_RGB_HandleTypeDef_t *hlcd, I2C_HandleTypeDef* hi2c,
        uint8_t cols, uint8_t rows)
{
    hlcd->hi2c = hi2c;
    hlcd->cols = cols;
    hlcd->rows = rows;
    hlcd->showfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
    return lcd1602_begin(hlcd, cols, rows);
}

/**
 * @brief Begin display operation with full initialization sequence
 * @param[in,out] hlcd Pointer to LCD handle
 * @param[in] cols Number of columns
 * @param[in] lines Number of lines
 * @return HAL_OK on success, accumulated error code on failure
 *
 * Performs the complete LCD initialization sequence:
 * 1. Wait for power stabilization (50ms)
 * 2. Send function set command 3 times (per HD44780 spec)
 * 3. Configure display control, clear display, set entry mode
 * 4. Initialize RGB backlight to white
 */
HAL_StatusTypeDef lcd1602_begin(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t cols, uint8_t lines)
{
    uint8_t err_c = HAL_OK;
    if (lines > 1) {
        hlcd->showfunction |= LCD_2LINE;
    }
    hlcd->numlines = lines;
    hlcd->currline = 0;

#ifdef LCD_USE_RTOS_INIT
    osDelay(50);
#else
    HAL_Delay(50);
#endif

    err_c += lcd1602_command(hlcd, LCD_FUNCTIONSET | hlcd->showfunction);

#ifdef LCD_USE_RTOS_INIT
    osDelay(5);
#else
    HAL_Delay(5);
#endif

    err_c += lcd1602_command(hlcd, LCD_FUNCTIONSET | hlcd->showfunction);
#ifdef LCD_USE_RTOS_INIT
    osDelay(5);
#else
    HAL_Delay(5);
#endif

    err_c += lcd1602_command(hlcd, LCD_FUNCTIONSET | hlcd->showfunction);

    hlcd->showcontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    err_c += lcd1602_display(hlcd);

    err_c += lcd1602_clear(hlcd);

    hlcd->showmode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    err_c += lcd1602_command(hlcd, LCD_ENTRYMODESET | hlcd->showmode);

    err_c += lcd1602_setReg(hlcd, REG_MODE1, 0);
    err_c += lcd1602_setReg(hlcd, REG_OUTPUT, 0xFF);
    err_c += lcd1602_setReg(hlcd, REG_MODE2, 0x20);

    err_c += lcd1602_setColorWhite(hlcd);
    if(err_c)
    {
        return err_c;
    }
    return HAL_OK;
}

/**
 * @brief Send raw data bytes to the LCD via I2C
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] data Pointer to data buffer
 * @param[in] len Number of bytes to send
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_send(LCD1602_RGB_HandleTypeDef_t *hlcd,
        uint8_t *data, uint8_t len)
{
    return HAL_I2C_Master_Transmit(hlcd->hi2c, LCD_ADDRESS, data, len, HAL_MAX_DELAY);
}

/**
 * @brief Send a command byte to the LCD
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] value Command byte to send
 * @return HAL_OK on success
 *
 * Commands are sent with 0x80 prefix to indicate command mode.
 */
HAL_StatusTypeDef lcd1602_command(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t value)
{
    uint8_t data[3] = {0x80, value};
    return lcd1602_send(hlcd, data, 2);
}

/**
 * @brief Turn on the display
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 *
 * Sets the display on flag and sends the display control command.
 */
HAL_StatusTypeDef lcd1602_display(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showcontrol |= LCD_DISPLAYON;
    return lcd1602_command(hlcd, LCD_DISPLAYCONTROL | hlcd->showcontrol);
}

/**
 * @brief Clear the display and return cursor to home
 * @param[in] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 *
 * Clears all display content. Requires 200ms delay for execution.
 */
HAL_StatusTypeDef lcd1602_clear(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    uint8_t err;
    err = lcd1602_command(hlcd, LCD_CLEARDISPLAY);
#ifdef LCD_USE_RTOS
    osDelay(200);
#else
    HAL_Delay(200);
#endif
    return err;
}

/**
 * @brief Write a value to an RGB controller register
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] addr Register address
 * @param[in] data Value to write
 * @return HAL_OK on success
 *
 * Communicates with the RGB backlight controller at RGB_ADDRESS.
 */
HAL_StatusTypeDef lcd1602_setReg(LCD1602_RGB_HandleTypeDef_t *hlcd,
        uint8_t addr, uint8_t data)
{
    uint8_t tx_data[2] = {addr, data};
    return HAL_I2C_Master_Transmit(hlcd->hi2c, RGB_ADDRESS, tx_data,
            2, HAL_MAX_DELAY);

}

/**
 * @brief Set RGB backlight color
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] r Red intensity (0-255)
 * @param[in] g Green intensity (0-255)
 * @param[in] b Blue intensity (0-255)
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_setRGB(LCD1602_RGB_HandleTypeDef_t *hlcd,
        uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t err_c = 0;
    err_c += lcd1602_setReg(hlcd, REG_RED, r);
    err_c += lcd1602_setReg(hlcd, REG_GREEN, g);
    err_c += lcd1602_setReg(hlcd, REG_BLUE, b);
    if(err_c){
        return err_c;
    }
    return HAL_OK;
}

/**
 * @brief Set cursor position
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] col Column (0 to cols-1)
 * @param[in] row Row (0 or 1)
 * @return HAL_OK on success
 *
 * Row 0 starts at DDRAM address 0x00, row 1 starts at 0x40.
 */
HAL_StatusTypeDef lcd1602_setCursor(LCD1602_RGB_HandleTypeDef_t *hlcd,
        uint8_t col, uint8_t row)
{
    col = (row == 0 ? col|0x80 : col|0xc0);
    uint8_t data[3] = {0x80, col};
    return lcd1602_send(hlcd, data, 2);
}

/**
 * @brief Write a single character to the display
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] value ASCII character or custom character index (0-7)
 * @return HAL_OK on success
 *
 * Characters are sent with 0x40 prefix to indicate data mode.
 */
HAL_StatusTypeDef lcd1602_write_char(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t value)
{
    uint8_t data[3] = {0x40, value};
    return lcd1602_send(hlcd, data, 2);
}

/**
 * @brief Write a null-terminated string to the display
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] str Null-terminated string to write
 * @return HAL_OK on success
 *
 * Writes each character in sequence until null terminator.
 */
HAL_StatusTypeDef lcd1602_send_string(LCD1602_RGB_HandleTypeDef_t *hlcd, const char *str)
{
    uint8_t err_c = 0;
    uint8_t i;
    for(i = 0; str[i] != '\0'; i++)
    {
        err_c += lcd1602_write_char(hlcd, str[i]);
    }
    if(err_c)
    {
        return err_c;
    }
    return HAL_OK;

}

/**
 * @brief Enable RGB LED blinking
 * @param[in] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 *
 * Configures the RGB controller for blinking mode.
 */
HAL_StatusTypeDef lcd1602_BlinkLED(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    uint8_t err_c = 0;
    err_c += lcd1602_setReg(hlcd, 0x07, 0x17);
    err_c += lcd1602_setReg(hlcd, 0x06, 0x7f);
    if(err_c)
        {
            return err_c;
        }
        return HAL_OK;
}

/**
 * @brief Disable RGB LED blinking
 * @param[in] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 *
 * Returns RGB controller to steady-on mode.
 */
HAL_StatusTypeDef lcd1602_noBlinkLED(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    uint8_t err_c = 0;
    err_c += lcd1602_setReg(hlcd, 0x07, 0x00);
    err_c += lcd1602_setReg(hlcd, 0x06, 0xff);
    if(err_c)
    {
        return err_c;
    }
    return HAL_OK;
}

/**
 * @brief Stop cursor blinking (cursor still visible)
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_stopBlink(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showcontrol &= ~LCD_BLINKON;
    return lcd1602_command(hlcd, LCD_DISPLAYCONTROL | hlcd->showcontrol);
}

/**
 * @brief Enable cursor blinking
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_blink(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showcontrol |= LCD_BLINKON;
    return lcd1602_command(hlcd, LCD_DISPLAYCONTROL | hlcd->showcontrol);
}

/**
 * @brief Hide the cursor
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_noCursor(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showcontrol &= ~LCD_CURSORON;
    return lcd1602_command(hlcd, LCD_DISPLAYCONTROL | hlcd->showcontrol);
}

/**
 * @brief Show the cursor (underscore)
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_cursor(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showcontrol |= LCD_CURSORON;
    return lcd1602_command(hlcd, LCD_DISPLAYCONTROL | hlcd->showcontrol);
}

/**
 * @brief Scroll display content left by one position
 * @param[in] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_scrollDisplayLeft(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    return lcd1602_command(hlcd, LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}

/**
 * @brief Scroll display content right by one position
 * @param[in] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_scrollDisplayRight(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    return lcd1602_command(hlcd, LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

/**
 * @brief Set text direction to left-to-right
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 *
 * Characters are written left to right, cursor moves right.
 */
HAL_StatusTypeDef lcd1602_leftToRight(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showmode |= LCD_ENTRYLEFT;
    return lcd1602_command(hlcd, LCD_ENTRYMODESET | hlcd->showmode);
}

/**
 * @brief Set text direction to right-to-left
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 *
 * Characters are written right to left, cursor moves left.
 */
HAL_StatusTypeDef lcd1602_rightToLeft(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showmode &= ~LCD_ENTRYLEFT;
    return lcd1602_command(hlcd, LCD_ENTRYMODESET | hlcd->showmode);
}

/**
 * @brief Disable autoscroll mode
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 *
 * Display does not shift when characters are written.
 */
HAL_StatusTypeDef lcd1602_noAutoscroll(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showmode &= ~LCD_ENTRYSHIFTINCREMENT;
    return lcd1602_command(hlcd, LCD_ENTRYMODESET | hlcd->showmode);
}

/**
 * @brief Enable autoscroll mode
 * @param[in,out] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 *
 * Display shifts when characters are written to keep cursor visible.
 */
HAL_StatusTypeDef lcd1602_autoscroll(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showmode |= LCD_ENTRYSHIFTINCREMENT;
    return lcd1602_command(hlcd, LCD_ENTRYMODESET | hlcd->showmode);
}

/**
 * @brief Create a custom character in CGRAM
 * @param[in] hlcd Pointer to LCD handle
 * @param[in] location CGRAM location (0-7, masked to 3 bits)
 * @param[in] charmap Array of 8 bytes defining character pattern (5 bits per row)
 * @return HAL_OK on success
 *
 * Custom characters are accessed by writing their location (0-7) to the display.
 * Each byte in charmap defines one row of the 5x8 character pattern.
 */
HAL_StatusTypeDef lcd1602_customSymbol(LCD1602_RGB_HandleTypeDef_t *hlcd,
        uint8_t location, uint8_t charmap[])
{
    uint8_t err_c = HAL_OK;
    location &= 0x7;
    err_c += lcd1602_command(hlcd, LCD_SETCGRAMADDR | (location << 3));

    uint8_t data[9];
    data[0] = 0x40;
    for(int i=0; i<8; i++)
        data[i+1] = charmap[i];

    err_c += lcd1602_send(hlcd, data, 9);
    if (err_c)
    {
        return err_c;
    }
    return HAL_OK;
}

/**
 * @brief Set backlight to white (full brightness all colors)
 * @param[in] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef lcd1602_setColorWhite(LCD1602_RGB_HandleTypeDef_t *hlcd) {
    return lcd1602_setRGB(hlcd, 255, 255, 255);
}

/**
 * @brief Return cursor to home position (0,0)
 * @param[in] hlcd Pointer to LCD handle
 * @return HAL_OK on success
 *
 * Moves cursor to upper-left corner. Requires 200ms delay for execution.
 */
HAL_StatusTypeDef lcd1602_home(LCD1602_RGB_HandleTypeDef_t *hlcd) {

    HAL_StatusTypeDef err = HAL_OK;
    uint8_t cmd = {LCD_RETURNHOME};

    err = lcd1602_command(hlcd, cmd);
#ifdef LCD_USE_RTOS
    osDelay(200);
#else
    HAL_Delay(200);
#endif

    if (err != HAL_OK) {
        return err;
    }

    return HAL_OK;
}
