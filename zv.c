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
#include <assert.h>
#include <fcntl.h>

#define ARRAY_BLK 128

#define __FILENAME__ (strrchr(__FILE__, '/') ? (strrchr(__FILE__, '/') + 1) : __FILE__)

#define _print_info(ident, fp, fmt)					\
    fprintf((fp), "[%s:%s:%d]: ", (ident), (__FILENAME__), (__LINE__));	\
    va_list args;							\
    va_start(args, fmt);						\
    vfprintf((fp), fmt, args);						\
    va_end(args);							\
    putc('\n', (fp));							\
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
    fprintf(stderr, "[ERROR:%s:%d]: ", __FILENAME__, __LINE__);
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

// ===============================
// alloc array dynamically
static void* array_alloc(void *arr, int cnt, int size) {
    void *narr = realloc(arr, cnt * size);
    if (narr == NULL)
	zv_err("realloc error");

    return narr;
}

// ===============================
// common event related behaviors

static int adjust_pri(zv_watcher *w) {
    assert(w);

    int pri = w -> priority;
    if (pri < ZV_MIN_PRI)
	pri = ZV_MIN_PRI;
    if (pri > ZV_MAX_PRI)
	pri = ZV_MAX_PRI;
    return pri;
}

/* feed an occurred event to `zv_loop` */
void zv_feed_event(zv_loop *lp, zv_watcher *w, int revents) {
    int pri = adjust_pri(w);

    if (w -> pending) {
	/* if watcher is already in pending list */
	lp -> anpendings[pri][w->pending-1].events |= revents;
	return;
    }
    if (lp -> pendingcnt[pri] == lp -> pendingmax[pri] || lp -> pendingmax[pri] == 0) {
	// need more sapce
	array_alloc(lp -> anpendings, lp -> pendingmax[pri]+ARRAY_BLK, sizeof(struct ANPENDING));
	lp -> pendingmax[pri] += ARRAY_BLK;
    }
    w -> pending = ++(lp -> pendingcnt[pri]);
    (lp -> anpendings[pri])[w -> pending - 1].watcher = w;	
    (lp -> anpendings[pri])[w -> pending - 1].events = revents;    
}

// ===============================
/* file desriptor related events */

// feed events happened on fd to `zv_loop`
void fd_event(zv_loop *lp, int fd, int revents) {
    assert(fd >= 0 && fd <= ZV_OPENFD_MAX);

    struct ANFD *anfdp = (lp -> anfds)[fd];
    assert(anfdp);
    
    struct ANFD anfd;
    for (int i=0; i<(lp -> anfds_cnt)[fd]; i++) {
	anfd = *(anfdp + i);
	if (anfd.active && (anfd.events & revents)) {
	    zv_feed_event(lp, anfd.watcher, revents & anfd.events);
	}
    }
}

// check if fd opened on a file
int fd_valid(int fd) {
    if (fcntl(fd, F_GETFD) == -1)
	return 0;
    return 1;
}

// kill a fd
void fd_kill(zv_loop *lp, int fd) {
    assert(fd >= 0 && fd <= ZV_OPENFD_MAX);

    struct ANFD *anfdp = (lp -> anfds)[fd];
    assert(anfdp);

    struct ANFD anfd;
    for (int i=0; i<(lp -> anfds_cnt)[fd]; i++) {
	anfd = *(anfdp + i);
	if (anfd.active) {
	    zv_io_stop(lp, (zv_io *)anfd.watcher);
	    anfd.active = 0;
	    // events on fd are interrupted, so we sent an error
	    zv_feed_event(lp, anfd.watcher, ZV_ERROR | ZV_READ | ZV_WRITE);
	}
    }    
}

void fd_change(zv_loop *lp, int fd) {
    assert(fd >= 0 && fd <= ZV_OPENFD_MAX);
    assert(fd_valid(fd));
    
    lp -> fdchanges[fd] = 1;
}
