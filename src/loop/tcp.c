#define _GNU_SOURCE
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "tcp.h"

#define BUF_SIZE 4096

/* ─── socket ─────────────────────────────────────────────── */

struct MariTcpSocket {
    MariIOHandle base;
    int          fd;
    JSContext   *ctx;
    JSValue      on_data;
    JSValue      on_close;
    JSValue      self;   /* keeps JS object alive until EOF */
    MariLoop    *loop;
};

static void socket_dispatch(MariIOHandle *handle, MariLoop *loop) {
    MariTcpSocket *s = (MariTcpSocket *)handle;
    char buf[BUF_SIZE];
    ssize_t n;

    while ((n = read(s->fd, buf, sizeof(buf))) > 0) {
        if (JS_IsFunction(s->ctx, s->on_data)) {
            JSValue chunk = JS_NewStringLen(s->ctx, buf, n);
            JSValue ret   = JS_Call(s->ctx, s->on_data, JS_UNDEFINED, 1, &chunk);
            if (JS_IsException(ret)) {
                JSValue exc = JS_GetException(s->ctx);
                const char *msg = JS_ToCString(s->ctx, exc);
                fprintf(stderr, "Uncaught exception in socket data handler: %s\n", msg);
                JS_FreeCString(s->ctx, msg);
                JS_FreeValue(s->ctx, exc);
                loop->had_error = 1;
            }
            JS_FreeValue(s->ctx, ret);
            JS_FreeValue(s->ctx, chunk);
        }
    }

    if (n == 0 || (n < 0 && errno != EAGAIN)) {
        if (JS_IsFunction(s->ctx, s->on_close)) {
            JSValue ret = JS_Call(s->ctx, s->on_close, JS_UNDEFINED, 0, NULL);
            JS_FreeValue(s->ctx, ret);
        }
        mari_loop_remove_fd(loop, s->fd);
        close(s->fd);
        s->fd = -1;
        mari_loop_dec_alive(loop);
        /* break cycles: closures may capture the socket object, so free
           callbacks before releasing self to avoid keeping the cycle alive */
        JS_FreeValue(s->ctx, s->on_data);  s->on_data  = JS_UNDEFINED;
        JS_FreeValue(s->ctx, s->on_close); s->on_close = JS_UNDEFINED;
        JSValue self = s->self;
        s->self = JS_UNDEFINED;
        JS_FreeValue(s->ctx, self);
    }
}

void mari_tcp_socket_gc_mark(MariTcpSocket *s, JSRuntime *rt, JS_MarkFunc *mark_func) {
    JS_MarkValue(rt, s->on_data,  mark_func);
    JS_MarkValue(rt, s->on_close, mark_func);
    JS_MarkValue(rt, s->self,     mark_func);
}

/* called by JS finalizer */
void mari_tcp_socket_free(MariTcpSocket *s, JSRuntime *rt) {
    if (s->fd >= 0) close(s->fd);
    JS_FreeValueRT(rt, s->on_data);
    JS_FreeValueRT(rt, s->on_close);
    JS_FreeValueRT(rt, s->self);
    free(s);
}

static void set_nonblock(int fd) {
    int f = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, f | O_NONBLOCK);
}

/* returns the raw struct; caller wraps in JS object and sets self */
MariTcpSocket *mari_tcp_socket_new(MariLoop *loop, JSContext *ctx, int fd) {
    MariTcpSocket *s = calloc(1, sizeof(*s));
    s->base.dispatch = socket_dispatch;
    s->fd            = fd;
    s->ctx           = ctx;
    s->on_data       = JS_UNDEFINED;
    s->on_close      = JS_UNDEFINED;
    s->self          = JS_UNDEFINED;
    s->loop          = loop;
    set_nonblock(fd);
    mari_loop_add_fd(loop, fd, EPOLLIN | EPOLLET, (MariIOHandle *)s);
    mari_loop_inc_alive(loop);
    return s;
}

void mari_tcp_socket_on(MariTcpSocket *s, const char *event, JSValue cb) {
    if (strcmp(event, "data") == 0) {
        JS_FreeValue(s->ctx, s->on_data);
        s->on_data = JS_DupValue(s->ctx, cb);
    } else if (strcmp(event, "close") == 0) {
        JS_FreeValue(s->ctx, s->on_close);
        s->on_close = JS_DupValue(s->ctx, cb);
    }
}

void mari_tcp_socket_write(MariTcpSocket *s, const char *data, size_t len) {
    if (s->fd >= 0) write(s->fd, data, len);
}

void mari_tcp_socket_end(MariTcpSocket *s) {
    if (s->fd >= 0) shutdown(s->fd, SHUT_WR);
}

