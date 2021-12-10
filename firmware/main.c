
#include <msp430.h>
#include <stdio.h>
#include <string.h>
#include "proj.h"
#include "driverlib.h"
#include "glue.h"
#include "ui.h"
#include "sig.h"
#include "timer_a0.h"
#include "uart_mapping.h"

int16_t tue;                    // seconds until end
volatile uint8_t port1_last_event;
struct settings_t s;

static void uart_bcl_rx_irq(uint32_t msg)
{
    parse_user_input();
    uart_bcl_set_eol();
}

void check_events(void)
{
    uint16_t msg = SYS_MSG_NULL;

    // drivers/timer_a0
    if (timer_a0_get_event()) {
        msg |= timer_a0_get_event();
        timer_a0_rst_event();
    }
    // button presses
    if (port1_last_event) {
        msg |= SYS_MSG_P1IFG;
        port1_last_event = 0;
    }
    // uart RX
    if (uart_bcl_get_event() == UART_EV_RX) {
        msg |= SYS_MSG_UART_BCL_RX;
        uart_bcl_rst_event();
    }

    eh_exec(msg);
}

static void timer_a0_irq(const uint32_t msg)
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

static void port1_irq(const uint32_t msg)
{
    tue = s.standby_time;
}

void i2c_init(void)
{
    i2c_pin_init();

#if I2C_USE_DEV > 3
    // enhanced USCI capable microcontroller
    EUSCI_B_I2C_initMasterParam param = {0};

    param.selectClockSource = EUSCI_B_I2C_CLOCKSOURCE_SMCLK;
    param.i2cClk = SMCLK_FREQ;
    param.dataRate = EUSCI_B_I2C_SET_DATA_RATE_400KBPS;
    param.byteCounterThreshold = 0;
    param.autoSTOPGeneration = EUSCI_B_I2C_NO_AUTO_STOP;
    EUSCI_B_I2C_initMaster(I2C_BASE_ADDR, &param);
#elif I2C_USE_DEV < 4
    // USCI capable microcontroller
    USCI_B_I2C_initMasterParam param = {0};

    param.selectClockSource = USCI_B_I2C_CLOCKSOURCE_SMCLK;
    param.i2cClk = SMCLK_FREQ;
    param.dataRate = USCI_B_I2C_SET_DATA_RATE_400KBPS;
    USCI_B_I2C_initMaster(I2C_BASE_ADDR, &param);
#endif

    i2c_irq_init(I2C_BASE_ADDR);
}

void pin_init()
{
    // watchdog triggers after 25sec when not cleared
#ifdef USE_WATCHDOG
    WDTCTL = WDTPW + WDTIS__512K + WDTSSEL__ACLK + WDTCNTCL;
#else
    WDTCTL = WDTPW + WDTHOLD;
#endif

    /// P1.1 is VBAT (button press detect)
    P1DIR &= ~TRIG1;
    // IRQ triggers on rising edge
    P1IES &= ~TRIG1;
    // Reset IRQ flags
    P1IFG &= ~TRIG1;
    // Enable button interrupts
    P1IE |= TRIG1;

#ifndef USE_ADC
    // P4.1,P4.2 are i2c with external pullups
    P4OUT = 0x6;
#endif

    // P6.0 is VBAT in (A0)
    P6SEL |= BIT0;
    P6DIR &= ~BIT0;

    // disable VUSB LDO and SLDO
    USBKEYPID = 0x9628;
    USBPWRCTL &= ~(SLDOEN + VUSBEN);
    USBKEYPID = 0x9600;
}

void settings_init(void)
{
    uint8_t *src_p, *dst_p;
    uint8_t i;

    src_p = FLASH_ADDR;
    dst_p = (uint8_t *) & s;
    if ((*src_p) != FLASH_VER) {
        src_p = (uint8_t *) & defaults;
    }
    for (i = sizeof(s); i > 0; i--) {
        *dst_p++ = *src_p++;
    }
}

int main(void)
{
    // stop watchdog
    WDTCTL = WDTPW | WDTHOLD;
    msp430_hal_init(HAL_GPIO_DIR_OUTPUT | HAL_GPIO_OUT_LOW);
    pin_init();
#ifdef USE_SIG
    sig0_on;
#endif

    timer_a0_init();

    clock_pin_init();
    clock_init();

    uart_bcl_pin_init();
    uart_bcl_init();
#if defined UART0_RX_USES_RINGBUF || defined UART1_RX_USES_RINGBUF || \
    defined UART2_RX_USES_RINGBUF || defined UART3_RX_USES_RINGBUF
    uart_bcl_set_rx_irq_handler(uart_bcl_rx_ringbuf_handler);
#else
    uart_bcl_set_rx_irq_handler(uart_bcl_rx_simple_handler);
#endif

#ifdef USE_ADC
    i2c_init();
    ADS1110_init(I2C_BASE_ADDR, ADS1110_ED0, ADS1110_BITS_16 | ADS1110_PGA_2 | ADS1110_SC | ADS1110_ST);
#endif

    settings_init();

    tue = s.standby_time;
    latch_enable;
    uc_enable;

#ifdef USE_SIG
    sig0_off;
    sig1_off;
    sig2_off;
    sig3_off;
    sig4_off;
    sig5_off;
#endif

    eh_init();
    eh_register(&port1_irq, SYS_MSG_P1IFG);
    eh_register(&timer_a0_irq, SYS_MSG_TIMER0_CCR1);
    eh_register(&uart_bcl_rx_irq, SYS_MSG_UART_BCL_RX);

    _BIS_SR(GIE);

    timer_a0_delay_noblk_ccr1(_1s);

    display_version();

    while (1) {
        // sleep
        _BIS_SR(LPM3_bits + GIE);
        __no_operation();
#ifdef USE_WATCHDOG
        WDTCTL = (WDTCTL & 0xff) | WDTPW | WDTCNTCL;
#endif
        check_events();
        check_events();
        check_events();
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

