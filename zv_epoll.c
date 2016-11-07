// epoll backended method

#include "zv.h"
#include "config.h"

#include <sys/epoll.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

void fd_kill(zv_loop *lp, int fd);
void fd_event(zv_loop *lp, int fd, int revents);

static void epoll_modify(zv_loop *lp, int fd, int nevs) {
    assert(lp);
    assert(fd >= 0 && fd <= ZV_OPENFD_MAX);
    if (!nevs)
	return;
    if (nevs == -1) {
	/* we delete fd */
	if (epoll_ctl(lp -> backend_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
	    zv_err(0, "err happened on epoll_modified while delete a fd: %d", fd);
	}
	return;
    }

    struct epoll_event ev;
    ev.events = ((nevs & ZV_READ) ? EPOLLIN : 0) | ((nevs & ZV_WRITE ) ? EPOLLOUT : 0);
    ev.data.fd = fd;

    if (epoll_ctl(lp -> backend_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
	if (errno == ENOENT) {
	    if (epoll_ctl(lp -> backend_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		fd_kill(lp, fd);
		zv_err(0, "epoll_modified errorwhile adding a fd: %d", fd);
	    }
	}else {
	    fd_kill(lp, fd);
	    zv_err(0, "epoll_modifed error while modifying a fd: %d", fd);
	}
    }
}

static void epoll_poll(zv_loop *lp, zv_tstamp timedout) {
    assert(lp);

    int block = (timedout > 0.0) ? (int)(timedout * 1000) : -1;
    int ready;
 again:
    ready = epoll_wait(lp -> backend_fd,
			   lp -> epoll_events,
			   lp -> epoll_eventmax,
			   block);

    if (ready == -1 && errno == EINTR) {
	/* interrupted, so we restart it */
	goto again;
    } else {
	zv_err(1, "epoll_wait error");
    }
    
    struct epoll_event *ev;
    int got, fd;
    for (int i=0; i<ready; i++) {
	ev = (lp -> epoll_events) + i;
	fd = (ev -> data).fd;
	
	got = (((ev -> events) & (EPOLLIN | EPOLLHUP | EPOLLERR)) ? ZV_READ : 0) |
	    (((ev -> events)) & (EPOLLOUT | EPOLLHUP | EPOLLERR) ? ZV_WRITE : 0);
	
	fd_event(lp, fd, got);
    }
    if (ready == (lp -> epoll_eventmax)) {
	/* need more space */
	(lp -> epoll_events) = (struct epoll_event *)malloc(sizeof(struct epoll_event) *
							    (lp -> epoll_eventmax + EPOLL_EVENTBLK));
	lp -> epoll_eventmax += EPOLL_EVENTBLK;
    }    
}

void epoll_init(zv_loop *lp) {
    assert(lp);
    lp -> backend = 1;
    int epollfd;
    /* size arguement is ignored, so 256 is fine */
    epollfd = epoll_create(256);
    if (epollfd < 0)
	zv_err(1, "epoll create error");
    lp -> backend_fd = epollfd;

    lp -> epoll_eventmax = EPOLL_EVENTBLK;
    lp -> epoll_events = (struct epoll_event *)malloc(sizeof(struct epoll_event) * EPOLL_EVENTBLK);
    if (lp -> epoll_events == NULL)
	zv_err(1, "malloc error");
    lp -> backend_modify = epoll_modify;
    lp -> backend_poll = epoll_poll;
}

void epoll_destroy(zv_loop *lp) {
    lp -> backend_fd = -1;
    
    free(lp -> epoll_events);
    if(errno)
	zv_err(1, "free epoll_events error");
    lp -> epoll_events = NULL;	/* prevent from dangling pointer */
}
