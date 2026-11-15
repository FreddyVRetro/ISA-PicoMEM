#if USE_CDROM
#include "cdrom_error_msg.h"

#include <stdarg.h>
#include <stdio.h>
#include "../../pm_debug.h"

static char _error[256];

void cdrom_errorstr_set(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(_error, 256, fmt, ap);
    PM_ERROR("[CDR] setting cdrom error: %s\n", _error);
    va_end(ap);
}

void cdrom_errorstr_clear(void) {
    _error[0] = 0;
}

bool cdrom_errorstr_is_set(void) {
    return _error[0] != 0;
}

const char* cdrom_errorstr_get(void) {
    return _error;
}

#endif // USE_CDROM
