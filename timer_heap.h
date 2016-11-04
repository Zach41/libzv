#ifndef TIMER_HEAP_H
#define TIMER_HEAP_H

#define TIMER_BLK 128

struct zv_loop;
struct zv_timer;

void theap_init(struct zv_loop *lp);
void theap_destroy(struct zv_loop *lp);
void theap_makeempty(struct zv_loop *lp);
void theap_insert(struct zv_timer *w, struct zv_loop *lp);
struct zv_timer *theap_deletemin(struct zv_loop *lp);
struct zv_timer *theap_findmin(struct zv_loop *lp);
/* int theap_isfull(struct zv_loop *lp); */
int theap_isempty(struct zv_loop *lp);
/* void theap_buildheap(struct zv_loop *lp, struct zv_timer **timers); */

#endif /* TIMER_HEAP_H */
