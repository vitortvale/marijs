#define _GNU_SOURCE
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include "timers.h"

#define MAX_TIMERS 256

typedef struct {
    MariIOHandle base;
    int          fd;
    int          id;
    int          is_interval;
    uint32_t     delay_ms;
    JSContext   *ctx;
    JSValue      callback;
    MariLoop    *loop;
} MariTimer;

static MariTimer g_timers[MAX_TIMERS];
static int       g_next_id = 1;

void mari_timers_init(void) {
    for (int i = 0; i < MAX_TIMERS; i++)
        g_timers[i].fd = -1;
}

static void timer_dispatch(MariIOHandle *handle, MariLoop *loop) {
    MariTimer *t = (MariTimer *)handle;

    uint64_t count;
    read(t->fd, &count, sizeof(count));

    JSValue ret = JS_Call(t->ctx, t->callback, JS_UNDEFINED, 0, NULL);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(t->ctx);
        const char *msg = JS_ToCString(t->ctx, exc);
        fprintf(stderr, "Uncaught exception in timer: %s\n", msg);
        JS_FreeCString(t->ctx, msg);
        JS_FreeValue(t->ctx, exc);
    }
    JS_FreeValue(t->ctx, ret);

    if (!t->is_interval) {
        mari_loop_remove_fd(loop, t->fd);
        close(t->fd);
        t->fd = -1;
        JS_FreeValue(t->ctx, t->callback);
        mari_loop_dec_alive(loop);
    }
}

int mari_timer_set(MariLoop *loop, JSContext *ctx, JSValue cb,
                   uint32_t delay_ms, int is_interval) {
    MariTimer *t = NULL;
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (g_timers[i].fd == -1) { t = &g_timers[i]; break; }
    }
    if (!t) return -1;

    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0) return -1;

    /* timerfd it_value={0,0} disarms — use 1ms floor for delay=0 */
    uint32_t effective = delay_ms > 0 ? delay_ms : 1;
    struct itimerspec spec = {0};
    spec.it_value.tv_sec  = effective / 1000;
    spec.it_value.tv_nsec = (effective % 1000) * 1000000L;
    if (is_interval)
        spec.it_interval = spec.it_value;
    timerfd_settime(fd, 0, &spec, NULL);

    t->base.dispatch = timer_dispatch;
    t->fd             = fd;
    t->id             = g_next_id++;
    t->is_interval    = is_interval;
    t->delay_ms       = delay_ms;
    t->ctx            = ctx;
    t->callback       = JS_DupValue(ctx, cb);
    t->loop           = loop;

    mari_loop_add_fd(loop, fd, EPOLLIN, (MariIOHandle *)t);
    mari_loop_inc_alive(loop);

    return t->id;
}

void mari_timer_clear(MariLoop *loop, int id) {
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (g_timers[i].fd == -1 || g_timers[i].id != id) continue;
        MariTimer *t = &g_timers[i];
        mari_loop_remove_fd(loop, t->fd);
        close(t->fd);
        t->fd = -1;
        JS_FreeValue(t->ctx, t->callback);
        mari_loop_dec_alive(loop);
        return;
    }
}
