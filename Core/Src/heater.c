/**
 * @file heater.c
 * @brief Implementation of heater control for kiln heating coils
 * @author Dennis Rathgeb
 * @date Apr 5, 2024
 *
 * Implements multi-level heater control with PWM, door safety,
 * and temperature sampling for PID control.
 */

#include "heater.h"

/**
 * @brief Reset heater parameters to default state
 * @param[in,out] hheater Pointer to heater handle
 *
 * Sets:
 * - heater_level to 0 (off)
 * - heater_level_prev to 0xff (no previous level)
 * - flag_door_open to 0 (closed)
 * - All coils to COIL_OFF with time_pwm_last = 0
 * - gradient_control_enabled to 0 (disabled by default)
 */
static void heater_set_default_params(Heater_HandleTypeDef_t* hheater)
{
    hheater->heater_level = 0;
    hheater->heater_level_prev = 0xff;

    /* TODO: Set to 1 for production (door starts open) */
    hheater->flag_door_open = 0;

    hheater->coils.coil1.state = COIL_OFF;
    hheater->coils.coil1.time_pwm_last = 0;

    hheater->coils.coil2.state = COIL_OFF;
    hheater->coils.coil2.time_pwm_last = 0;

    hheater->coils.coil3.state = COIL_OFF;
    hheater->coils.coil3.time_pwm_last = 0;

    /* Gradient control disabled by default - set hgc and enable flag to use */
    hheater->gradient_control_enabled = 0;
}

/**
 * @brief Initialize the heater module
 * @param[out] hheater Pointer to heater handle to initialize
 * @param[in] htemp Pointer to MAX31855 temperature sensor handle
 * @param[in] coil1_port GPIO port for coil 1
 * @param[in] coil1_pin GPIO pin for coil 1
 * @param[in] coil2_port GPIO port for coil 2
 * @param[in] coil2_pin GPIO pin for coil 2
 * @param[in] coil3_port GPIO port for coil 3
 * @param[in] coil3_pin GPIO pin for coil 3
 * @return HAL_OK on success, HAL_ERROR if hheater is NULL
 */
HAL_StatusTypeDef initHeater(Heater_HandleTypeDef_t* hheater, MAX31855_HandleTypeDef_t* htemp,
        GPIO_TypeDef* coil1_port, uint16_t coil1_pin,
        GPIO_TypeDef* coil2_port, uint16_t coil2_pin,
        GPIO_TypeDef* coil3_port, uint16_t coil3_pin)
{
    if(NULL == hheater)
    {
        return HAL_ERROR;
    }

    hheater->coils.coil1.port = coil1_port;
    hheater->coils.coil1.pin = coil1_pin;

    hheater->coils.coil2.port = coil2_port;
    hheater->coils.coil2.pin = coil2_pin;

    hheater->coils.coil3.port = coil3_port;
    hheater->coils.coil3.pin = coil3_pin;

    heater_set_default_params(hheater);

    hheater->htemp = htemp;
    hheater->time_counter = 0;
    hheater->hgc = NULL;  /* Must be set separately to enable gradient control */

    return HAL_OK;
}

/**
 * @brief Set coil GPIO pin high (turn on)
 * @param[in] coil Pointer to coil structure
 */
static void heater_set_coil_on(heater_coil_t* coil)
{
    HAL_GPIO_WritePin(coil->port, coil->pin, GPIO_PIN_SET);
}

/**
 * @brief Set coil GPIO pin low (turn off)
 * @param[in] coil Pointer to coil structure
 */
static void heater_set_coil_off(heater_coil_t* coil)
{
    HAL_GPIO_WritePin(coil->port, coil->pin, GPIO_PIN_RESET);
}

/**
 * @brief Toggle coil GPIO pin state
 * @param[in] coil Pointer to coil structure
 */
static void heater_toggle_coil(heater_coil_t* coil)
{
    HAL_GPIO_TogglePin(coil->port, coil->pin);
}

/**
 * @brief Update PWM state for a coil
 * @param[in,out] coil Pointer to coil structure
 *
 * Implements 50% duty cycle PWM by toggling the coil every
 * PWM_ON_SECONDS interval.
 *
 * @todo Use RTC-based timing to avoid uint32_t overflow
 */
