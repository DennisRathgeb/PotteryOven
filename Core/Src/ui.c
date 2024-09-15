/*
 * ui.c
 *
 *  Created on: Apr 20, 2024
 *      Author: Dennis Rathgeb
 */

#include "ui.h"

static ui_program_t p1 = {3,{288,300,150},{0,1,1},{200,80,120}};
static ui_program_t p2 = {5,{80,60,150,300,80},{0,1,0,0,1},{15,80,120,300,600}};
static ui_program_t p3 = {2,{300,150},{0,0},{300,80}};

//default settings
ui_setting_t kp_gradient = {"  KP GRADIENT:  ",8.2};
ui_setting_t ki_gradient = {"  KI GRADIENT:  ",20};
ui_setting_t kd_gradient = {"  KD GRADIENT:  ",42.23};
ui_setting_t interval_gradient = {"  INT GRADIENT:  ",100};

ui_setting_t kp_setpoint = {"  KP SETPOINT:  ",6};
ui_setting_t ki_setpoint = {"  KI SETPOINT:  ",0.2};
ui_setting_t kd_setpoint = {"  KD SETPOINT:  ",-3.2};
ui_setting_t interval_setpoint = {" INT SETPOINT:  ",100};

//holds a temp program for creating new projects or modifying existing ones
static ui_program_t c_program = {1,{0},{0},{0}};
//temporary holder for inputs
static uint16_t temp_counter = 0;
static uint16_t temp_counter_single = 0;
static uint8_t temp_sign = 0;
static uint8_t temp_bool = 0;

//empty char
char empty_text_buf[UI_LCD_CHAR_SIZE] = "                \n";

//scrollview counter to iterate over arrays in a menupoint
static uint8_t scroll_counter = 0;

// Define the custom character pattern for "°/"
uint8_t degree_slash[] = {
    0b01000,
    0b10101,
    0b01010,
    0b00100,
    0b01000,
    0b10100,
    0b00111,
    0b00101
};
// Define the custom character pattern for "°/"
uint8_t degree[] = {
    0b00110,
    0b01001,
    0b01001,
    0b00110,
    0b00000,
    0b00000,
    0b00000,
    0b00000
};

/*
 * init function for ui struct handle
 */
void initUI(Ui_HandleTypeDef_t* ui, Event_Queue_HandleTypeDef_t *queue, LCD1602_RGB_HandleTypeDef_t *hlcd)
{
    ui->hlcd = hlcd;
    ui->queue = queue;
    ui->state = PROGRAMS;
    ui->last_state = NO_MENUPOINT;

    ui->programs.cur_index = 0;
    ui->programs.length = 3;
    ui->programs.program_list[0] = p1;
    ui->programs.program_list[1] = p2;
    ui->programs.program_list[2] = p3;

    ui->settings.cur_index = 0;
    ui->settings.length = 8;
    ui->settings.setting_list[0] = kp_gradient;
    ui->settings.setting_list[1] = ki_gradient;
    ui->settings.setting_list[2] = kd_gradient;
    ui->settings.setting_list[3] = interval_gradient;
    ui->settings.setting_list[4] = kp_setpoint;
    ui->settings.setting_list[5] = ki_setpoint;
    ui->settings.setting_list[6] = kd_setpoint;
    ui->settings.setting_list[7] = interval_setpoint;

    lcd1602_customSymbol(ui->hlcd, 1,degree_slash);
    lcd1602_customSymbol(ui->hlcd, 0,degree);
}

/*
 * displays two rows of chars on the display
 */
void ui_print_lcd(Ui_HandleTypeDef_t *ui, char top[UI_LCD_CHAR_SIZE],char bottom[UI_LCD_CHAR_SIZE])
{
    lcd1602_setCursor(ui->hlcd, 0, 0);
    lcd1602_send_string(ui->hlcd, top);
    lcd1602_setCursor(ui->hlcd, 0, 1);
    lcd1602_send_string(ui->hlcd, bottom);
}
/*
 * prints C/h and C in correct spot on LCD for program detailed view
 */
