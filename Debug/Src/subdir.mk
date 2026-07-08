################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/Lora.c \
../Src/Uart.c \
../Src/delay.c \
../Src/main.c \
../Src/syscalls.c \
../Src/sysmem.c 

C_DEPS += \
./Src/Lora.d \
./Src/Uart.d \
./Src/delay.d \
./Src/main.d \
./Src/syscalls.d \
./Src/sysmem.d 

OBJS += \
./Src/Lora.o \
./Src/Uart.o \
./Src/delay.o \
./Src/main.o \
./Src/syscalls.o \
./Src/sysmem.o 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o Src/%.su Src/%.cyclo: ../Src/%.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DSTM32F401xE -DSTM32F401CEUx -DSTM32 -DSTM32F4 -c -I../Inc -I"D:/parvasense Mit app F411RE/CMSIS/Core/Include" -I"D:/parvasense Mit app F411RE/CMSIS/Device/ST/STM32F4xx/Include" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Src

clean-Src:
	-$(RM) ./Src/Lora.cyclo ./Src/Lora.d ./Src/Lora.o ./Src/Lora.su ./Src/Uart.cyclo ./Src/Uart.d ./Src/Uart.o ./Src/Uart.su ./Src/delay.cyclo ./Src/delay.d ./Src/delay.o ./Src/delay.su ./Src/main.cyclo ./Src/main.d ./Src/main.o ./Src/main.su ./Src/syscalls.cyclo ./Src/syscalls.d ./Src/syscalls.o ./Src/syscalls.su ./Src/sysmem.cyclo ./Src/sysmem.d ./Src/sysmem.o ./Src/sysmem.su

.PHONY: clean-Src

