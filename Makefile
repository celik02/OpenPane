##############################################
# STM32F446RE — Bare-metal, CMSIS only
##############################################

TARGET   = dashboard
BUILD_DIR = build

DEBUG = 1
OPT   = -Og

######################################
# Sources
######################################
C_SOURCES = \
src/system_stm32f4xx.c

CXX_SOURCES = \
src/main.cpp \
src/rtos.cpp \
src/syscalls.cpp

ASM_SOURCES = startup/startup_stm32f446xx.s

######################################
# Toolchain
######################################
PREFIX = arm-none-eabi-
CC  = $(PREFIX)gcc
CXX = $(PREFIX)g++
AS  = $(PREFIX)gcc -x assembler-with-cpp
CP  = $(PREFIX)objcopy
SZ  = $(PREFIX)size
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

######################################
# Flags
######################################
CPU      = -mcpu=cortex-m4
FPU      = -mfpu=fpv4-sp-d16
FLOAT    = -mfloat-abi=hard
MCU      = $(CPU) -mthumb $(FPU) $(FLOAT)

C_DEFS   = -DSTM32F446xx

ROOT = $(abspath .)

C_INCLUDES = \
-I$(ROOT)/inc \
-I$(ROOT)/CMSIS/core \
-I$(ROOT)/CMSIS/device/Include

CFLAGS   = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections
CXXFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections \
           -std=c++17 -fno-exceptions -fno-rtti
ASFLAGS  = $(MCU) $(OPT) -Wall -fdata-sections -ffunction-sections

ifeq ($(DEBUG), 1)
CFLAGS   += -g -gdwarf-2
CXXFLAGS += -g -gdwarf-2
endif

CFLAGS   += -MMD -MP -MF"$(@:%.o=%.d)"
CXXFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

LDSCRIPT = linker/stm32f446re.ld
LDFLAGS  = $(MCU) -specs=nano.specs -T$(LDSCRIPT) -lc -lm -lnosys \
           -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

######################################
# Build
######################################
BEAR := $(shell command -v bear 2>/dev/null)

ifdef BEAR
ifndef BEAR_WRAP
.DEFAULT_GOAL := _bear_wrap
.PHONY: _bear_wrap
_bear_wrap:
	$(BEAR) -- $(MAKE) BEAR_WRAP=1 all
endif
endif

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

OBJECTS  = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(CXX_SOURCES:.cpp=.o)))
vpath %.cpp $(sort $(dir $(CXX_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.cpp Makefile | $(BUILD_DIR)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	-rm -fR $(BUILD_DIR)

flash: $(BUILD_DIR)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
	        -c "program $(BUILD_DIR)/$(TARGET).elf verify reset exit"

-include $(wildcard $(BUILD_DIR)/*.d)