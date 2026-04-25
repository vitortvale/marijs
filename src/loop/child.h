#ifndef MARI_LOOP_CHILD_H
#define MARI_LOOP_CHILD_H

#include "../../deps/quickjs/quickjs.h"
#include "loop.h"

void mari_child_exec(MariLoop *loop, JSContext *ctx,
                     const char *cmd, JSValue cb);

#endif
