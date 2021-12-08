#ifndef __UART1_H__
#define __UART1_H__

#include "proj.h"

#define UART1_RXBUF_SZ     9

void uart1_init();
uint16_t uart1_tx_str(char *str, uint16_t size);

enum uart1_tevent {
    UART1_EV_NONE = 0,
    UART1_EV_RX = BIT0,
    UART1_EV_TX = BIT1
};

uint8_t uart1_get_event(void);
void uart1_rst_event(void);
char *uart1_get_rx_buf(void);
void uart1_set_eol(void);

#endif
