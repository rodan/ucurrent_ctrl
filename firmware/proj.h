#ifndef __PROJ_H__
#define __PROJ_H__

#include <msp430.h>
#include <stdlib.h>
#include <inttypes.h>
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
#define led_switch          P2OUT ^= BIT0

#define VERSION             2   // must be incremented if struct settings_t changes
#define FLASH_ADDR          SEGMENT_B

#define true                1
#define false               0

void main_init(void);
void settings_init(void);
void wake_up(void);
void check_events(void);
uint8_t str_to_uint16(char *str, uint16_t *out, const uint8_t seek, const uint8_t len, const uint16_t min, const uint16_t max);

struct settings_t {
    uint8_t ver;                // firmware version
    int8_t eadc_en;             // enable external adc. 0 false, 1 true, -1 false but ADS1110 present
    uint8_t adc_en;             // enable internal adc for battery check
    int16_t eadc_delta;         // 0V correction for the EADC
    uint16_t standby_time;      // number of TA0 overflows after which latch is disabled
                                // (1 overflow takes 2 seconds)
    uint16_t standby_unused;    // innactivity timeout after which latch is disabled
                                // enable_adc must be 1 for this timer to be enabled
};

static const struct settings_t defaults = {
    VERSION,    // ver
    1,          // eadc_en
    0,          // adc_en
    0,          // eadc_delta
    900,        // standby_time
    600,        // standby_unused
};


#endif
