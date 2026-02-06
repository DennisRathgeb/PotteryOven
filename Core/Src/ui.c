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
extern TemperatureController_HandleTypeDef_t htc;
extern CoolingBrake_HandleTypeDef_t hcb;

/* Default programs for initialization */
static ui_program_t p1 = {3, {288, 300, 150}, {0, 1, 1}, {200, 80, 120}};
static ui_program_t p2 = {5, {80, 60, 150, 300, 80}, {0, 1, 0, 0, 1}, {15, 80, 120, 300, 600}};
static ui_program_t p3 = {2, {300, 150}, {0, 0}, {300, 80}};

/** @brief Settings category names */
static const char* settings_category_names[SETTINGS_NUM_CATEGORIES] = {
    "   INNER LOOP   ",
    "   OUTER LOOP   ",
    " COOLING BRAKE  ",
    "  SSR TIMING    ",
    "     STATUS     "
};

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
 * @brief Initialize settings with default values and bounds
 * @param[out] settings Pointer to settings structure
 */
static void ui_init_settings(ui_settings_t *settings)
{
    settings->cur_category = 0;
    settings->edit_mode = 0;

    /* Category 0: Inner Loop (Gradient Controller) */
    ui_settings_category_data_t *cat = &settings->categories[SETTINGS_CAT_INNER_LOOP];
    snprintf(cat->name, UI_LCD_CHAR_SIZE, "%s", settings_category_names[0]);
    cat->length = 4;
    cat->cur_index = 0;

    /* Kc - Proportional Gain */
    snprintf(cat->params[0].name, UI_LCD_CHAR_SIZE, "   Kc (Gain):   ");
    cat->params[0].value = 100.0f;
    cat->params[0].min_val = SETTINGS_KC_MIN;
    cat->params[0].max_val = SETTINGS_KC_MAX;
    cat->params[0].inc_btn = SETTINGS_KC_INC_BTN;
    cat->params[0].inc_enc = SETTINGS_KC_INC_ENC;
    cat->params[0].decimals = 0;

    /* Ti - Integral Time */
    snprintf(cat->params[1].name, UI_LCD_CHAR_SIZE, "   Ti (sec):    ");
    cat->params[1].value = 60.0f;
    cat->params[1].min_val = SETTINGS_TI_MIN;
    cat->params[1].max_val = SETTINGS_TI_MAX;
    cat->params[1].inc_btn = SETTINGS_TI_INC_BTN;
    cat->params[1].inc_enc = SETTINGS_TI_INC_ENC;
    cat->params[1].decimals = 0;

    /* Taw - Anti-windup Time */
    snprintf(cat->params[2].name, UI_LCD_CHAR_SIZE, "   Taw (sec):   ");
    cat->params[2].value = 60.0f;
    cat->params[2].min_val = SETTINGS_TAW_MIN;
    cat->params[2].max_val = SETTINGS_TAW_MAX;
    cat->params[2].inc_btn = SETTINGS_TAW_INC_BTN;
    cat->params[2].inc_enc = SETTINGS_TAW_INC_ENC;
    cat->params[2].decimals = 0;

    /* alpha - EMA Filter */
    snprintf(cat->params[3].name, UI_LCD_CHAR_SIZE, "    Alpha:      ");
    cat->params[3].value = 0.85f;
    cat->params[3].min_val = SETTINGS_ALPHA_MIN;
    cat->params[3].max_val = SETTINGS_ALPHA_MAX;
    cat->params[3].inc_btn = SETTINGS_ALPHA_INC_BTN;
    cat->params[3].inc_enc = SETTINGS_ALPHA_INC_ENC;
    cat->params[3].decimals = 2;

    /* Category 1: Outer Loop (Temperature Controller) */
    cat = &settings->categories[SETTINGS_CAT_OUTER_LOOP];
    snprintf(cat->name, UI_LCD_CHAR_SIZE, "%s", settings_category_names[1]);
    cat->length = 2;
    cat->cur_index = 0;

    /* Kp_T - Outer Proportional Gain */
    snprintf(cat->params[0].name, UI_LCD_CHAR_SIZE, "   Kp_T:        ");
    cat->params[0].value = 61.0f;
    cat->params[0].min_val = SETTINGS_KPT_MIN;
    cat->params[0].max_val = SETTINGS_KPT_MAX;
    cat->params[0].inc_btn = SETTINGS_KPT_INC_BTN;
    cat->params[0].inc_enc = SETTINGS_KPT_INC_ENC;
    cat->params[0].decimals = 0;

    /* T_band - Deadband */
    snprintf(cat->params[1].name, UI_LCD_CHAR_SIZE, " T_band (\xDF""C):  ");
    cat->params[1].value = 5.0f;
    cat->params[1].min_val = SETTINGS_TBAND_MIN;
    cat->params[1].max_val = SETTINGS_TBAND_MAX;
    cat->params[1].inc_btn = SETTINGS_TBAND_INC_BTN;
    cat->params[1].inc_enc = SETTINGS_TBAND_INC_ENC;
    cat->params[1].decimals = 0;

    /* Category 2: Cooling Brake */
    cat = &settings->categories[SETTINGS_CAT_COOLING_BRAKE];
    snprintf(cat->name, UI_LCD_CHAR_SIZE, "%s", settings_category_names[2]);
    cat->length = 4;
    cat->cur_index = 0;

    /* g_min - Cooling Rate Limit (stored positive, displayed negative) */
    snprintf(cat->params[0].name, UI_LCD_CHAR_SIZE, "g_min (\xDF""C/h): ");
    cat->params[0].value = 100.0f;  /* Stored positive */
    cat->params[0].min_val = SETTINGS_GMIN_MIN;
    cat->params[0].max_val = SETTINGS_GMIN_MAX;
    cat->params[0].inc_btn = SETTINGS_GMIN_INC_BTN;
    cat->params[0].inc_enc = SETTINGS_GMIN_INC_ENC;
    cat->params[0].decimals = 0;

    /* Hysteresis */
    snprintf(cat->params[1].name, UI_LCD_CHAR_SIZE, "Hyst (\xDF""C/h):  ");
    cat->params[1].value = 6.0f;
    cat->params[1].min_val = SETTINGS_HYST_MIN;
    cat->params[1].max_val = SETTINGS_HYST_MAX;
    cat->params[1].inc_btn = SETTINGS_HYST_INC_BTN;
    cat->params[1].inc_enc = SETTINGS_HYST_INC_ENC;
    cat->params[1].decimals = 0;

    /* Kb - Brake Gain */
    snprintf(cat->params[2].name, UI_LCD_CHAR_SIZE, "   Kb:          ");
    cat->params[2].value = 3000.0f;
    cat->params[2].min_val = SETTINGS_KB_MIN;
    cat->params[2].max_val = SETTINGS_KB_MAX;
    cat->params[2].inc_btn = SETTINGS_KB_INC_BTN;
    cat->params[2].inc_enc = SETTINGS_KB_INC_ENC;
    cat->params[2].decimals = 0;

    /* u_brake_max - Max Brake Power */
    snprintf(cat->params[3].name, UI_LCD_CHAR_SIZE, " Brake Max (%): ");
    cat->params[3].value = 10.0f;
    cat->params[3].min_val = SETTINGS_UBRAKE_MIN;
    cat->params[3].max_val = SETTINGS_UBRAKE_MAX;
    cat->params[3].inc_btn = SETTINGS_UBRAKE_INC_BTN;
    cat->params[3].inc_enc = SETTINGS_UBRAKE_INC_ENC;
    cat->params[3].decimals = 0;

    /* Category 3: SSR Timing */
    cat = &settings->categories[SETTINGS_CAT_SSR_TIMING];
    snprintf(cat->name, UI_LCD_CHAR_SIZE, "%s", settings_category_names[3]);
    cat->length = 2;
    cat->cur_index = 0;

    /* Window Period */
    snprintf(cat->params[0].name, UI_LCD_CHAR_SIZE, " Window (sec):  ");
    cat->params[0].value = 20.0f;
    cat->params[0].min_val = SETTINGS_SSRWIN_MIN;
    cat->params[0].max_val = SETTINGS_SSRWIN_MAX;
    cat->params[0].inc_btn = SETTINGS_SSRWIN_INC_BTN;
    cat->params[0].inc_enc = SETTINGS_SSRWIN_INC_ENC;
    cat->params[0].decimals = 0;

    /* Min Switch Time */
    snprintf(cat->params[1].name, UI_LCD_CHAR_SIZE, " Min Sw (sec):  ");
    cat->params[1].value = 5.0f;
    cat->params[1].min_val = SETTINGS_SSRMIN_MIN;
    cat->params[1].max_val = SETTINGS_SSRMIN_MAX;
    cat->params[1].inc_btn = SETTINGS_SSRMIN_INC_BTN;
    cat->params[1].inc_enc = SETTINGS_SSRMIN_INC_ENC;
    cat->params[1].decimals = 0;

    /* Category 4: Status (read-only) */
    cat = &settings->categories[SETTINGS_CAT_STATUS];
    snprintf(cat->name, UI_LCD_CHAR_SIZE, "%s", settings_category_names[4]);
    cat->length = 4;
    cat->cur_index = 0;

    /* Temperature - read-only */
    snprintf(cat->params[0].name, UI_LCD_CHAR_SIZE, " Temp (\xDF""C):    ");
    cat->params[0].value = 0.0f;
    cat->params[0].decimals = 1;

    /* Gradient - read-only */
    snprintf(cat->params[1].name, UI_LCD_CHAR_SIZE, " Grad (\xDF""C/h):  ");
    cat->params[1].value = 0.0f;
    cat->params[1].decimals = 1;

    /* Duty - read-only */
    snprintf(cat->params[2].name, UI_LCD_CHAR_SIZE, "  Duty (%):     ");
    cat->params[2].value = 0.0f;
    cat->params[2].decimals = 1;

    /* Mode - read-only */
    snprintf(cat->params[3].name, UI_LCD_CHAR_SIZE, "    Mode:       ");
    cat->params[3].value = 0.0f;
    cat->params[3].decimals = 0;
}

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

    /* Initialize settings with default values */
    ui_init_settings(&ui->settings);

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

