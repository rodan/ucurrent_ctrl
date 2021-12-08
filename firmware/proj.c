
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
#include "drivers/ads1110.h"
#include "drivers/flash.h"
#include "drivers/adc.h"

#ifdef HARDWARE_I2C
    #include "drivers/i2c.h"
#endif

int16_t tue;                    // ticks until end

volatile uint8_t port1_last_event;

struct settings_t s;

char str_temp[120];

void display_menu(void)
{
    sprintf(str_temp,
            "\r\n --- uCurrent controller ver %d --\t  poweroff in %d sec\r\n",
            VERSION, tue);
    uart1_tx_str(str_temp, strlen(str_temp));

    sprintf(str_temp,
            "  * settings\t\e[33;1mE\e[0m: eadc %u   \e[33;1mA\e[0m: adc %u   \e[33;1mT\e[0m: s_t %u   \e[33;1mU\e[0m: s_u %u\r\n",
            s.eadc_en, s.adc_en, s.standby_time, s.standby_unused);
    uart1_tx_str(str_temp, strlen(str_temp));

#ifdef USE_ADC
    sprintf(str_temp,
            "  * ads1110 (status 0x%02x, conv %s%01d.%04dV, rel delta %s%01d.%04dV)\r\n",
            eadc.config, (eadc.conv + s.eadc_delta) < 0 ? "-" : "",
            abs((int16_t) (eadc.conv + s.eadc_delta) / 10000),
            abs((int16_t) (eadc.conv + s.eadc_delta) % 10000),
            s.eadc_delta < 0 ? "-" : "", abs((int16_t) s.eadc_delta / 10000),
            abs((int16_t) s.eadc_delta % 10000));
    uart1_tx_str(str_temp, strlen(str_temp));
#endif

    sprintf(str_temp,
            "  \e[33;1mD\e[0m: relative delta     \e[33;1mL\e[0m: load defaults     \e[33;1mS\e[0m: save settings\r\nChoice? ");
    uart1_tx_str(str_temp, strlen(str_temp));

}

static void timer_a0_irq(enum sys_message msg)
{
    timer_a0_delay_noblk_ccr1(_1s);
    led_on;
    tue--;
    if (tue < 1) {
        uc_disable;
        latch_disable;
#ifdef USE_ADC
    } else {
        if (s.eadc_en == 1) {
            // trigger conversion
            timer_a0_delay_noblk_ccr2(_10ms);
        }
#endif
    }
}

static void port1_irq(enum sys_message msg)
{
    tue = s.standby_time;
}

