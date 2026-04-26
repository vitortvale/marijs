#ifndef MARI_LOOP_TCP_H
#define MARI_LOOP_TCP_H

#include "../../deps/quickjs/quickjs.h"
#include "loop.h"

typedef struct MariTcpSocket MariTcpSocket;
typedef struct MariTcpServer MariTcpServer;

/* called from JS binding methods */
void mari_tcp_socket_write(MariTcpSocket *s, const char *data, size_t len);
void mari_tcp_socket_end(MariTcpSocket *s);
void mari_tcp_socket_on(MariTcpSocket *s, const char *event, JSValue cb);

void mari_tcp_socket_set_self(MariTcpSocket *s, JSValue self);
void mari_tcp_socket_free(MariTcpSocket *s, JSRuntime *rt);
void mari_tcp_server_free(MariTcpServer *srv, JSRuntime *rt);
void mari_tcp_set_class_ids(JSClassID sock_id, JSClassID srv_id);
void mari_tcp_socket_gc_mark(MariTcpSocket *s, JSRuntime *rt, JS_MarkFunc *mark_func);
void mari_tcp_server_gc_mark(MariTcpServer *srv, JSRuntime *rt, JS_MarkFunc *mark_func);
MariTcpSocket *mari_tcp_socket_new(MariLoop *loop, JSContext *ctx, int fd);

/* called from net.c init */
MariTcpServer *mari_tcp_server_create(MariLoop *loop, JSContext *ctx, JSValue on_conn);
void           mari_tcp_server_listen(MariTcpServer *srv, int port, JSValue cb);
void           mari_tcp_server_close(MariTcpServer *srv);
MariTcpSocket *mari_tcp_socket_connect(MariLoop *loop, JSContext *ctx,
                                        int port, const char *host, JSValue on_connect);

#endif
