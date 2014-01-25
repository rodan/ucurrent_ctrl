#ifndef __PROJ_H__
#define __PROJ_H__

#include <msp430.h>
#include <stdlib.h>
#include "config.h"

// button
#define TRIG1               BIT1

// bitbang i2c
#define I2C_MASTER_DIR      P4DIR
#define I2C_MASTER_OUT      P4OUT
#define I2C_MASTER_IN       P4IN
#define I2C_MASTER_SCL      BIT2
#define I2C_MASTER_SDA      BIT1

#define latch_enable        P1OUT |= BIT4
#define latch_disable       P1OUT &= ~BIT4
#define uc_enable           P1OUT |= BIT5
#define uc_disable          P1OUT &= ~BIT5
#define led_on              P2OUT |= BIT0
#define led_off             P2OUT &= ~BIT0

#define true                1
#define false               0

void main_init(void);
void wake_up(void);
void check_events(void);

#endif
