#if USE_CDROM
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "86box_compat.h"

void fatal(const char *fmt, ...) {
    // Buffer reduced from 1024 to 128 bytes to lower stack usage (Core 0 stack is
    // only 2KB total). Fatal messages are short/fixed strings, so 128B is plenty.
    // vsnprintf() is used instead of vsprintf() to stay safe if a format string
    // ever produces more output than the buffer can hold.
    char    temp[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(temp, sizeof(temp), fmt, ap);
    printf("%s",temp);    
    va_end(ap);
    while(1) {
    }
}

#endif // USE_CDROM
