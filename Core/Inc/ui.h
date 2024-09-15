/*
 * ui.h
 *
 *  Created on: Apr 20, 2024
 *      Author: burni
 */

#ifndef INC_UI_H_
#define INC_UI_H_

#include <stdio.h>
#include <stdint.h>
#include "main.h"
#include "log.h"
#include <stm32f0xx_hal.h>
#include "lcd1602_rgb.h"
#include "arm_math.h"
#include "event.h"

#define UI_ENABLE_LOG

#define UI_LCD_CHAR_SIZE 17

//defines max allowed temperature value
#define MAX_TEMPERATURE 1300
//defines max allowed gradient value both in negative and positive direction
#define MAX_GRADIENT 650
//defines max allowed time setting value
#define MAX_SETTING_TIME 20
//defines max allowed setting value both in negative and positive direction
#define MAX_SETTING 20

//defines steps per event for buttons and encoder in input operations
#define BUTTON_INC 5
#define ENC_INC 20

#define BUTTON_INC_FLOAT_MILLIS 100
#define ENC_INC_FLOAT_MILLIS 1000
//defines range of # of sequences a program is allowed to have
#define MIN_PROGRAM_SEQ_LENGTH 1
#define MAX_PROGRAM_SEQ_LENGTH 10

//defines max amount of programs that are allowed to be stored on MCU
#define MAX_PROGRAMS 10
#define MAX_SETTINGS 8


//different menu points for state machine
typedef enum
{
   NO_MENUPOINT,
   SETTINGS,
   SETTINGS_OVERVIEW,
   SETPOINT,
   PROGRAMS,
   PROGRAMS_OVERVIEW,
   PROGRAM_DETAILED,
   CREATE_PROGRAM,
   CREATE_PROGRAM_DETAILED
}ui_menupoint_t;

//struct for programm
typedef struct
{
    uint8_t length;
    uint16_t gradient[MAX_PROGRAM_SEQ_LENGTH];
    uint16_t gradient_negative[MAX_PROGRAM_SEQ_LENGTH];
    uint16_t temperature[MAX_PROGRAM_SEQ_LENGTH];
}ui_program_t;



//struct that includes all programs in a list like manner
typedef struct
{
    uint8_t length;
    ui_program_t program_list[MAX_PROGRAMS];
    uint8_t cur_index;
}ui_programs_t;

//struct for indivitual Setting
typedef struct
{
    char name[UI_LCD_CHAR_SIZE];
    float32_t value;

}ui_setting_t;


//struct that includes all programs in a list like manner
typedef struct
{
    uint8_t length;
    ui_setting_t setting_list[MAX_SETTINGS];
    uint8_t cur_index;
}ui_settings_t;


//handle for UI
typedef struct
{
    ui_menupoint_t state;
    ui_menupoint_t last_state;
    ui_programs_t programs;
    ui_settings_t settings;

    LCD1602_RGB_HandleTypeDef_t *hlcd;
    Event_Queue_HandleTypeDef_t *queue;

}Ui_HandleTypeDef_t;

void initUI(Ui_HandleTypeDef_t* ui, Event_Queue_HandleTypeDef_t *queue, LCD1602_RGB_HandleTypeDef_t *hlcd);
HAL_StatusTypeDef ui_update(Ui_HandleTypeDef_t *ui);
#endif /* INC_UI_H_ */
