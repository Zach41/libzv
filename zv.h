#ifndef _ZV_H_
#define _ZV_H_

#include "config.h"
#include "timer_heap.h"

/* event mask */
#define ZV_NONE        0x00L
#define ZV_READ        0x01L
#define ZV_WRITE       0x02L
#define ZV_TIMEDOUT    0x04L
#define ZV_SIGNAL      0x08L
#define ZV_IDLE        0x10L
#define ZV_PREPARE     0x20L
#define ZV_CHECK       0x40L
#define ZV_ERROR       0x80L

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
typedef struct zv_watcher {
    WATCHER(zv_watcher)
} zv_watcher;

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
    struct zv_watcher *watcher;
    int events;
    int active;
};

struct ANPENDING {
    struct zv_watcher *watcher;
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
    struct ANFD *anfds[ZV_OPENFD_MAX];
    int anfds_cnt[ZV_OPENFD_MAX];
    
    /* current pending events */
    struct ANPENDING *anpendings[NUM_PRI];
    int pendingcnt[NUM_PRI];
    int pendingmax[NUM_PRI];

    /* fds whose events is about to change */
    int fdchanges[ZV_OPENFD_MAX];
    
    struct zv_timer **timers;
    int timer_max;
    int timer_cnt;
    
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

// user interfaces
void zv_io_start(zv_loop *lp, zv_io *w);
void zv_io_stop(zv_loop *lp, zv_io *w);

void zv_timer_start(zv_loop *lp, zv_timer *w);
void zv_timer_stop(zv_loop *lp, zv_timer *w);

void zv_signal_start(zv_loop *lp, zv_signal *w);
void zv_signal_stop(zv_loop *lp, zv_signal *w);

void zv_idle_start(zv_loop *lp, zv_idle *w);
void zv_idle_stop(zv_loop *lp, zv_idle *w);

void zv_prepare_start(zv_loop *lp, zv_prepare *w);
void zv_prepare_stop(zv_loop *lp, zv_prepare *w);

void zv_check_start(zv_loop *lp, zv_check *w);
void zv_check_stop(zv_loop *lp, zv_check *w);

void zv_feed_event(zv_loop *lp, zv_watcher *w, int revents);
void fd_event(zv_loop *lp, int fd, int revents);


#endif // _ZV_H
