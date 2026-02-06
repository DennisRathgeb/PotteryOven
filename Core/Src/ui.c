/**
 * @file ui.c
 * @brief Implementation of menu-driven user interface
 * @author Dennis Rathgeb
 * @date Apr 20, 2024
 *
 * Implements a state machine-based menu system for kiln control.
 * Handles navigation, program creation, and settings adjustment
 * via LCD display and rotary encoder input.
 */

#include "ui.h"
#include "heater.h"

/* External references to global handles defined in main.c */
extern Heater_HandleTypeDef_t hheater;
extern GradientController_HandleTypeDef_t hgc;

/* Default programs for initialization */
static ui_program_t p1 = {3, {288, 300, 150}, {0, 1, 1}, {200, 80, 120}};
static ui_program_t p2 = {5, {80, 60, 150, 300, 80}, {0, 1, 0, 0, 1}, {15, 80, 120, 300, 600}};
static ui_program_t p3 = {2, {300, 150}, {0, 0}, {300, 80}};

/* Default gradient controller settings */
ui_setting_t setting_gain   = {"     GAIN:      ", 100.0};  /**< Kc (proportional gain) */
ui_setting_t setting_filter = {"    FILTER:     ", 0.8};    /**< alpha (EMA coefficient) */
ui_setting_t setting_ti     = {"  INT TIME(s):  ", 60.0};   /**< Ti (integral time in seconds) */

/** @brief Temporary program for creating/modifying programs */
static ui_program_t c_program = {1, {0}, {0}, {0}};

/** @brief Temporary counter for input values */
static uint16_t temp_counter = 0;

/** @brief Temporary counter for single-digit inputs */
static uint16_t temp_counter_single = 0;

/** @brief Temporary sign flag: 0=positive, 1=negative */
static uint8_t temp_sign = 0;

/** @brief Temporary boolean for edit mode toggle */
static uint8_t temp_bool = 0;

/** @brief Empty text buffer for clearing LCD line */
char empty_text_buf[UI_LCD_CHAR_SIZE] = "                \n";

/** @brief Scroll counter for navigating arrays in menus */
static uint8_t scroll_counter = 0;

/** @brief Custom character pattern for degree/slash symbol */
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

/** @brief Custom character pattern for degree symbol */
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

/**
 * @brief Initialize the UI module
 * @param[out] ui Pointer to UI handle to initialize
 * @param[in] queue Pointer to event queue for input events
 * @param[in] hlcd Pointer to LCD handle for display output
 *
 * Sets up initial state, loads default programs and settings,
 * and creates custom LCD characters for temperature display.
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
    ui->settings.length = 3;
    ui->settings.setting_list[0] = setting_gain;
    ui->settings.setting_list[1] = setting_filter;
    ui->settings.setting_list[2] = setting_ti;

    lcd1602_customSymbol(ui->hlcd, 1, degree_slash);
    lcd1602_customSymbol(ui->hlcd, 0, degree);
}

/**
 * @brief Display two rows of text on the LCD
 * @param[in] ui Pointer to UI handle
 * @param[in] top Text for top row (16 chars max)
 * @param[in] bottom Text for bottom row (16 chars max)
 */
void ui_print_lcd(Ui_HandleTypeDef_t *ui, char top[UI_LCD_CHAR_SIZE], char bottom[UI_LCD_CHAR_SIZE])
{
    lcd1602_setCursor(ui->hlcd, 0, 0);
    lcd1602_send_string(ui->hlcd, top);
    lcd1602_setCursor(ui->hlcd, 0, 1);
    lcd1602_send_string(ui->hlcd, bottom);
}

/**
 * @brief Print custom degree symbols on LCD for program view
 * @param[in] ui Pointer to UI handle
 *
 * Displays degrees C/h symbol at position 4 and degrees C at position 13
 * on the bottom row for the program detailed view.
 */
void ui_print_lcd_program_symb(Ui_HandleTypeDef_t *ui)
{
    lcd1602_setCursor(ui->hlcd, 4, 1);
    lcd1602_write_char(ui->hlcd, 1);

    lcd1602_setCursor(ui->hlcd, 13, 1);
    lcd1602_write_char(ui->hlcd, 0);
}

