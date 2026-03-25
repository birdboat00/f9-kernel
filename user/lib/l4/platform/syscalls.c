/* Copyright (c) 2013 The F9 Microkernel Project. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ARM Cortex-M syscall implementations */

#include <l4/types.h>
#include <l4/utcb.h>
#include <platform/link.h>
#include <syscall.h>
#include __L4_INC_ARCH(syscalls.h)

__USER_TEXT
void *L4_KernelInterface(L4_Word_t *ApiVersion,
                         L4_Word_t *ApiFlags,
                         L4_Word_t *KernelId)
{
    return &kip_start;
}

__USER_TEXT
L4_ThreadId_t L4_ExchangeRegisters(L4_ThreadId_t dest,
                                   L4_Word_t control,
                                   L4_Word_t sp,
                                   L4_Word_t ip,
                                   L4_Word_t flags,
                                   L4_Word_t UserDefHandle,
                                   L4_ThreadId_t pager,
                                   L4_Word_t *old_control,
                                   L4_Word_t *old_sp,
                                   L4_Word_t *old_ip,
                                   L4_Word_t *old_flags,
                                   L4_Word_t *old_UserDefhandle,
                                   L4_ThreadId_t *old_pager)
{
    L4_ThreadId_t result = {0};
    /* FIXME: unimplemented */
    return result;
}

__USER_TEXT
L4_Word_t L4_ThreadControl(L4_ThreadId_t dest,
                           L4_ThreadId_t SpaceSpecifier,
                           L4_ThreadId_t Scheduler,
                           L4_ThreadId_t Pager,
                           void *UtcbLocation)
{
    register L4_Word_t r0 __asm__("r0") = dest.raw;
    register L4_Word_t r1 __asm__("r1") = SpaceSpecifier.raw;
    register L4_Word_t r2 __asm__("r2") = Scheduler.raw;
    register L4_Word_t r3 __asm__("r3") = Pager.raw;
    register L4_Word_t r4 __asm__("r4") = (L4_Word_t) UtcbLocation;

    __asm__ __volatile__("svc %[syscall_num]\n"
                         : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4)
                         : [syscall_num] "i"(SYS_THREAD_CONTROL)
                         : "memory", "r12");

    return r0;
}

__USER_TEXT
L4_Clock_t L4_SystemClock(void)
{
    register L4_Word_t r0 __asm__("r0");
    register L4_Word_t r1 __asm__("r1");

    __asm__ __volatile__("svc %[syscall_num]\n"
                         : "=r"(r0), "=r"(r1)
                         : [syscall_num] "i"(SYS_SYSTEM_CLOCK)
                         : "memory", "r2", "r3", "r12");

    L4_Clock_t result;
    result.raw = ((uint64_t) r1 << 32) | r0;
    return result;
}

__USER_TEXT
void L4_ThreadSwitch(L4_ThreadId_t dest) {}

__USER_TEXT
L4_Word_t L4_Schedule(L4_ThreadId_t dest,
                      L4_Word_t TimeControl,
                      L4_Word_t ProcessorControl,
                      L4_Word_t PrioControl,
                      L4_Word_t PreemptionControl,
                      L4_Word_t *old_TimeControl)
{
    register L4_Word_t r0 __asm__("r0") = dest.raw;
    register L4_Word_t r1 __asm__("r1") = TimeControl;
    register L4_Word_t r2 __asm__("r2") = ProcessorControl;
    register L4_Word_t r3 __asm__("r3") = PrioControl;
    register L4_Word_t r4 __asm__("r4") = PreemptionControl;
    register L4_Word_t r5 __asm__("r5") = (L4_Word_t) old_TimeControl;

    __asm__ __volatile__("svc %[syscall_num]\n"
                         : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4),
                           "+r"(r5)
                         : [syscall_num] "i"(SYS_SCHEDULE)
                         : "memory", "r12");

    /* Write back old_TimeControl if pointer provided */
    if (old_TimeControl)
        *old_TimeControl = r5;

    return r0;
}

__USER_TEXT
L4_Word_t L4_TimerNotify(L4_Word_t ticks,
                         L4_Word_t notify_bits,
                         L4_Word_t periodic)
{
    register L4_Word_t r0 __asm__("r0") = ticks;
    register L4_Word_t r1 __asm__("r1") = notify_bits;
    register L4_Word_t r2 __asm__("r2") = periodic;

    __asm__ __volatile__("svc %[syscall_num]\n"
                         : "+r"(r0), "+r"(r1), "+r"(r2)
                         : [syscall_num] "i"(SYS_TIMER_NOTIFY)
                         : "memory", "r3", "r12");

    return r0;
}