void ui_print_lcd_program_symb(Ui_HandleTypeDef_t *ui)
{
    lcd1602_setCursor(ui->hlcd, 4, 1);
    lcd1602_write_char(ui->hlcd, 1);

    lcd1602_setCursor(ui->hlcd, 13, 1);
    lcd1602_write_char(ui->hlcd, 0);
}

/*
 * takes two buffers of same length and fills them with program datapoints
 */
static void ui_fill_lcd_buf_program(ui_program_t *program, uint8_t program_index, char text_buf_top[][17],char text_buf_bottom[][17], uint8_t length){
    //update display data
    char gradient_buf[5];
    char sign;


    for (int i = 0; i < length; ++i) {
        //fil digit_buf with
        sign = (program->gradient_negative[i] == 1) ? '-' : ' ';
        uint16_t gradient = program->gradient[i];
        if(100 > gradient)
        {
            snprintf(gradient_buf, 5, " %c%2d",sign, gradient);
        }
        else
        {
            snprintf(gradient_buf, 5, "%c%3d",sign, gradient);
        }
        snprintf(text_buf_top[i], sizeof(text_buf_top[i]), "    P%d: %d/%d    ",program_index+1,i+1,length);
        snprintf(text_buf_bottom[i], sizeof(text_buf_bottom[i]), "%s  -> %4d C",gradient_buf , program->temperature[i] );
    }
}
/*
 * fills program struct with values from events
 * uses local variables scroll_counter,temp_sign temp_counter_single and temp_counter
 */
static void ui_fill_program_values(ui_program_t *program)
{
    temp_sign = (temp_counter == 0 && temp_sign == 1)? 0: temp_sign;
    switch (scroll_counter % 2)
    {
        //temperature element or length
        case 0:
            //limit value to positive only for length and temperature.
            if(temp_sign == 1)
            {
                temp_sign = 0;
                temp_counter = 0;
            }
            //length element
            if (0 == scroll_counter) {
                //length range[1,MAXPROGRAM_SEQ_LENGTH]: limit upper bound
                if(MAX_PROGRAM_SEQ_LENGTH < temp_counter_single)
                {
                    temp_counter_single = MAX_PROGRAM_SEQ_LENGTH;
                }
                //length range[1,MIN_PROGRAM_SEQ_LENGTH]: limit lower bound
                if(MIN_PROGRAM_SEQ_LENGTH > temp_counter_single)
                {
                    temp_counter_single = MIN_PROGRAM_SEQ_LENGTH;
                }

                program->length = temp_counter_single;
                break;
            }
            //temperature element
            //temperature range[0,MAX_TEMPERATURE]: limit upper bound
            if(MAX_TEMPERATURE < temp_counter)
            {
                temp_counter = MAX_TEMPERATURE;
            }
            program->temperature[(scroll_counter-1)/2] = temp_counter;
            break;
        //gradient element
        case 1:
            //gradient range[-MAX_GRADIENT,MAX_GRADIENT]
            if(MAX_GRADIENT < temp_counter)
            {
                temp_counter = MAX_GRADIENT;
            }

            program->gradient[(scroll_counter-1)/2] = temp_counter;
            program->gradient_negative[(scroll_counter-1)/2] = temp_sign;
            break;

        default:
            break;
    }

}

/////////////////////////PROGRAM PAGES/////////////////////////////
/*
static HAL_StatusTypeDef ui_update_(Ui_HandleTypeDef_t *ui,event_type_t event)
{
    switch (event) {
        case NO_EVENT:  // nothing tbd

            break;
        case BUT1:      // navigate left / down

            break;
        case BUT2:      // navigate right / up

            break;
        case BUT3:      // navigate back

            break;
        case BUT4:      // start / stop

            break;
        case ENC_BUT:   // Enter

            break;
        case ENC_UP:    // navigate right / up

            break;
        case ENC_DOWN:  // navigate left / down

            break;
        default:        // unknown event
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }

    lcd1602_setCursor(ui->hlcd, 0, 0);
    lcd1602_send_string(ui->hlcd, "                ");
    lcd1602_setCursor(ui->hlcd, 0, 1);
    lcd1602_send_string(ui->hlcd, "                ");

    return HAL_OK;
}
*/


