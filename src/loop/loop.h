#ifndef MARI_LOOP_H
#define MARI_LOOP_H

#include "../../deps/quickjs/quickjs.h"
#include <stdint.h>

typedef struct MariLoop {
    int epoll_fd;
    int alive;
    int had_error;
} MariLoop;

typedef struct MariIOHandle {
    void (*dispatch)(struct MariIOHandle *handle, MariLoop *loop);
} MariIOHandle;

void mari_loop_init(MariLoop *loop);
void mari_loop_free(MariLoop *loop);
void mari_loop_run(MariLoop *loop, JSRuntime *rt);
void mari_loop_inc_alive(MariLoop *loop);
void mari_loop_dec_alive(MariLoop *loop);
void mari_loop_add_fd(MariLoop *loop, int fd, uint32_t events, MariIOHandle *handle);
void mari_loop_remove_fd(MariLoop *loop, int fd);

#endif
