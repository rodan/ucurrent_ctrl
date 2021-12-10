
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "glue.h"
#include "version.h"
#include "uart_mapping.h"
#include "rtca_now.h"
#include "ui.h"

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

    if (input == NULL) {
        display_menu();
        return;
    }

    char f = input[0];

    if (f == '?') {
        display_menu();
    } 
}