uint8_t str_to_uint16(char *str, uint16_t * out, const uint8_t seek,
                      const uint8_t len, const uint16_t min, const uint16_t max)
{
    uint16_t val = 0;
    uint32_t pow = 1;
    uint8_t i, c;

    for (i = len; i > seek; i--) {
        c = str[i-1] - 48;
        if (c < 10) {
            val += c * pow;
            pow *= 10;
        } else {
            if (val) {
                // if we already have a number and this char is unexpected
                break;
            }
        }
    }

    if ((val >= min) && (val <= max)) {
        *out = val;
    } else {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
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
        for (i = sizeof(s); i > 0; i--) {
            *dst_p++ = *src_p++;
        }
        display_menu();
        tue = s.standby_time;
    } else if (p == 68) {       // get relative delta
        s.eadc_delta = 0 - eadc.conv;
        display_menu();
    } else if (p == 69) {       // [E]xternal ADC
        if (str_to_uint16(input, &u16, 1, strlen(input), 0, 1) == EXIT_SUCCESS) {
            s.eadc_en = u16;
            display_menu();
        }
    } else if (p == 65) {       // internal [A]DC
        if (str_to_uint16(input, &u16, 1, strlen(input), 0, 1) == EXIT_SUCCESS) {
            s.adc_en = u16;
            display_menu();
        }
    } else if (p == 84) {       // generic [T]imeout
        if (str_to_uint16(input, &u16, 1, strlen(input), 30, 65535) == EXIT_SUCCESS) {
            s.standby_time = u16;
            display_menu();
        }
    } else if (p == 85) {       // [U]nused timeout
        if (str_to_uint16(input, &u16, 1, strlen(input), 30, 65535) == EXIT_SUCCESS) {
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

#ifdef USE_ADC
    #ifdef HARDWARE_I2C 
        i2c_init();
    #endif
    ads1110_init(ED0, BITS_16 | PGA_2 | SC | ST);
#endif

    sys_messagebus_register(&port1_irq, SYS_MSG_P1IFG);
    sys_messagebus_register(&timer_a0_irq, SYS_MSG_TIMER0_CCR1);
    sys_messagebus_register(&uart1_rx_irq, SYS_MSG_UART1_RX);

    timer_a0_delay_noblk_ccr1(_1s);
    display_menu();

    while (1) {
        // sleep
        _BIS_SR(LPM3_bits + GIE);
        __no_operation();
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

    P1SEL = 0;
    P1OUT = 0;
    P1DIR = 0xf9;

    // IRQ triggers on rising edge
    P1IES &= ~TRIG1;
    // Reset IRQ flags
    P1IFG &= ~TRIG1;
    // Enable button interrupts
    P1IE |= TRIG1;

    P2SEL = 0;
    P2OUT = 0;
    P2DIR = 0xff;

    P3SEL = 0;
    P3OUT = 0;
    P3DIR = 0xff;

    P4SEL = 0x30;
    P4OUT = 0x6;
#ifdef USE_ADC
    P4OUT = 0;
  #ifdef HARDWARE_I2C
    P4SEL |= BIT1 + BIT2;
  #endif
#endif
    P4DIR = 0xcf;

    P5SEL = 0;
    P5OUT = 0;
    P5DIR = 0xff;

    P6SEL = 0x1;
    P6OUT = 0;
    P6DIR = 0xfe;

    PJOUT = 0;
    PJDIR = 0xff;

    // disable VUSB LDO and SLDO
    USBKEYPID = 0x9628;
    USBPWRCTL &= ~(SLDOEN + VUSBEN);
    USBKEYPID = 0x9600;

#ifdef USE_XT1
    // enable external LF crystal if one is present
    P5SEL |= BIT4+BIT5;
    UCSCTL6 &= ~XT1OFF;
    // Loop until XT1 stabilizes
    do {
        UCSCTL7 &= ~XT1LFOFFG;                 // clear XT1 fault flags
        SFRIFG1 &= ~OFIFG;                     // clear fault flags
    } while ( UCSCTL7 & XT1LFOFFG );           // test XT1 fault flag
    UCSCTL6 &= ~(XT1DRIVE0 + XT1DRIVE1);
    UCSCTL4 |= SELA__XT1CLK;
#else
    // use internal 32768 Hz oscillator
    UCSCTL4 |= SELA__REFOCLK;
#endif

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
    for (i = sizeof(s); i > 0; i--) {
        *dst_p++ = *src_p++;
    }
}

void check_events(void)
{
    struct sys_messagebus *p = messagebus;
    enum sys_message msg = SYS_MSG_NONE;

    // drivers/timer_a0
    if (timer_a0_last_event) {
        msg |= timer_a0_last_event;
        timer_a0_last_event = TIMER_A0_EVENT_NONE;
    }
    // button presses
    if (port1_last_event) {
        msg |= SYS_MSG_P1IFG;
        port1_last_event = 0;
    }
    // uart RX
    if (uart1_last_event == UART1_EV_RX) {
        msg |= SYS_MSG_UART1_RX;
        uart1_last_event = UART1_EV_NONE;
    }

    while (p) {
        // notify listener if he registered for any of these messages
        if (msg & p->listens) {
            p->fn(msg);
        }
        p = p->next;
    }
}

// Port 1 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
#elif defined(__GNUC__)
__attribute__ ((interrupt(PORT1_VECTOR)))
void Port1_ISR(void)
#else
#error Compiler not supported!
#endif
{
    if (P1IFG & TRIG1) {
        port1_last_event = 1;
        P1IFG &= ~TRIG1;
        LPM3_EXIT;
    }
}
