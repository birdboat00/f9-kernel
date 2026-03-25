/* Copyright (c) 2013 The F9 Microkernel Project. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ARM Semihosting support for QEMU console I/O */

#include <softirq.h>
#include <types.h>

/* ARM Semihosting operation numbers */
#define SYS_WRITEC 0x03 /* Write one character to debug channel */
#define SYS_WRITE0 0x04 /* Write null-terminated string */
#define SYS_READ 0x06   /* Read from file/console */
#define SYS_READC 0x07  /* Read one character from debug channel */
#define SYS_ISTTY 0x09  /* Check if file descriptor is a TTY */

/* File handles for semihosting */
#define STDIN_FILENO 0

/*
 * Perform ARM semihosting call
 * For Cortex-M: Uses BKPT #0xAB instruction
 */
static inline long semihosting_call(int reason, void *arg)
{
    register long r0 asm("r0") = reason;
    register void *r1 asm("r1") = arg;

    asm volatile("bkpt #0xAB" : "=r"(r0) : "0"(r0), "r"(r1) : "memory");

    return r0;
}

/*
 * Write one character via semihosting
 */
void semihosting_putc(uint8_t c)
{
    semihosting_call(SYS_WRITEC, &c);
}

/*
 * Read one character via semihosting (blocking)
 */
uint8_t semihosting_getc(void)
{
    return (uint8_t) semihosting_call(SYS_READC, NULL);
}

/*
 * Write null-terminated string via semihosting
 */
void semihosting_puts(const char *str)
{
    semihosting_call(SYS_WRITE0, (void *) str);
}

#ifdef CONFIG_STDIO_SEMIHOSTING
/* Kernel-space __l4_putchar/__l4_getchar for kernel printf.
 * User-space printf() uses IPC to THREAD_LOG, not __l4_putchar.
 */
void __l4_putchar(uint8_t chr)
{
    semihosting_putc(chr);
}

uint8_t __l4_getchar(void)
{
    return semihosting_getc();
}
#endif /* CONFIG_STDIO_SEMIHOSTING */
