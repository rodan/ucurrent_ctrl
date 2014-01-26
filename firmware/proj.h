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

#define VERSION             1   // must be incremented if struct settings changes
#define FLASH_ADDR          SEGMENT_B

#define true                1
#define false               0

void main_init(void);
void settings_init(void);
void wake_up(void);
void check_events(void);

struct settings_t {
    uint8_t ver;                // firmware version
    int8_t enable_eadc;         // enable external adc. 0 false, 1 true, -1 false but ADS1110 present
    uint8_t enable_adc;         // enable internal adc for battery check
    uint16_t standby_time;      // number of TA0 overflows after which latch is disabled
                                // (1 overflow takes 2 seconds)
    uint16_t standby_unused;    // innactivity timeout after which latch is disabled
                                // enable_adc must be 1 for this timer to be enabled
};

static const struct settings_t defaults = {
    VERSION,    // ver
    1,          // enable_eadc
    0,          // enable_adc
    900,        // standby_time
    600,        // standby_unused
};


#endif
