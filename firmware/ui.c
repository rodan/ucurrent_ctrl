
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "proj.h"
#include "glue.h"
#include "version.h"
#include "uart_mapping.h"
#include "rtca_now.h"
#include "flash.h"
#include "ads1110.h"
#include "timer_a2.h"
#include "ui.h"

extern struct settings_t s;
extern struct ads1110_t eadc;

void display_menu(void)
{
    char sconv[CONV_BASE_10_BUF_SZ];

    display_version();
    uart_bcl_print("  * settings\t\e[33;1mE\e[0m: eadc ");
    uart_bcl_print(_itoa(sconv, s.eadc_en));
    uart_bcl_print("   \e[33;1mA\e[0m: adc ");
    uart_bcl_print(_utoa(sconv, s.adc_en));
    uart_bcl_print("   \e[33;1mT\e[0m: s_t ");
    uart_bcl_print(_utoa(sconv, s.standby_time));
    uart_bcl_print("   \e[33;1mU\e[0m: s_u ");
    uart_bcl_print(_utoa(sconv, s.standby_unused));
    uart_bcl_print("\r\n");

#ifdef USE_ADC
    uart_bcl_print("  * ads1110 (status ");
    uart_bcl_print(_utoh(sconv, eadc.config));
    uart_bcl_print(", conv ");
    uart_bcl_print((eadc.conv + s.eadc_delta) < 0 ? "-" : "");
    uart_bcl_print(_itoa(sconv, abs((int16_t) (eadc.conv + s.eadc_delta) / 10000)));
    uart_bcl_print(".");
    uart_bcl_print(_itoa(sconv, abs((int16_t) (eadc.conv + s.eadc_delta) % 10000)));
    uart_bcl_print("V, rel delta ");
    uart_bcl_print(s.eadc_delta < 0 ? "-" : "");
    uart_bcl_print(_itoa(sconv, abs((int16_t) (s.eadc_delta) / 10000)));
    uart_bcl_print(".");
    uart_bcl_print(_itoa(sconv, abs((int16_t) (s.eadc_delta) % 10000)));
    uart_bcl_print("V)\r\n");
#endif

    uart_bcl_print("  \e[33;1mD\e[0m: relative delta     \e[33;1mL\e[0m: load defaults     \e[33;1mS\e[0m: save settings\r\nChoice? ");
}

void display_version(void)
{
    char sconv[CONV_BASE_10_BUF_SZ];

    uart_bcl_print("uCurrent controller rev");
    uart_bcl_print(_utoa(sconv, REV));
    uart_bcl_print("b");
    uart_bcl_print(_utoa(sconv, BUILD));
    uart_bcl_print("\r\n");
}

void display_schedule(void)
{
    uint8_t c;
    uint32_t trigger;
    uint8_t flag;
    char itoa_buf[CONV_BASE_10_BUF_SZ];

    for (c = 0; c < TIMER_A2_SLOTS_COUNT; c++) {
        timer_a2_get_trigger_slot(c, &trigger, &flag);
        uart_bcl_print(_utoa(itoa_buf, c));
        uart_bcl_print(" \t");
        uart_bcl_print(_utoa(itoa_buf, trigger));
        uart_bcl_print(" \t");
        uart_bcl_print(_utoa(itoa_buf, flag));
        uart_bcl_print("\r\n");
    }
    trigger = timer_a2_get_trigger_next();
    uart_bcl_print("sch next ");
    uart_bcl_print(_utoa(itoa_buf, trigger));
    uart_bcl_print("\r\n");
}


#define PARSER_CNT 16

void parse_user_input(void)
{
#if defined UART0_RX_USES_RINGBUF || defined UART1_RX_USES_RINGBUF || \
    defined UART2_RX_USES_RINGBUF || defined UART3_RX_USES_RINGBUF
    struct ringbuf *rbr = uart_bcl_get_rx_ringbuf();
    uint8_t rx;
    uint8_t c = 0;
    char input[PARSER_CNT];

    memset(input, 0, PARSER_CNT);

    // read the entire ringbuffer
    while (ringbuf_get(rbr, &rx)) {
        if (c < PARSER_CNT-1) {
            input[c] = rx;
        }
        c++;
    }
#else
    char *input = uart_bcl_get_rx_buf();
#endif
    uint16_t u16;
    uint8_t i;
    uint8_t *src_p, *dst_p;


    if (input == NULL) {
        display_menu();
        return;
    }

    char f = input[0];

    if (f == '?') {
        display_menu();
    } else if (f == 's') {       // show [s]chedule
        display_schedule();
    } else if (f == 'S') {       // [S]ave to flash
        flash_save(FLASH_ADDR, (void *)&s, sizeof(s));
        display_menu();
    } else if (f == 'L') {       // [L]oad defaults
        src_p = (uint8_t *) & defaults;
        dst_p = (uint8_t *) & s;
        for (i = sizeof(s); i > 0; i--) {
            *dst_p++ = *src_p++;
        }
        display_menu();
        //tue = s.standby_time;
    } else if (f == 'D') {       // get relative delta
        s.eadc_delta = 0 - eadc.conv;
        display_menu();
    } else if (f == 'E') {       // [E]xternal ADC
        if (str_to_uint16(input, &u16, 1, strlen(input), 0, 1) == EXIT_SUCCESS) {
            s.eadc_en = u16;
            display_menu();
        }
    } else if (f == 'A') {       // internal [A]DC
        if (str_to_uint16(input, &u16, 1, strlen(input), 0, 1) == EXIT_SUCCESS) {
            s.adc_en = u16;
            display_menu();
        }
    } else if (f == 'T') {       // generic [T]imeout
        if (str_to_uint16(input, &u16, 1, strlen(input), 30, 65535) == EXIT_SUCCESS) {
            s.standby_time = u16;
            display_menu();
        }
    } else if (f == 'U') {       // [U]nused timeout
        if (str_to_uint16(input, &u16, 1, strlen(input), 30, 65535) == EXIT_SUCCESS) {
            s.standby_unused = u16;
            display_menu();
        } 
    } else {
        uart1_tx_str("\e[31;1merr\e[0m\r\n", 17);
    }
}
