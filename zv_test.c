#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>

#include "zv.h"

void test_timers(void) {
    zv_loop *lp = (zv_loop *)calloc(1, sizeof(zv_loop));

    theap_init(lp);
    assert(lp -> timer_cnt == 0);
    assert(lp -> timer_max == TIMER_BLK);
    assert((lp -> timers[0] -> at) == -1.0);

    zv_timer *timers = (zv_timer *)calloc(256, sizeof(zv_timer));
    for (int i=0; i<=255; i++) {
	timers[i].at = (zv_tstamp)(256-i);
	theap_insert(&timers[i], lp);	
    }
    assert(lp -> timer_cnt == 256);
    assert(lp -> timer_max == 256);
    zv_timer *top_timer = theap_findmin(lp);
    assert(top_timer -> at == 1.0);

    zv_debug("DEBUG:%d", __COUNTER__);
    zv_timer t;
    t.at = 500.0;
    theap_insert(&t, lp);
    assert(lp -> timer_cnt == 257);
    assert(lp -> timer_max = 384);

    zv_debug("DEBUG:%d", __COUNTER__);
    zv_timer *tptr;
    for (int i=1; i<=256; i++) {
	tptr = theap_deletemin(lp);
	assert(tptr -> at == (zv_tstamp)i);
    }
    zv_debug("DEBUG:%d", __COUNTER__);
    theap_makeempty(lp);
    assert(lp -> timer_cnt == 0);
    assert(lp -> timer_max == TIMER_BLK);

    assert(theap_isempty(lp));
}

int main(void) {
    test_timers();
    
    zv_warn("warning test");
    zv_info("info test");
    zv_debug("debug test");

    /* open("noexist", O_RDONLY); */
    zv_err("err test");

    return 0;
}
