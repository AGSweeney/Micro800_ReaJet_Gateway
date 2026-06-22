################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/enip/enip_runtime.cpp 

CPP_DEPS += \
./src/enip/enip_runtime.d 

OBJS += \
./src/enip/enip_runtime.o 


# Each subdirectory must supply rules for building sources it contributes
src/enip/%.o: ../src/enip/%.cpp src/enip/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GNU C++ Compiler'
	m68k-unknown-elf-g++ -std=gnu++17 -I"D:\Micro800_ReaJet_Gateway\BurnerGateway\src" -IC:/nburn/nbrtos/include -IC:/nburn/platform/NANO54415/include -IC:/nburn/arch/coldfire/include -IC:/nburn/arch/coldfire/cpu/MCF5441X/include -IC:/nburn/libraries/include -O2 -Wall -c -fmessage-length=0 -fdata-sections -ffunction-sections -gdwarf-2 -fno-exceptions -fno-rtti -Wno-write-strings -fno-omit-frame-pointer -falign-functions=4 -fasynchronous-unwind-tables -mcpu=54415 -DNANO54415 -DMCF5441X -DCOLDFIRE -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-src-2f-enip

clean-src-2f-enip:
	-$(RM) ./src/enip/enip_runtime.d ./src/enip/enip_runtime.o

.PHONY: clean-src-2f-enip

