
#include "lcd1602_rgb.h"




HAL_StatusTypeDef lcd1602_init(LCD1602_RGB_HandleTypeDef_t *hlcd, I2C_HandleTypeDef* hi2c,
        uint8_t cols, uint8_t rows)
{
    hlcd->hi2c = hi2c;
    hlcd->cols = cols;
    hlcd->rows = rows;
    hlcd->showfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
    return lcd1602_begin(hlcd, cols, rows);
}


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
//OK??
HAL_StatusTypeDef lcd1602_send(LCD1602_RGB_HandleTypeDef_t *hlcd,
        uint8_t *data, uint8_t len)
{
    return HAL_I2C_Master_Transmit(hlcd->hi2c, LCD_ADDRESS, data, len, HAL_MAX_DELAY);
}

//OK??
HAL_StatusTypeDef lcd1602_command(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t value)
{
    uint8_t data[3] = {0x80, value};
    return lcd1602_send(hlcd, data, 2);
}


HAL_StatusTypeDef lcd1602_display(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showcontrol |= LCD_DISPLAYON;
    return lcd1602_command(hlcd, LCD_DISPLAYCONTROL | hlcd->showcontrol);
}

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

HAL_StatusTypeDef lcd1602_setReg(LCD1602_RGB_HandleTypeDef_t *hlcd,
        uint8_t addr, uint8_t data)
{
    uint8_t tx_data[2] = {addr, data};
    return HAL_I2C_Master_Transmit(hlcd->hi2c, RGB_ADDRESS, tx_data,
            2, HAL_MAX_DELAY);

}

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

HAL_StatusTypeDef lcd1602_setCursor(LCD1602_RGB_HandleTypeDef_t *hlcd,
        uint8_t col, uint8_t row)
{
    col = (row == 0 ? col|0x80 : col|0xc0);
    uint8_t data[3] = {0x80, col};
    return lcd1602_send(hlcd, data, 2);
}

HAL_StatusTypeDef lcd1602_write_char(LCD1602_RGB_HandleTypeDef_t *hlcd, uint8_t value)
{
    uint8_t data[3] = {0x40, value};
    return lcd1602_send(hlcd, data, 2);
}

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

HAL_StatusTypeDef lcd1602_stopBlink(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showcontrol &= ~LCD_BLINKON;
    return lcd1602_command(hlcd, LCD_DISPLAYCONTROL | hlcd->showcontrol);
}

HAL_StatusTypeDef lcd1602_blink(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showcontrol |= LCD_BLINKON;
    return lcd1602_command(hlcd, LCD_DISPLAYCONTROL | hlcd->showcontrol);
}

HAL_StatusTypeDef lcd1602_noCursor(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showcontrol &= ~LCD_CURSORON;
    return lcd1602_command(hlcd, LCD_DISPLAYCONTROL | hlcd->showcontrol);
}

HAL_StatusTypeDef lcd1602_cursor(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showcontrol |= LCD_CURSORON;
    return lcd1602_command(hlcd, LCD_DISPLAYCONTROL | hlcd->showcontrol);
}

HAL_StatusTypeDef lcd1602_scrollDisplayLeft(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    return lcd1602_command(hlcd, LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}

HAL_StatusTypeDef lcd1602_scrollDisplayRight(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    return lcd1602_command(hlcd, LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

HAL_StatusTypeDef lcd1602_leftToRight(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showmode |= LCD_ENTRYLEFT;
    return lcd1602_command(hlcd, LCD_ENTRYMODESET | hlcd->showmode);
}

HAL_StatusTypeDef lcd1602_rightToLeft(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showmode &= ~LCD_ENTRYLEFT;
    return lcd1602_command(hlcd, LCD_ENTRYMODESET | hlcd->showmode);
}

HAL_StatusTypeDef lcd1602_noAutoscroll(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showmode &= ~LCD_ENTRYSHIFTINCREMENT;
    return lcd1602_command(hlcd, LCD_ENTRYMODESET | hlcd->showmode);
}

HAL_StatusTypeDef lcd1602_autoscroll(LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    hlcd->showmode |= LCD_ENTRYSHIFTINCREMENT;
    return lcd1602_command(hlcd, LCD_ENTRYMODESET | hlcd->showmode);
}


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

HAL_StatusTypeDef lcd1602_setColorWhite(LCD1602_RGB_HandleTypeDef_t *hlcd) {
    return lcd1602_setRGB(hlcd, 255, 255, 255);
}
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