/**
 * @brief Fill LCD buffers with program step data
 * @param[in] program Pointer to program structure
 * @param[in] program_index Program number (0-based) for display
 * @param[out] text_buf_top Array of top row strings
 * @param[out] text_buf_bottom Array of bottom row strings
 * @param[in] length Number of steps to format
 *
 * Formats program steps for display showing:
 * - Top: "P#: step/total"
 * - Bottom: "gradient -> temperature"
 */
static void ui_fill_lcd_buf_program(ui_program_t *program, uint8_t program_index,
        char text_buf_top[][17], char text_buf_bottom[][17], uint8_t length)
{
    char gradient_buf[5];
    char sign;

    for (int i = 0; i < length; ++i) {
        sign = (program->gradient_negative[i] == 1) ? '-' : ' ';
        uint16_t gradient = program->gradient[i];
        if(100 > gradient)
        {
            snprintf(gradient_buf, 5, " %c%2d", sign, gradient);
        }
        else
        {
            snprintf(gradient_buf, 5, "%c%3d", sign, gradient);
        }
        snprintf(text_buf_top[i], sizeof(text_buf_top[i]), "    P%d: %d/%d    ",
                program_index+1, i+1, length);
        snprintf(text_buf_bottom[i], sizeof(text_buf_bottom[i]), "%s  -> %4d C",
                gradient_buf, program->temperature[i]);
    }
}

/**
 * @brief Update program values from temporary input variables
 * @param[in,out] program Pointer to program to update
 *
 * Uses scroll_counter to determine which field to update:
 * - 0: program length
 * - Odd: gradient value
 * - Even: temperature value
 *
 * Applies bounds checking for all values.
 */