/*============================================================================*/
/* Settings Menu Handlers                                                      */
/*============================================================================*/

/**
 * @brief Handle SETTINGS_CATEGORIES menu state
 * @param[in,out] ui Pointer to UI handle
 * @param[in] event Input event to process
 * @return HAL_OK on success, HAL_ERROR on invalid event
 *
 * Shows list of settings categories. ENC_BUT enters selected category.
 */
static HAL_StatusTypeDef ui_update_settings_categories(Ui_HandleTypeDef_t *ui, event_type_t event)
{
    switch (event) {
        case NO_EVENT:
            break;

        case BUT1:
        case ENC_DOWN:
            if (ui->settings.cur_category == 0) {
                ui->settings.cur_category = SETTINGS_NUM_CATEGORIES - 1;
            } else {
                ui->settings.cur_category--;
            }
            break;

        case BUT2:
        case ENC_UP:
            ui->settings.cur_category++;
            if (ui->settings.cur_category >= SETTINGS_NUM_CATEGORIES) {
                ui->settings.cur_category = 0;
            }
            break;

        case BUT3:
            ui->settings.cur_category = 0;
            ui->state = SETTINGS;
            break;

        case BUT4:
            break;

        case ENC_BUT:
            /* Enter selected category */
            switch (ui->settings.cur_category) {
                case SETTINGS_CAT_INNER_LOOP:
                    ui->state = SETTINGS_INNER_LOOP;
                    break;
                case SETTINGS_CAT_OUTER_LOOP:
                    ui->state = SETTINGS_OUTER_LOOP;
                    break;
                case SETTINGS_CAT_COOLING_BRAKE:
                    ui->state = SETTINGS_COOLING_BRAKE;
                    break;
                case SETTINGS_CAT_SSR_TIMING:
                    ui->state = SETTINGS_SSR_TIMING;
                    break;
                case SETTINGS_CAT_STATUS:
                    ui->state = SETTINGS_STATUS;
                    break;
                default:
                    break;
            }
            ui->settings.categories[ui->settings.cur_category].cur_index = 0;
            ui->settings.edit_mode = 0;
            break;

        default:
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }

    /* Display current category name */
    char buf[UI_LCD_CHAR_SIZE];
    snprintf(buf, sizeof(buf), "    %d / %d      ", ui->settings.cur_category + 1, SETTINGS_NUM_CATEGORIES);
    ui_print_lcd(ui, settings_category_names[ui->settings.cur_category], buf);

    return HAL_OK;
}

