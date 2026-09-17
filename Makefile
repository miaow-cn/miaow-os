# SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
BUILD ?= build/os
CFLAGS := -std=c23 -O2 -g -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -mgeneral-regs-only -mno-outline-atomics -mcpu=cortex-a710 -Iinclude -Ikernel
QEMU := qemu-system-aarch64
QEMU_FLAGS := -machine virt-10.1,gic-version=3,virtualization=off,secure=off,its=off -cpu cortex-a710 -smp 1 -m 128M -display none -serial stdio -monitor none -no-reboot
KERNEL_OBJECTS := $(addprefix $(BUILD)/kernel/,boot.o vectors.o main.o uart.o task.o timer.o payloads.o)
APP0 ?= apps/sequence.c
APP1 ?= apps/primes.c
APP2 ?= apps/checksum.c
CFLAGS += $(EXTRA_CFLAGS)

.PHONY: all run
all: $(BUILD)/kernel.bin

$(BUILD)/apps/linker.ld: apps/linker.ld.S include/abi.h
	@mkdir -p $(@D)
	$(CC) -E -P -x c -Iinclude $< -o $@

$(BUILD)/apps/start.o: apps/start.S include/abi.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

define app_rules
$(BUILD)/apps/$(1).o: $$(APP$(1)) apps/app.h include/abi.h
	@mkdir -p $$(@D)
	$(CC) $(CFLAGS) -Iapps -c $$< -o $$@
$(BUILD)/apps/$(1).elf: $(BUILD)/apps/$(1).o $(BUILD)/apps/start.o $(BUILD)/apps/linker.ld
	$(LD) --build-id=none --orphan-handling=error --defsym=APP_INDEX=$(1) -T $(BUILD)/apps/linker.ld $(BUILD)/apps/start.o $$< -o $$@
$(BUILD)/apps/$(1).bin: $(BUILD)/apps/$(1).elf
	$(OBJCOPY) -O binary $$< $$@
endef
$(eval $(call app_rules,0))
$(eval $(call app_rules,1))
$(eval $(call app_rules,2))

$(BUILD)/kernel/payloads.o: kernel/payloads.S $(BUILD)/apps/0.bin $(BUILD)/apps/1.bin $(BUILD)/apps/2.bin
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -DAPP0_BIN='"$(BUILD)/apps/0.bin"' -DAPP1_BIN='"$(BUILD)/apps/1.bin"' -DAPP2_BIN='"$(BUILD)/apps/2.bin"' -c $< -o $@

$(BUILD)/kernel/%.o: kernel/%.c kernel/kernel.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/kernel/%.o: kernel/%.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/kernel.elf: $(KERNEL_OBJECTS) kernel/linker.ld
	$(LD) --build-id=none -T kernel/linker.ld $(KERNEL_OBJECTS) -o $@

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

run: all
	$(QEMU) $(QEMU_FLAGS) -kernel $(BUILD)/kernel.bin

-include $(KERNEL_OBJECTS:.o=.d)