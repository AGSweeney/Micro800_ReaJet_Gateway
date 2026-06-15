################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/reajet/reajet_runtime.cpp 

CPP_DEPS += \
./src/reajet/reajet_runtime.d 

OBJS += \
./src/reajet/reajet_runtime.o 


# Each subdirectory must supply rules for building sources it contributes
src/reajet/%.o: ../src/reajet/%.cpp src/reajet/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GNU C++ Compiler'
	m68k-unknown-elf-g++ -std=gnu++17 -I"D:\Micro800_ReaJet_Gateway\BurnerGateway\src" -IC:/nburn/nbrtos/include -IC:/nburn/platform/MOD5441X/include -IC:/nburn/arch/coldfire/include -IC:/nburn/arch/coldfire/cpu/MCF5441X/include -IC:/nburn/libraries/include -O2 -Wall -c -fmessage-length=0 -fdata-sections -ffunction-sections -gdwarf-2 -fno-exceptions -fno-rtti -Wno-write-strings -fno-omit-frame-pointer -falign-functions=4 -fasynchronous-unwind-tables -mcpu=54415 -DMOD5441X -DMCF5441X -DCOLDFIRE -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-src-2f-reajet

clean-src-2f-reajet:
	-$(RM) ./src/reajet/reajet_runtime.d ./src/reajet/reajet_runtime.o

.PHONY: clean-src-2f-reajet

