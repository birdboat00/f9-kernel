/* STM32L4 USART implementation.
 *
 * The L4 USART has a different register layout from the F4:
 *   F4: SR(+0x00), DR(+0x04), BRR(+0x08), CR1(+0x0C), CR2(+0x10), CR3(+0x14)
 *   L4: CR1(+0x00), CR2(+0x04), CR3(+0x08), BRR(+0x0C), ISR(+0x1C), RDR(+0x24),
 * TDR(+0x28)
 *
 * This file defines USART_OPS_DEFINED and USART_BAUD_DEFINED to override
 * the F4 fallbacks in platform/stm32-common/usart.c.
 */

#include INC_PLAT(registers.h)
#include INC_PLAT(gpio.h)

#define USART_OPS_DEFINED
#define USART_BAUD_DEFINED

/* STM32L4 USART register map */
struct usart_regs {
    volatile uint32_t CR1;  /* +0x00 */
    volatile uint32_t CR2;  /* +0x04 */
    volatile uint32_t CR3;  /* +0x08 */
    volatile uint32_t BRR;  /* +0x0C */
    volatile uint32_t GTPR; /* +0x10 */
    volatile uint32_t RTOR; /* +0x14 */
    volatile uint32_t RQR;  /* +0x18 */
    volatile uint32_t ISR;  /* +0x1C (replaces F4 SR) */
    volatile uint32_t ICR;  /* +0x20 */
    volatile uint32_t RDR;  /* +0x24 (replaces F4 DR for read) */
    volatile uint32_t TDR;  /* +0x28 (replaces F4 DR for write) */
};

/* L4 ISR bits (compatible positions with F4 SR for common flags) */
#define L4_USART_ISR_RXNE (1 << 5)
#define L4_USART_ISR_TC (1 << 6)
#define L4_USART_ISR_TXE (1 << 7)

static int16_t usart_baud(uint32_t base, uint32_t baud)
{
    uint32_t apb_clock;
    uint32_t sws = *RCC_CFGR & RCC_CFGR_SWS_M;

    if (sws == RCC_CFGR_SWS_PLL) {
        if (base == USART1_BASE)
            apb_clock = 80000000; /* APB2 */
        else
            apb_clock = 40000000; /* APB1 */
    } else {
        apb_clock = 4000000; /* MSI fallback */
    }

    /* L4 BRR is a simple 16-bit divider (no mantissa/fraction split
     * when OVER8=0, which is the default) */
    return (uint16_t) (apb_clock / baud);
}

int usart_status(struct usart_dev *usart, uint16_t st)
{
    struct usart_regs *uregs = (struct usart_regs *) usart->base;
    return (uregs->ISR & st);
}

uint8_t usart_getc(struct usart_dev *usart)
{
    struct usart_regs *uregs = (struct usart_regs *) usart->base;
    return (uregs->RDR & 0xff);
}

void usart_putc(struct usart_dev *usart, uint8_t c)
{
    struct usart_regs *uregs = (struct usart_regs *) usart->base;
    uregs->TDR = c;
}

void usart_init(struct usart_dev *usart)
{
    struct usart_regs *uregs;

    /* Enable peripheral clock */
    *(usart->rcc_apbenr) |= usart->rcc_reset;

    /* Configure GPIO pins */
    gpio_config(&usart->tx);
    gpio_config(&usart->rx);

    uregs = (struct usart_regs *) usart->base;

    /* Disable USART during configuration */
    uregs->CR1 &= ~USART_CR1_UE;

    /* 8-bit word length (M[1:0] = 00) */
    uregs->CR1 &= ~(USART_CR1_M9);

    /* 1 stop bit (STOP[1:0] = 00) */
    uregs->CR2 &= ~(3 << 12);

    /* Set baud rate */
    uregs->BRR = usart_baud(usart->base, usart->baud);

    /* Enable USART, receiver, and transmitter */
    uregs->CR1 |= (USART_CR1_UE | USART_CR1_RE | USART_CR1_TE);
}
