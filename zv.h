#ifndef _ZV_H_
#define _ZV_H_

#include "config.h"

typedef double zv_tstamp;

struct zv_loop;

#define ZV_CB_DECLARE(type) void (*cb)(struct zv_loop *lp, struct type *w, int revents);

#define WATCHER(type) \
    int active;            \
    int priority;          \
    int pending;           \
    void *data; /* user defined data*/    \
    ZV_CB_DECLARE(type)

// used to represent a wathcer, such as zv_io
struct zv_watcher {
    WATCHER(zv_watcher)
};

typedef struct zv_io {
    WATCHER(zv_io)
    int fd;
    int events;
} zv_io;

typedef struct zv_timer {
    WATCHER(zv_timer)
    zv_tstamp at;
    zv_tstamp repeat;    
} zv_timer;

typedef struct zv_prepare {
    WATCHER(zv_prepare)    
} zv_prepare;

typedef struct zv_check {
    WATCHER(zv_check)
} zv_check;

typedef struct zv_signal {
    WATCHER(zv_signal)
    struct zv_signal *next;
    int signo;    
} zv_signal;

typedef struct zv_idle {
    WATCHER(zv_idle)
} zv_idle;

// ================================
// loop related data structures
struct ANFD {
    struct zv_watcher watcher;
    struct zv_watcher *next;	/* a fd may have many watchers */
    int events;
};

struct ANPENDING {
    struct zv_watcher *w;
    int events;
};

typedef struct zv_loop {
    zv_tstamp zv_now;
    int loop_cnt;		/* how many loops have been so far */
    void (*backend_modify) (struct zv_loop *loop, int fd, int nev, int oev);
    void (*backend_poll) (struct zv_loop *loop, zv_tstamp timeout);
    int backend_fd;		/* for example, epoll use it */
    
#ifdef EPOLL_BACKEND
    struct epoll_event *epoll_events;
    int epoll_eventmax;
#endif // EPOLL_BACKEND

    /* current watching fds */
    struct ANFD anfds[ZV_OPENFD_MAX];
    int anfds_cnt;
    
    /* current pending events */
    struct ANPENDING *anpendings[NUM_PRI];
    int pendingcnt[NUM_PRI];

    /* fds whose events is about to change */
    int fdchanges[ZV_OPENFD_MAX];
    int fdchange_cnt;
    
    struct zv_timer *timers;

    struct zv_idle **idles[NUM_PRI];
    int idle_cnt[NUM_PRI];

    struct zv_prepare *prepares;
    int prepare_cnt;
    
    struct zv_check *check;
    int check_cnt;
} zv_loop;

// ================================
// common functions
zv_tstamp zv_time(void);

void zv_err(const char *cmt, ...);

void zv_warn(const char *cmt, ...);

void zv_info(const char *cmt, ...);

void zv_debug(const char *cmt, ...);

#endif // _ZV_H
