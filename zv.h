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
struct zv_watcher;

#define ZV_CB_DECLARE(type) void (*cb)(struct zv_loop *lp, struct type *w, int revents);
typedef void (*w_cb) (struct zv_loop *lp, struct zv_watcher *w, int revents);

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
    int idx;
} zv_prepare;

typedef struct zv_check {
    WATCHER(zv_check)
    int idx;
} zv_check;

typedef struct zv_signal {
    WATCHER(zv_signal)
    int signo;
    int idx;
} zv_signal;

typedef struct zv_idle {
    WATCHER(zv_idle)
    int idx;
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
    int active;
};

typedef struct zv_loop {
    int is_default;		/* indicate wether this is default loop */
    int backend;
    zv_tstamp zv_now;
    int activecnt;		/* how many watchers hold the loop right now */
    int loop_cnt;		/* how many loops have been so far */
    void (*backend_modify) (struct zv_loop *loop, int fd, int evs);
    void (*backend_poll) (struct zv_loop *loop, zv_tstamp timedout);
    int backend_fd;		/* for example, epoll use it */
    
#ifdef EPOLL_BACKEND
    struct epoll_event *epoll_events;
    int epoll_eventmax;
#endif // EPOLL_BACKEND

    /* current watching fds */
    struct ANFD *anfds[ZV_OPENFD_MAX];
    int anfds_max[ZV_OPENFD_MAX];
    int anfds_cnt[ZV_OPENFD_MAX];
    
    /* current pending events */
    struct ANPENDING *anpendings[NUM_PRI];
    int pendingmax[NUM_PRI];

    /* fds whose events is about to change */
    int fdchanges[ZV_OPENFD_MAX];
    
    struct zv_timer **timers;
    int timer_max;
    int timer_cnt;
    
    struct zv_idle **idles[NUM_PRI];
    int idle_max[NUM_PRI];
    int idle_cnt[NUM_PRI];

    struct zv_prepare **prepares;
    int prepare_max;
    int prepare_cnt;
    
    struct zv_check **checks;
    int check_max;
    int check_cnt;
} zv_loop;

// ================================
// common functions
zv_tstamp zv_time(void);

void zv_err(int flag, const char *cmt, ...);

void zv_warn(const char *cmt, ...);

void zv_info(const char *cmt, ...);

void zv_debug(const char *cmt, ...);

// user interfaces
void zv_io_init(zv_io *w, w_cb cb, int fd, int events);
void zv_io_start(zv_loop *lp, zv_io *w);
void zv_io_stop(zv_loop *lp, zv_io *w);

void zv_timer_init(zv_timer *w, w_cb cb, zv_tstamp after, zv_tstamp repeat);
void zv_timer_start(zv_loop *lp, zv_timer *w);
void zv_timer_stop(zv_loop *lp, zv_timer *w);

void zv_signal_init(zv_signal *w, w_cb cb, int signo);
void zv_signal_start(zv_loop *lp, zv_signal *w);
void zv_feed_signal(zv_loop *lp, int signo);
void zv_signal_stop(zv_loop *lp, zv_signal *w);

void zv_idle_init(zv_idle *w, w_cb cb);
void zv_idle_start(zv_loop *lp, zv_idle *w);
void zv_idle_stop(zv_loop *lp, zv_idle *w);

void zv_prepare_init(zv_prepare *w, w_cb cb);
void zv_prepare_start(zv_loop *lp, zv_prepare *w);
void zv_prepare_stop(zv_loop *lp, zv_prepare *w);

void zv_check_init(zv_check *w, w_cb cb);
void zv_check_start(zv_loop *lp, zv_check *w);
void zv_check_stop(zv_loop *lp, zv_check *w);

void zv_feed_event(zv_loop *lp, zv_watcher *w, int revents);
void fd_event(zv_loop *lp, int fd, int revents);

void zv_invoke(zv_loop *lp, zv_watcher *w, int revents);
int  clear_pending(zv_loop *lp, zv_watcher *w);

void zv_loop_init(zv_loop *lp);
zv_loop *zv_default_loop();
void zv_loop_run(zv_loop *lp);

#endif // _ZV_H