/**
 * @brief Generic handler for settings parameter editing
 * @param[in,out] ui Pointer to UI handle
 * @param[in] event Input event to process
 * @param[in] cat_index Category index
 * @param[in] back_state State to return to on BUT3
 * @return HAL_OK on success, HAL_ERROR on invalid event
 */
static HAL_StatusTypeDef ui_update_settings_params(Ui_HandleTypeDef_t *ui, event_type_t event,
                                                    uint8_t cat_index, ui_menupoint_t back_state)
{
    ui_settings_category_data_t *cat = &ui->settings.categories[cat_index];
    ui_setting_param_t *param;

    /* For STATUS category, update live values first */
    if (cat_index == SETTINGS_CAT_STATUS) {
        /* Temperature */
        cat->params[0].value = max31855_get_temp_f32(hheater.htemp);
        /* Gradient (convert from Q16.16 °C/s to °C/h) */
        if (hheater.hgc != NULL) {
            cat->params[1].value = Q16_TO_FLOAT(hheater.hgc->g_f_prev) * 3600.0f;
        }
        /* Duty (convert from Q16.16 to %) */
        cat->params[2].value = Q16_TO_FLOAT(hheater.ssr.duty_current) * 100.0f;
        /* Mode */
        cat->params[3].value = (float32_t)hheater.control_mode;
    }

    switch (event) {
        case NO_EVENT:
            break;

        case BUT1:
        case ENC_DOWN:
            if (ui->settings.edit_mode && cat_index != SETTINGS_CAT_STATUS) {
                /* Edit mode: decrease value */
                param = &cat->params[cat->cur_index];
                float32_t inc = (event == BUT1) ? param->inc_btn : param->inc_enc;
                param->value -= inc;
                if (param->value < param->min_val) {
                    param->value = param->min_val;
                }
            } else {
                /* Navigation mode: scroll list */
                if (cat->cur_index == 0) {
                    cat->cur_index = cat->length - 1;
                } else {
                    cat->cur_index--;
                }
            }
            break;

        case BUT2:
        case ENC_UP:
            if (ui->settings.edit_mode && cat_index != SETTINGS_CAT_STATUS) {
                /* Edit mode: increase value */
                param = &cat->params[cat->cur_index];
                float32_t inc = (event == BUT2) ? param->inc_btn : param->inc_enc;
                param->value += inc;
                if (param->value > param->max_val) {
                    param->value = param->max_val;
                }
            } else {
                /* Navigation mode: scroll list */
                cat->cur_index++;
                if (cat->cur_index >= cat->length) {
                    cat->cur_index = 0;
                }
            }
            break;

        case BUT3:
            cat->cur_index = 0;
            ui->settings.edit_mode = 0;
            ui->state = back_state;
            break;

        case BUT4:
            /* Apply settings immediately when BUT4 pressed */
            ui_apply_all_settings(ui);
            break;

        case ENC_BUT:
            /* Toggle edit mode (except for STATUS which is read-only) */
            if (cat_index != SETTINGS_CAT_STATUS) {
                ui->settings.edit_mode = !ui->settings.edit_mode;
            }
            break;

        default:
#ifdef UI_ENABLE_LOG
            logMsg(LOG_ERROR, "unknown event: %u", event);
#endif
            return HAL_ERROR;
    }

    /* Display current parameter */
    param = &cat->params[cat->cur_index];
    char buf[UI_LCD_CHAR_SIZE];

    /* Format value based on decimals setting */
    if (cat_index == SETTINGS_CAT_STATUS && cat->cur_index == 3) {
        /* Mode: display as text */
        const char* mode_str;
        switch ((int)param->value) {
            case CTRL_MODE_HEAT:        mode_str = "HEAT"; break;
            case CTRL_MODE_COOL_PASSIVE: mode_str = "COOL"; break;
            case CTRL_MODE_COOL_BRAKE:  mode_str = "BRAKE"; break;
            default:                    mode_str = "OFF"; break;
        }
        snprintf(buf, sizeof(buf), "     %s        ", mode_str);
    } else if (cat_index == SETTINGS_CAT_COOLING_BRAKE && cat->cur_index == 0) {
        /* g_min: display as negative */
        snprintf(buf, sizeof(buf), "    -%.0f        ", param->value);
    } else if (param->decimals == 0) {
        snprintf(buf, sizeof(buf), "     %.0f        ", param->value);
    } else if (param->decimals == 1) {
        snprintf(buf, sizeof(buf), "     %.1f       ", param->value);
    } else {
        snprintf(buf, sizeof(buf), "     %.2f       ", param->value);
    }

    /* Add edit indicator */
    if (ui->settings.edit_mode) {
        buf[0] = '>';
        buf[15] = '<';
    }

    ui_print_lcd(ui, param->name, buf);

    return HAL_OK;
}

