/*
 * heater.c
 *
 *  Created on: Apr 5, 2024
 *      Author: Dennis Rathgeb
 */

#include "heater.h"

/*
 * resets all params but the coils pin/ports to default state
 * door  open
 * heater is off
 * prev heater level 0xff (none)
 * coils off
 * pwm last = 0
 */
static void heater_set_default_params(Heater_HandleTypeDef_t* hheater)
{
    hheater->heater_level = 0;
    hheater->heater_level_prev = 0xff;

    //TODO set 1!!!
    hheater->flag_door_open = 0;

    hheater->coils.coil1.state =  COIL_OFF;
    hheater->coils.coil1.time_pwm_last = 0;

    hheater->coils.coil2.state =  COIL_OFF;
    hheater->coils.coil2.time_pwm_last = 0;

    hheater->coils.coil3.state =  COIL_OFF;
    hheater->coils.coil3.time_pwm_last = 0;
}
/*
 * init function of heater instance. sets all ports and pins plus default values
 */
HAL_StatusTypeDef initHeater(Heater_HandleTypeDef_t* hheater,MAX31855_HandleTypeDef_t* htemp, GPIO_TypeDef* coil1_port, uint16_t coil1_pin,
        GPIO_TypeDef* coil2_port, uint16_t coil2_pin, GPIO_TypeDef* coil3_port, uint16_t coil3_pin)
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

    return HAL_OK;

}


/*
 * LL set coil On
 */
static void heater_set_coil_on(heater_coil_t* coil)
{
    HAL_GPIO_WritePin(coil->port, coil->pin, GPIO_PIN_SET);

}

/*
 * LL set coil Off
 */
static void heater_set_coil_off(heater_coil_t* coil)
{
    HAL_GPIO_WritePin(coil->port, coil->pin, GPIO_PIN_RESET);
}

/*
 * LL toggle coil
 */
static void heater_toggle_coil(heater_coil_t* coil)
{
    HAL_GPIO_TogglePin(coil->port, coil->pin);
}
/*
 * pwm control of a coil
 */
static void heater_update_pwm_coil(heater_coil_t* coil)
{
    //coil wasnt in pwm mode allready, set last tick
    if(0 == coil->time_pwm_last){
        coil->time_pwm_last = HAL_GetTick();
    }
    //coil was in pwm mode allready, toggle
    else{
        uint32_t time = HAL_GetTick();
        if(time >= (PWM_ON_SECONDS + coil->time_pwm_last))
        {
            coil->time_pwm_last = time;
            heater_toggle_coil(coil);
        }
    }
}

/*
 * sets state of individual heater coil according to params stored in instance
 */
//TODO Implement using RTC because uint32_t will overflow at some point
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

/*
 * checks if door flag was set and turns heater of
 */
static HAL_StatusTypeDef heater_check_door_state(Heater_HandleTypeDef_t* hheater)
{
    if(NULL == hheater){
        return HAL_ERROR;
    }

    //pause heating , door is open
    if(hheater->flag_door_open)
    {
        //check if previous level was allredy set
        if(0xff == hheater->heater_level_prev)
        {
            hheater->heater_level_prev = hheater->heater_level;
        }

        //check if allready in level 0 (off)
        if(0 != hheater->heater_level)
        {
            //set level to 0 (off)
            if(HAL_OK != heater_set_level(hheater, 0))
                    {
                       return HAL_ERROR;
                    }
        }


    }

    // door closed
    if(!hheater->flag_door_open)
    {

        //check if we need to resume old heater level
        if(0xff != hheater->heater_level_prev)
        {
            if(HAL_OK != heater_set_level(hheater,hheater->heater_level_prev))
            {
                return HAL_ERROR;
            }
            hheater->heater_level_prev = 0xff;
        }

    }
    return HAL_OK;
}

/*
 * Turn heater off
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

/*
 * HL set the heater level from 1-6
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


/*
 * sets state of heater according to params stored in instance
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
/*
 * calculates slope from temperature array of Heater_HandleTypeDef struct
 */
