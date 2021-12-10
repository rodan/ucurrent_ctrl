
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
#include "ui.h"

extern struct settings_t s;
extern int16_t tue;
extern struct ads1110_t eadc;

static const char menu_str[]="\
 available commands:\r\n\r\n\
 \e[33;1m?\e[0m - show menu\r\n";

void display_menu(void)
{
    display_version();
    uart_bcl_print(menu_str);
}

void display_version(void)
{
    char sconv[CONV_BASE_10_BUF_SZ];

    uart_bcl_print("ucurrent ctrl b");
    uart_bcl_print(_utoa(sconv, BUILD));
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
        tue = s.standby_time;
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
