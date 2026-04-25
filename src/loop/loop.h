#ifndef MARI_LOOP_H
#define MARI_LOOP_H

#include "../../deps/quickjs/quickjs.h"

typedef struct {
    int epoll_fd;
    int alive;
} MariLoop;

void mari_loop_init(MariLoop *loop);
void mari_loop_free(MariLoop *loop);
void mari_loop_run(MariLoop *loop, JSRuntime *rt);
void mari_loop_inc_alive(MariLoop *loop);
void mari_loop_dec_alive(MariLoop *loop);

#endif
