#ifndef MARI_LOOP_TIMERS_H
#define MARI_LOOP_TIMERS_H

#include "../../deps/quickjs/quickjs.h"
#include "loop.h"

void mari_timers_init(void);
int  mari_timer_set(MariLoop *loop, JSContext *ctx, JSValue cb, uint32_t delay_ms, int is_interval);
void mari_timer_clear(MariLoop *loop, int id);

#endif
