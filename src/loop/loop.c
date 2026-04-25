#include <sys/epoll.h>
#include <unistd.h>
#include <stdio.h>
#include "loop.h"

#define EPOLL_MAX_EVENTS 64

void mari_loop_init(MariLoop *loop) {
    loop->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    loop->alive = 0;
}

void mari_loop_free(MariLoop *loop) {
    if (loop->epoll_fd >= 0) {
        close(loop->epoll_fd);
        loop->epoll_fd = -1;
    }
}

void mari_loop_inc_alive(MariLoop *loop) {
    loop->alive++;
}

void mari_loop_dec_alive(MariLoop *loop) {
    loop->alive--;
}

static void drain_jobs(JSRuntime *rt) {
    JSContext *ctx;
    int ret;
    while ((ret = JS_ExecutePendingJob(rt, &ctx)) > 0)
        ;
    if (ret < 0) {
        JSValue exc = JS_GetException(ctx);
        const char *msg = JS_ToCString(ctx, exc);
        fprintf(stderr, "Uncaught exception in microtask: %s\n", msg);
        JS_FreeCString(ctx, msg);
        JS_FreeValue(ctx, exc);
    }
}

void mari_loop_add_fd(MariLoop *loop, int fd, uint32_t events, MariIOHandle *handle) {
    struct epoll_event ev = { .events = events, .data.ptr = handle };
    epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

void mari_loop_remove_fd(MariLoop *loop, int fd) {
    epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

void mari_loop_run(MariLoop *loop, JSRuntime *rt) {
    struct epoll_event events[EPOLL_MAX_EVENTS];

    for (;;) {
        drain_jobs(rt);

        if (loop->alive <= 0)
            break;

        int n = epoll_wait(loop->epoll_fd, events, EPOLL_MAX_EVENTS, -1);
        if (n < 0)
            break;

        for (int i = 0; i < n; i++) {
            MariIOHandle *h = events[i].data.ptr;
            h->dispatch(h, loop);
        }

        drain_jobs(rt);
    }
}
