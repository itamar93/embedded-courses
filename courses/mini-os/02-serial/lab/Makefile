# Mini OS lab build: make | make qemu | make flash | make clean
APP ?= $(notdir $(wildcard apps/*))

TARGET_NAME = kernel
LINKER_SCRIPT = linker.ld
BUILD_DIR = build

SRCS  = $(wildcard platform/stm32f411/boot/*.c)
SRCS += $(wildcard hal/*/*.c)
SRCS += $(wildcard drivers/*/*.c)
SRCS += $(wildcard lib/src/*.c)
SRCS += apps/$(APP)/main.c

OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
QEMU ?= qemu-system-arm

INCLUDE_DIRS = -Iplatform/stm32f411/include -I. -Ihal -Idrivers -Ilib/inc

CFLAGS = -c -mcpu=cortex-m4 -mthumb -std=gnu11 -g -Wall -O0 $(INCLUDE_DIRS)
LDFLAGS = -nostdlib -T $(LINKER_SCRIPT) -Wl,-Map=$(BUILD_DIR)/$(TARGET_NAME).map

all: $(BUILD_DIR)/$(TARGET_NAME).elf $(BUILD_DIR)/$(TARGET_NAME).bin

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET_NAME).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@
	arm-none-eabi-size $@

$(BUILD_DIR)/$(TARGET_NAME).bin: $(BUILD_DIR)/$(TARGET_NAME).elf
	$(OBJCOPY) -O binary $< $@

# USART2 is QEMU's second serial port; pin changes go to stderr as JSON
qemu: all
	$(QEMU) -M blackpill-f411 -display none -monitor none \
		-chardev stdio,id=uart -serial null -serial chardev:uart \
		-chardev file,id=pins,path=/dev/stderr \
		-kernel $(BUILD_DIR)/$(TARGET_NAME).elf

# Hold BOOT0, plug in USB-C, release BOOT0: the STM32 enters its DFU bootloader
flash: all
	dfu-util -a 0 -s 0x08000000:leave -D $(BUILD_DIR)/$(TARGET_NAME).bin

# Or with an ST-LINK clone on the SWD header
flash-stlink: all
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program $(BUILD_DIR)/$(TARGET_NAME).elf verify reset exit"

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all qemu flash flash-stlink clean
