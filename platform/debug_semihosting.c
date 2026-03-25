/* Copyright (c) 2013 The F9 Microkernel Project. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <platform/debug_device.h>
#include <platform/debug_semihosting.h>
#include <types.h>

/* ARM Semihosting operation numbers */
#define SYS_WRITEC 0x03
#define SYS_WRITE0 0x04
#define SYS_READC 0x07

/* Debug state: async (buffered) or panic (synchronous) */
extern enum { DBG_ASYNC, DBG_PANIC } dbg_state;

static inline long semihosting_call(int reason, void *arg)
{
    register long r0 asm("r0") = reason;
    register void *r1 asm("r1") = arg;
    asm volatile("bkpt #0xAB" : "=r"(r0) : "0"(r0), "r"(r1) : "memory");
    return r0;
}

static uint8_t dbg_semihosting_getchar(void)
{
    /* Blocking read via semihosting */
    return (uint8_t) semihosting_call(SYS_READC, NULL);
}

static void dbg_semihosting_putchar(uint8_t c)
{
    semihosting_call(SYS_WRITEC, &c);
}

static void dbg_semihosting_start_panic(void)
{
    /* Set panic state to prevent interactive prompts in KDB */
    dbg_state = DBG_PANIC;
}

void dbg_semihosting_init(void)
{
    dbg_dev_t device = {
        .dev_id = DBG_DEV_SEMIHOSTING,
        .getchar = dbg_semihosting_getchar,
        .putchar = dbg_semihosting_putchar,
        .start_panic = dbg_semihosting_start_panic,
    };

    dbg_register_device(&device);
}
