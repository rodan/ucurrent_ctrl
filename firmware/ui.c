
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "proj.h"
#include "glue.h"
#include "version.h"
#include "rtca_now.h"
#include "flash.h"
#include "ads1110.h"
#include "timer_a2.h"
#include "ui.h"

extern struct settings_t s;
extern uart_descriptor bc;

#ifdef USE_ADC
extern struct ads1110_t eadc;
#endif

void display_menu(void)
{
    char sconv[CONV_BASE_10_BUF_SZ];
    uint32_t trigger;
    uint8_t flag;

    display_version();
    uart_print(&bc, "  * settings\t\e[33;1mE\e[0m: eadc ");
    uart_print(&bc, _itoa(sconv, s.eadc_en));
    uart_print(&bc, "   \e[33;1mA\e[0m: adc ");
    uart_print(&bc, _utoa(sconv, s.adc_en));
    uart_print(&bc, "   \e[33;1mT\e[0m: s_t ");
    uart_print(&bc, _utoa(sconv, s.standby_time));
    uart_print(&bc, "   \e[33;1mU\e[0m: s_u ");
    uart_print(&bc, _utoa(sconv, s.standby_unused));
    uart_print(&bc, "\r\n");
    uart_print(&bc, "  * status:  system halt in ");
    timer_a2_get_trigger_slot(SCHEDULE_PS_HALT, &trigger, &flag);
    uart_print(&bc, _utoa(sconv, (trigger - systime())/100 ));
    uart_print(&bc, "s\r\n");

#ifdef USE_ADC
    uart_print(&bc, "  * ads1110 (status ");
    uart_print(&bc, _utoh(sconv, eadc.config));
    uart_print(&bc, ", conv ");
    uart_print(&bc, (eadc.conv + s.eadc_delta) < 0 ? "-" : "");
    uart_print(&bc, _itoa(sconv, abs((int16_t) (eadc.conv + s.eadc_delta) / 10000)));
    uart_print(&bc, ".");
    uart_print(&bc, _itoa(sconv, abs((int16_t) (eadc.conv + s.eadc_delta) % 10000)));
    uart_print(&bc, "V, rel delta ");
    uart_print(&bc, s.eadc_delta < 0 ? "-" : "");
    uart_print(&bc, _itoa(sconv, abs((int16_t) (s.eadc_delta) / 10000)));
    uart_print(&bc, ".");
    uart_print(&bc, _itoa(sconv, abs((int16_t) (s.eadc_delta) % 10000)));
    uart_print(&bc, "V)\r\n");
#endif

    uart_print(&bc, "  \e[33;1mD\e[0m: relative delta     \e[33;1mL\e[0m: load defaults     \e[33;1mS\e[0m: save settings\r\nChoice? ");
}

void display_version(void)
{
    char sconv[CONV_BASE_10_BUF_SZ];

    uart_print(&bc, "uCurrent controller rev");
    uart_print(&bc, _utoa(sconv, REV));
    uart_print(&bc, "b");
    uart_print(&bc, _utoa(sconv, BUILD));
    uart_print(&bc, "\r\n");
}

void display_schedule(void)
{
    uint8_t c;
    uint32_t trigger;
    uint8_t flag;
    char itoa_buf[CONV_BASE_10_BUF_SZ];

    for (c = 0; c < TIMER_A2_SLOTS_COUNT; c++) {
        timer_a2_get_trigger_slot(c, &trigger, &flag);
        uart_print(&bc, _utoa(itoa_buf, c));
        uart_print(&bc, " \t");
        uart_print(&bc, _utoa(itoa_buf, trigger));
        uart_print(&bc, " \t");
        uart_print(&bc, _utoa(itoa_buf, flag));
        uart_print(&bc, "\r\n");
    }
    trigger = timer_a2_get_trigger_next();
    uart_print(&bc, "sch next ");
    uart_print(&bc, _utoa(itoa_buf, trigger));
    uart_print(&bc, "\r\n");
}


#define PARSER_CNT 16

void parse_user_input(void)
{
#if defined UART_RX_USES_RINGBUF
    struct ringbuf *rbr = uart_get_rx_ringbuf(&bc);
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
    char *input = uart_get_rx_buf(&bc);
#endif

    uint16_t u16;
    uint8_t i;
    uint8_t *src_p, *dst_p;
    char sconv[CONV_BASE_10_BUF_SZ];

    if (input == NULL) {
        display_menu();
        return;
    }

    char f = input[0];

    if ((f == '?') || (f == 0x0d)) {
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
#ifdef USE_ADC
        s.eadc_delta = 0 - eadc.conv;
        display_menu();
#endif
    } else if (f == 'E') {       // [E]xternal ADC
        if (str_to_uint16(input, &u16, 1, strlen(input), 0, 1) == EXIT_SUCCESS) {
            s.eadc_en = u16;
            //if (s.eadc_en) {
            //    timer_a2_set_trigger_slot(SCHEDULE_ADC_SM, systime() + 100, TIMER_A2_EVENT_ENABLE);
            //}
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
        uart_print(&bc, "\e[31;1merr\e[0m  ");
        uart_print(&bc, _utoh(sconv, f));
        uart_print(&bc, " command unknown\r\n");
    }
}
