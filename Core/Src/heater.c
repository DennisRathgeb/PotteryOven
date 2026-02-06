/**
 * @file heater.c
 * @brief Implementation of heater control for kiln heating coils
 * @author Dennis Rathgeb
 * @date Apr 5, 2024
 *
 * Implements SSR windowing (time-proportioning control) for continuous
 * duty cycle output. The controller runs every 1 second, updating the
 * gradient estimate and PI controller. The SSR window manages 20-second
 * ON/OFF timing with a 5-second minimum switch time to avoid relay wear.
 *
 * Door safety is integrated into SSR window update - opening the door
 * forces all coils OFF immediately.
 */

#include "heater.h"
#include "ui.h"

/*============================================================================*/
/* SSR Window Functions                                                        */
/*============================================================================*/

/**
 * @brief Initialize SSR window state
 * @param[out] ssr Pointer to SSR window handle
 */
static void ssr_window_init(SSRWindow_HandleTypeDef_t *ssr)
{
    ssr->window_start_tick = 0;
    ssr->ton_ms = 0;
    ssr->ssr_on = 0;
    ssr->duty_current = Q16_ZERO;
}

/**
 * @brief Clamp duty cycle to avoid tiny ON/OFF blips
 * @param[in] u Raw duty cycle as Q16.16 [0, 65536]
 * @return Clamped duty: 0 if u < 0.25, 65536 if u > 0.75, else u
 *
 * With Tw=20s and Tmin=5s:
 * - u < 0.25 would give ton < 5s → clamp to OFF
 * - u > 0.75 would give toff < 5s → clamp to ON
 */
static q16_t ssr_clamp_duty(q16_t u)
{
    if (u < SSR_DUTY_MIN_Q16) {
        return Q16_ZERO;
    }
    if (u > SSR_DUTY_MAX_Q16) {
        return Q16_ONE;
    }
    return u;
}

/**
 * @brief Update SSR window state and set GPIO
 * @param[in,out] hheater Pointer to heater handle
 * @param[in] u_raw Raw duty cycle from controller [0, 65536]
 * @return 1 if new window started (for logging), 0 otherwise
 *
 * Called every control update (1s). Manages 20s window timing.
 */
static uint8_t ssr_window_update(Heater_HandleTypeDef_t *hheater, q16_t u_raw)
{
    SSRWindow_HandleTypeDef_t *ssr = &hheater->ssr;
    uint32_t now = HAL_GetTick();
    uint32_t Tw_ms = SSR_WINDOW_SECONDS * 1000;
    uint8_t new_window = 0;

    /* Check if new window should start */
    if (ssr->window_start_tick == 0 ||
        (now - ssr->window_start_tick) >= Tw_ms) {

        /* Start new window */
        ssr->window_start_tick = now;
        new_window = 1;

        /* Clamp duty to avoid tiny pulses */
        q16_t u = ssr_clamp_duty(u_raw);
        ssr->duty_current = u;

        /* Calculate ON time: ton = u * Tw */
        /* u is Q16.16 [0, 65536], Tw_ms = 20000 */
        /* ton_ms = (u * Tw_ms) >> 16 */
        ssr->ton_ms = (uint32_t)(((uint64_t)u * Tw_ms) >> 16);

        /* Start with SSR ON (if any duty) */
        ssr->ssr_on = (ssr->ton_ms > 0) ? 1 : 0;
    }

    /* Within window: determine ON or OFF based on elapsed time */
    uint32_t elapsed = now - ssr->window_start_tick;

    if (elapsed < ssr->ton_ms) {
        ssr->ssr_on = 1;
    } else {
        ssr->ssr_on = 0;
    }

    /* Door safety: force off if door is open */
    if (hheater->flag_door_open) {
        ssr->ssr_on = 0;
    }

    /* Apply to all coils (single SSR control) */
    GPIO_PinState state = ssr->ssr_on ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(hheater->coils.coil1.port, hheater->coils.coil1.pin, state);
    HAL_GPIO_WritePin(hheater->coils.coil2.port, hheater->coils.coil2.pin, state);
    HAL_GPIO_WritePin(hheater->coils.coil3.port, hheater->coils.coil3.pin, state);

    return new_window;
}

/**
 * @brief Force all coils off immediately
 * @param[in] hheater Pointer to heater handle
 */