__USER_TEXT
__attribute__((naked)) L4_MsgTag_t L4_Ipc(L4_ThreadId_t to,
                                          L4_ThreadId_t FromSpecifier,
                                          L4_Word_t Timeouts,
                                          L4_ThreadId_t *from)
{
    /* Naked function - we manage the stack ourselves to prevent the compiler
     * from storing locals that would be overwritten by the SVC exception frame.
     *
     * Parameters in registers (ARM AAPCS):
     * R0 = to.raw
     * R1 = FromSpecifier.raw
     * R2 = Timeouts
     * R3 = from (pointer)
     *
     * Return value in R0 = MsgTag.raw
     *
     * CRITICAL: Must preserve callee-saved registers R4-R11 per AAPCS.
     */
    __asm__ __volatile__(
        /* Save callee-saved registers R4-R11 and R3 (from pointer), LR */
        "push {r3-r11, lr}\n"

        /* Load MR0-MR7 from UTCB into R4-R11 */
        "ldr r12, =current_utcb\n"
        "ldr r12, [r12]\n"    /* r12 = utcb */
        "add r12, r12, #48\n" /* r12 = &utcb->mr_low[0] */
        "ldmia r12, {r4-r11}\n"

        /* SVC syscall - exception frame saves R0-R3, R12, LR, PC, xPSR */
        /* Kernel preserves R4-R11 across syscall */
        "svc %[syscall_num]\n"

        /* Reload UTCB MR pointer and store received MRs.
         * LR was restored to caller's return address by the exception
         * frame -- it no longer holds the UTCB pointer.
         */
        "ldr r12, =current_utcb\n"
        "ldr r12, [r12]\n"
        "add r12, r12, #48\n" /* r12 = &utcb->mr_low[0] */
        "stmia r12, {r4-r11}\n"

        /* Restore callee-saved registers (including original R3 with 'from'
           pointer) */
        "pop {r3-r11, lr}\n"

        /* Load return value: MR0 = tag (utcb->mr_low[0]) */
        "ldr r12, =current_utcb\n"
        "ldr r12, [r12]\n"
        "ldr r1, [r12, #48]\n"

        /* Store 'from' result if pointer is non-NULL AND aligned */
        "cmp r3, #0\n"
        "beq 1f\n"       /* Skip if NULL */
        "tst r3, #3\n"   /* Check 4-byte alignment */
        "bne 1f\n"       /* Skip if misaligned */
        "str r0, [r3]\n" /* *from = R0 (from kernel) */
        "1:\n"

        /* Return tag in R0 */
        "mov r0, r1\n"
        "bx lr\n"
        :
        : [syscall_num] "i"(SYS_IPC)
        : "memory");
}


__USER_TEXT
L4_MsgTag_t L4_Lipc(L4_ThreadId_t to,
                    L4_ThreadId_t FromSpecifier,
                    L4_Word_t Timeouts,
                    L4_ThreadId_t *from)
{
    return L4_Ipc(to, FromSpecifier, Timeouts, from);
}

__USER_TEXT
void L4_Unmap(L4_Word_t control) {}

__USER_TEXT
L4_Word_t L4_SpaceControl(L4_ThreadId_t SpaceSpecifier,
                          L4_Word_t control,
                          L4_Fpage_t KernelInterfacePageArea,
                          L4_Fpage_t UtcbArea,
                          L4_ThreadId_t redirector,
                          L4_Word_t *old_control)
{
    L4_Word_t result = 0;
    return result;
}

__USER_TEXT
L4_Word_t L4_ProcessorControl(L4_Word_t ProcessorNo,
                              L4_Word_t InternalFrequency,
                              L4_Word_t ExternalFrequency,
                              L4_Word_t voltage)
{
    L4_Word_t result = 0;
    return result;
}

__USER_TEXT
L4_Word_t L4_MemoryControl(L4_Word_t control, const L4_Word_t *attributes)
{
    L4_Word_t result = 0;
    return result;
}

__USER_TEXT
L4_Word_t L4_NotifyWait(L4_Word_t mask)
{
    register L4_Word_t r0 __asm__("r0") = mask;

    __asm__ __volatile__("svc %[syscall_num]\n"
                         : "+r"(r0)
                         : [syscall_num] "i"(SYS_NOTIFY_WAIT)
                         : "memory", "r1", "r2", "r3", "r12");

    return r0;
}

__USER_TEXT
L4_Word_t L4_NotifyPost(L4_ThreadId_t target, L4_Word_t bits)
{
    register L4_Word_t r0 __asm__("r0") = target.raw;
    register L4_Word_t r1 __asm__("r1") = bits;

    __asm__ __volatile__("svc %[syscall_num]\n"
                         : "+r"(r0), "+r"(r1)
                         : [syscall_num] "i"(SYS_NOTIFY_POST)
                         : "memory", "r2", "r3", "r12");

    return r0;
}

__USER_TEXT
L4_Word_t L4_NotifyClear(L4_Word_t bits)
{
    register L4_Word_t r0 __asm__("r0") = bits;

    __asm__ __volatile__("svc %[syscall_num]\n"
                         : "+r"(r0)
                         : [syscall_num] "i"(SYS_NOTIFY_CLEAR)
                         : "memory", "r1", "r2", "r3", "r12");

    return r0;
}
