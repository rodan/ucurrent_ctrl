#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "i2c_config.h"

#define USE_XT1
//#define USE_XT2

//#define SMCLK_FREQ_1M
//#define SMCLK_FREQ_4M
#define SMCLK_FREQ_8M
//#define SMCLK_FREQ_16M

#define UART_USES_UCA1
#define UART_RXBUF_SZ 16
#define UART_RX_USES_RINGBUF
#define UART_TXBUF_SZ 32
#define UART_TX_USES_IRQ

//#define USE_ADC
//#define USE_I2C_MASTER
//#define CONFIG_ADS1110

//#define USE_WATCHDOG

//#define USE_SIG
//#define CONFIG_DEBUG

#endif
