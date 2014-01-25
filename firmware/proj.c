
//  MSP430F5510 based project that controls the power delivery to a uCurrent
//
//  author:          Petre Rodan <petre.rodan@simplex.ro>
//  available from:  https://github.com/rodan/ucurrent_ctrl
//  license:         GNU GPLv3

#include <stdio.h>
#include <string.h>
#include "proj.h"
#include "drivers/sys_messagebus.h"
#include "drivers/timer_a0.h"
#include "drivers/uart1.h"
#include "drivers/serial_bitbang.h"
#include "drivers/ads1110.h"
#include "drivers/adc.h"

// an integer representing the time after which the uC will disable the latch
// standby_time(sec) = standby_time(int) * 2sec
// (2sec is the overflow time of TA0R)
uint16_t standby_time;

int16_t tue; // ticks until end

volatile uint8_t port1_last_event;
struct ads1110 eadc;

char str_temp[83];

void display_menu(void)
{
    uart1_tx_str("\r\nuCurrent controller\r\n  time until poweroff: ", 47);
    sprintf(str_temp, "%d sec\r\n ADS1110 status:\r\n  config:     0x%02x\r\n  conversion: %s%01d.%04dV\r\n", tue*2, eadc.config, 
            eadc.conv < 0 ? "-": "", abs((int16_t) eadc.conv / 10000), abs((int16_t) eadc.conv % 10000) );
    uart1_tx_str(str_temp, strlen(str_temp));
}

static void timer_a0_irq(enum sys_message msg)
{
    tue--;
    if (tue < 1) {
        uc_disable;
        latch_disable;
    } else {
        // trigger conversion
        ads1110_config(ED0, BITS_16 | PGA_2 | SC | ST);
        timer_a0_delay(70000);
        ads1110_read(ED0, &eadc);
        ads1110_convert(&eadc);
    }
}

static void port1_irq(enum sys_message msg)
{
    // debounce
    timer_a0_delay(50000);
    if ((P1IN & TRIG1) == 0) {
        return;
    } else {
        tue = standby_time;
        display_menu();
    }
}

int main(void)
{
    main_init();
    uart1_init();
    standby_time = 900;         // by default shut down after 30minutes
    // read from flash
    tue = standby_time;
    latch_enable;
    uc_enable;
    led_on;

    ads1110_config(ED0, BITS_16 | PGA_2 | SC | ST);

    sys_messagebus_register(&timer_a0_irq, SYS_MSG_TIMER0_IFG);
    sys_messagebus_register(&port1_irq, SYS_MSG_P1IFG);

    while (1) {
        // sleep
        _BIS_SR(LPM3_bits + GIE);
        __no_operation();
        //wake_up();
#ifdef USE_WATCHDOG
        WDTCTL = (WDTCTL & 0xff) | WDTPW | WDTCNTCL;
#endif
        check_events();
    }
}

void main_init(void)
{
    // watchdog triggers after 25sec when not cleared
#ifdef USE_WATCHDOG
    WDTCTL = WDTPW + WDTIS__512K + WDTSSEL__ACLK + WDTCNTCL;
#else
    WDTCTL = WDTPW + WDTHOLD;
#endif
    P5SEL |= BIT5 + BIT4;

    P1SEL = 0;
    P1DIR = 0xfd;
    P1OUT = 0;

    // IRQ triggers on rising edge
    P1IES &= ~TRIG1;
    // Reset IRQ flags
    P1IFG &= ~TRIG1;
    // Enable button interrupts
    P1IE |= TRIG1;

    P2SEL = 0;
    P2DIR = 0x1;
    P2OUT = 0;

    P3SEL = 0;
    P3DIR = 0x1f;
    P3OUT = 0;

    P4SEL = 0;
    P4DIR = 0xff;
    P4OUT = 0;

    //P5SEL is set above
    P5DIR = 0xff;
    P5OUT = 0;

    P6DIR = 0xfe;
    P6OUT = 0;

    PJDIR = 0xff;
    PJOUT = 0;

    // disable VUSB LDO and SLDO
    USBKEYPID = 0x9628;
    USBPWRCTL &= ~(SLDOEN + VUSBEN);
    USBKEYPID = 0x9600;

    timer_a0_init();
}

/*
void wake_up(void)
{
}
*/

void check_events(void)
{
    struct sys_messagebus *p = messagebus;
    enum sys_message msg = 0;

    // drivers/timer_a0
    if (timer_a0_last_event == TIMER_A0_EVENT_IFG) {
        msg |= BIT0;
        timer_a0_last_event = 0;
    }
    // button presses
    if (port1_last_event) {
        msg |= BIT1;
        port1_last_event = 0;
    }

    while (p) {
        // notify listener if he registered for any of these messages
        if (msg & p->listens) {
            p->fn(msg);
        }
        p = p->next;
    }
}

#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{
    if (P1IFG & TRIG1) {
        port1_last_event = 1;
        P1IFG &= ~TRIG1;
        LPM3_EXIT;
    }
}