//////////////////////////7

/*
 * updates cretae_program_detailed menu point in SM, scroll_counter 0: length, 1: gradient, 2:temp,3:grad.... until length2 is reached
 */
static HAL_StatusTypeDef ui_update_create_program_detailed(Ui_HandleTypeDef_t *ui,event_type_t event)
{
    uint8_t index = ui->programs.length;
    uint8_t length = c_program.length;

    switch (event) {
        case NO_EVENT:      // nothing tbd

            break;
        case BUT1:          // navigate left / down
        case ENC_DOWN:
            //negative
            if(0 >= temp_counter || temp_sign == 1)
            {
                temp_sign = 1;
                temp_counter = (event == BUT1)? (temp_counter + BUTTON_INC) : (temp_counter + ENC_INC);
                temp_counter_single++;
            }
            //positive
            else
            {
                if(event == BUT1)
                {
                    temp_counter = (BUTTON_INC > temp_counter)? 0 : (temp_counter - BUTTON_INC);
                }
                else
                {
                    temp_counter = (ENC_INC > temp_counter)? 0 : (temp_counter - ENC_INC);
                }
                temp_counter_single--;
            }
            break;

        case BUT2:          // navigate right / up
        case ENC_UP:
            //positive
            if(0 >= temp_counter || temp_sign == 0)
            {
                temp_sign = 0;
                temp_counter = (event == BUT2)? (temp_counter + BUTTON_INC) : (temp_counter + ENC_INC);
                temp_counter_single++;
            }
            //negative
            else
            {
                if(event == BUT2){
                    temp_counter = (BUTTON_INC > temp_counter)? 0 : (temp_counter - BUTTON_INC);
                }
                else
                {
                    temp_counter = (ENC_INC > temp_counter)? 0 : (temp_counter - ENC_INC);
                }

                temp_counter_single--;
            }
            break;

        case BUT3:          // navigate back
            ui->state = CREATE_PROGRAM;
            //reset temp variables
            c_program.length = 1;
            c_program.gradient[0] = 0;
            c_program.gradient_negative[0] = 0;
            c_program.temperature[0] = 0;
            scroll_counter = 0;
            temp_counter_single = 0;
            break;
        case BUT4:          // start / stop
            //TODO start
            break;
        case ENC_BUT:       // Enter
            //check if length of program is reached
            if((length*2) <= scroll_counter)
            {
                //add program to programs list
                ui->programs.length++;
                ui->programs.program_list[index] = c_program;
                ui->programs.cur_index = index;
                //reset temp variables
                c_program.length = 1;
                c_program.gradient[0] = 0;
                c_program.gradient_negative[0] = 0;
                c_program.temperature[0] = 0;
                scroll_counter = 0;
                temp_counter = 0;
                temp_counter_single = 0;
                //back to program overview
                ui->state = PROGRAMS_OVERVIEW;
                break;
            }
            //increment scroll_counter
            scroll_counter++;
            temp_counter = 0;
            temp_counter_single = 0;

            break;
        default:        // unknown event
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }
    //update program values according according to scroll_counter:
    ui_fill_program_values(&c_program);

    //update display data
    char text_buf_top[length][UI_LCD_CHAR_SIZE];
    char text_buf_bottom[length][UI_LCD_CHAR_SIZE];

    ui_fill_lcd_buf_program(&c_program, index, text_buf_top, text_buf_bottom, length);

    if(scroll_counter == 0)
    {
        char text_buf[20];
        snprintf(text_buf, sizeof(text_buf), "   length: %2d   ",c_program.length);
        ui_print_lcd(ui, text_buf, empty_text_buf);
        return HAL_OK;
    }
    //update display
    ui_print_lcd(ui,text_buf_top[(scroll_counter-1)/2] ,text_buf_bottom[(scroll_counter-1)/2]);
    ui_print_lcd_program_symb(ui);

    return HAL_OK;
}

/*
 * updates cretae_program menu point in SM
 */
