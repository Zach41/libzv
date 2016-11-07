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
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

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
	struct ANPENDING pending = (lp -> anpendings)[pri][w -> pending -1];
	assert(pending.active);
	
	pending.events |= revents;
	return;
    }

    int idx;
    struct ANPENDING pending;
    for (idx = 0; idx < (lp -> pendingmax)[pri]; idx++) {
	pending = (lp -> anpendings)[pri][idx];
	if (pending.active == 0)
	    break;
    }
    if (idx == lp -> pendingmax[pri]) {
	// need more space
	(lp -> anpendings)[pri] = array_alloc((lp -> anpendings)[pri],
		    lp -> pendingmax[pri]+ARRAY_BLK,
		    sizeof(struct ANPENDING));
	lp -> pendingmax[pri] += ARRAY_BLK;
    }
    w -> pending = idx + 1;
    (lp -> anpendings)[pri][idx].active = 1;
    (lp -> anpendings)[pri][idx].events = revents;
    (lp -> anpendings)[pri][idx].watcher = w;
}

/* void queue_events(zv_loop *lp, zv_watcher **w, int eventcnt, int type) { */
/*     for (int i=0; i<eventcnt; i++) { */
/* 	zv_feed_event(lp, w[i], type); */
/*     } */
/* } */

// ===============================
/* file desriptor related events */