static void heater_update_pwm_coil(heater_coil_t* coil)
{
    /* First call: initialize timestamp */
    if(0 == coil->time_pwm_last){
        coil->time_pwm_last = HAL_GetTick();
    }
    /* Subsequent calls: toggle if interval elapsed */
    else{
        uint32_t time = HAL_GetTick();
        if(time >= (PWM_ON_SECONDS + coil->time_pwm_last))
        {
            coil->time_pwm_last = time;
            heater_toggle_coil(coil);
        }
    }
}

/**
 * @brief Apply configured state to a single coil
 * @param[in,out] coil Pointer to coil structure
 * @return HAL_OK on success, HAL_ERROR if coil is NULL or invalid state
 *
 * Handles OFF, ON, and PWM states. Resets time_pwm_last when
 * transitioning out of PWM mode.
 */
static HAL_StatusTypeDef heater_set_coil_state(heater_coil_t* coil)
{
    if(NULL == coil)
    {
        return HAL_ERROR;
    }
    switch (coil->state) {
        case COIL_OFF:
            coil->time_pwm_last = 0;
            heater_set_coil_off(coil);
            break;
        case COIL_ON:
            coil->time_pwm_last = 0;
            heater_set_coil_on(coil);
            break;
        case COIL_PWM:
            heater_update_pwm_coil(coil);
            break;
        default:
            return HAL_ERROR;
            break;
    }
    return HAL_OK;
}

/**
 * @brief Check door state and pause/resume heating accordingly
 * @param[in,out] hheater Pointer to heater handle
 * @return HAL_OK on success, HAL_ERROR on failure
 *
 * If door is open:
 * - Saves current level to heater_level_prev
 * - Sets heater to level 0 (off)
 *
 * If door closes after being open:
 * - Restores previous level from heater_level_prev
 * - Resets heater_level_prev to 0xff
 */
static HAL_StatusTypeDef heater_check_door_state(Heater_HandleTypeDef_t* hheater)
{
    if(NULL == hheater){
        return HAL_ERROR;
    }

    /* Door is open - pause heating */
    if(hheater->flag_door_open)
    {
        /* Save previous level if not already saved */
        if(0xff == hheater->heater_level_prev)
        {
            hheater->heater_level_prev = hheater->heater_level;
        }

        /* Turn off heater if not already off */
        if(0 != hheater->heater_level)
        {
            if(HAL_OK != heater_set_level(hheater, 0))
            {
               return HAL_ERROR;
            }
        }
    }

    /* Door is closed */
    if(!hheater->flag_door_open)
    {
        /* Resume previous level if one was saved */
        if(0xff != hheater->heater_level_prev)
        {
            if(HAL_OK != heater_set_level(hheater, hheater->heater_level_prev))
            {
                return HAL_ERROR;
            }
            hheater->heater_level_prev = 0xff;
        }
    }
    return HAL_OK;
}

/**
 * @brief Turn off all heaters and reset to default state
 * @param[in,out] hheater Pointer to heater handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef heater_turn_off(Heater_HandleTypeDef_t* hheater)
{
    if(HAL_OK != heater_set_level(hheater, 0))
    {
        return HAL_ERROR;
    }
    heater_set_default_params(hheater);
    return HAL_OK;
}

/**
 * @brief Set the heater power level (0-6)
 * @param[in,out] hheater Pointer to heater handle
 * @param[in] level Power level (0-6)
 * @return HAL_OK on success, HAL_ERROR for invalid level
 *
 * Level to coil state mapping:
 * | Level | Coil1 | Coil2 | Coil3 |
 * |-------|-------|-------|-------|
 * | 0     | OFF   | OFF   | OFF   |
 * | 1     | PWM   | OFF   | OFF   |
 * | 2     | ON    | OFF   | OFF   |
 * | 3     | ON    | PWM   | OFF   |
 * | 4     | ON    | ON    | OFF   |
 * | 5     | ON    | ON    | PWM   |
 * | 6     | ON    | ON    | ON    |
 */