static HAL_StatusTypeDef ui_update_create_program(Ui_HandleTypeDef_t *ui,event_type_t event)
{
    switch (event) {
        case NO_EVENT:  // nothing tbd

            break;
        case BUT1:      // navigate left / down
            ui->programs.cur_index = ui->programs.length-1;
            ui->state = PROGRAMS_OVERVIEW;
            break;
        case BUT2:      // navigate right / up
            ui->programs.cur_index = 0;
            ui->state = PROGRAMS_OVERVIEW;
            break;
        case BUT3:      // navigate back
            ui->programs.cur_index = 0;
            ui->state = PROGRAMS;
            break;
        case BUT4:      // start / stop
            //nothing
            break;
        case ENC_BUT:   // Enter
            ui->state = CREATE_PROGRAM_DETAILED;
            break;
        case ENC_UP:    // navigate right / up
            ui->programs.cur_index = 0;
            ui->state = PROGRAMS_OVERVIEW;
            break;
        case ENC_DOWN:  // navigate left / down
            ui->programs.cur_index = ui->programs.length-1;
            ui->state = PROGRAMS_OVERVIEW;
            break;
        default:        // unknown event
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }

    ui_print_lcd(ui, "   CREATE NEW    ", "    PROGRAM     ");


    return HAL_OK;
}

/*
 * updates PROGRAM_OVERVIEW menupoint
 */
static HAL_StatusTypeDef ui_update_program_detailed(Ui_HandleTypeDef_t *ui,event_type_t event)
{
    uint8_t index = ui->programs.cur_index;
    uint8_t length = ui->programs.length;
    ui_program_t program = ui->programs.program_list[index];

    char text_buf_top[length][UI_LCD_CHAR_SIZE];
    char text_buf_bottom[length][UI_LCD_CHAR_SIZE];
    ui_fill_lcd_buf_program(&program, index, text_buf_top, text_buf_bottom, program.length);

    switch (event) {
        case NO_EVENT:  // nothing tbd

            break;
        case BUT1:      // navigate left / down
            if(0 >= scroll_counter) scroll_counter = length-1;
            else scroll_counter--;
            break;
        case BUT2:      // navigate right / up
            if(length-1 <= scroll_counter) scroll_counter = 0;
            else scroll_counter++;
            break;
        case BUT3:      // navigate back
            ui->state = PROGRAMS_OVERVIEW;
            scroll_counter = 0;
            break;
        case BUT4:      // start / stop

            break;
        case ENC_BUT:   // Enter
            ui->state = PROGRAMS_OVERVIEW;
            scroll_counter = 0;
            break;
        case ENC_UP:    // navigate right / up
            if(program.length-1 <= scroll_counter) scroll_counter = 0;
            else scroll_counter++;
            break;
        case ENC_DOWN:  // navigate left / down
            if(0 >= scroll_counter) scroll_counter = program.length-1;
            else scroll_counter--;
            break;
        default:        // unknown event
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }


    ui_print_lcd(ui, text_buf_top[scroll_counter], text_buf_bottom[scroll_counter]);
    ui_print_lcd_program_symb(ui);


    return HAL_OK;
}

/*
 * updates programs_overview menu point in SM
 */
