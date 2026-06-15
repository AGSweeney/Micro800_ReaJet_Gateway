################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/mapping/mapping_runtime.cpp 

CPP_DEPS += \
./src/mapping/mapping_runtime.d 

OBJS += \
./src/mapping/mapping_runtime.o 


# Each subdirectory must supply rules for building sources it contributes
src/mapping/%.o: ../src/mapping/%.cpp src/mapping/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GNU C++ Compiler'
	m68k-unknown-elf-g++ -std=gnu++17 -I"D:\Micro800_ReaJet_Gateway\BurnerGateway\src" -IC:/nburn/nbrtos/include -IC:/nburn/platform/MOD5441X/include -IC:/nburn/arch/coldfire/include -IC:/nburn/arch/coldfire/cpu/MCF5441X/include -IC:/nburn/libraries/include -O2 -Wall -c -fmessage-length=0 -fdata-sections -ffunction-sections -gdwarf-2 -fno-exceptions -fno-rtti -Wno-write-strings -fno-omit-frame-pointer -falign-functions=4 -fasynchronous-unwind-tables -mcpu=54415 -DMOD5441X -DMCF5441X -DCOLDFIRE -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-src-2f-mapping

clean-src-2f-mapping:
	-$(RM) ./src/mapping/mapping_runtime.d ./src/mapping/mapping_runtime.o

.PHONY: clean-src-2f-mapping

