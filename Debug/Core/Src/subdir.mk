################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/MAX31855.c \
../Core/Src/encoder.c \
../Core/Src/event.c \
../Core/Src/heater.c \
../Core/Src/lcd1602_rgb.c \
../Core/Src/log.c \
../Core/Src/main.c \
../Core/Src/pid.c \
../Core/Src/stm32f0xx_hal_msp.c \
../Core/Src/stm32f0xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f0xx.c \
../Core/Src/ui.c 

OBJS += \
./Core/Src/MAX31855.o \
./Core/Src/encoder.o \
./Core/Src/event.o \
./Core/Src/heater.o \
./Core/Src/lcd1602_rgb.o \
./Core/Src/log.o \
./Core/Src/main.o \
./Core/Src/pid.o \
./Core/Src/stm32f0xx_hal_msp.o \
./Core/Src/stm32f0xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f0xx.o \
./Core/Src/ui.o 

C_DEPS += \
./Core/Src/MAX31855.d \
./Core/Src/encoder.d \
./Core/Src/event.d \
./Core/Src/heater.d \
./Core/Src/lcd1602_rgb.d \
./Core/Src/log.d \
./Core/Src/main.d \
./Core/Src/pid.d \
./Core/Src/stm32f0xx_hal_msp.d \
./Core/Src/stm32f0xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f0xx.d \
./Core/Src/ui.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DARM_MATH_CM0 -DUSE_HAL_DRIVER -DSTM32F030x8 -c -I../Core/Inc -I../Drivers/CMSIS/DSP/Include -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/MAX31855.cyclo ./Core/Src/MAX31855.d ./Core/Src/MAX31855.o ./Core/Src/MAX31855.su ./Core/Src/encoder.cyclo ./Core/Src/encoder.d ./Core/Src/encoder.o ./Core/Src/encoder.su ./Core/Src/event.cyclo ./Core/Src/event.d ./Core/Src/event.o ./Core/Src/event.su ./Core/Src/heater.cyclo ./Core/Src/heater.d ./Core/Src/heater.o ./Core/Src/heater.su ./Core/Src/lcd1602_rgb.cyclo ./Core/Src/lcd1602_rgb.d ./Core/Src/lcd1602_rgb.o ./Core/Src/lcd1602_rgb.su ./Core/Src/log.cyclo ./Core/Src/log.d ./Core/Src/log.o ./Core/Src/log.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/pid.cyclo ./Core/Src/pid.d ./Core/Src/pid.o ./Core/Src/pid.su ./Core/Src/stm32f0xx_hal_msp.cyclo ./Core/Src/stm32f0xx_hal_msp.d ./Core/Src/stm32f0xx_hal_msp.o ./Core/Src/stm32f0xx_hal_msp.su ./Core/Src/stm32f0xx_it.cyclo ./Core/Src/stm32f0xx_it.d ./Core/Src/stm32f0xx_it.o ./Core/Src/stm32f0xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f0xx.cyclo ./Core/Src/system_stm32f0xx.d ./Core/Src/system_stm32f0xx.o ./Core/Src/system_stm32f0xx.su ./Core/Src/ui.cyclo ./Core/Src/ui.d ./Core/Src/ui.o ./Core/Src/ui.su

.PHONY: clean-Core-2f-Src