static void heater_force_all_off(Heater_HandleTypeDef_t *hheater)
{
    HAL_GPIO_WritePin(hheater->coils.coil1.port, hheater->coils.coil1.pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(hheater->coils.coil2.port, hheater->coils.coil2.pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(hheater->coils.coil3.port, hheater->coils.coil3.pin, GPIO_PIN_RESET);
}

/*============================================================================*/
/* Heater Initialization and Default Parameters                                */
/*============================================================================*/

/**
 * @brief Reset heater parameters to default state
 * @param[in,out] hheater Pointer to heater handle
 *
 * Sets:
 * - flag_door_open to 0 (closed)
 * - SSR window state to initial values
 * - gradient_control_enabled to 0 (disabled by default)
 */
static void heater_set_default_params(Heater_HandleTypeDef_t* hheater)
{
    /* TODO: Set to 1 for production (door starts open) */
    hheater->flag_door_open = 0;

    /* Initialize SSR window state */
    ssr_window_init(&hheater->ssr);

    /* Gradient control disabled by default - set hgc/htc/hcb and enable flag to use */
    hheater->gradient_control_enabled = 0;
    hheater->hgc = NULL;
    hheater->htc = NULL;
    hheater->hcb = NULL;
    hheater->control_mode = CTRL_MODE_HEAT;

    /* Program execution state - no program running */
    hheater->active_program = NULL;
    hheater->current_step = 0;
    hheater->target_temperature = 0;
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
    /* hgc and htc must be set separately to enable gradient control */

    /* Ensure all coils are off initially */
    heater_force_all_off(hheater);

    return HAL_OK;
}

/**
 * @brief Turn off all heaters and reset to default state
 * @param[in,out] hheater Pointer to heater handle
 * @return HAL_OK on success
 */
HAL_StatusTypeDef heater_turn_off(Heater_HandleTypeDef_t* hheater)
{
    if (hheater == NULL) {
        return HAL_ERROR;
    }

    /* Turn off SSR and reset window state */
    hheater->ssr.ssr_on = 0;
    hheater->ssr.ton_ms = 0;
    hheater->ssr.duty_current = Q16_ZERO;

    /* Force all coils off */
    heater_force_all_off(hheater);

    /* Reset to default parameters */
    heater_set_default_params(hheater);

    return HAL_OK;
}

/**
 * @brief RTC interrupt handler for temperature sampling and control
 * @param[in,out] hheater Pointer to heater handle
 * @param[in] hrtc Pointer to RTC handle
 *
 * Called from RTC alarm interrupt every INTERUPT_INTERVAL_SECONDS (1s).
 *
 * Operation:
 * 1. Read temperature from MAX31855
 * 2. Update gradient estimate (EMA-filtered)
 * 3. Run outer temperature loop (if enabled)
 * 4. Run inner gradient PI controller
 * 5. Update SSR window (manages 20s ON/OFF timing)
 * 6. Log at start of each new SSR window
 */
void heater_on_interupt(Heater_HandleTypeDef_t* hheater, RTC_HandleTypeDef *hrtc)
{
    (void)hrtc;  /* RTC handle not used with new logging */

    /* Read temperature from MAX31855 */
    max31855_read_data(hheater->htemp);
    float32_t temp_float = max31855_get_temp_f32(hheater->htemp);

    /* Convert float temperature to milli-degrees (int32_t for full range) */
    int32_t T_mdeg = (int32_t)(temp_float * 1000.0f);
    q16_t u = Q16_ZERO;
    q16_t g_f = Q16_ZERO;

    /* Gradient control: 3-mode control (heat / cool_passive / cool_brake) */
    if (hheater->gradient_control_enabled && hheater->hgc != NULL)
    {
        /* Check if we're in a cooling step */
        uint8_t is_cooling = 0;
        q16_t g_min = CB_DEFAULT_G_MIN;  /* Default cooling limit */

        if (hheater->htc != NULL && hheater->htc->enabled) {
            is_cooling = hheater->htc->is_cooling;
            if (is_cooling) {
                /* During cooling, g_max holds the magnitude; negate it for g_min */
                g_min = -hheater->htc->g_max;
            }
        }

        /*
         * Step 1: Run gradient estimator ONCE per sample
         * This updates T_prev and g_f_prev - must only be called once!
         */
        g_f = GradientController_EstimateGradient(hheater->hgc, T_mdeg);

        if (!is_cooling) {
            /* ===== HEATING MODE ===== */
            hheater->control_mode = CTRL_MODE_HEAT;

            /* Brake off */
            if (hheater->hcb != NULL) {
                CoolingBrake_Reset(hheater->hcb);
            }

            /* Outer loop: compute gradient setpoint from temperature error */
            q16_t g_sp = Q16_ZERO;
            if (hheater->htc != NULL && hheater->htc->enabled) {
                g_sp = TemperatureController_Update(hheater->htc, T_mdeg);
            }

            /* Set gradient setpoint and run inner PI */
            GradientController_SetSetpoint(hheater->hgc, g_sp);
            u = GradientController_RunPI(hheater->hgc);

        } else {
            /* ===== COOLING STEP ===== */
            /* g_min is the allowed cooling limit (negative) */

            /* Update brake controller */
            q16_t u_brake = Q16_ZERO;
            if (hheater->hcb != NULL) {
                CoolingBrake_SetLimit(hheater->hcb, g_min);
                u_brake = CoolingBrake_Update(hheater->hcb, g_f);
            }

            if (u_brake > 0) {
                /* BRAKE MODE - cooling too fast */
                hheater->control_mode = CTRL_MODE_COOL_BRAKE;
                u = u_brake;
            } else {
                /* PASSIVE COOLING MODE - within limits */
                hheater->control_mode = CTRL_MODE_COOL_PASSIVE;
                u = Q16_ZERO;
            }

            /* Freeze inner PI integrator to prevent windup */
            GradientController_FreezeIntegrator(hheater->hgc);
        }

        /* Check step completion via outer loop */
        if (hheater->active_program != NULL && hheater->htc != NULL) {
            if (TemperatureController_AtTarget(hheater->htc, T_mdeg)) {
                heater_advance_program_step(hheater);
            }
        }
    }

    /* Apply SSR windowing - manages 20s window timing */
    uint8_t new_window = ssr_window_update(hheater, u);

#ifdef HEATER_ENABLE_LOG
    /* Log at start of each new SSR window (every 20s) */
    if (new_window) {
        /* Convert gradient to °C/h for readability */
        float g_f_per_hour = Q16_TO_FLOAT(g_f) * 3600.0f;
        printf("T=%.1f g=%.1f°C/h duty=%.2f ssr=%d M=%d\r\n",
               temp_float, g_f_per_hour,
               Q16_TO_FLOAT(hheater->ssr.duty_current),
               hheater->ssr.ssr_on, hheater->control_mode);
    }
#endif
}

/*============================================================================*/
/* Program Execution Functions                                                 */
/*============================================================================*/

/**
 * @brief Convert gradient from degrees C/hour to Q16.16 degrees C/second
 * @param[in] gradient_per_hour Gradient magnitude in degrees C per hour
 * @param[in] is_negative 1 for negative (cooling), 0 for positive (heating)
 * @return Gradient in degrees C per second as Q16.16 fixed-point
 *
 * g [°C/s] = g [°C/h] / 3600
 * Q16.16: (gradient << 16) / 3600
 */
static q16_t heater_gradient_to_q16(uint16_t gradient_per_hour, uint8_t is_negative)
{
    /* Convert °C/h to °C/s as Q16.16 */
    q16_t result = ((int32_t)gradient_per_hour << 16) / 3600;
    return is_negative ? -result : result;
}

/**
 * @brief Advance to next step in program or complete program
 * @param[in,out] hheater Pointer to heater handle
 *
 * Called when current step target temperature is reached.
 * Advances to next step or stops program if all steps complete.
 */
static void heater_advance_program_step(Heater_HandleTypeDef_t* hheater)
{
    if (hheater == NULL || hheater->active_program == NULL) {
        return;
    }

    ui_program_t* program = (ui_program_t*)hheater->active_program;
    hheater->current_step++;

    /* Check if program is complete */
    if (hheater->current_step >= program->length) {
        heater_stop_program(hheater);
#ifdef HEATER_ENABLE_LOG
        printf("Program complete\r\n");
#endif
        return;
    }

    /* Set new target for next step via outer temperature loop */
    uint8_t step = hheater->current_step;
    hheater->target_temperature = program->temperature[step];

    /* Set outer loop target (temperature controller generates gradient setpoints) */
    if (hheater->htc != NULL) {
        int32_t T_set_mdeg = (int32_t)program->temperature[step] * 1000;
        q16_t g_max_q16 = heater_gradient_to_q16(program->gradient[step], 0);
        TemperatureController_SetTarget(hheater->htc, T_set_mdeg, g_max_q16,
                                         program->gradient_negative[step]);
    }

#ifdef HEATER_ENABLE_LOG
    printf("Step %d: gradient=%d%s, target=%d\r\n",
           step + 1,
           program->gradient[step],
           program->gradient_negative[step] ? " (cooling)" : "",
           hheater->target_temperature);
#endif
}

/**
 * @brief Start executing a firing program
 * @param[in,out] hheater Pointer to heater handle
 * @param[in] program Pointer to ui_program_t to execute
 * @return HAL_OK on success, HAL_ERROR if invalid parameters
 */
HAL_StatusTypeDef heater_start_program(Heater_HandleTypeDef_t* hheater, void* program)
{
    if (hheater == NULL || program == NULL || hheater->hgc == NULL) {
        return HAL_ERROR;
    }

    ui_program_t* prog = (ui_program_t*)program;
    if (prog->length == 0) {
        return HAL_ERROR;
    }

    hheater->active_program = program;
    hheater->current_step = 0;
    hheater->target_temperature = prog->temperature[0];

    /* Set outer loop target instead of direct gradient setting */
    if (hheater->htc != NULL) {
        int32_t T_set_mdeg = (int32_t)prog->temperature[0] * 1000;
        q16_t g_max_q16 = heater_gradient_to_q16(prog->gradient[0], 0);
        TemperatureController_Reset(hheater->htc);  /* Reset before setting target */
        TemperatureController_SetTarget(hheater->htc, T_set_mdeg, g_max_q16,
                                         prog->gradient_negative[0]);
        hheater->htc->enabled = 1;
    }

    /* Reset gradient controller state and enable gradient control */
    GradientController_Reset(hheater->hgc);
    hheater->gradient_control_enabled = 1;

#ifdef HEATER_ENABLE_LOG
    printf("Program started: %d steps\r\n", prog->length);
    printf("Step 1: gradient=%d%s, target=%d\r\n",
           prog->gradient[0],
           prog->gradient_negative[0] ? " (cooling)" : "",
           hheater->target_temperature);
#endif

    return HAL_OK;
}

/**
 * @brief Stop executing the current program
 * @param[in,out] hheater Pointer to heater handle
 * @return HAL_OK on success, HAL_ERROR if hheater is NULL
 */
HAL_StatusTypeDef heater_stop_program(Heater_HandleTypeDef_t* hheater)
{
    if (hheater == NULL) {
        return HAL_ERROR;
    }

    hheater->gradient_control_enabled = 0;
    hheater->active_program = NULL;
    hheater->current_step = 0;
    hheater->target_temperature = 0;

    /* Disable temperature controller (outer loop) */
    if (hheater->htc != NULL) {
        hheater->htc->enabled = 0;
    }

    /* Turn off SSR and reset window state */
    hheater->ssr.ssr_on = 0;
    hheater->ssr.ton_ms = 0;
    hheater->ssr.duty_current = Q16_ZERO;
    heater_force_all_off(hheater);

#ifdef HEATER_ENABLE_LOG
    printf("Program stopped\r\n");
#endif

    return HAL_OK;
}

/**
 * @brief Set temperature target for cascaded control
 * @param[in,out] hheater Pointer to heater handle
 * @param[in] T_set_celsius Temperature setpoint in degrees C
 * @param[in] g_max_per_hour Maximum gradient in degrees C per hour
 * @return HAL_OK on success, HAL_ERROR if invalid parameters
 */
HAL_StatusTypeDef heater_set_temperature_target(Heater_HandleTypeDef_t* hheater,
                                                 uint16_t T_set_celsius,
                                                 uint16_t g_max_per_hour)
{
    if (hheater == NULL || hheater->htc == NULL || hheater->hgc == NULL) {
        return HAL_ERROR;
    }

    int32_t T_set_mdeg = (int32_t)T_set_celsius * 1000;
    q16_t g_max_q16 = heater_gradient_to_q16(g_max_per_hour, 0);

    TemperatureController_SetTarget(hheater->htc, T_set_mdeg, g_max_q16, 0);
    hheater->htc->enabled = 1;
    hheater->target_temperature = T_set_celsius;

    GradientController_Reset(hheater->hgc);
    hheater->gradient_control_enabled = 1;

    return HAL_OK;
}
