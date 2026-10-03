// TinyTang — force-included into the TinyDesk Shell core sources.
//
// The shell prints with printf, and with CONFIG_NEWLIB off (which it must be;
// see proj.conf) printf is the SDK's, which writes to the UART.  That object
// cannot be replaced -- it also defines bflb_uart_set_console, which board.c
// needs, so the linker always pulls it in and a second printf is a duplicate
// symbol.
//
// So the shell's own translation units are compiled with printf/puts/putchar
// redirected to this port's functions, which write to the USB CDC console.
// Only the shell core gets this; the SDK keeps its own.

#ifndef TDSH_STDIO_REDIRECT_H
#define TDSH_STDIO_REDIRECT_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int tdsh_printf(const char *fmt, ...);
int tdsh_puts(const char *s);
int tdsh_putchar(int c);

#ifdef __cplusplus
}
#endif

#define printf  tdsh_printf
#define puts    tdsh_puts
#define putchar tdsh_putchar

#endif /* TDSH_STDIO_REDIRECT_H */