float32_t heater_calculate_slope(Heater_HandleTypeDef_t* hheater) {

    float32_t* temp = hheater->temperature;
    float32_t mean_temp = 0;
    //float32_t covariance = 0;
    //float32_t variance_time = 0;
    //float32_t variance_temp = 0;
    uint32_t total_intervals = PID_CALC_INTERVAL_SECONDS / TEMPERATURE_SAMPLING_INTERVAL_SECONDS;
    float32_t mean_time = ((total_intervals + 1.0f) * TEMPERATURE_SAMPLING_INTERVAL_SECONDS) / 2.0f;

    arm_mean_f32(temp, total_intervals, &mean_temp);
    // Calculate the numerator and denominator of the gradient formula
    float32_t numerator = 0;
    float32_t denominator = 0;
    for (uint32_t i = 0; i < total_intervals; i++) {
        numerator += (i * TEMPERATURE_SAMPLING_INTERVAL_SECONDS - mean_time) * (temp[i] - mean_temp);
        denominator += (i * TEMPERATURE_SAMPLING_INTERVAL_SECONDS - mean_time)
                * (i * TEMPERATURE_SAMPLING_INTERVAL_SECONDS - mean_time);
    }
    float32_t slope = numerator / denominator;
//    //variance of time
//    for (uint8_t i = 1; i <= total_intervals; i++) {
//        float32_t xi = i * TEMPERATURE_SAMPLING_INTERVAL_SECONDS;
//        variance_time += (xi - mean_time) * (xi - mean_time);
//    }
//    variance_time = variance_time / (total_intervals - 1);
//    // Calculate the mean of temp using CMSIS DSP function
//    arm_mean_f32(temp, total_intervals, &mean_temp);
//
//    // Calculate the variance of x and y using CMSIS DSP function
//    arm_var_f32(temp, total_intervals, &variance_temp);
//
//    // Calculate covariance using the formula: Cov(X, Y) = Var(X) + Var(Y) - 2 * mean(X) * mean(Y)
//    covariance = variance_time + variance_temp - (2 * mean_time * mean_temp);
//
//    // Calculate the slope (m)
//    float slope = covariance / variance_time;

    return slope;
}
/*
 * calculates slope from temperature array of Heater_HandleTypeDef struct
 */
float32_t heater_calculate_mean(Heater_HandleTypeDef_t* hheater) {

    float32_t* temp = hheater->temperature;
    float32_t mean_temp = 0;

    uint32_t total_intervals = PID_CALC_INTERVAL_SECONDS / TEMPERATURE_SAMPLING_INTERVAL_SECONDS;

    arm_mean_f32(temp, total_intervals, &mean_temp);

    return mean_temp;
}
/*
 * sets all elements of temperature array in heater_HandleTypeDef struct to zero
 */
void heater_set_temperature_zero(Heater_HandleTypeDef_t* hheater)
{

    uint8_t length = PID_CALC_INTERVAL_SECONDS / INTERUPT_INTERVAL_SECONDS;
    for(uint8_t i = 0; i < length; i++)
    {
        hheater->temperature[i] = 0;
    }
}

void heater_print_test(RTC_HandleTypeDef *hrtc, float32_t temperature)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    HAL_RTC_GetTime(hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(hrtc, &sDate, RTC_FORMAT_BIN);

    printf("%02d:%02d:%02d,%.2f\r\n", sTime.Hours, sTime.Minutes, sTime.Seconds,temperature);

}
void heater_on_interupt(Heater_HandleTypeDef_t* hheater,RTC_HandleTypeDef *hrtc)
{
    hheater->time_counter++;
    //printf("counter: %u \r\n", hheater->time_counter);
    //check if interval for sampling temperature has passed
    if(TEMPERATURE_SAMPLING_INTERVAL_SECONDS / INTERUPT_INTERVAL_SECONDS  <= hheater->time_counter)
        {
            max31855_read_data(hheater->htemp);
            hheater->temperature[hheater->time_counter - 1] = max31855_get_temp_f32(hheater->htemp);
            heater_print_test(hrtc,hheater->temperature[hheater->time_counter - 1]);
        }
    //check if intervall for pid is met
    if(PID_CALC_INTERVAL_SECONDS / INTERUPT_INTERVAL_SECONDS  <= hheater->time_counter)
    {
        //TODO
        float32_t slope = heater_calculate_slope(hheater);
        float32_t mean = heater_calculate_mean(hheater);
        //TODO pid calculation

        printf("slope: %f, mean: %f\r\n",slope * 3600,mean);
        heater_set_temperature_zero(hheater);
        hheater->time_counter = 0;
        return;
    }


}

