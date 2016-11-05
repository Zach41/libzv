#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "timer_heap.h"
#include "zv.h"

static struct zv_timer *sen_timer(void) {
    struct zv_timer *sen_timer;
    sen_timer = (struct zv_timer *)calloc(1, sizeof(struct zv_timer));
    if (sen_timer == NULL)
	zv_err("calloc error");
    sen_timer -> at = -1.0;

    return sen_timer;
}

void theap_init(struct zv_loop *lp) {
    assert(lp);

    lp -> timers = (struct zv_timer **)calloc((TIMER_BLK+1), sizeof(void *));
    if (lp -> timers == NULL) {
	zv_err("calloc error");
    }

    lp -> timer_cnt = 0;
    lp -> timer_max = TIMER_BLK;
    // sentinel    
    lp -> timers[0] = sen_timer();
    lp -> timer_cnt = 0;    
}

void theap_destroy(struct zv_loop *lp) {
    assert(lp);

    free(lp -> timers);
    lp -> timers = NULL;	/* protect again dangling pointers */
    if (errno)
	zv_err("free error");
    lp -> timer_cnt = 0;
    lp -> timer_max = 0;
}

void theap_makeempty(struct zv_loop *lp) {
    assert(lp);

    lp -> timers = (struct zv_timer **)calloc((TIMER_BLK+1), sizeof(void *));
    if (lp -> timers == NULL)
	zv_err("calloc error");
    lp -> timer_cnt = 0;
    lp -> timer_max = TIMER_BLK;
    lp -> timers[0] = sen_timer();
}

void theap_insert(struct zv_timer *w, struct zv_loop *lp) {
    assert(lp && lp -> timers);
    assert(w && w -> at >= 0);
    
    if (lp -> timer_cnt == lp -> timer_max) {
	lp -> timer_max += TIMER_BLK;
	lp -> timers = (struct zv_timer **)realloc(lp -> timers,
						   sizeof(void *) * (lp -> timer_max + 1));
	if (lp -> timers == NULL)
	    zv_err("realloc error");
    }
    int i;

    for (i = ++(lp -> timer_cnt); (lp -> timers[i/2] -> at) > w -> at; i /= 2) {
	lp -> timers[i] = lp -> timers[i/2];
    }
    lp -> timers[i] = w;
}

struct zv_timer *theap_findmin(struct zv_loop *lp) {
    assert(lp);
    if (theap_isempty(lp))
	zv_err("timer heap is empty");

    return lp -> timers[1];
}

struct zv_timer *theap_deletemin(struct zv_loop *lp) {
    assert(lp);
    if (theap_isempty(lp))
	zv_err("timer heap is empty");
    
    int i, child;
    struct zv_timer *min_timer, *last_timer;

    min_timer = lp -> timers[1];
    last_timer = lp -> timers[(lp -> timer_cnt)--];
    
    for (i = 1; i*2 <= lp -> timer_cnt; i=child) {
	child = 2*i;
	if (child != lp -> timer_cnt &&
	    (lp -> timers[child] -> at) > (lp -> timers[child+1] -> at))
	    child++;
	if (last_timer -> at > (lp -> timers[child] -> at))
	    lp -> timers[i] = lp -> timers[child];
	else
	    break;
    }
    lp -> timers[i] = last_timer;

    return min_timer;
}

int theap_isempty(struct zv_loop *lp) {
    assert(lp);

    return lp -> timer_cnt == 0;
}
