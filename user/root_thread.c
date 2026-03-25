/* Copyright (c) 2013 The F9 Microkernel Project. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <l4/ipc.h>
#include <l4/kip.h>
#include <l4/thread.h>
#include <l4/utcb.h>
#include <l4io.h>
#include <platform/link.h>
#include <types.h>
#include <user_runtime.h>

extern user_struct user_runtime_start[];
extern user_struct user_runtime_end[];

int __USER_TEXT L4_Map(L4_ThreadId_t where, memptr_t base, size_t size)
{
    volatile L4_Word_t where_raw = where.raw;
    volatile L4_Word_t base_saved = base;
    volatile L4_Word_t size_saved = size;

    L4_Msg_t msg;
    volatile L4_Word_t page[2] = {(base_saved & 0xFFFFFFC0) | 0x0A,
                                  size_saved & 0xFFFFFFC0};

    L4_MsgPut(&msg, 0, 0, NULL, 2, (L4_Word_t *) page);
    L4_MsgLoad(&msg);

    L4_ThreadId_t where_reload = {.raw = where_raw};
    L4_Send(where_reload);
    return 0;
}

memptr_t __USER_TEXT get_free_base(kip_t *kip_ptr)
{
    kip_mem_desc_t *desc =
        ((void *) kip_ptr) + kip_ptr->memory_info.s.memory_desc_ptr;
    int n = kip_ptr->memory_info.s.n;
    int i = 0;

    for (i = 0; i < n; ++i) {
        if ((desc[i].size & 0x3F) == 4)
            return desc[i].base & 0xFFFFFFC0;
    }

    return 0;
}

void __USER_TEXT map_user_sections(kip_t *kip_ptr, L4_ThreadId_t tid)
{
    (void) kip_ptr;
    volatile L4_Word_t tid_raw = tid.raw;

    volatile L4_Word_t utext_base = (L4_Word_t) &user_text_start;
    volatile L4_Word_t utext_size = (L4_Word_t) &user_text_end - utext_base;
    if (utext_size > 0) {
        L4_ThreadId_t tid_reload = {.raw = tid_raw};
        L4_Map(tid_reload, utext_base, utext_size);
    }

    volatile L4_Word_t udata_base = (L4_Word_t) &user_data_start;
    volatile L4_Word_t udata_size = (L4_Word_t) &user_data_end - udata_base;
    if (udata_size > 0) {
        L4_ThreadId_t tid_reload = {.raw = tid_raw};
        L4_Map(tid_reload, udata_base, udata_size);
    }

    volatile L4_Word_t ubss_base = (L4_Word_t) &user_bss_start;
    volatile L4_Word_t ubss_size = (L4_Word_t) &user_bss_end - ubss_base;
    if (ubss_size > 0) {
        L4_ThreadId_t tid_reload = {.raw = tid_raw};
        L4_Map(tid_reload, ubss_base, ubss_size);
    }
}

extern void thread_container(void);

__USER_TEXT
static void start_thread(L4_Word_t tid_raw,
                         L4_Word_t entry,
                         L4_Word_t sp,
                         L4_Word_t stack_size)
{
    volatile L4_Word_t saved_tid_raw = tid_raw;
    volatile L4_Word_t saved_entry = entry;
    volatile L4_Word_t saved_sp = sp;
    volatile L4_Word_t saved_stack_size = stack_size;

    L4_Msg_t msg;
    L4_ThreadId_t tid = {.raw = saved_tid_raw};

    L4_MsgClear(&msg);
    L4_MsgAppendWord(&msg, (L4_Word_t) thread_container);
    L4_MsgAppendWord(&msg, saved_sp);
    L4_MsgAppendWord(&msg, saved_stack_size);
    L4_MsgAppendWord(&msg, saved_entry);
    L4_MsgAppendWord(&msg, 0);
    L4_MsgLoad(&msg);
    L4_Send(tid);
}

/* Bootstrap user threads with a real stack.
 * 512 bytes is too small for the semihosting-backed test path and can fault
 * before the thread reaches its entry function.
 */
#define STACK_SIZE PAGER_BOOTSTRAP_STACK_SIZE