// feed events happened on fd to `zv_loop`
void fd_event(zv_loop *lp, int fd, int revents) {
    assert(fd >= 0 && fd <= ZV_OPENFD_MAX);

    struct ANFD *anfdp = (lp -> anfds)[fd];
    assert(anfdp);
    
    struct ANFD anfd;
    for (int i=0; i<(lp -> anfds_max)[fd]; i++) {
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
    for (int i=0; i<(lp -> anfds_max)[fd]; i++) {
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

void fd_reify(zv_loop *lp) {
    assert(lp);

    struct ANFD anfd;
    for (int fd = 0; fd<ZV_OPENFD_MAX; fd++) {
	if ((lp -> fdchanges)[fd]) {
	    int events = ZV_NONE;
	    for (int j=0; j<(lp -> anfds_max)[fd]; j++) {
		anfd = ((lp -> anfds)[fd])[j];
		if (anfd.active == 0)
		    continue;
		
		zv_io *w = (zv_io *)(anfd.watcher);
		if (w -> active == 0)
		    continue;
		if (w -> events != anfd.events) {
		    anfd.events = w -> events;
		}
		events |= w -> events;
	    }
	    lp -> backend_modify(lp, fd, events, NULL);
	    (lp -> fdchanges)[fd] = 0;
	}
    }
}

// ===============================
// invoke pending events & clear pendings

int clear_pending(zv_loop *lp, zv_watcher *w) {
    assert(lp && w);
    if (w -> pending) {
	int pri = adjust_pri(w);
	struct ANPENDING pending = (lp -> anpendings)[pri][w -> pending - 1];
	assert(pending.active);

	int events = pending.events;
	pending.events = ZV_NONE;
	pending.active = 0;
	pending.watcher = NULL;
	    
	return events;
    }
    return 0;    
}

void zv_invoke(zv_loop *lp, zv_watcher *w, int revents) {
    assert(lp && w);
    
    (w -> cb)(lp, w, revents);
}

void call_pending(zv_loop *lp) {
    for (int pri = ZV_MAX_PRI; pri >= ZV_MIN_PRI; pri--) {
	struct ANPENDING *pendings = (lp -> anpendings)[pri];
	struct ANPENDING pending;
	for (int i=0; i<(lp -> pendingmax)[pri]; i++) {
	    pending = *(pendings + i);
	    if (pending.active) {
		assert(pending.watcher);

		zv_invoke(lp, pending.watcher, pending.events);
	    }
	    pending.active = 0;
	    pending.events = ZV_NONE;
	    pending.watcher = NULL;
	}
    }
}


// ==================================
// timers

void timers_reify(zv_loop *lp) {
    assert(lp);
    
    zv_tstamp now = lp -> zv_now;

    zv_timer *top;
    while (!theap_isempty(lp) &&
	   (top = theap_deletemin(lp)) && (top -> at < now)) {
	if (top -> repeat > 0.0) {
	    top -> at = now + top -> repeat;
	    theap_insert(top, lp);
	} else {
	    zv_timer_stop(lp, top);
	}
	if (top -> active)
	    zv_feed_event(lp, (zv_watcher *)top, ZV_TIMEDOUT);
    }

    lp -> zv_now = zv_time();
}

// ===================================
// idles

void idles_reify(zv_loop *lp) {
    assert(lp);

    zv_idle **idles;
    for (int pri = ZV_MAX_PRI; pri >= ZV_MIN_PRI; pri--) {
	if ((lp -> idle_cnt)[pri] == 0)
	    continue;
	idles = (lp -> idles)[pri];
	for (int i=0; i<(lp -> idle_cnt)[pri]; i++) {
	    zv_idle *idle = idles[i];
	    if (idle -> active) {
		zv_feed_event(lp, (zv_watcher *)idle, ZV_IDLE);
	    }
	}
    }
}

// ====================================
// signals

static int pipefd[2];
static zv_io *sig_io;
static int sigrefs[SIGNUM];
static zv_signal **signals[SIGNUM];
static int signals_max[SIGNUM];

/* we use a thread to receive signals */
void * sig_handler(void *arg) {
    (void)arg;			/* unused */
    int err, signo;

    sigset_t mask;
    sigfillset(&mask);
    for (;;) {
	err = sigwait(&mask, &signo);
	if (err != 0) {
	    zv_err("sigwait error");
	}
	if (sigrefs[signo])
	    write(pipefd[1], (char *)&signo, 1);
    }
}

static void sig_cb(zv_loop *lp, zv_watcher *w, int revents) {
    int signo, n;
    if (revents & ZV_READ) {
	n = read(pipefd[0], &signo, 1);
	if (n != 1) {
	    zv_err("read from signal pipe error");
	}
	zv_feed_signal(lp, signo);
    }
}

// ====================================
// zv_loop

void zv_loop_init(zv_loop *lp) {
    assert(lp);

    lp -> zv_now = zv_time();
    lp -> loop_cnt = 0;

    // TODO: setup backends

    for (int fd=0; fd<ZV_OPENFD_MAX; fd++) {
	(lp -> anfds)[fd] = NULL;
	(lp -> anfds_max)[fd] = 0;
	(lp -> fdchanges)[fd] = 0;
    }

    for (int pri=ZV_MIN_PRI; pri<=ZV_MAX_PRI; pri++) {
	(lp -> anpendings)[pri] = NULL;
	(lp -> pendingmax)[pri] = 0;

	(lp -> idles)[pri] = NULL;
	(lp -> idle_max)[pri] = 0;
	(lp -> idle_cnt)[pri] = 0;
    }

    theap_init(lp);

    lp -> prepares = NULL;
    lp -> prepare_max = lp -> prepare_cnt = 0;
    
    lp -> checks = NULL;
    lp -> check_max = lp -> check_cnt = 0;

    /* for (int signo = 0; signo < SIGNUM; signo++) { */
    /* 	(lp -> signals)[signo] = NULL; */
    /* 	(lp -> signals_cnt)[signo] = 0; */
    /* 	(lp -> signals_max)[signo] = 0; */
    /* } */

    lp -> is_default = 0;
}

pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;

zv_loop *zv_default_loop() {
    static zv_loop *lp;
    
    pthread_mutex_lock(&init_mutex);
    if (lp == NULL) {
	lp = (zv_loop *)calloc(1, sizeof(zv_loop));
	if (lp == NULL)
	    zv_err("calloc error");
	zv_loop_init(lp);
	lp -> is_default = 1;

	// only default loop deals with signal
	if (pipe(pipefd) < 0)
	    zv_err("pipe error");
	zv_io_init(sig_io, sig_cb, pipefd[1], ZV_READ);
	zv_io_start(lp, sig_io);
    }
    pthread_mutex_unlock(&init_mutex);

    return lp;    
}

void ref_loop(zv_loop *lp) {
    (lp -> activecnt)++;
}

void unref_loop(zv_loop *lp) {
    (lp -> activecnt)--;
}

void zv_loop_run(zv_loop *lp) {
    assert(lp);
    
    call_pending(lp);		/* incase there is any pending events */

    do {
	// prepare events
	if (lp -> prepare_cnt) {
	    int cnt = lp -> prepare_cnt;
	    zv_prepare *w;
	    for (int i=0; i<(lp -> prepare_max); i++) {
		w = (lp -> prepares)[i];
		if (w && w -> active) {
		    zv_feed_event(lp, (zv_watcher *)w, ZV_PREPARE);
		    cnt--;
		}
		if (cnt == 0)
		    break;
	    }
	}
	call_pending(lp);

	// fd events
	fd_reify(lp);

	// caculate blocking time
	zv_tstamp block = (theap_findmin(lp) -> at) - lp -> zv_now;
	(lp -> backend_poll)(lp, block);

	timers_reify(lp);

	call_pending(lp);

	/* checks */
	if (lp -> check_cnt) {
	    int cnt = lp -> check_cnt;
	    zv_check *w;
	    for (int i=0; i<(lp -> check_max); i++) {
		w = (lp -> checks)[i];
		if (w && w -> active) {
		    zv_feed_event(lp, (zv_watcher *)w, ZV_CHECK);
		    cnt--;
		}
		if (cnt == 0)
		    break;
	    }
	}
	call_pending(lp);	
    } while (lp -> activecnt);
}


// =================================
static void zv_init(zv_watcher *w, w_cb cb) {
    assert(w);

    w -> active = 0;
    w -> priority = DEFEAUL_PRI;
    w -> pending = 0;
    w -> data = NULL;
    w -> cb = cb;
}

static void zv_set_priority(zv_watcher *w, int npri, int *opri) {
    assert(w);

    if (opri) {
	*opri = w -> priority;
    }
    w -> priority = npri;
    adjust_pri(w);    
}

static void zv_start(zv_loop *lp, zv_watcher *w) {
    adjust_pri(w);
    w -> active = 1;
    ref_loop(lp);
}

static void zv_stop(zv_loop *lp, zv_watcher *w) {
    w -> active = 0;
    unref_loop(lp);
}

static void add_anfd(zv_loop *lp, int fd, zv_io *w) {
    assert(fd >= 0 && fd <= ZV_OPENFD_MAX);
    
    int idx;
    struct ANFD *anfds = (lp -> anfds)[fd];
    for (idx = 0; idx <= (lp -> anfds_max)[fd]; idx++) {
	if (anfds[idx].active == 0)
	    break;
    }
    if (idx == (lp -> anfds_max)[fd]) {
	(lp -> anfds)[fd] = array_alloc((lp -> anfds)[fd],
					(lp -> anfds_max)[fd] + ARRAY_BLK,
					sizeof(struct ANFD));
	(lp -> anfds_max)[fd] += ARRAY_BLK;
	anfds = (lp -> anfds)[fd];
    }
    anfds[idx].watcher = (zv_watcher *)w;
    anfds[idx].events = w -> events;
    anfds[idx].active = w -> active;
}

static void delete_anfd(zv_loop *lp, int fd, zv_io *w) {
    assert(fd >= 0 && fd <= ZV_OPENFD_MAX);

    int idx;
    struct ANFD *anfds = (lp -> anfds)[fd];
    for (idx = 0; idx < (lp -> anfds_max)[fd]; idx++) {
	if (anfds[idx].watcher == (zv_watcher *)w) {
	    anfds[idx].active = 0;
	    anfds[idx].watcher = NULL;
	    break;
	}
    }
}

/* zv_io */
void zv_io_init(zv_io *w, w_cb cb, int fd, int events) {
    zv_init((zv_watcher *)w, cb);

    w -> fd = fd;
    w -> events = events;
}

void zv_io_start(zv_loop *lp, zv_io *w) {
    assert(lp && w);
    assert(w -> fd <= ZV_OPENFD_MAX && w -> fd >= 0);
    
    if (w -> active)
	return;
    zv_start(lp, (zv_watcher *)w);

    add_anfd(lp, w -> fd, w);
    fd_change(lp, w -> fd);
}

void zv_io_stop(zv_loop *lp, zv_io *w) {
    assert(lp && w);
    
    clear_pending(lp,(zv_watcher *)w);

    if (w -> active == 0)
	return;

    delete_anfd(lp, w -> fd, w);
    zv_stop(lp, (zv_watcher *)w);
}

/* zv_timer */
void zv_timer_init(zv_timer *w, w_cb cb, zv_tstamp after, zv_tstamp repeat) {
    assert(after >= 0.0);
    
    zv_init((zv_watcher *)w, cb);

    w -> at = zv_time() + after;
    if (repeat > 0.0) {
	w -> repeat = repeat;
    }
}

void zv_timer_start(zv_loop *lp, zv_timer *w) {
    assert(lp && w);

    if (w -> active)
	return;
    zv_start(lp, (zv_watcher *)w);

    theap_insert(w, lp);
}

void zv_timer_stop(zv_loop *lp, zv_timer *w) {
    assert(lp && w);
    clear_pending(lp, (zv_watcher *)w);
    w -> repeat = 0.0;
    zv_stop(lp, ( zv_watcher *)w);
}

/* zv_signal */
void zv_signal_init(zv_signal *w, w_cb cb, int signo) {
    assert(w);
    zv_init((zv_watcher *)w, cb);
    w -> priority = ZV_MAX_PRI;	/* we give signal the highest priority */
    w -> signo = signo;
}

void zv_signal_start(zv_loop *lp, zv_signal *w) {
    assert(lp && w);
    assert(w -> signo >=0 && w -> signo <= SIGNUM);
    
    if (!lp -> is_default)
	return;    
    if (w -> active)
	return;
    
    zv_start(lp, (zv_watcher *)w);    
    sigrefs[w -> signo] += 1;

    int idx, signo = w -> signo;
    zv_signal **sigs = signals[signo];
    
    for (idx = 0; idx < signals_max[signo]; idx++) {
	if (sigs[idx] == NULL || sigs[idx] -> active == 0)
	    break;
    }
    if (idx == signals_max[signo]) {
	signals[signo] = array_alloc(signals[signo],
				     signals_max[signo] + ARRAY_BLK,
				     sizeof(void *));
	signals_max[signo] += ARRAY_BLK;
	sigs = signals[signo];
    }
    sigs[idx] = w;
    w -> idx = idx;
}

void zv_signal_stop(zv_loop *lp, zv_signal *w) {
    assert(lp && w);
    assert(w -> signo >= 0 && w -> signo <= SIGNUM);

    if (!w -> active)
	return;
    if (!lp -> is_default)
	return;

    zv_stop(lp, (zv_watcher *)w);
    sigrefs[w -> signo] -= 1;

    zv_signal **sigs = signals[w -> signo];
    sigs[w -> idx] = NULL;
}

void zv_feed_signal(zv_loop *lp, int signo) {
    assert(lp);
    assert(signo >= 0 && signo <= SIGNUM);
    
    if (!lp -> is_default)
	return;

    zv_signal **sigs = signals[signo];   
    for (int idx = 0; idx<signals_max[signo]; idx++) {
	if (sigs[idx] && sigs[idx] -> active) {
	    zv_feed_event(lp, (zv_watcher *)sigs[idx], ZV_SIGNAL);
	}
    }
}

/* zv_idle */
void zv_idle_init(zv_idle *w, w_cb cb) {
    assert(w);

    zv_init((zv_watcher *)w, cb);
    w -> priority = ZV_MIN_PRI;	/* idle's default priority is min_pri */
}

void zv_idle_start(zv_loop *lp, zv_idle *w) {
    assert(lp && w);

    if (w -> active)
	return;
    zv_start(lp, (zv_watcher *)w);

    int pri = w -> priority, idx;
    zv_idle **idles = (lp -> idles)[pri];
    zv_idle *idle;
    for (idx = 0; idx < (lp -> idle_max)[pri]; idx++) {
	idle = idles[idx];
	if (idle == NULL || idle -> active == 0) {
	    break;
	}
    }
    if (idx == (lp -> idle_max)[pri]) {
	(lp -> idles)[pri] = array_alloc((lp -> idles)[pri],
					 (lp -> idle_max)[pri] + ARRAY_BLK,
					 sizeof(void *));
	(lp -> idle_max)[pri] += ARRAY_BLK;
	idles = (lp -> idles)[pri];
    }
    idles[idx] = w;
    w -> idx = idx;
    (lp -> idle_cnt)[pri] += 1;
}

void zv_idle_stop(zv_loop *lp, zv_idle *w) {
    assert(lp && w);

    if (!w -> active)
	return;
    zv_stop(lp, (zv_watcher *)w);

    (lp -> idles)[w -> priority][w -> idx] = NULL;
}

/* zv_prepare */
void zv_prepare_init(zv_prepare *w, w_cb cb) {
    assert(w);

    zv_init((zv_watcher *)w, cb);    
}

void zv_prepare_start(zv_loop *lp, zv_prepare *w) {
    assert(lp && w);
    if (w -> active)
	return;
    zv_start(lp, (zv_watcher *)w);

    int idx;
    zv_prepare **prepares = (lp -> prepares), *prepare;
    for (idx = 0; idx < (lp -> prepare_max); idx++) {
	prepare = prepares[idx];
	if (prepare == NULL || prepare -> active == 0) {
	    break;
	}
    }
    if (idx == (lp -> prepare_max)) {
	(lp -> prepares) = array_alloc((lp -> prepares),
				       (lp -> prepare_max) + ARRAY_BLK,
				       sizeof(void *));
	lp -> prepare_max += ARRAY_BLK;
	prepares = (lp -> prepares);
    }
    prepares[idx] = w;
    w -> idx = idx;
    lp -> prepare_cnt += 1;
}

void zv_prepare_stop(zv_loop *lp, zv_prepare *w) {
    assert(lp && w);
    if (!w -> active)
	return;

    zv_stop(lp, (zv_watcher *)w);
    (lp -> prepares)[w -> idx] = NULL;
}

/* zv_check */
void zv_check_init(zv_check *w, w_cb cb) {
    assert(w);

    zv_init((zv_watcher *)w, cb);
}

void zv_check_start(zv_loop *lp, zv_check *w) {
    assert(lp && w);

    if (w -> active)
	return;
    zv_start(lp, (zv_watcher *)w);

    int idx;
    zv_check **checks = (lp -> checks), *check;
    for (idx = 0; idx < (lp -> check_max); idx++) {
	check = checks[idx];
	if (check == NULL || check -> active == 0)
	    break;
    }
    if (idx == (lp -> check_max)) {
	(lp -> checks) = array_alloc((lp -> checks),
				     (lp -> check_max) + ARRAY_BLK,
				     sizeof(void *));
	(lp -> check_max) += ARRAY_BLK;
	checks = (lp -> checks);
    }
    checks[idx] = w;
    w -> idx = idx;
    (lp -> check_cnt) += 1;
}

void zv_check_stop(zv_loop *lp, zv_check *w) {
    assert(lp && w);
    if (!w -> active)
	return;

    zv_stop(lp, (zv_watcher *)w);
    (lp -> checks)[w -> idx] = NULL;
}
