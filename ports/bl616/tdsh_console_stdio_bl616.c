// TinyTang — the shell's printf family, routed to the shell's output.
//
// The shell core is compiled with printf/puts/putchar redirected here (see
// tdsh_stdio_redirect.h), because the SDK's printf goes to the UART.  The
// output is tdsh_bl616_output: the console, or, while the shell runs in
// TinyDesk's Terminal window, that window and the USB console.

#include "tdsh_bl616.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int tdsh_printf(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        size_t len = (size_t)n < sizeof(buf) ? (size_t)n : sizeof(buf) - 1;
        tdsh_bl616_output(buf, len);
    }
    return n;
}

int tdsh_puts(const char *s)
{
    tdsh_bl616_output(s, strlen(s));
    tdsh_bl616_output("\r\n", 2);
    return 0;
}

int tdsh_putchar(int c)
{
    char ch = (char)c;
    tdsh_bl616_output(&ch, 1);
    return c;
}
