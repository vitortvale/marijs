#ifndef MARI_LOOP_FS_H
#define MARI_LOOP_FS_H

#include "../../deps/quickjs/quickjs.h"
#include "loop.h"

void mari_fs_read(MariLoop *loop, JSContext *ctx,
                  const char *path, JSValue cb);
void mari_fs_write(MariLoop *loop, JSContext *ctx,
                   const char *path, const char *data, size_t len, JSValue cb);

#endif
