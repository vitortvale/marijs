#define _GNU_SOURCE
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "fs.h"

typedef struct {
    MariIOHandle base;
    int          event_fd;
    pthread_t    thread;
    int          is_write;
    char        *path;
    char        *buf;      /* result buf for read; input for write */
    size_t       len;
    int          error;    /* errno on failure, 0 on success */
    JSContext   *ctx;
    JSValue      callback;
    MariLoop    *loop;
} MariFsOp;

static void fs_dispatch(MariIOHandle *handle, MariLoop *loop) {
    MariFsOp *op = (MariFsOp *)handle;

    uint64_t val;
    read(op->event_fd, &val, sizeof(val));

    JSValue args[2];
    if (op->error) {
        args[0] = JS_NewString(op->ctx, strerror(op->error));
        args[1] = JS_NULL;
    } else {
        args[0] = JS_NULL;
        args[1] = op->buf
                  ? JS_NewStringLen(op->ctx, op->buf, op->len)
                  : JS_UNDEFINED;
    }

    JSValue ret = JS_Call(op->ctx, op->callback, JS_UNDEFINED, 2, args);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(op->ctx);
        const char *msg = JS_ToCString(op->ctx, exc);
        fprintf(stderr, "Uncaught exception in fs callback: %s\n", msg);
        JS_FreeCString(op->ctx, msg);
        JS_FreeValue(op->ctx, exc);
        loop->had_error = 1;
    }
    JS_FreeValue(op->ctx, ret);
    JS_FreeValue(op->ctx, args[0]);
    JS_FreeValue(op->ctx, args[1]);

    mari_loop_remove_fd(loop, op->event_fd);
    close(op->event_fd);
    JS_FreeValue(op->ctx, op->callback);
    free(op->buf);
    free(op->path);
    free(op);
    mari_loop_dec_alive(loop);
}

static void *read_thread(void *arg) {
    MariFsOp *op = arg;
    FILE *f = fopen(op->path, "rb");
    if (!f) { op->error = errno; goto done; }
    fseek(f, 0, SEEK_END);
    op->len = (size_t)ftell(f);
    rewind(f);
    op->buf = malloc(op->len + 1);
    if (!op->buf) { fclose(f); op->error = ENOMEM; goto done; }
    fread(op->buf, 1, op->len, f);
    op->buf[op->len] = '\0';
    fclose(f);
done:
    write(op->event_fd, &(uint64_t){1}, sizeof(uint64_t));
    return NULL;
}

static void *write_thread(void *arg) {
    MariFsOp *op = arg;
    FILE *f = fopen(op->path, "wb");
    if (!f) { op->error = errno; goto done; }
    fwrite(op->buf, 1, op->len, f);
    fclose(f);
done:
    write(op->event_fd, &(uint64_t){1}, sizeof(uint64_t));
    return NULL;
}

static MariFsOp *fs_op_new(MariLoop *loop, JSContext *ctx,
                            const char *path, JSValue cb, int is_write) {
    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd < 0) return NULL;

    MariFsOp *op = calloc(1, sizeof(*op));
    op->base.dispatch = fs_dispatch;
    op->event_fd      = efd;
    op->is_write      = is_write;
    op->path          = strdup(path);
    op->ctx           = ctx;
    op->callback      = JS_DupValue(ctx, cb);
    op->loop          = loop;

    mari_loop_add_fd(loop, efd, EPOLLIN, (MariIOHandle *)op);
    mari_loop_inc_alive(loop);
    return op;
}

void mari_fs_read(MariLoop *loop, JSContext *ctx,
                  const char *path, JSValue cb) {
    MariFsOp *op = fs_op_new(loop, ctx, path, cb, 0);
    if (!op) return;
    pthread_create(&op->thread, NULL, read_thread, op);
    pthread_detach(op->thread);
}

void mari_fs_write(MariLoop *loop, JSContext *ctx,
                   const char *path, const char *data, size_t len, JSValue cb) {
    MariFsOp *op = fs_op_new(loop, ctx, path, cb, 1);
    if (!op) return;
    op->buf = malloc(len);
    op->len = len;
    if (op->buf) memcpy(op->buf, data, len);
    pthread_create(&op->thread, NULL, write_thread, op);
    pthread_detach(op->thread);
}
