/* KDB UART input driver for semihosting configurations.
 *
 * Enables USART1 RX interrupt for KDB interactive input while semihosting
 * handles debug output. Pressing '?' activates KDB under QEMU.
 *
 * Uses QEMU's USART1 emulation directly -- minimal init (no GPIO/RCC
 * needed since QEMU pre-initializes the USART hardware model).
 * Requires: qemu-system-arm -serial mon:stdio -semihosting
 */

#ifdef CONFIG_KDB_UART_INPUT

#include <platform/debug_device.h>
#include <platform/irq.h>
#include <softirq.h>
#include <types.h>

#include "board.h"
#include INC_PLAT(registers.h)

/* STM32L4 USART1 register access using platform USART1_BASE.
 * Register layout: L4 uses CR1(+0x00)/BRR(+0x0C)/ISR(+0x1C)/RDR(+0x24)
 */
#define KDB_USART1_CR1 (*(volatile uint32_t *) (USART1_BASE + 0x00))
#define KDB_USART1_BRR (*(volatile uint32_t *) (USART1_BASE + 0x0C))
#define KDB_USART1_ISR (*(volatile uint32_t *) (USART1_BASE + 0x1C))
#define KDB_USART1_RDR (*(volatile uint32_t *) (USART1_BASE + 0x24))

#define KDB_USART_ISR_RXNE (1 << 5)
#define KDB_USART_CR1_UE (1 << 0)
#define KDB_USART_CR1_RE (1 << 2)
#define KDB_USART_CR1_RXNEIE (1 << 5)

/* Simple circular buffer for received characters */
#define KDB_RX_BUFSIZE 16
static volatile uint8_t kdb_rx_buf[KDB_RX_BUFSIZE];
static volatile uint8_t kdb_rx_head, kdb_rx_tail;

void __kdb_uart_irq_handler(void)
{
    if (KDB_USART1_ISR & KDB_USART_ISR_RXNE) {
        uint8_t chr = (uint8_t) KDB_USART1_RDR;
        uint8_t next = (kdb_rx_head + 1) % KDB_RX_BUFSIZE;
        if (next != kdb_rx_tail) {
            kdb_rx_buf[kdb_rx_head] = chr;
            kdb_rx_head = next;
        }
        softirq_schedule(KDB_SOFTIRQ);
    }
}

IRQ_HANDLER(BOARD_UART_HANDLER, __kdb_uart_irq_handler);

static uint8_t kdb_uart_getchar(void)
{
    if (kdb_rx_head == kdb_rx_tail)
        return 0;
    uint8_t chr = kdb_rx_buf[kdb_rx_tail];
    kdb_rx_tail = (kdb_rx_tail + 1) % KDB_RX_BUFSIZE;
    return chr;
}

void kdb_uart_input_init(void)
{
    kdb_rx_head = 0;
    kdb_rx_tail = 0;

    /* Enable USART1 peripheral clock on APB2 before touching registers.
     * sys_clock_init() resets APB enables, so we must re-enable here.
     */
    *(volatile uint32_t *) RCC_APB2ENR |= RCC_APB2ENR_USART1EN;

    /* Minimal USART1 init: baud rate, receiver, RX interrupt */
    KDB_USART1_BRR = 35; /* ~115200 baud at 4MHz MSI (4000000/115200) */
    KDB_USART1_CR1 = KDB_USART_CR1_UE | KDB_USART_CR1_RE | KDB_USART_CR1_RXNEIE;

    NVIC_SetPriority(BOARD_UART_DEVICE, 0xe, 0);
    NVIC_ClearPendingIRQ(BOARD_UART_DEVICE);
    NVIC_EnableIRQ(BOARD_UART_DEVICE);

    /* Override semihosting getchar with UART RX reader.
     * Output (putchar) remains on semihosting.
     */
    extern dbg_dev_t *cur_dev;
    cur_dev->getchar = kdb_uart_getchar;
}

#endif /* CONFIG_KDB_UART_INPUT */