static HAL_StatusTypeDef ui_update_programs_overview(Ui_HandleTypeDef_t *ui,event_type_t event)
{
    switch (event) {
        case NO_EVENT:  // nothing tbd

            break;
        case BUT1:      // navigate left / down
            ui->programs.cur_index--;
            if(ui->programs.cur_index >= ui->programs.length)
            {
                ui->state = CREATE_PROGRAM;
            }
            break;
        case BUT2:      // navigate right / up
            ui->programs.cur_index++;
            if(ui->programs.cur_index >= ui->programs.length)
            {
                ui->state = CREATE_PROGRAM;
            }
            break;
        case BUT3:      // navigate back
            ui->programs.cur_index = 0;
            ui->state = PROGRAMS;
            break;
        case BUT4:      // start / stop
            //nothing
            break;
        case ENC_BUT:   // Enter
            ui->state = PROGRAM_DETAILED;
            break;
        case ENC_UP:    // navigate right / up
            ui->programs.cur_index++;
            if(ui->programs.cur_index >= ui->programs.length)
            {
                ui->state = CREATE_PROGRAM;
            }
            break;
        case ENC_DOWN:  // navigate left / down
            ui->programs.cur_index--;
            if(ui->programs.cur_index >= ui->programs.length)
            {
                ui->programs.cur_index = 0;
                ui->state = CREATE_PROGRAM;
            }
            break;
        default:        // unknown event
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }


    uint8_t index = ui->programs.cur_index;
    if(index == 0)
    {
        ui_print_lcd(ui,"  SCHRUEBRAND    " , empty_text_buf);
    }
    else if(index == 1)
    {
        ui_print_lcd(ui,"  GLASURBRAND    " , empty_text_buf);
    }
    else
    {
        char buf[20];
        snprintf(buf, sizeof(buf), "   PROGRAM %u    ", index+1);
        ui_print_lcd(ui, buf, empty_text_buf);

    }





    return HAL_OK;
}

/*
 * updates settings_overview menu point in SM
 */
static HAL_StatusTypeDef ui_update_settings_overview(Ui_HandleTypeDef_t *ui,event_type_t event)
{
    uint8_t index;

    switch (event) {
        case NO_EVENT:  // nothing tbd

            break;
        case BUT1:      // navigate left / down
        case ENC_DOWN:
            //setting value
            if(temp_bool)
            {
                index = ui->settings.cur_index;
                float32_t val = ui->settings.setting_list[index].value;
                float32_t inc_millis = (event==BUT1)? BUTTON_INC_FLOAT_MILLIS : ENC_INC_FLOAT_MILLIS;
                val -= (inc_millis / 1000);
                if (val > MAX_SETTING) {
                    val = MAX_SETTING;
                } else if (val < -MAX_SETTING) {
                    val = -MAX_SETTING;
                }
                ui->settings.setting_list[index].value = val;
                break;
            }
            ui->settings.cur_index--;
            if(ui->settings.cur_index >= ui->settings.length)
            {
                ui->settings.cur_index = ui->settings.length-1;
            }
            break;
        case BUT2:      // navigate right / up
        case ENC_UP:
            //setting value
            if(temp_bool)
            {
                index= ui->settings.cur_index;
                float32_t val = ui->settings.setting_list[index].value;
                float32_t inc_millis = (event==BUT2)? BUTTON_INC_FLOAT_MILLIS : ENC_INC_FLOAT_MILLIS;
                val += (inc_millis / 1000);
                if (val > MAX_SETTING) {
                    val = MAX_SETTING;
                } else if (val < -MAX_SETTING) {
                    val = -MAX_SETTING;
                }
                ui->settings.setting_list[index].value = val;
                break;
            }
            //scrolling
            ui->settings.cur_index++;
            if(ui->settings.cur_index >= ui->settings.length)
            {
                ui->settings.cur_index = 0;
            }
            break;
        case BUT3:      // navigate back
            ui->settings.cur_index = 0;
            ui->state = SETTINGS;
            break;
        case BUT4:      // start / stop
            //nothing
            break;
        case ENC_BUT:   // Enter
            temp_bool = (temp_bool)? 0 : 1;
            break;

        default:        // unknown event
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }
    index = ui->settings.cur_index;
    ui_setting_t setting = ui->settings.setting_list[index];
    char buf[UI_LCD_CHAR_SIZE];
    snprintf(buf, sizeof(buf), "     %.1f        ",setting.value);

    ui_print_lcd(ui, setting.name, buf);






    return HAL_OK;
}

/*
 * updates programs menu point in SM
 */
