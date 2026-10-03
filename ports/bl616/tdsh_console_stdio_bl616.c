// TinyTang — the shell's printf family, routed to the USB CDC console.
//
// The shell core is compiled with printf/puts/putchar redirected here (see
// tdsh_stdio_redirect.h).  The prompt already worked because it goes through
// the port's console callbacks; command output did not, because it goes
// through the SDK's printf to the UART.

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
        tdsh_bl616_console_write(buf, len);
    }
    return n;
}

int tdsh_puts(const char *s)
{
    tdsh_bl616_console_write(s, strlen(s));
    tdsh_bl616_console_write("\r\n", 2);
    return 0;
}

int tdsh_putchar(int c)
{
    char ch = (char)c;
    tdsh_bl616_console_write(&ch, 1);
    return c;
}
