# Pottery Kiln Controller

An STM32-based controller for ceramic kiln/oven temperature management with PID control, firing programs, and persistent settings.

> **Work in Progress**: This project is under active development. Build plans, hardware schematics, and PCB layouts will be added in the future. Pictures of the finished build will follow.

## About

This controller is designed for a **self-built pottery kiln**. It uses a custom PCB that integrates:
- User interface (buttons, rotary encoder, 16x2 LCD display)
- 3 SSRs (solid-state relays) for **3-phase operation** of the heating coils
- Thermocouple interface for temperature measurement

The kiln itself is a DIY build intended primarily for ceramic/pottery firing (bisque and glaze firings).

**Interested in the PCB layout or build details?** Feel free to reach out - contact info in my GitHub profile.

## Hardware

- **MCU**: STM32F030C8 (Cortex-M0, 64KB Flash, 8KB RAM)
- **Temperature Sensor**: MAX31855 thermocouple-to-digital converter (SPI)
- **Display**: 16x2 RGB LCD (I2C)
- **Input**: Rotary encoder + 4 buttons
- **Output**: 3 heating coils via SSR (solid-state relay)
- **Safety**: Door switch for automatic heater cutoff

## Features

- **Cascaded Control**: Outer temperature P-loop + inner gradient PI-loop
- **SSR Windowing**: Time-proportioning control with configurable window period
- **Firing Programs**: Up to 10 programs with 10 steps each (heating/cooling profiles)
- **Flash Persistence**: Settings and programs survive power cycles
- **Cooling Brake**: Automatic heat application to limit cooling rate
- **Menu-Driven UI**: Navigate programs and settings via LCD/encoder

## Project Structure

```
PotteryOven/
├── Core/
│   ├── Inc/                    # Header files
│   │   ├── flash_storage.h     # Flash read/write API
│   │   ├── settings.h          # Centralized settings
│   │   ├── programs.h          # Firing programs storage
│   │   ├── pid.h               # Gradient/temperature controllers
│   │   ├── heater.h            # Heater and SSR control
│   │   ├── ui.h                # User interface state machine
│   │   ├── MAX31855.h          # Temperature sensor driver
│   │   ├── lcd1602_rgb.h       # LCD driver
│   │   ├── encoder.h           # Rotary encoder driver
│   │   └── event.h             # Event queue
│   └── Src/                    # Source files
│       ├── main.c              # Entry point and initialization
│       ├── flash_storage.c     # Flash operations
│       ├── settings.c          # Settings load/save
│       ├── programs.c          # Programs load/save
│       ├── pid.c               # Controller implementations
│       ├── heater.c            # Heater control logic
│       ├── ui.c                # Menu system
│       └── ...
├── Drivers/                    # STM32 HAL and CMSIS
├── STM32F030C8TX_FLASH.ld      # Linker script (62KB code + 2KB storage)
└── Oven_PID_v1.ioc             # STM32CubeMX configuration
```

## Building

### STM32CubeIDE
1. File > Import > Existing Projects into Workspace
2. Select the project directory
3. Project > Build Project (Ctrl+B)

### CMake
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Flash Memory Layout

```
0x08000000 ┬─────────────────────┐
           │  Code + Constants   │  62KB
0x0800F800 ├─────────────────────┤
           │  Settings (1KB)     │  Controller parameters
0x0800FC00 ├─────────────────────┤
           │  Programs (1KB)     │  Firing profiles
0x08010000 └─────────────────────┘
```

## Control Architecture

```
                    ┌──────────────────┐
   T_setpoint ──────►  Temperature     │
                    │  Controller (P)  ├──── g_setpoint
                    └────────┬─────────┘         │
                             │                   ▼
                    ┌────────┴─────────┐  ┌──────────────┐
   T_measured ──────►  Gradient        │  │   Cooling    │
                    │  Estimator (EMA) ├──►   Brake (P)  │
                    └────────┬─────────┘  └──────┬───────┘
                             │                   │
                    ┌────────┴─────────┐         │
   g_filtered ──────►  Gradient        │◄────────┘
                    │  Controller (PI) │
                    └────────┬─────────┘
                             │
                    ┌────────┴─────────┐
   duty_cycle ──────►  SSR Windowing   ├──── Heater ON/OFF
                    └──────────────────┘
```

## Configuration

Settings are accessible via the LCD menu (Settings > Categories):

| Category | Parameters |
|----------|------------|
| Inner Loop | Kc (gain), Ti (integral), Taw (anti-windup), Alpha (EMA) |
| Outer Loop | Kp_T (gain), T_band (deadband) |
| Cooling Brake | g_min (limit), Hysteresis, Kb (gain), u_max (power) |
| SSR Timing | Window period, Min switch time |

Press BUT4 to apply and save settings to flash.

## Pin Mapping

| Function | Port | Pin |
|----------|------|-----|
| Heater Coil 1 | PA3 | SW1 |
| Heater Coil 2 | PA4 | SW2 |
| Heater Coil 3 | PA5 | SW3 |
| Door Switch | PB2 | SW_C |
| Encoder A | PB4 | ENC_A |
| Encoder B | PB3 | ENC_B |
| Encoder Button | PA15 | BUT5 |
| MAX31855 CS | PB12 | SPI2_NSS |

## License

This project uses STM32 HAL libraries licensed under BSD-3-Clause by STMicroelectronics.