/**
 * @brief Handle SETTINGS_INNER_LOOP menu state
 */
static HAL_StatusTypeDef ui_update_settings_inner_loop(Ui_HandleTypeDef_t *ui, event_type_t event)
{
    return ui_update_settings_params(ui, event, SETTINGS_CAT_INNER_LOOP, SETTINGS_CATEGORIES);
}

/**
 * @brief Handle SETTINGS_OUTER_LOOP menu state
 */
static HAL_StatusTypeDef ui_update_settings_outer_loop(Ui_HandleTypeDef_t *ui, event_type_t event)
{
    return ui_update_settings_params(ui, event, SETTINGS_CAT_OUTER_LOOP, SETTINGS_CATEGORIES);
}

/**
 * @brief Handle SETTINGS_COOLING_BRAKE menu state
 */
static HAL_StatusTypeDef ui_update_settings_cooling_brake(Ui_HandleTypeDef_t *ui, event_type_t event)
{
    return ui_update_settings_params(ui, event, SETTINGS_CAT_COOLING_BRAKE, SETTINGS_CATEGORIES);
}

/**
 * @brief Handle SETTINGS_SSR_TIMING menu state
 */
static HAL_StatusTypeDef ui_update_settings_ssr_timing(Ui_HandleTypeDef_t *ui, event_type_t event)
{
    return ui_update_settings_params(ui, event, SETTINGS_CAT_SSR_TIMING, SETTINGS_CATEGORIES);
}