static HAL_StatusTypeDef ui_update_programs(Ui_HandleTypeDef_t *ui,event_type_t event)
{
    switch (event) {
        case NO_EVENT:  // nothing tbd

            break;
        case BUT1:      // navigate left / down
            ui->state = SETTINGS;
            break;
        case BUT2:      // navigate right / up
            ui->state = SETPOINT;
            break;
        case BUT3:      // navigate back
            //nothing
            break;
        case BUT4:      // start / stop
            //nothing
            break;
        case ENC_BUT:   // Enter
            ui->state = PROGRAMS_OVERVIEW;
            break;
        case ENC_UP:    // navigate right / up
            ui->state = SETPOINT;
            break;
        case ENC_DOWN:  // navigate left / down
            ui->state = SETTINGS;
            break;
        default:        // unknown event
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }

    ui_print_lcd(ui,"    PROGRAMS    " , empty_text_buf);


    return HAL_OK;
}

/*
 * updates setpoint menu point in SM
 */
static HAL_StatusTypeDef ui_update_setpoint(Ui_HandleTypeDef_t *ui,event_type_t event)
{
    switch (event) {
        case NO_EVENT:  // nothing tbd

            break;
        case BUT1:      // navigate left / down
            ui->state = PROGRAMS;
            break;
        case BUT2:      // navigate right / up
            ui->state = SETTINGS;
            break;
        case BUT3:      // navigate back

            break;
        case BUT4:      // start / stop

            break;
        case ENC_BUT:   // Enter
            //TODO choose
            break;
        case ENC_UP:    // navigate right / up
            ui->state = SETTINGS;
            break;
        case ENC_DOWN:  // navigate left / down
            ui->state = PROGRAMS;
            break;
        default:        // unknown event
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }

    ui_print_lcd(ui,"    SETPOINT    " , empty_text_buf);

    return HAL_OK;
}

/*
 * updates settings menu point in SM
 */
static HAL_StatusTypeDef ui_update_settings(Ui_HandleTypeDef_t *ui,event_type_t event)
{
    switch (event) {
        case NO_EVENT:  // nothing tbd

            break;
        case BUT1:      // navigate left / down
            ui->state = SETPOINT;
            break;
        case BUT2:      // navigate right / up
            ui->state = PROGRAMS;
            break;
        case BUT3:      // navigate back

            break;
        case BUT4:      // start / stop

            break;
        case ENC_BUT:   // Enter
            ui->state = SETTINGS_OVERVIEW;
            break;
        case ENC_UP:    // navigate right / up
            ui->state = PROGRAMS;
            break;
        case ENC_DOWN:  // navigate left / down
            ui->state = SETPOINT;
            break;
        default:        // unknown event
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }

    ui_print_lcd(ui,"    SETTINGS    " , empty_text_buf);


    return HAL_OK;
}

//////////////EVENTS AND MAIN STATEMACHINE FOR MENU////////////////////
/*
 * checks flags if events occured returns NO_EVENT if no event occured
 */
event_type_t ui_get_events(Ui_HandleTypeDef_t *ui)
{
    if (!event_isEmpty(ui->queue))
    {
          return event_dequeue(ui->queue);
    }

    return NO_EVENT;
}
/*
 * update function for UI Statemachine
 */
HAL_StatusTypeDef ui_update(Ui_HandleTypeDef_t *ui)
{
    event_type_t cur_event = ui_get_events(ui);

    //no change
    if(ui->last_state == ui->state && cur_event == NO_EVENT)
    {
        return HAL_OK;
    }
    ui->last_state = ui->state;

    switch (ui->state) {
        case SETTINGS:
            return ui_update_settings(ui,cur_event);
            break;

        case SETTINGS_OVERVIEW:
            return ui_update_settings_overview(ui,cur_event);
            break;

        case SETPOINT:
            return ui_update_setpoint(ui,cur_event);
            break;

        case PROGRAMS:
            return ui_update_programs(ui,cur_event);
            break;

        case PROGRAMS_OVERVIEW:
            return ui_update_programs_overview(ui, cur_event);
            break;

        case PROGRAM_DETAILED:
            return ui_update_program_detailed(ui, cur_event);
            break;

        case CREATE_PROGRAM:
            return ui_update_create_program(ui, cur_event);
            break;

        case CREATE_PROGRAM_DETAILED:
            return ui_update_create_program_detailed(ui, cur_event);
            break;

        default:
            return HAL_OK;
            break;
    }
    return HAL_OK;
}