HAL_StatusTypeDef heater_set_level(Heater_HandleTypeDef_t* hheater, uint8_t level)
{
    hheater->heater_level = level;

    switch (level) {
        case 0:
            hheater->coils.coil1.state = COIL_OFF;
            hheater->coils.coil2.state = COIL_OFF;
            hheater->coils.coil3.state = COIL_OFF;
            break;
        case 1:
            hheater->coils.coil1.state = COIL_PWM;
            hheater->coils.coil2.state = COIL_OFF;
            hheater->coils.coil3.state = COIL_OFF;
            break;
        case 2:
            hheater->coils.coil1.state = COIL_ON;
            hheater->coils.coil2.state = COIL_OFF;
            hheater->coils.coil3.state = COIL_OFF;
            break;
        case 3:
            hheater->coils.coil1.state = COIL_ON;
            hheater->coils.coil2.state = COIL_PWM;
            hheater->coils.coil3.state = COIL_OFF;
            break;
        case 4:
            hheater->coils.coil1.state = COIL_ON;
            hheater->coils.coil2.state = COIL_ON;
            hheater->coils.coil3.state = COIL_OFF;
            break;
        case 5:
            hheater->coils.coil1.state = COIL_ON;
            hheater->coils.coil2.state = COIL_ON;
            hheater->coils.coil3.state = COIL_PWM;
            break;
        case 6:
            hheater->coils.coil1.state = COIL_ON;
            hheater->coils.coil2.state = COIL_ON;
            hheater->coils.coil3.state = COIL_ON;
            break;
        default:
            return HAL_ERROR;
            break;
    }
    return HAL_OK;
}

/**
 * @brief Apply configured state to all coil GPIO outputs
 * @param[in,out] hheater Pointer to heater handle
 * @return HAL_OK on success
 *
 * Checks door state first, then updates all three coils.
 * Must be called periodically for PWM to work correctly.
 */
HAL_StatusTypeDef heater_set_state(Heater_HandleTypeDef_t* hheater)
{
    if(HAL_OK != heater_check_door_state(hheater))
    {
        return HAL_ERROR;
    }

    heater_set_coil_state(&hheater->coils.coil1);
    heater_set_coil_state(&hheater->coils.coil2);
    heater_set_coil_state(&hheater->coils.coil3);

    return HAL_OK;
}

/**
 * @brief Calculate temperature slope using linear regression
 * @param[in] hheater Pointer to heater handle
 * @return Temperature slope in degrees C per second
 *
 * Uses the least squares method to fit a line to the temperature samples:
 * slope = sum((t - mean_t) * (T - mean_T)) / sum((t - mean_t)^2)
 *
 * Where t is time and T is temperature.
 */
float32_t heater_calculate_slope(Heater_HandleTypeDef_t* hheater) {

    float32_t* temp = hheater->temperature;
    float32_t mean_temp = 0;
    uint32_t total_intervals = PID_CALC_INTERVAL_SECONDS / TEMPERATURE_SAMPLING_INTERVAL_SECONDS;
    float32_t mean_time = ((total_intervals + 1.0f) * TEMPERATURE_SAMPLING_INTERVAL_SECONDS) / 2.0f;

    arm_mean_f32(temp, total_intervals, &mean_temp);

    /* Calculate numerator and denominator of slope formula */
    float32_t numerator = 0;
    float32_t denominator = 0;
    for (uint32_t i = 0; i < total_intervals; i++) {
        numerator += (i * TEMPERATURE_SAMPLING_INTERVAL_SECONDS - mean_time) * (temp[i] - mean_temp);
        denominator += (i * TEMPERATURE_SAMPLING_INTERVAL_SECONDS - mean_time)
                * (i * TEMPERATURE_SAMPLING_INTERVAL_SECONDS - mean_time);
    }
    float32_t slope = numerator / denominator;

    return slope;
}

/**
 * @brief Calculate mean temperature from sample buffer
 * @param[in] hheater Pointer to heater handle
 * @return Mean temperature in degrees C
 *
 * Uses CMSIS-DSP arm_mean_f32 for efficient calculation.
 */