void __USER_TEXT __root_thread(kip_t *kip_ptr, utcb_t *utcb_ptr)
{
    extern void *current_utcb;
    *(void *volatile *) &current_utcb = utcb_ptr;

    L4_ThreadId_t myself = {.raw = utcb_ptr->t_globalid};
    volatile char *free_mem = (char *) get_free_base(kip_ptr);

    /* Validate free_mem base - 0 means no free memory found */
    if (!free_mem) {
        /* No free memory available, halt */
        while (1)
            L4_Sleep(L4_Never);
    }

    int num_users = user_runtime_end - user_runtime_start;

    for (int i = 0; i < num_users; ++i) {
        user_struct *ptr = &user_runtime_start[i];
        user_fpage_t *fpage = ptr->fpages;
        user_fpage_t *res_fpage = &ptr->fpages[RES_FPAGE];

        volatile L4_Word_t tid_raw;
        {
            L4_ThreadId_t tid_tmp =
                L4_GlobalId(ptr->tid + kip_ptr->thread_info.s.user_base, 2);
            tid_raw = tid_tmp.raw;
        }
        ptr->thread_num = tid_raw;

        /* Allocate RES_FPAGE first and place the pager thread's initial UTCB
         * and bootstrap stack inside its reserved prefix. That keeps the
         * pager's working set inside the same large user fpage it will later
         * use for child threads.
         */
        {
            L4_Word_t res_size = res_fpage->size;
            volatile L4_Word_t stack_align = res_size;
            volatile L4_Word_t stack_mask = ~(stack_align - 1);
            volatile L4_Word_t fm = (L4_Word_t) free_mem;
            fm = (fm + stack_align - 1) & stack_mask;
            res_fpage->base = fm;
            free_mem = (char *) (fm + res_size);
        }

        volatile L4_Word_t boot_base = res_fpage->base;
        volatile L4_Word_t utcb_base = boot_base;

        /* create thread */
        {
            volatile L4_Word_t tid_saved = tid_raw;
            L4_ThreadId_t tid = {.raw = tid_saved};
            L4_ThreadControl(tid, tid, L4_nilthread, myself,
                             (void *) utcb_base);
        }

        /* map user_text, user_data and user_bss */
        {
            volatile L4_Word_t tid_saved = tid_raw;
            L4_ThreadId_t tid_for_sections = {.raw = tid_saved};
            map_user_sections(kip_ptr, tid_for_sections);
        }

        {
            volatile L4_Word_t tid_saved = tid_raw;
            L4_ThreadId_t tid_for_stack = {.raw = tid_saved};
            L4_Map(tid_for_stack, res_fpage->base, res_fpage->size);
        }
        volatile L4_Word_t stack_top =
            (L4_Word_t) res_fpage->base + UTCB_SIZE + STACK_SIZE;
        volatile L4_Word_t stack_size = STACK_SIZE;

        /* map fpages */
        while (fpage->base || fpage->size) {
            if (fpage == res_fpage) {
                fpage++;
                continue;
            }
            if (fpage->base) {
                volatile L4_Word_t tid_saved = tid_raw;
                L4_ThreadId_t tid_reload = {.raw = tid_saved};
                L4_Map(tid_reload, fpage->base, fpage->size);
            } else {
                /* Align dynamic allocations to the fpage's own size.
                 * This ensures the fpage can be a single MPU region.
                 * fpage->size MUST be a non-zero power of 2.
                 */
                L4_Word_t fpage_size = fpage->size;
                if (!fpage_size || (fpage_size & (fpage_size - 1))) {
                    printf("ERROR: invalid fpage size %p\n",
                           (void *) fpage_size);
                    fpage++;
                    continue;
                }
                volatile L4_Word_t align_mask = ~(fpage_size - 1);
                volatile L4_Word_t fm = (L4_Word_t) free_mem;
                fm = (fm + fpage_size - 1) & align_mask;
                {
                    volatile L4_Word_t tid_saved = tid_raw;
                    L4_ThreadId_t tid_reload = {.raw = tid_saved};
                    L4_Map(tid_reload, fm, fpage_size);
                }
                fpage->base = fm;
                free_mem = (char *) (fm + fpage_size);
            }

            fpage++;
        }

        /* start thread */
        {
            volatile L4_Word_t entry_saved = (L4_Word_t) ptr->entry;
            start_thread(tid_raw, entry_saved, stack_top, stack_size);
        }
    }
    while (1)
        L4_Sleep(L4_Never);
}

DECLARE_THREAD(root_thread, __root_thread);
