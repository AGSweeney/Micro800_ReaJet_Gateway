################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/http/http_handlers_core.cpp \
../src/http/http_handlers_tags.cpp \
../src/http/http_register.cpp 

CPP_DEPS += \
./src/http/http_handlers_core.d \
./src/http/http_handlers_tags.d \
./src/http/http_register.d 

OBJS += \
./src/http/http_handlers_core.o \
./src/http/http_handlers_tags.o \
./src/http/http_register.o 


# Each subdirectory must supply rules for building sources it contributes
src/http/%.o: ../src/http/%.cpp src/http/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GNU C++ Compiler'
	m68k-unknown-elf-g++ -std=gnu++17 -I"D:\Micro800_ReaJet_Gateway\BurnerGateway\src" -IC:/nburn/nbrtos/include -IC:/nburn/platform/MOD5441X/include -IC:/nburn/arch/coldfire/include -IC:/nburn/arch/coldfire/cpu/MCF5441X/include -IC:/nburn/libraries/include -O2 -Wall -c -fmessage-length=0 -fdata-sections -ffunction-sections -gdwarf-2 -fno-exceptions -fno-rtti -Wno-write-strings -fno-omit-frame-pointer -falign-functions=4 -fasynchronous-unwind-tables -mcpu=54415 -DMOD5441X -DMCF5441X -DCOLDFIRE -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-src-2f-http

clean-src-2f-http:
	-$(RM) ./src/http/http_handlers_core.d ./src/http/http_handlers_core.o ./src/http/http_handlers_tags.d ./src/http/http_handlers_tags.o ./src/http/http_register.d ./src/http/http_register.o

.PHONY: clean-src-2f-http

