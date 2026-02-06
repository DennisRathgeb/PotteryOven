# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a **pottery kiln/oven controller** built on STM32F030C8 using STM32CubeIDE. The system controls heating coils via PID, reads thermocouple temperature via MAX31855, and provides a menu-driven UI via LCD and rotary encoder.

## Build Commands

This is an STM32CubeIDE project. Build via the IDE:
- **Import**: File > Import > Existing Projects into Workspace
- **Build**: Project > Build Project (or Ctrl+B)
- **Clean**: Project > Clean
- **Debug**: Run > Debug (requires ST-Link)

The project uses the HAL library and CMSIS-DSP (`arm_math.h` for `float32_t`, `arm_mean_f32`, etc.).

## Architecture

### Main Loop & Initialization (`main.c`)
Entry point initializes all peripherals and modules in order:
1. HAL and clocks
2. GPIO, I2C1, SPI2, USART1, RTC, TIM3
3. Application modules: log, MAX31855, heater, LCD, UI, encoder, event queue

The main loop is currently a test sequence. Real operation happens through interrupts.

### Interrupt-Driven Design
- **RTC Alarm A** (every 1 second): Triggers `heater_on_interupt()` for temperature sampling, gradient control, and SSR window update
- **TIM3 Input Capture**: Encoder rotation detection via `HAL_TIM_IC_CaptureCallback()`
- **GPIO EXTI**: Button presses (BUT1-5), encoder pins, door switch (SW_C) via `HAL_GPIO_EXTI_Callback()`

### Module Dependency Graph
```
main.c
├── heater.c ──► MAX31855.c (temperature reading via SPI)
│              └► pid.c (PID control logic)
├── ui.c ──────► lcd1602_rgb.c (16x2 RGB LCD via I2C)
│              └► event.c (event queue)
├── encoder.c ─► event.c
└── log.c (UART debug output, printf redirection)
```

### Key Data Flow

**Temperature Control Loop** (RTC interrupt driven, 1s interval):
```
RTC Alarm → heater_on_interupt() → MAX31855 SPI read
  └► GradientController_EstimateGradient() → EMA-filtered gradient
  └► TemperatureController_Update() → gradient setpoint (outer P loop)
  └► GradientController_RunPI() → duty cycle output (inner PI loop)
  └► ssr_window_update() → ON/OFF timing within 20s window → GPIO
```

**User Input Flow** (GPIO/TIM interrupt driven):
```
Button/Encoder → HAL_GPIO_EXTI_Callback() → event_enqueue() → ui_update() → LCD display
```

### Heater Control (`heater.c`)

Uses **SSR windowing** (time-proportioning control) for continuous duty cycle output:
- Controller outputs duty cycle [0, 1] every 1 second
- SSR window converts duty to ON/OFF timing within 20-second window
- Duty clamping: u < 0.25 → fully OFF, u > 0.75 → fully ON (avoids short pulses)
- Minimum switch time: 5 seconds (prevents relay wear)

**Control Modes:**
- `CTRL_MODE_HEAT`: PI gradient controller active
- `CTRL_MODE_COOL_PASSIVE`: Heater off, natural cooling within limits
- `CTRL_MODE_COOL_BRAKE`: Brake controller applies heat to slow cooling

Door safety: Opening door forces all coils OFF immediately via `flag_door_open`.

### UI State Machine (`ui.c`)

Menu structure using `ui_menupoint_t` enum:
```
PROGRAMS ←→ SETPOINT ←→ SETTINGS
    ↓                       ↓
PROGRAMS_OVERVIEW      SETTINGS_OVERVIEW
    ↓
PROGRAM_DETAILED / CREATE_PROGRAM
```

Navigation: BUT1/ENC_DOWN=left/down, BUT2/ENC_UP=right/up, BUT3=back, ENC_BUT=enter

Programs define firing profiles with gradient (°C/h) and target temperature sequences.

### Event Queue (`event.c`)

Linked-list FIFO queue for decoupling interrupt handlers from UI processing. Events: BUT1-4, ENC_BUT, ENC_UP, ENC_DOWN.

## Hardware Pin Mapping (from `main.h`)

| Function | Port | Pin |
|----------|------|-----|
| Heater Coil 1 | PA3 | SW1 |
| Heater Coil 2 | PA4 | SW2 |
| Heater Coil 3 | PA5 | SW3 |
| Door Switch | PB2 | SW_C |
| Encoder A | PB4 | ENC_A |
| Encoder B | PB3 | ENC_B |
| Encoder Button | PA15 | BUT5 |
| Buttons 1-4 | PF7,PF6,PB1,PB0 | BUT1-4 |
| SPI2 NSS | PB12 | MAX31855 CS |

## Peripheral Usage

- **I2C1**: LCD1602 RGB display (addr 0x7c LCD, 0xc0 RGB controller)
- **SPI2**: MAX31855 thermocouple-to-digital converter
- **USART1**: Debug logging @ 9600 baud
- **RTC**: Periodic alarm for temperature sampling
- **TIM3**: Encoder input capture

## Configuration Constants

In `heater.h`:
- `INTERUPT_INTERVAL_SECONDS`: 1s RTC alarm interval (sensor & control rate)
- `TEMPERATURE_SAMPLING_INTERVAL_SECONDS`: 1s
- `SSR_WINDOW_SECONDS`: 20s window period
- `SSR_MIN_SWITCH_SECONDS`: 5s minimum ON/OFF time
- `SSR_DUTY_MIN_Q16`: 16384 (0.25) - below this → fully OFF
- `SSR_DUTY_MAX_Q16`: 49152 (0.75) - above this → fully ON

In `pid.h`:
- `GC_DEFAULT_KC`: 100 (proportional gain)
- `GC_DEFAULT_TS_OVER_TI`: 1092 (Ts/Ti for 60s integral time)
- `GC_DEFAULT_ALPHA`: 55706 (0.85 EMA filter coefficient)

In `ui.h`:
- `MAX_TEMPERATURE`: 1300°C
- `MAX_GRADIENT`: 650°C/h
- `MAX_PROGRAMS`: 10, `MAX_PROGRAM_SEQ_LENGTH`: 10

## Debug Logging

Enable/disable per-module logging via defines:
- `HEATER_ENABLE_LOG` in heater.h
- `UI_ENABLE_LOG` in ui.h
- `EVENT_ENABLE_LOG` in event.h
- `LOG_LEVEL_THRESHOLD` in log.h (0=DEBUG, 3=ERROR only)

## Code Conventions

- All modules use HAL-style handle typedefs: `*_HandleTypeDef_t`
- Functions return `HAL_StatusTypeDef` (HAL_OK/HAL_ERROR)
- CubeMX-generated code between `/* USER CODE BEGIN */` and `/* USER CODE END */` markers
- printf output redirected to UART via `REROUTE_PRINTF` define
