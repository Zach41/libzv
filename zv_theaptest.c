#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

#include "zv.h"

/* tests for timer heap */

static void theap_test_initdesotry(void **state) {
    (void)state;		/* unused */
    zv_loop *lp = (zv_loop *)test_calloc(1, sizeof(zv_loop));

    theap_init(lp);
    
    assert_int_equal(lp -> timer_cnt, 0);
    assert_int_equal(lp -> timer_max, TIMER_BLK);
    assert_non_null(lp -> timers);
    assert_non_null(lp -> timers[0]);
    assert_true((lp -> timers[0] -> at) == -1.0);

    theap_destroy(lp);

    assert_null(lp -> timers);
    assert_int_equal(lp -> timer_cnt, 0);
    assert_int_equal(lp -> timer_max, 0);

    test_free(lp);
}

static int theap_test_setup(void **state) {
    zv_loop *lp = (zv_loop *)test_calloc(1, sizeof(zv_loop));        
    theap_init(lp);
    *state = (void *)lp;

    return 0;
}

static int theap_test_teardown(void **state) {
    theap_destroy((zv_loop *)(*state));
    test_free(*state);
    
    return 0;
}

static void theap_test_insert(void **state) {
    zv_loop *lp = (zv_loop *)(*state);

    zv_timer *timers = (zv_timer *)test_calloc(TIMER_BLK*2, sizeof(zv_timer));
    for (int i=0; i<TIMER_BLK*2; i++) {
    	timers[i].at = i+1;
    	theap_insert(&timers[i], lp);
    }

    assert_int_equal(lp -> timer_cnt, TIMER_BLK*2);
    assert_int_equal(lp -> timer_max, TIMER_BLK*2);
    assert_non_null(lp -> timers);
    assert_true((lp -> timers[0] -> at) == -1.0);

    zv_timer *t = (zv_timer *)test_calloc(1, sizeof(zv_timer));
    t -> at = 0.5;

    theap_insert(t, lp);
    assert_int_equal(lp -> timer_cnt, TIMER_BLK * 2 + 1);
    assert_int_equal(lp -> timer_max, TIMER_BLK * 3);
    assert_true((lp -> timers[1] -> at) == 0.5);

    /* free memory */
    test_free(timers);
    test_free(t);
}

static void theap_test_deletemin(void **state) {
    zv_loop *lp = (zv_loop *)(*state);

    zv_timer *timers = (zv_timer *)test_calloc(TIMER_BLK*2, sizeof(zv_timer));
    for (int i=0; i<TIMER_BLK*2; i++) {
	timers[i].at = i+1;
	theap_insert(&timers[i], lp);
    }
    zv_timer *t;
    int cnt = TIMER_BLK*2;
    for (int i=1; i<=TIMER_BLK*2; i++) {
	t = theap_deletemin(lp);
	assert_true(t -> at ==  (zv_tstamp)i);
	assert_int_equal(lp -> timer_cnt, (--cnt));
    }

    test_free(timers);
}

static void theap_test_findmin(void **state) {
    zv_loop *lp = (zv_loop *)(*state);

    zv_timer *timers = (zv_timer *)test_calloc(TIMER_BLK*2, sizeof(zv_timer));
    for (int i=0; i<TIMER_BLK*2; i++) {
	timers[i].at = i+1;
	theap_insert(&timers[0], lp);
    }

    zv_timer *t = theap_findmin(lp);
    assert_non_null(t);
    assert_true(t -> at == 1.0);

    test_free(timers);
}

static void theap_test_isempty(void **state) {
    zv_loop *lp = (zv_loop *)(*state);

    assert_true(theap_isempty(lp));
    zv_timer *t = (zv_timer *)test_calloc(1, sizeof(zv_timer));
    t -> at = 1.0;
    theap_insert(t, lp);
    assert_false(theap_isempty(lp));

    test_free(t);
}

static void theap_test_makeempty(void **state) {
    zv_loop *lp = (zv_loop *)(*state);

    zv_timer *timers = (zv_timer *)test_calloc(TIMER_BLK*2, sizeof(zv_timer));
    for (int i=0; i<TIMER_BLK*2; i++) {
	timers[i].at = i+1;
	theap_insert(&timers[i], lp);
    }
    theap_makeempty(lp);
    assert_int_equal(lp -> timer_cnt, 0);
    assert_int_equal(lp -> timer_max, TIMER_BLK);
    assert_non_null(lp -> timers[0]);
    assert_true((lp -> timers[0] -> at) == -1.0);

    test_free(timers);
}

int main(void) {

    const struct CMUnitTest tests[] = {
	cmocka_unit_test(theap_test_initdesotry),
	cmocka_unit_test_setup_teardown(theap_test_findmin,
					theap_test_setup,
					theap_test_teardown),
	cmocka_unit_test_setup_teardown(theap_test_isempty,
					theap_test_setup,
					theap_test_teardown),
	cmocka_unit_test_setup_teardown(theap_test_insert,
					theap_test_setup,
					theap_test_teardown),
	cmocka_unit_test_setup_teardown(theap_test_deletemin,
					theap_test_setup,
					theap_test_teardown),
	cmocka_unit_test_setup_teardown(theap_test_makeempty,
					theap_test_setup,
					theap_test_teardown),	
    };
    
    return cmocka_run_group_tests_name("Timer Heap Test", tests, NULL, NULL);
}