void mari_tcp_socket_set_self(MariTcpSocket *s, JSValue self) {
    JS_FreeValue(s->ctx, s->self);
    s->self = JS_DupValue(s->ctx, self);
}

/* ─── server ──────────────────────────────────────────────── */

struct MariTcpServer {
    MariIOHandle base;
    int          fd;
    JSContext   *ctx;
    JSValue      on_connection;
    JSValue      socket_proto; /* prototype for new socket objects */
    MariLoop    *loop;
};

/* set by net.c after class setup */
static JSClassID g_socket_class_id;
static JSClassID g_server_class_id;

void mari_tcp_set_class_ids(JSClassID sock_id, JSClassID srv_id) {
    g_socket_class_id = sock_id;
    g_server_class_id = srv_id;
}

static void server_dispatch(MariIOHandle *handle, MariLoop *loop) {
    MariTcpServer *srv = (MariTcpServer *)handle;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int cfd;

    while ((cfd = accept(srv->fd, (struct sockaddr *)&addr, &len)) >= 0) {
        MariTcpSocket *s = mari_tcp_socket_new(loop, srv->ctx, cfd);

        JSValue proto    = JS_GetClassProto(srv->ctx, g_socket_class_id);
        JSValue sock_obj = JS_NewObjectProtoClass(srv->ctx, proto, g_socket_class_id);
        JS_FreeValue(srv->ctx, proto);
        JS_SetOpaque(sock_obj, s);
        s->self = JS_DupValue(srv->ctx, sock_obj); /* keep alive until EOF */

        JSValue ret = JS_Call(srv->ctx, srv->on_connection,
                              JS_UNDEFINED, 1, &sock_obj);
        if (JS_IsException(ret)) {
            JSValue exc = JS_GetException(srv->ctx);
            const char *msg = JS_ToCString(srv->ctx, exc);
            fprintf(stderr, "Uncaught exception in connection handler: %s\n", msg);
            JS_FreeCString(srv->ctx, msg);
            JS_FreeValue(srv->ctx, exc);
            loop->had_error = 1;
        }
        JS_FreeValue(srv->ctx, ret);
        JS_FreeValue(srv->ctx, sock_obj);
    }
}

void mari_tcp_server_close(MariTcpServer *srv) {
    if (srv->fd >= 0) {
        mari_loop_remove_fd(srv->loop, srv->fd);
        close(srv->fd);
        srv->fd = -1;
        mari_loop_dec_alive(srv->loop);
    }
}

void mari_tcp_server_gc_mark(MariTcpServer *srv, JSRuntime *rt, JS_MarkFunc *mark_func) {
    JS_MarkValue(rt, srv->on_connection, mark_func);
}

void mari_tcp_server_free(MariTcpServer *srv, JSRuntime *rt) {
    if (srv->fd >= 0) close(srv->fd);
    JS_FreeValueRT(rt, srv->on_connection);
    free(srv);
}

MariTcpServer *mari_tcp_server_create(MariLoop *loop, JSContext *ctx, JSValue on_conn) {
    MariTcpServer *srv = calloc(1, sizeof(*srv));
    srv->base.dispatch = server_dispatch;
    srv->fd            = -1;
    srv->ctx           = ctx;
    srv->on_connection = JS_DupValue(ctx, on_conn);
    srv->loop          = loop;
    return srv;
}

void mari_tcp_server_listen(MariTcpServer *srv, int port, JSValue cb) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) return;
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(port),
        .sin_addr   = { .s_addr = INADDR_ANY }
    };
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
        listen(fd, 128) < 0) { close(fd); return; }
    srv->fd = fd;
    mari_loop_add_fd(srv->loop, fd, EPOLLIN, (MariIOHandle *)srv);
    mari_loop_inc_alive(srv->loop);
    if (JS_IsFunction(srv->ctx, cb)) {
        JSValue ret = JS_Call(srv->ctx, cb, JS_UNDEFINED, 0, NULL);
        JS_FreeValue(srv->ctx, ret);
    }
}

MariTcpSocket *mari_tcp_socket_connect(MariLoop *loop, JSContext *ctx,
                                         int port, const char *host, JSValue on_connect) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return NULL;
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(port)
    };
    inet_pton(AF_INET, host, &addr.sin_addr);
    connect(fd, (struct sockaddr *)&addr, sizeof(addr));

    MariTcpSocket *s = mari_tcp_socket_new(loop, ctx, fd);
    if (JS_IsFunction(ctx, on_connect)) {
        JSValue ret = JS_Call(ctx, on_connect, JS_UNDEFINED, 0, NULL);
        JS_FreeValue(ctx, ret);
    }
    return s;
}