float32_t heater_calculate_mean(Heater_HandleTypeDef_t* hheater) {

    float32_t* temp = hheater->temperature;
    float32_t mean_temp = 0;

    uint32_t total_intervals = PID_CALC_INTERVAL_SECONDS / TEMPERATURE_SAMPLING_INTERVAL_SECONDS;

    arm_mean_f32(temp, total_intervals, &mean_temp);

    return mean_temp;
}

/**
 * @brief Clear temperature sample buffer
 * @param[in,out] hheater Pointer to heater handle
 *
 * Sets all elements in the temperature array to zero.
 */
void heater_set_temperature_zero(Heater_HandleTypeDef_t* hheater)
{
    uint8_t length = PID_CALC_INTERVAL_SECONDS / INTERUPT_INTERVAL_SECONDS;
    for(uint8_t i = 0; i < length; i++)
    {
        hheater->temperature[i] = 0;
    }
}

/**
 * @brief Print temperature reading with RTC timestamp
 * @param[in] hrtc Pointer to RTC handle
 * @param[in] temperature Temperature value to print
 *
 * Output format: HH:MM:SS,temperature
 * Used for logging temperature data to UART.
 */
void heater_print_test(RTC_HandleTypeDef *hrtc, float32_t temperature)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    HAL_RTC_GetTime(hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(hrtc, &sDate, RTC_FORMAT_BIN);

    printf("%02d:%02d:%02d,%.2f\r\n", sTime.Hours, sTime.Minutes, sTime.Seconds, temperature);
}

/**
 * @brief RTC interrupt handler for temperature sampling and gradient control
 * @param[in,out] hheater Pointer to heater handle
 * @param[in] hrtc Pointer to RTC handle
 *
 * Called from RTC alarm interrupt every INTERUPT_INTERVAL_SECONDS.
 *
 * Operation:
 * 1. Increment time counter
 * 2. At TEMPERATURE_SAMPLING_INTERVAL: read temperature
 *    - If gradient control enabled: update controller and set heater level
 *    - Store temperature in buffer for slope/mean calculation
 * 3. At PID_CALC_INTERVAL: calculate slope and mean for logging, then reset
 */
void heater_on_interupt(Heater_HandleTypeDef_t* hheater, RTC_HandleTypeDef *hrtc)
{
    hheater->time_counter++;

    /* Check if temperature sampling interval elapsed */
    if(TEMPERATURE_SAMPLING_INTERVAL_SECONDS / INTERUPT_INTERVAL_SECONDS <= hheater->time_counter)
    {
        /* Read temperature from MAX31855 */
        max31855_read_data(hheater->htemp);
        float32_t temp_float = max31855_get_temp_f32(hheater->htemp);
        hheater->temperature[hheater->time_counter - 1] = temp_float;
        heater_print_test(hrtc, temp_float);

        /* Gradient control: update controller and set heater level */
        if (hheater->gradient_control_enabled && hheater->hgc != NULL)
        {
            /* Convert float temperature to milli-degrees (single float operation) */
            int16_t T_mdeg = (int16_t)(temp_float * 1000.0f);

            /* Update gradient controller - returns output in Q16.16 [0, 65536] */
            q16_t u = GradientController_Update(hheater->hgc, T_mdeg);

            /* Convert controller output to heater level (0-6) */
            uint8_t level = GradientController_GetHeaterLevel(u);

            /* Apply heater level and update coil states */
            heater_set_level(hheater, level);
            heater_set_state(hheater);

#ifdef HEATER_ENABLE_LOG
            /* Log gradient control status */
            printf("GC: T=%d mdeg, u=%ld, level=%d\r\n",
                   (int)T_mdeg, (long)u, (int)level);
#endif
        }
    }

    /* Check if PID calculation interval elapsed (for logging slope/mean) */
    if(PID_CALC_INTERVAL_SECONDS / INTERUPT_INTERVAL_SECONDS <= hheater->time_counter)
    {
        float32_t slope = heater_calculate_slope(hheater);
        float32_t mean = heater_calculate_mean(hheater);

#ifdef HEATER_ENABLE_LOG
        /* Log slope (°C/h) and mean temperature */
        printf("slope: %f, mean: %f\r\n", slope * 3600, mean);
#endif

        heater_set_temperature_zero(hheater);
        hheater->time_counter = 0;
        return;
    }
}
