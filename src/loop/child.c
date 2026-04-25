#define _GNU_SOURCE
#include <sys/epoll.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "child.h"

#define BUF_CHUNK 4096

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} Buf;

static void buf_append(Buf *b, const char *chunk, size_t n) {
    if (b->len + n + 1 > b->cap) {
        b->cap = b->len + n + BUF_CHUNK;
        b->data = realloc(b->data, b->cap);
    }
    memcpy(b->data + b->len, chunk, n);
    b->len += n;
    b->data[b->len] = '\0';
}

typedef struct {
    MariIOHandle base;   /* for stdout fd */
    int          stdout_fd;
    int          stderr_fd;
    pid_t        pid;
    Buf          out;
    Buf          err;
    int          out_done;
    int          err_done;
    JSContext   *ctx;
    JSValue      callback;
    MariLoop    *loop;
} MariChild;

/* second handle for stderr — shares the same MariChild */
typedef struct {
    MariIOHandle base;
    MariChild   *child;
} MariChildErr;

static void finish_if_done(MariChild *c) {
    if (!c->out_done || !c->err_done) return;

    int status = 0;
    waitpid(c->pid, &status, 0);
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    JSValue args[3];
    args[0] = (code != 0)
        ? JS_NewString(c->ctx, c->err.len ? c->err.data : "process exited non-zero")
        : JS_NULL;
    args[1] = c->out.data ? JS_NewString(c->ctx, c->out.data) : JS_NewString(c->ctx, "");
    args[2] = c->err.data ? JS_NewString(c->ctx, c->err.data) : JS_NewString(c->ctx, "");

    JSValue ret = JS_Call(c->ctx, c->callback, JS_UNDEFINED, 3, args);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(c->ctx);
        const char *msg = JS_ToCString(c->ctx, exc);
        fprintf(stderr, "Uncaught exception in exec callback: %s\n", msg);
        JS_FreeCString(c->ctx, msg);
        JS_FreeValue(c->ctx, exc);
        c->loop->had_error = 1;
    }
    JS_FreeValue(c->ctx, ret);
    JS_FreeValue(c->ctx, args[0]);
    JS_FreeValue(c->ctx, args[1]);
    JS_FreeValue(c->ctx, args[2]);
    JS_FreeValue(c->ctx, c->callback);

    free(c->out.data);
    free(c->err.data);
    free(c);
}

static void stdout_dispatch(MariIOHandle *handle, MariLoop *loop) {
    MariChild *c = (MariChild *)handle;
    char tmp[BUF_CHUNK];
    ssize_t n;
    while ((n = read(c->stdout_fd, tmp, sizeof(tmp))) > 0)
        buf_append(&c->out, tmp, n);
    if (n == 0 || (n < 0 && errno != EAGAIN)) {
        mari_loop_remove_fd(loop, c->stdout_fd);
        close(c->stdout_fd);
        c->out_done = 1;
        mari_loop_dec_alive(loop);
        finish_if_done(c);
    }
}

static void stderr_dispatch(MariIOHandle *handle, MariLoop *loop) {
    MariChildErr *ce = (MariChildErr *)handle;
    MariChild    *c  = ce->child;
    char tmp[BUF_CHUNK];
    ssize_t n;
    while ((n = read(c->stderr_fd, tmp, sizeof(tmp))) > 0)
        buf_append(&c->err, tmp, n);
    if (n == 0 || (n < 0 && errno != EAGAIN)) {
        mari_loop_remove_fd(loop, c->stderr_fd);
        close(c->stderr_fd);
        free(ce);
        c->err_done = 1;
        mari_loop_dec_alive(loop);
        finish_if_done(c);
    }
}

void mari_child_exec(MariLoop *loop, JSContext *ctx,
                     const char *cmd, JSValue cb) {
    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) < 0 || pipe(err_pipe) < 0) return;

    pid_t pid = fork();
    if (pid < 0) return;

    if (pid == 0) {
        /* child */
        close(out_pipe[0]); close(err_pipe[0]);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[1]); close(err_pipe[1]);
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(127);
    }

    /* parent */
    close(out_pipe[1]); close(err_pipe[1]);

    MariChild *c = calloc(1, sizeof(*c));
    c->base.dispatch = stdout_dispatch;
    c->stdout_fd     = out_pipe[0];
    c->stderr_fd     = err_pipe[0];
    c->pid           = pid;
    c->ctx           = ctx;
    c->callback      = JS_DupValue(ctx, cb);
    c->loop          = loop;

    MariChildErr *ce = calloc(1, sizeof(*ce));
    ce->base.dispatch = stderr_dispatch;
    ce->child         = c;

    /* set non-blocking */
    int flags;
    flags = fcntl(c->stdout_fd, F_GETFL); fcntl(c->stdout_fd, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(c->stderr_fd, F_GETFL); fcntl(c->stderr_fd, F_SETFL, flags | O_NONBLOCK);

    mari_loop_add_fd(loop, c->stdout_fd, EPOLLIN | EPOLLET, (MariIOHandle *)c);
    mari_loop_add_fd(loop, c->stderr_fd, EPOLLIN | EPOLLET, (MariIOHandle *)ce);
    mari_loop_inc_alive(loop); /* stdout */
    mari_loop_inc_alive(loop); /* stderr */
}
