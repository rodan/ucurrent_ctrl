
#include <msp430.h>
#include <stdio.h>
#include <string.h>
#include "proj.h"
#include "driverlib.h"
#include "glue.h"
#include "ui.h"
#include "sig.h"
#include "timer_a2.h"
#include "uart_mapping.h"

#ifdef USE_ADC
extern struct ads1110_t eadc;
#endif

volatile uint8_t port1_last_event;
struct settings_t s;

static void uart_bcl_rx_irq(uint32_t msg)
{
    parse_user_input();
    uart_bcl_set_eol();
}

static void scheduler_irq(uint32_t msg)
{
    timer_a2_scheduler_handler();
}

void check_events(void)
{
    uint16_t msg = SYS_MSG_NULL;
    uint16_t ev;

    // uart RX
    if (uart_bcl_get_event() == UART_EV_RX) {
        msg |= SYS_MSG_UART_BCL_RX;
        uart_bcl_rst_event();
    }
    // timer_a2
    ev = timer_a2_get_event();
    if (ev) {
        if (ev & TIMER_A2_EVENT_CCR1) {
            msg |= SYS_MSG_TIMERA2_CCR1;
        }
        timer_a2_rst_event();
    }
    // timer_a2-based scheduler
    ev = timer_a2_get_event_schedule();
    if (ev) {
        if (ev & (1 << SCHEDULE_PB_11)) {
            msg |= SYS_MSG_P11_TMOUT_INT;
        }
        if (ev & (1 << SCHEDULE_ADC_SM)) {
            msg |= SYS_MSG_SCH_ADC_SM_INT;
        }
        if (ev & (1 << SCHEDULE_PS_HALT)) {
            msg |= SYS_MSG_SCH_PS_HALT_INT;
        }
        timer_a2_rst_event_schedule();
    }
    // button presses
    if (port1_last_event) {
        msg |= SYS_MSG_P11_INT;
        port1_last_event = 0;
    }

    eh_exec(msg);
}

#ifdef USE_ADC
static void ads1110_state_machine(const uint32_t msg)
{
    if (!s.eadc_en) {
        return;
    }

    switch (eadc.state) {
        case ADS1110_STATE_CONVERT:
            // conversion should be ready
            led_switch;
            ADS1110_read(I2C_BASE_ADDR, ADS1110_ED0, &eadc);
            ADS1110_convert(&eadc);
            eadc.state = ADS1110_STATE_STANDBY;
            timer_a2_set_trigger_slot(SCHEDULE_ADC_SM, systime() + 100, TIMER_A2_EVENT_ENABLE);
        break;
        case ADS1110_STATE_STANDBY:
        default:
            // trigger new single conversion
            ADS1110_config(I2C_BASE_ADDR, ADS1110_ED0, ADS1110_BITS_16 | ADS1110_PGA_2 | ADS1110_SC | ADS1110_ST);
            eadc.state = ADS1110_STATE_CONVERT;
            timer_a2_set_trigger_slot(SCHEDULE_ADC_SM, systime() + 10, TIMER_A2_EVENT_ENABLE);
        break;
    }
}
#endif

static void halt_irq(const uint32_t msg)
{
    uc_disable;
    latch_disable;
}

static void button_11_irq(const uint32_t msg)
{
    if (P1IN & TRIG1) {
        timer_a2_set_trigger_slot(SCHEDULE_PB_11, systime() + 100, TIMER_A2_EVENT_ENABLE);
        //uart_bcl_print("PB11 short 1\r\n");
    } else {
        timer_a2_set_trigger_slot(SCHEDULE_PB_11, 0, TIMER_A2_EVENT_DISABLE);
        timer_a2_set_trigger_slot(SCHEDULE_PS_HALT, systime() + ((uint32_t) s.standby_time * 100), TIMER_A2_EVENT_ENABLE);
        uart_bcl_print("PB11 short\r\n");
    }
}

static void button_11_long_press_irq(uint32_t msg)
{
    // ignore next edge
    P1IES &= ~TRIG1;
    P1IFG &= ~TRIG1;
    //uart_bcl_print("PB11 long\r\n");
    display_menu();
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

void sys_init()
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
    sys_init();
#ifdef USE_SIG
    sig0_on;
#endif

    clock_pin_init();
    clock_init();

    timer_a2_init();

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
    latch_enable;
    uc_enable;

#ifdef USE_SIG
    sig0_off;
    sig1_off;
    sig2_off;
    sig3_off;
    sig4_on;
    sig5_off;
#endif

    eh_init();
    eh_register(&button_11_irq, SYS_MSG_P11_INT);
    eh_register(&button_11_long_press_irq, SYS_MSG_P11_TMOUT_INT);
    eh_register(&uart_bcl_rx_irq, SYS_MSG_UART_BCL_RX);
    eh_register(&scheduler_irq, SYS_MSG_TIMERA2_CCR1);
    eh_register(&halt_irq, SYS_MSG_SCH_PS_HALT_INT);
#ifdef USE_ADC
    eh_register(&ads1110_state_machine, SYS_MSG_SCH_ADC_SM_INT);
#endif
    timer_a2_set_trigger_slot(SCHEDULE_PS_HALT, systime() + ((uint32_t) s.standby_time * 100), TIMER_A2_EVENT_ENABLE);
    timer_a2_set_trigger_slot(SCHEDULE_ADC_SM, systime() + 100, TIMER_A2_EVENT_ENABLE);

    _BIS_SR(GIE);

    led_on;
    display_version();

    while (1) {
        // sleep
#ifdef USE_SIG
        sig4_off;
#endif
        _BIS_SR(LPM1_bits + GIE);
        __no_operation();
#ifdef USE_WATCHDOG
        WDTCTL = (WDTCTL & 0xff) | WDTPW | WDTCNTCL;
#endif
#ifdef USE_SIG
        sig4_on;
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
        // listen for opposite edge
        P1IES ^= TRIG1;
        _BIC_SR_IRQ(LPM3_bits);
    }
        
    P1IFG = 0;
}