static void ui_fill_program_values(ui_program_t *program)
{
    temp_sign = (temp_counter == 0 && temp_sign == 1) ? 0 : temp_sign;
    switch (scroll_counter % 2)
    {
        case 0:  /* Temperature or length element */
            /* Limit to positive values only */
            if(temp_sign == 1)
            {
                temp_sign = 0;
                temp_counter = 0;
            }
            /* Length element (scroll_counter == 0) */
            if (0 == scroll_counter) {
                if(MAX_PROGRAM_SEQ_LENGTH < temp_counter_single)
                {
                    temp_counter_single = MAX_PROGRAM_SEQ_LENGTH;
                }
                if(MIN_PROGRAM_SEQ_LENGTH > temp_counter_single)
                {
                    temp_counter_single = MIN_PROGRAM_SEQ_LENGTH;
                }
                program->length = temp_counter_single;
                break;
            }
            /* Temperature element */
            if(MAX_TEMPERATURE < temp_counter)
            {
                temp_counter = MAX_TEMPERATURE;
            }
            program->temperature[(scroll_counter-1)/2] = temp_counter;
            break;

        case 1:  /* Gradient element */
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

/**
 * @brief Handle CREATE_PROGRAM_DETAILED menu state
 * @param[in,out] ui Pointer to UI handle
 * @param[in] event Input event to process
 * @return HAL_OK on success, HAL_ERROR on invalid event
 *
 * Allows entering program values step by step:
 * - scroll_counter 0: program length
 * - scroll_counter 1+: alternating gradient/temperature values
 * - ENC_BUT advances to next field or saves program when complete
 */
static HAL_StatusTypeDef ui_update_create_program_detailed(Ui_HandleTypeDef_t *ui, event_type_t event)
{
    uint8_t index = ui->programs.length;
    uint8_t length = c_program.length;

    switch (event) {
        case NO_EVENT:
            break;

        case BUT1:
        case ENC_DOWN:  /* Decrease value */
            if(0 >= temp_counter || temp_sign == 1)
            {
                temp_sign = 1;
                temp_counter = (event == BUT1) ? (temp_counter + BUTTON_INC) : (temp_counter + ENC_INC);
                temp_counter_single++;
            }
            else
            {
                if(event == BUT1)
                {
                    temp_counter = (BUTTON_INC > temp_counter) ? 0 : (temp_counter - BUTTON_INC);
                }
                else
                {
                    temp_counter = (ENC_INC > temp_counter) ? 0 : (temp_counter - ENC_INC);
                }
                temp_counter_single--;
            }
            break;

        case BUT2:
        case ENC_UP:  /* Increase value */
            if(0 >= temp_counter || temp_sign == 0)
            {
                temp_sign = 0;
                temp_counter = (event == BUT2) ? (temp_counter + BUTTON_INC) : (temp_counter + ENC_INC);
                temp_counter_single++;
            }
            else
            {
                if(event == BUT2){
                    temp_counter = (BUTTON_INC > temp_counter) ? 0 : (temp_counter - BUTTON_INC);
                }
                else
                {
                    temp_counter = (ENC_INC > temp_counter) ? 0 : (temp_counter - ENC_INC);
                }
                temp_counter_single--;
            }
            break;

        case BUT3:  /* Navigate back - cancel program creation */
            ui->state = CREATE_PROGRAM;
            c_program.length = 1;
            c_program.gradient[0] = 0;
            c_program.gradient_negative[0] = 0;
            c_program.temperature[0] = 0;
            scroll_counter = 0;
            temp_counter_single = 0;
            break;

        case BUT4:  /* Start/stop - not implemented */
            break;

        case ENC_BUT:  /* Enter - advance to next field or save */
            if((length*2) <= scroll_counter)
            {
                /* Save program */
                ui->programs.length++;
                ui->programs.program_list[index] = c_program;
                ui->programs.cur_index = index;
                /* Reset temporary values */
                c_program.length = 1;
                c_program.gradient[0] = 0;
                c_program.gradient_negative[0] = 0;
                c_program.temperature[0] = 0;
                scroll_counter = 0;
                temp_counter = 0;
                temp_counter_single = 0;
                ui->state = PROGRAMS_OVERVIEW;
                break;
            }
            scroll_counter++;
            temp_counter = 0;
            temp_counter_single = 0;
            break;

        default:
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }

    ui_fill_program_values(&c_program);

    char text_buf_top[length][UI_LCD_CHAR_SIZE];
    char text_buf_bottom[length][UI_LCD_CHAR_SIZE];
    ui_fill_lcd_buf_program(&c_program, index, text_buf_top, text_buf_bottom, length);

    if(scroll_counter == 0)
    {
        char text_buf[20];
        snprintf(text_buf, sizeof(text_buf), "   length: %2d   ", c_program.length);
        ui_print_lcd(ui, text_buf, empty_text_buf);
        return HAL_OK;
    }

    ui_print_lcd(ui, text_buf_top[(scroll_counter-1)/2], text_buf_bottom[(scroll_counter-1)/2]);
    ui_print_lcd_program_symb(ui);

    return HAL_OK;
}

/**
 * @brief Handle CREATE_PROGRAM menu state
 * @param[in,out] ui Pointer to UI handle
 * @param[in] event Input event to process
 * @return HAL_OK on success, HAL_ERROR on invalid event
 *
 * Entry point for creating a new program.
 * Shows "CREATE NEW PROGRAM" message.
 */
static HAL_StatusTypeDef ui_update_create_program(Ui_HandleTypeDef_t *ui, event_type_t event)
{
    switch (event) {
        case NO_EVENT:
            break;
        case BUT1:
            ui->programs.cur_index = ui->programs.length-1;
            ui->state = PROGRAMS_OVERVIEW;
            break;
        case BUT2:
            ui->programs.cur_index = 0;
            ui->state = PROGRAMS_OVERVIEW;
            break;
        case BUT3:
            ui->programs.cur_index = 0;
            ui->state = PROGRAMS;
            break;
        case BUT4:
            break;
        case ENC_BUT:
            ui->state = CREATE_PROGRAM_DETAILED;
            break;
        case ENC_UP:
            ui->programs.cur_index = 0;
            ui->state = PROGRAMS_OVERVIEW;
            break;
        case ENC_DOWN:
            ui->programs.cur_index = ui->programs.length-1;
            ui->state = PROGRAMS_OVERVIEW;
            break;
        default:
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }

    ui_print_lcd(ui, "   CREATE NEW    ", "    PROGRAM     ");
    return HAL_OK;
}

/**
 * @brief Handle PROGRAM_DETAILED menu state
 * @param[in,out] ui Pointer to UI handle
 * @param[in] event Input event to process
 * @return HAL_OK on success, HAL_ERROR on invalid event
 *
 * Displays detailed view of selected program with gradient and
 * temperature for each step. Scroll to navigate between steps.
 */
static HAL_StatusTypeDef ui_update_program_detailed(Ui_HandleTypeDef_t *ui, event_type_t event)
{
    uint8_t index = ui->programs.cur_index;
    uint8_t length = ui->programs.length;
    ui_program_t program = ui->programs.program_list[index];

    char text_buf_top[length][UI_LCD_CHAR_SIZE];
    char text_buf_bottom[length][UI_LCD_CHAR_SIZE];
    ui_fill_lcd_buf_program(&program, index, text_buf_top, text_buf_bottom, program.length);

    switch (event) {
        case NO_EVENT:
            break;
        case BUT1:
            if(0 >= scroll_counter) scroll_counter = length-1;
            else scroll_counter--;
            break;
        case BUT2:
            if(length-1 <= scroll_counter) scroll_counter = 0;
            else scroll_counter++;
            break;
        case BUT3:
            ui->state = PROGRAMS_OVERVIEW;
            scroll_counter = 0;
            break;
        case BUT4:
            /* Start/stop program execution */
            if (hheater.gradient_control_enabled) {
                /* Stop running program */
                heater_stop_program(&hheater);
                lcd1602_setRGB(ui->hlcd, 255, 255, 255);  /* White = idle */
            } else {
                /* Start current program */
                ui_program_t* selected = &ui->programs.program_list[index];
                /* Apply current settings to controller before starting */
                ui_apply_settings_to_controller(ui, &hgc);
                if (heater_start_program(&hheater, selected) == HAL_OK) {
                    lcd1602_setRGB(ui->hlcd, 255, 128, 0);  /* Orange = running */
                }
            }
            break;
        case ENC_BUT:
            ui->state = PROGRAMS_OVERVIEW;
            scroll_counter = 0;
            break;
        case ENC_UP:
            if(program.length-1 <= scroll_counter) scroll_counter = 0;
            else scroll_counter++;
            break;
        case ENC_DOWN:
            if(0 >= scroll_counter) scroll_counter = program.length-1;
            else scroll_counter--;
            break;
        default:
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }

    ui_print_lcd(ui, text_buf_top[scroll_counter], text_buf_bottom[scroll_counter]);
    ui_print_lcd_program_symb(ui);

    return HAL_OK;
}

/**
 * @brief Handle PROGRAMS_OVERVIEW menu state
 * @param[in,out] ui Pointer to UI handle
 * @param[in] event Input event to process
 * @return HAL_OK on success, HAL_ERROR on invalid event
 *
 * Shows list of available programs. Navigate to select,
 * ENC_BUT to view details, scrolling past end goes to CREATE_PROGRAM.
 */
static HAL_StatusTypeDef ui_update_programs_overview(Ui_HandleTypeDef_t *ui, event_type_t event)
{
    switch (event) {
        case NO_EVENT:
            break;
        case BUT1:
            ui->programs.cur_index--;
            if(ui->programs.cur_index >= ui->programs.length)
            {
                ui->state = CREATE_PROGRAM;
            }
            break;
        case BUT2:
            ui->programs.cur_index++;
            if(ui->programs.cur_index >= ui->programs.length)
            {
                ui->state = CREATE_PROGRAM;
            }
            break;
        case BUT3:
            ui->programs.cur_index = 0;
            ui->state = PROGRAMS;
            break;
        case BUT4:
            /* Start/stop program execution */
            if (hheater.gradient_control_enabled) {
                /* Stop running program */
                heater_stop_program(&hheater);
                lcd1602_setRGB(ui->hlcd, 255, 255, 255);  /* White = idle */
            } else {
                /* Start selected program */
                ui_program_t* selected = &ui->programs.program_list[ui->programs.cur_index];
                /* Apply current settings to controller before starting */
                ui_apply_settings_to_controller(ui, &hgc);
                if (heater_start_program(&hheater, selected) == HAL_OK) {
                    lcd1602_setRGB(ui->hlcd, 255, 128, 0);  /* Orange = running */
                }
            }
            break;
        case ENC_BUT:
            ui->state = PROGRAM_DETAILED;
            break;
        case ENC_UP:
            ui->programs.cur_index++;
            if(ui->programs.cur_index >= ui->programs.length)
            {
                ui->state = CREATE_PROGRAM;
            }
            break;
        case ENC_DOWN:
            ui->programs.cur_index--;
            if(ui->programs.cur_index >= ui->programs.length)
            {
                ui->programs.cur_index = 0;
                ui->state = CREATE_PROGRAM;
            }
            break;
        default:
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }

    uint8_t index = ui->programs.cur_index;
    if(index == 0)
    {
        ui_print_lcd(ui, "  SCHRUEBRAND    ", empty_text_buf);
    }
    else if(index == 1)
    {
        ui_print_lcd(ui, "  GLASURBRAND    ", empty_text_buf);
    }
    else
    {
        char buf[20];
        snprintf(buf, sizeof(buf), "   PROGRAM %u    ", index+1);
        ui_print_lcd(ui, buf, empty_text_buf);
    }

    return HAL_OK;
}

/**
 * @brief Handle SETTINGS_OVERVIEW menu state
 * @param[in,out] ui Pointer to UI handle
 * @param[in] event Input event to process
 * @return HAL_OK on success, HAL_ERROR on invalid event
 *
 * Shows list of settings. ENC_BUT toggles edit mode for adjusting values.
 */
/**
 * @brief Apply bounds to a setting value based on setting index
 * @param[in] index Setting index (0=gain, 1=filter, 2=Ti)
 * @param[in] val Value to clamp
 * @return Clamped value within appropriate bounds
 */
static float32_t ui_clamp_setting_value(uint8_t index, float32_t val)
{
    switch (index) {
        case 0:  /* Gain (Kc): 1-500 */
            if (val > MAX_SETTING_GAIN) val = MAX_SETTING_GAIN;
            else if (val < MIN_SETTING_GAIN) val = MIN_SETTING_GAIN;
            break;
        case 1:  /* Filter (alpha): 0.5-0.95 */
            if (val > MAX_SETTING_FILTER) val = MAX_SETTING_FILTER;
            else if (val < MIN_SETTING_FILTER) val = MIN_SETTING_FILTER;
            break;
        case 2:  /* Ti: 10-300 seconds */
            if (val > MAX_SETTING_TI) val = MAX_SETTING_TI;
            else if (val < MIN_SETTING_TI) val = MIN_SETTING_TI;
            break;
        default:
            break;
    }
    return val;
}

/**
 * @brief Get increment value for a setting based on index
 * @param[in] index Setting index (0=gain, 1=filter, 2=Ti)
 * @param[in] is_button 1 for button press, 0 for encoder
 * @return Increment value
 */
static float32_t ui_get_setting_increment(uint8_t index, uint8_t is_button)
{
    switch (index) {
        case 0:  /* Gain: 1 (button) or 10 (encoder) */
            return is_button ? 1.0f : 10.0f;
        case 1:  /* Filter: 0.01 (button) or 0.05 (encoder) */
            return is_button ? 0.01f : 0.05f;
        case 2:  /* Ti: 1 (button) or 10 (encoder) seconds */
            return is_button ? 1.0f : 10.0f;
        default:
            return is_button ? (BUTTON_INC_FLOAT_MILLIS / 1000.0f) : (ENC_INC_FLOAT_MILLIS / 1000.0f);
    }
}

static HAL_StatusTypeDef ui_update_settings_overview(Ui_HandleTypeDef_t *ui, event_type_t event)
{
    uint8_t index;

    switch (event) {
        case NO_EVENT:
            break;

        case BUT1:
        case ENC_DOWN:
            if(temp_bool)  /* Edit mode: adjust value */
            {
                index = ui->settings.cur_index;
                float32_t val = ui->settings.setting_list[index].value;
                float32_t inc = ui_get_setting_increment(index, (event == BUT1));
                val -= inc;
                val = ui_clamp_setting_value(index, val);
                ui->settings.setting_list[index].value = val;
                break;
            }
            /* Navigation mode: scroll list */
            ui->settings.cur_index--;
            if(ui->settings.cur_index >= ui->settings.length)
            {
                ui->settings.cur_index = ui->settings.length-1;
            }
            break;

        case BUT2:
        case ENC_UP:
            if(temp_bool)  /* Edit mode: adjust value */
            {
                index = ui->settings.cur_index;
                float32_t val = ui->settings.setting_list[index].value;
                float32_t inc = ui_get_setting_increment(index, (event == BUT2));
                val += inc;
                val = ui_clamp_setting_value(index, val);
                ui->settings.setting_list[index].value = val;
                break;
            }
            /* Navigation mode: scroll list */
            ui->settings.cur_index++;
            if(ui->settings.cur_index >= ui->settings.length)
            {
                ui->settings.cur_index = 0;
            }
            break;

        case BUT3:
            ui->settings.cur_index = 0;
            ui->state = SETTINGS;
            break;

        case BUT4:
            break;

        case ENC_BUT:  /* Toggle edit mode */
            temp_bool = (temp_bool) ? 0 : 1;
            break;

        default:
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }

    index = ui->settings.cur_index;
    ui_setting_t setting = ui->settings.setting_list[index];
    char buf[UI_LCD_CHAR_SIZE];

    /* Format value based on setting type */
    if (index == 1) {  /* Filter (alpha) - show 2 decimal places */
        snprintf(buf, sizeof(buf), "     %.2f       ", setting.value);
    } else {  /* Gain and Ti - show 1 decimal place */
        snprintf(buf, sizeof(buf), "     %.1f        ", setting.value);
    }

    ui_print_lcd(ui, setting.name, buf);

    return HAL_OK;
}

/**
 * @brief Handle PROGRAMS main menu state
 * @param[in,out] ui Pointer to UI handle
 * @param[in] event Input event to process
 * @return HAL_OK on success, HAL_ERROR on invalid event
 *
 * Main programs menu. Navigate left/right to SETTINGS/SETPOINT,
 * ENC_BUT to enter programs overview.
 */
static HAL_StatusTypeDef ui_update_programs(Ui_HandleTypeDef_t *ui, event_type_t event)
{
    switch (event) {
        case NO_EVENT:
            break;
        case BUT1:
            ui->state = SETTINGS;
            break;
        case BUT2:
            ui->state = SETPOINT;
            break;
        case BUT3:
            break;
        case BUT4:
            break;
        case ENC_BUT:
            ui->state = PROGRAMS_OVERVIEW;
            break;
        case ENC_UP:
            ui->state = SETPOINT;
            break;
        case ENC_DOWN:
            ui->state = SETTINGS;
            break;
        default:
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }

    ui_print_lcd(ui, "    PROGRAMS    ", empty_text_buf);
    return HAL_OK;
}

/**
 * @brief Handle SETPOINT menu state
 * @param[in,out] ui Pointer to UI handle
 * @param[in] event Input event to process
 * @return HAL_OK on success, HAL_ERROR on invalid event
 *
 * Setpoint selection menu (functionality TODO).
 */
static HAL_StatusTypeDef ui_update_setpoint(Ui_HandleTypeDef_t *ui, event_type_t event)
{
    switch (event) {
        case NO_EVENT:
            break;
        case BUT1:
            ui->state = PROGRAMS;
            break;
        case BUT2:
            ui->state = SETTINGS;
            break;
        case BUT3:
            break;
        case BUT4:
            break;
        case ENC_BUT:
            /* TODO: Implement setpoint selection */
            break;
        case ENC_UP:
            ui->state = SETTINGS;
            break;
        case ENC_DOWN:
            ui->state = PROGRAMS;
            break;
        default:
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }

    ui_print_lcd(ui, "    SETPOINT    ", empty_text_buf);
    return HAL_OK;
}

/**
 * @brief Handle SETTINGS main menu state
 * @param[in,out] ui Pointer to UI handle
 * @param[in] event Input event to process
 * @return HAL_OK on success, HAL_ERROR on invalid event
 *
 * Main settings menu. Navigate left/right to SETPOINT/PROGRAMS,
 * ENC_BUT to enter settings overview.
 */
static HAL_StatusTypeDef ui_update_settings(Ui_HandleTypeDef_t *ui, event_type_t event)
{
    switch (event) {
        case NO_EVENT:
            break;
        case BUT1:
            ui->state = SETPOINT;
            break;
        case BUT2:
            ui->state = PROGRAMS;
            break;
        case BUT3:
            break;
        case BUT4:
            break;
        case ENC_BUT:
            ui->state = SETTINGS_OVERVIEW;
            break;
        case ENC_UP:
            ui->state = PROGRAMS;
            break;
        case ENC_DOWN:
            ui->state = SETPOINT;
            break;
        default:
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }

    ui_print_lcd(ui, "    SETTINGS    ", empty_text_buf);
    return HAL_OK;
}

/**
 * @brief Get next event from UI event queue
 * @param[in] ui Pointer to UI handle
 * @return Next event from queue, or NO_EVENT if queue is empty
 */
event_type_t ui_get_events(Ui_HandleTypeDef_t *ui)
{
    if (!event_isEmpty(ui->queue))
    {
        return event_dequeue(ui->queue);
    }
    return NO_EVENT;
}

/**
 * @brief Main UI state machine update function
 * @param[in,out] ui Pointer to UI handle
 * @return HAL_OK on success, HAL_ERROR on invalid event in sub-handler
 *
 * Called from main loop. Processes events and dispatches to
 * appropriate state handler based on current menu state.
 * Only updates display if state changed or event occurred.
 */
HAL_StatusTypeDef ui_update(Ui_HandleTypeDef_t *ui)
{
    event_type_t cur_event = ui_get_events(ui);

    /* Skip update if no change */
    if(ui->last_state == ui->state && cur_event == NO_EVENT)
    {
        return HAL_OK;
    }
    ui->last_state = ui->state;

    switch (ui->state) {
        case SETTINGS:
            return ui_update_settings(ui, cur_event);

        case SETTINGS_OVERVIEW:
            return ui_update_settings_overview(ui, cur_event);

        case SETPOINT:
            return ui_update_setpoint(ui, cur_event);

        case PROGRAMS:
            return ui_update_programs(ui, cur_event);

        case PROGRAMS_OVERVIEW:
            return ui_update_programs_overview(ui, cur_event);

        case PROGRAM_DETAILED:
            return ui_update_program_detailed(ui, cur_event);

        case CREATE_PROGRAM:
            return ui_update_create_program(ui, cur_event);

        case CREATE_PROGRAM_DETAILED:
            return ui_update_create_program_detailed(ui, cur_event);

        default:
            return HAL_OK;
    }
    return HAL_OK;
}

/**
 * @brief Apply UI settings to gradient controller
 * @param[in] ui Pointer to UI handle containing settings
 * @param[in,out] hgc Pointer to gradient controller handle to configure
 *
 * Transfers Kc, alpha, and Ti settings from UI to gradient controller.
 * Settings mapping:
 * - setting_list[0]: Gain (Kc) - proportional gain
 * - setting_list[1]: Filter (alpha) - EMA coefficient
 * - setting_list[2]: Int Time (Ti) - integral time in seconds
 */
void ui_apply_settings_to_controller(Ui_HandleTypeDef_t* ui, GradientController_HandleTypeDef_t* hgc)
{
    if (ui == NULL || hgc == NULL) {
        return;
    }

    /* Apply Kc (gain) */
    hgc->Kc = Q16_FROM_FLOAT(ui->settings.setting_list[0].value);

    /* Apply alpha (EMA filter coefficient) */
    hgc->alpha = Q16_FROM_FLOAT(ui->settings.setting_list[1].value);
    hgc->one_minus_alpha = Q16_ONE - hgc->alpha;

    /* Apply Ti (integral time)
     * Ti_inv_Ts = Ts_ms / (Ti * 1000) as Q16.16
     * This is pre-computed for efficiency in the controller update loop
     */
    float ti_seconds = ui->settings.setting_list[2].value;
    hgc->Ti_inv_Ts = (q16_t)(((float)hgc->Ts_ms / (ti_seconds * 1000.0f)) * 65536.0f);

    /* Taw (anti-windup time constant) = Ti for simplicity */
    hgc->Taw_inv_Ts = hgc->Ti_inv_Ts;
}
