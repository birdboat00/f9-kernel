# Copyright (c) 2013 The F9 Microkernel Project. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# FPU compilation flags
CFLAGS_FPU-$(CONFIG_FPU) = -mfpu=fpv4-sp-d16 -mfloat-abi=hard

# CPU specific compilation flags
# All supported boards use Cortex-M4 (STM32F4, STM32F429, STM32L4)
CFLAGS_CPU = -mlittle-endian -mcpu=cortex-m4
CFLAGS_CPU += -mthumb -mthumb-interwork -Xassembler -mimplicit-it=thumb
CFLAGS_CPU += -mno-sched-prolog -mno-unaligned-access
CFLAGS_CPU += -Wdouble-promotion -fsingle-precision-constant
CFLAGS_CPU += $(CFLAGS_FPU-y)

# Prevent compiler from emitting FPU instructions in kernel code.
# GCC with -mfloat-abi=hard may use VFP for non-FP ops (e.g., vldr/vstr
# to zero a long long). This sets CONTROL.FPCA in handler mode, changing
# exception frames from 8 to 26 words and corrupting context switches.
# -mgeneral-regs-only does NOT change the ABI -- it only restricts which
# registers the compiler uses for code generation. Links fine with hard-float.
CFLAGS_KERNEL-$(CONFIG_FPU) = -mgeneral-regs-only
CFLAGS_KERNEL = $(CFLAGS_KERNEL-y)

platform-y = \
	bitops.o \
	debug_device.o \
	mpu.o \
	spinlock.o \
	irq.o \
	irq-latency.o

platform-$(CONFIG_DEBUG_DEV_UART) += debug_uart.o
platform-$(CONFIG_DEBUG_DEV_SEMIHOSTING) += debug_semihosting.o
platform-$(CONFIG_KDB_UART_INPUT) += kdb_uart.o
platform-$(CONFIG_DEBUG_DEV_RAM) += debug_ram.o
platform-$(CONFIG_STDIO_SEMIHOSTING) += semihosting.o

platform-KPROBES-$(CONFIG_KPROBES) = \
	kprobes-arch.o \
	breakpoint.o \
	breakpoint-hard.o \
	breakpoint-soft.o \
	hw_debug.o

platform-y += $(platform-KPROBES-y)

loader-platform-y = \
	irq.loader.o \
	debug_uart.loader.o \
	debug_device.loader.o
