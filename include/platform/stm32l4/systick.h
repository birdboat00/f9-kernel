#ifndef PLATFORM_STM32L4_SYSTICK_H_
#define PLATFORM_STM32L4_SYSTICK_H_

#include <platform/stm32l4/registers.h>

#define CORE_CLOCK (0x0a037a00) /* 168 MHz - QEMU SysTick runs at this rate */
#define SYSTICK_MAXRELOAD (0x00ffffff)

void init_systick(uint32_t tick_reload, uint32_t tick_next_reload);
void systick_disable(void);
uint32_t systick_now(void);
uint32_t systick_flag_count(void);

#endif
