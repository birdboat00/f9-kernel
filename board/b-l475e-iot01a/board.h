/* Copyright (c) 2013-2014 The F9 Microkernel Project. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef B_L475E_IOT01A_BOARD_H_
#define B_L475E_IOT01A_BOARD_H_

#include <platform/stm32l4/gpio.h>
#include <platform/stm32l4/nvic.h>
#include <platform/stm32l4/registers.h>
#include <platform/stm32l4/systick.h>
#include <platform/stm32l4/usart.h>

extern struct usart_dev console_uart;

/* ST-Link VCP uses USART1: PB6 (TX), PB7 (RX) */
#define BOARD_UART_DEVICE USART1_IRQn
#define BOARD_UART_HANDLER USART1_HANDLER
#define BOARD_USART_FUNC af_usart1
#define BOARD_USART_CONFIGS                               \
    .base = USART1_BASE, .rcc_apbenr = RCC_USART1_APBENR, \
    .rcc_reset = RCC_APB2RSTR_USART1RST,
#define BOARD_USART_TX_IO_PORT GPIOB
#define BOARD_USART_TX_IO_PIN 6
#define BOARD_USART_RX_IO_PORT GPIOB
#define BOARD_USART_RX_IO_PIN 7

#endif /* B_L475E_IOT01A_BOARD_H_ */