/**
 * @brief Handle SETTINGS_STATUS menu state (read-only)
 */
static HAL_StatusTypeDef ui_update_settings_status(Ui_HandleTypeDef_t *ui, event_type_t event)
{
    return ui_update_settings_params(ui, event, SETTINGS_CAT_STATUS, SETTINGS_CATEGORIES);
}

/**
 * @brief Handle SETTINGS_OVERVIEW menu state (legacy - redirects to categories)
 */
static HAL_StatusTypeDef ui_update_settings_overview(Ui_HandleTypeDef_t *ui, event_type_t event)
{
    /* Redirect to new categories menu */
    ui->state = SETTINGS_CATEGORIES;
    return ui_update_settings_categories(ui, event);
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
 * ENC_BUT to enter settings categories.
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
            ui->settings.cur_category = 0;
            ui->state = SETTINGS_CATEGORIES;
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

        case SETTINGS_CATEGORIES:
            return ui_update_settings_categories(ui, cur_event);

        case SETTINGS_INNER_LOOP:
            return ui_update_settings_inner_loop(ui, cur_event);

        case SETTINGS_OUTER_LOOP:
            return ui_update_settings_outer_loop(ui, cur_event);

        case SETTINGS_COOLING_BRAKE:
            return ui_update_settings_cooling_brake(ui, cur_event);

        case SETTINGS_SSR_TIMING:
            return ui_update_settings_ssr_timing(ui, cur_event);

        case SETTINGS_STATUS:
            return ui_update_settings_status(ui, cur_event);

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
 * @brief Apply all UI settings to controllers
 * @param[in] ui Pointer to UI handle containing settings
 *
 * Transfers all settings from UI to the respective controllers.
 */
void ui_apply_all_settings(Ui_HandleTypeDef_t* ui)
{
    if (ui == NULL) {
        return;
    }

    ui_settings_category_data_t *cat;

    /* ===== Inner Loop (Gradient Controller) ===== */
    cat = &ui->settings.categories[SETTINGS_CAT_INNER_LOOP];
    if (hheater.hgc != NULL) {
        GradientController_HandleTypeDef_t *gc = hheater.hgc;

        /* Kc (Gain) */
        gc->Kc = Q16_FROM_FLOAT(cat->params[0].value);

        /* Ti (Integral Time) - convert to Ts/Ti ratio */
        float ti_seconds = cat->params[1].value;
        gc->Ts_over_Ti = (q16_t)(((float)gc->Ts_ms / (ti_seconds * 1000.0f)) * 65536.0f);

        /* Taw (Anti-windup Time) - convert to Ts/Taw ratio */
        float taw_seconds = cat->params[2].value;
        gc->Ts_over_Taw = (q16_t)(((float)gc->Ts_ms / (taw_seconds * 1000.0f)) * 65536.0f);

        /* alpha (EMA Filter) */
        gc->alpha = Q16_FROM_FLOAT(cat->params[3].value);
        gc->one_minus_alpha = Q16_ONE - gc->alpha;
    }

    /* ===== Outer Loop (Temperature Controller) ===== */
    cat = &ui->settings.categories[SETTINGS_CAT_OUTER_LOOP];
    if (hheater.htc != NULL) {
        TemperatureController_HandleTypeDef_t *tc = hheater.htc;

        /* Kp_T (Outer Gain) */
        tc->Kp_T = (q16_t)cat->params[0].value;

        /* T_band (Deadband) - convert °C to milli-degrees */
        tc->T_band_mdeg = (int32_t)(cat->params[1].value * 1000.0f);
    }

    /* ===== Cooling Brake ===== */
    cat = &ui->settings.categories[SETTINGS_CAT_COOLING_BRAKE];
    if (hheater.hcb != NULL) {
        CoolingBrake_HandleTypeDef_t *cb = hheater.hcb;

        /* g_min (Cooling Limit) - convert °C/h to Q16.16 °C/s, negate */
        float g_min_per_hour = cat->params[0].value;  /* Stored positive */
        cb->g_min = -((int32_t)g_min_per_hour << 16) / 3600;

        /* Hysteresis - convert °C/h to Q16.16 °C/s */
        float hyst_per_hour = cat->params[1].value;
        cb->dg_hyst = ((int32_t)hyst_per_hour << 16) / 3600;

        /* Kb (Brake Gain) */
        cb->Kb = (q16_t)cat->params[2].value;

        /* u_brake_max - convert % to Q16.16 (0-65536) */
        cb->u_brake_max = (q16_t)(cat->params[3].value * 65536.0f / 100.0f);
    }

    /* ===== SSR Timing ===== */
    cat = &ui->settings.categories[SETTINGS_CAT_SSR_TIMING];
    {
        /* Window Period */
        hheater.ssr.window_seconds = (uint8_t)cat->params[0].value;

        /* Min Switch Time */
        hheater.ssr.min_switch_seconds = (uint8_t)cat->params[1].value;
    }

#ifdef UI_ENABLE_LOG
    printf("Settings applied\r\n");
#endif
}

/**
 * @brief Load current controller values into UI settings
 * @param[in,out] ui Pointer to UI handle to update
 *
 * Reads current values from all controllers and updates UI settings.
 */
void ui_load_settings_from_controllers(Ui_HandleTypeDef_t* ui)
{
    if (ui == NULL) {
        return;
    }

    ui_settings_category_data_t *cat;

    /* ===== Inner Loop (Gradient Controller) ===== */
    cat = &ui->settings.categories[SETTINGS_CAT_INNER_LOOP];
    if (hheater.hgc != NULL) {
        GradientController_HandleTypeDef_t *gc = hheater.hgc;

        /* Kc */
        cat->params[0].value = Q16_TO_FLOAT(gc->Kc);

        /* Ti - convert from Ts/Ti ratio to seconds */
        if (gc->Ts_over_Ti > 0) {
            cat->params[1].value = (float)gc->Ts_ms / (Q16_TO_FLOAT(gc->Ts_over_Ti) * 1000.0f);
        }

        /* Taw */
        if (gc->Ts_over_Taw > 0) {
            cat->params[2].value = (float)gc->Ts_ms / (Q16_TO_FLOAT(gc->Ts_over_Taw) * 1000.0f);
        }

        /* alpha */
        cat->params[3].value = Q16_TO_FLOAT(gc->alpha);
    }

    /* ===== Outer Loop (Temperature Controller) ===== */
    cat = &ui->settings.categories[SETTINGS_CAT_OUTER_LOOP];
    if (hheater.htc != NULL) {
        TemperatureController_HandleTypeDef_t *tc = hheater.htc;

        /* Kp_T */
        cat->params[0].value = (float)tc->Kp_T;

        /* T_band - convert milli-degrees to °C */
        cat->params[1].value = (float)tc->T_band_mdeg / 1000.0f;
    }

    /* ===== Cooling Brake ===== */
    cat = &ui->settings.categories[SETTINGS_CAT_COOLING_BRAKE];
    if (hheater.hcb != NULL) {
        CoolingBrake_HandleTypeDef_t *cb = hheater.hcb;

        /* g_min - convert Q16.16 °C/s to °C/h, make positive */
        cat->params[0].value = -Q16_TO_FLOAT(cb->g_min) * 3600.0f;

        /* Hysteresis - convert Q16.16 °C/s to °C/h */
        cat->params[1].value = Q16_TO_FLOAT(cb->dg_hyst) * 3600.0f;

        /* Kb */
        cat->params[2].value = (float)cb->Kb;

        /* u_brake_max - convert Q16.16 to % */
        cat->params[3].value = Q16_TO_FLOAT(cb->u_brake_max) * 100.0f;
    }

    /* ===== SSR Timing ===== */
    cat = &ui->settings.categories[SETTINGS_CAT_SSR_TIMING];
    {
        cat->params[0].value = (float)hheater.ssr.window_seconds;
        cat->params[1].value = (float)hheater.ssr.min_switch_seconds;
    }
}

/**
 * @brief Legacy function - Apply UI settings to gradient controller
 * @deprecated Use ui_apply_all_settings() instead
 */
void ui_apply_settings_to_controller(Ui_HandleTypeDef_t* ui, GradientController_HandleTypeDef_t* hgc)
{
    (void)hgc;  /* Unused - we use global handles now */
    ui_apply_all_settings(ui);
}
