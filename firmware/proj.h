#ifndef __PROJ_H__
#define __PROJ_H__

#include <msp430.h>
#include "flash.h"
#include "uart_mapping.h"

#define true                1
#define false               0

/*!
	\brief List of possible message types for the event handler.
	\sa eh_register()
*/
#define           SYS_MSG_NULL  0
#define    SYS_MSG_TIMER0_CCR0  0x1
#define    SYS_MSG_TIMER0_CCR1  0x2   // timer_a0_delay_noblk_ccr1
#define    SYS_MSG_TIMER0_CCR2  0x4   // timer_a0_delay_noblk_ccr2
#define    SYS_MSG_TIMER0_CCR3  0x8   // timer_a0_delay_noblk_ccr3
#define    SYS_MSG_TIMER0_CCR4  0x10
#define     SYS_MSG_TIMER0_IFG  0x20  // timer0 overflow
#define          SYS_MSG_P1IFG  0x40  // button press
#define    SYS_MSG_UART_BCL_RX  0x80  // UART received something

// button
#define                  TRIG1  BIT1
#define              FLASH_VER  2 // must be incremented if struct settings_t changes
#define             FLASH_ADDR  SEGMENT_B

#define latch_enable        P1OUT |= BIT4
#define latch_disable       P1OUT &= ~BIT4
#define uc_enable           P1OUT |= BIT5
#define uc_disable          P1OUT &= ~BIT5
#define led_on              P2OUT |= BIT0
#define led_off             P2OUT &= ~BIT0
#define led_switch          P2OUT ^= BIT0

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
    FLASH_VER,  // ver
    1,          // eadc_en
    0,          // adc_en
    0,          // eadc_delta
    900,        // standby_time
    600,        // standby_unused
};


#endif
