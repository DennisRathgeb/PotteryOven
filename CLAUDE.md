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
- **RTC Alarm A** (every ~2 seconds): Triggers `heater_on_interupt()` for temperature sampling and PID calculations
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

**Temperature Control Loop** (RTC interrupt driven):
```
RTC Alarm → heater_on_interupt() → MAX31855 SPI read → store in temperature array
  └► Every PID_CALC_INTERVAL_SECONDS: calculate slope/mean → PID adjustment
```

**User Input Flow** (GPIO/TIM interrupt driven):
```
Button/Encoder → HAL_GPIO_EXTI_Callback() → event_enqueue() → ui_update() → LCD display
```

### Heater Control (`heater.c`)

Controls 3 heating coils with 7 power levels (0-6):
- Level 0: All OFF
- Level 1: Coil1 PWM
- Level 2: Coil1 ON
- Level 3: Coil1 ON, Coil2 PWM
- Level 4: Coil1+2 ON
- Level 5: Coil1+2 ON, Coil3 PWM
- Level 6: All ON

PWM is manual toggle with `PWM_ON_SECONDS` (2s) period.

Door safety: Opening door (SW_C) pauses heating; closing resumes previous level.

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
- `PWM_ON_SECONDS`: 2s PWM toggle period
- `INTERUPT_INTERVAL_SECONDS`: 1s RTC alarm interval
- `TEMPERATURE_SAMPLING_INTERVAL_SECONDS`: 1s
- `PID_CALC_INTERVAL_SECONDS`: 10s

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
