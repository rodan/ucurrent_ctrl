
//  MSP430F5510 based project that controls the power delivery to a uCurrent
//
//  author:          Petre Rodan <2b4eda@subdimension.ro>
//  available from:  https://github.com/rodan/ucurrent_ctrl
//  license:         GNU GPLv3

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "proj.h"
#include "drivers/sys_messagebus.h"
#include "drivers/timer_a0.h"
#include "drivers/uart1.h"
#include "drivers/serial_bitbang.h"
#include "drivers/ads1110.h"
#include "drivers/adc.h"
#include "drivers/flash.h"

int16_t tue;                    // ticks until end

volatile uint8_t port1_last_event;
struct ads1110 eadc;

struct settings_t s;

char str_temp[120];

void display_menu(void)
{
    sprintf(str_temp,
            "\r\n --- uCurrent controller ver %d --\t  poweroff in %d sec\r\n",
            VERSION, tue * 2);
    uart1_tx_str(str_temp, strlen(str_temp));

    sprintf(str_temp,
            "  * ads1110 (status 0x%02x, conv %s%01d.%04dV, rel delta %s%01d.%04dV)\r\n",
            eadc.config, (eadc.conv + s.eadc_delta) < 0 ? "-" : "",
            abs((int16_t) (eadc.conv + s.eadc_delta) / 10000),
            abs((int16_t) (eadc.conv + s.eadc_delta) % 10000),
            s.eadc_delta < 0 ? "-" : "", abs((int16_t) s.eadc_delta / 10000),
            abs((int16_t) s.eadc_delta % 10000));
    uart1_tx_str(str_temp, strlen(str_temp));

    sprintf(str_temp,
            "  * settings\t\e[33;1mE\e[0m: eadc %d   \e[33;1mA\e[0m: adc %d   \e[33;1mT\e[0m: s_t %d   \e[33;1mU\e[0m: s_u %d\r\n",
            s.eadc_en, s.adc_en, s.standby_time, s.standby_unused);
    uart1_tx_str(str_temp, strlen(str_temp));

    sprintf(str_temp,
            "  \e[33;1mD\e[0m: relative delta     \e[33;1mL\e[0m: load defaults     \e[33;1mS\e[0m: save settings\r\nChoice? ");
    uart1_tx_str(str_temp, strlen(str_temp));

}

static void timer_a0_irq(enum sys_message msg)
{
    tue--;
    if (tue < 1) {
        uc_disable;
        latch_disable;
    } else {
        if (s.eadc_en == 1) {
            // trigger conversion
            ads1110_config(ED0, BITS_16 | PGA_2 | SC | ST);
            timer_a0_delay(70000);
            ads1110_read(ED0, &eadc);
            ads1110_convert(&eadc);
        }
    }
}

static void port1_irq(enum sys_message msg)
{
    // debounce
    timer_a0_delay(50000);
    if ((P1IN & TRIG1) == 0) {
        return;
    } else {
        tue = s.standby_time;
        display_menu();
    }
}

uint8_t str_to_uint16(char *str, uint16_t * out, const uint8_t seek,
                      const uint8_t len, const uint16_t min, const uint16_t max)
{
    uint16_t val = 0, pow = 1;
    uint8_t i;

    // pow() is missing in gcc, so we improvise
    for (i = 0; i < len - 1; i++) {
        pow *= 10;
    }
    for (i = 0; i < len; i++) {
        if ((str[seek + i] > 47) && (str[seek + i] < 58)) {
            val += (str[seek + i] - 48) * pow;
        }
        pow /= 10;
    }
    if ((val >= min) && (val <= max)) {
        *out = val;
    } else {
        sprintf(str_temp, "\e[31;1merr\e[0m specify an int between %u-%u\r\n",
                min, max);
        uart1_tx_str(str_temp, strlen(str_temp));
        return 0;
    }
    return 1;
}

static void uart1_rx_irq(enum sys_message msg)
{
    uint16_t u16;
    uint8_t p, i;
    uint8_t *src_p, *dst_p;
    char *input;

    input = (char *)uart1_rx_buf;

    p = input[0];
    if (p > 97) {
        p -= 32;
    }

    if (p == 0) {               // empty enter
        display_menu();
    } else if (p == 83) {       // [S]ave to flash
        flash_save(FLASH_ADDR, (void *)&s, sizeof(s));
        display_menu();
    } else if (p == 76) {       // [L]oad defaults
        src_p = (uint8_t *) & defaults;
        dst_p = (uint8_t *) & s;
        for (i = 0; i < sizeof(s); i++) {
            *dst_p++ = *src_p++;
        }
        display_menu();
        tue = s.standby_time;
    } else if (p == 68) {       // get relative delta
        s.eadc_delta = 0 - eadc.conv;
        display_menu();
    } else if (p == 69) {       // [E]xternal ADC
        if (str_to_uint16(input, &u16, 1, strlen(input) - 1, 0, 1)) {
            s.eadc_en = u16;
            display_menu();
        }
    } else if (p == 65) {       // internal [A]DC
        if (str_to_uint16(input, &u16, 1, strlen(input) - 1, 0, 1)) {
            s.adc_en = u16;
            display_menu();
        }
    } else if (p == 84) {       // generic [T]imeout
        if (str_to_uint16(input, &u16, 1, strlen(input) - 1, 30, 65535)) {
            s.standby_time = u16;
            display_menu();
        }
    } else if (p == 85) {       // [U]nused timeout
        if (str_to_uint16(input, &u16, 1, strlen(input) - 1, 30, 65535)) {
            s.standby_unused = u16;
            display_menu();
        }
    } else {
        uart1_tx_str("\e[31;1merr\e[0m\r\n", 17);
    }

    //sprintf(str_temp, "\r\n%d\r\n", p);
    //uart1_tx_str(str_temp, strlen(str_temp));

    uart1_p = 0;
    uart1_rx_enable = 1;
}

int main(void)
{
    main_init();
    uart1_init();
    settings_init();

    tue = s.standby_time;
    latch_enable;
    uc_enable;
    led_on;

    ads1110_config(ED0, BITS_16 | PGA_2 | SC | ST);

    sys_messagebus_register(&timer_a0_irq, SYS_MSG_TIMER0_IFG);
    sys_messagebus_register(&port1_irq, SYS_MSG_P1IFG);
    sys_messagebus_register(&uart1_rx_irq, SYS_MSG_UART1_RX);

    display_menu();

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

void settings_init(void)
{
    uint8_t *src_p, *dst_p;
    uint8_t i;

    src_p = FLASH_ADDR;
    dst_p = (uint8_t *) & s;
    if ((*src_p) != VERSION) {
        src_p = (uint8_t *) & defaults;
    }
    for (i = 0; i < sizeof(s); i++) {
        *dst_p++ = *src_p++;
    }
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
    // uart RX
    if (uart1_last_event == UART1_EV_RX) {
        msg |= BIT2;
        uart1_last_event = 0;
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
