#include "config.h"
#include "zv.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef CLOCK_TIME_BACKEND
#include <time.h>
#else
#include <sys/time.h>
#endif // CLOCK_TIME_BACKEND
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#define _print_info(ident, fp, fmt)           \
	fprintf((fp), "[%s]: ", (ident));     \
	va_list args;                         \
	va_start(args, fmt);		      \
	vfprintf((fp), fmt, args);            \
	va_end(args);                         \
	putc('\n', (fp));		      \
	fflush((fp));


// ===============================
zv_tstamp zv_time(void) {
    zv_tstamp now;
#ifdef CLOCK_TIME_BACKEND
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) < 0) {
	zv_err("clock_gettime error");
    }
    now = ts.tv_sec * 1e-9 + ts.tv_nsec;
#else
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) {
	zv_err("gettimeofday error");
    }
    now = tv.tv_sec * 1e-6 + tv.tv_usec;
#endif
    return now;
}

void zv_err(const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "[ERROR]: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    if (errno) {
	fprintf(stderr, ": %s\n", strerror(errno));
    } else {
	putc('\n', stderr);
    }
    va_end(args);
    exit(-1);
}

void zv_warn(const char *fmt, ...) {
    _print_info("WARNING", stdout, fmt)
}

void zv_info(const char *fmt, ...) {
    _print_info("INFO", stdout, fmt)
}

void zv_debug(const char *fmt, ...) {
    _print_info("DEBUG", stdout, fmt)
}
