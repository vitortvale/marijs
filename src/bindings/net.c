#include "../loop/tcp.h"
#include "net.h"

static JSClassID g_socket_class_id;
static JSClassID g_server_class_id;

/* ── socket methods ─────────────────────────────────────── */

static JSValue js_socket_on(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    MariTcpSocket *s = JS_GetOpaque(this_val, g_socket_class_id);
    if (!s || argc < 2) return JS_UNDEFINED;
    const char *event = JS_ToCString(ctx, argv[0]);
    if (event) { mari_tcp_socket_on(s, event, argv[1]); JS_FreeCString(ctx, event); }
    return JS_DupValue(ctx, this_val);
}

static JSValue js_socket_write(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    MariTcpSocket *s = JS_GetOpaque(this_val, g_socket_class_id);
    if (!s || argc < 1) return JS_UNDEFINED;
    size_t len;
    const char *data = JS_ToCStringLen(ctx, &len, argv[0]);
    if (data) { mari_tcp_socket_write(s, data, len); JS_FreeCString(ctx, data); }
    return JS_DupValue(ctx, this_val);
}

static JSValue js_socket_end(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    MariTcpSocket *s = JS_GetOpaque(this_val, g_socket_class_id);
    if (s) mari_tcp_socket_end(s);
    return JS_UNDEFINED;
}

/* ── server methods ─────────────────────────────────────── */

static JSValue js_server_listen(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    MariTcpServer *srv = JS_GetOpaque(this_val, g_server_class_id);
    if (!srv || argc < 1) return JS_UNDEFINED;
    uint32_t port = 0;
    JS_ToUint32(ctx, &port, argv[0]);
    JSValue cb = argc >= 2 ? argv[1] : JS_UNDEFINED;
    mari_tcp_server_listen(srv, (int)port, cb);
    return JS_DupValue(ctx, this_val);
}

static JSValue js_server_close(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    MariTcpServer *srv = JS_GetOpaque(this_val, g_server_class_id);
    if (srv) mari_tcp_server_close(srv);
    return JS_UNDEFINED;
}

/* ── finalizers ─────────────────────────────────────────── */

static void socket_finalizer(JSRuntime *rt, JSValue val) {
    MariTcpSocket *s = JS_GetOpaque(val, g_socket_class_id);
    if (s) mari_tcp_socket_free(s, rt);
}

static void server_finalizer(JSRuntime *rt, JSValue val) {
    MariTcpServer *srv = JS_GetOpaque(val, g_server_class_id);
    if (srv) mari_tcp_server_free(srv, rt);
}

static void socket_gc_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) {
    MariTcpSocket *s = JS_GetOpaque(val, g_socket_class_id);
    if (s) mari_tcp_socket_gc_mark(s, rt, mark_func);
}

static void server_gc_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) {
    MariTcpServer *srv = JS_GetOpaque(val, g_server_class_id);
    if (srv) mari_tcp_server_gc_mark(srv, rt, mark_func);
}

static JSClassDef socket_class = { "TcpSocket", .finalizer = socket_finalizer, .gc_mark = socket_gc_mark };
static JSClassDef server_class = { "TcpServer", .finalizer = server_finalizer, .gc_mark = server_gc_mark };

/* ── net.createServer / net.connect ─────────────────────── */

static JSValue js_create_server(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) return JS_UNDEFINED;
    MariLoop *loop = JS_GetContextOpaque(ctx);
    MariTcpServer *srv = mari_tcp_server_create(loop, ctx, argv[0]);

    JSValue srv_obj = JS_NewObjectClass(ctx, g_server_class_id);
    JS_SetOpaque(srv_obj, srv);
    return srv_obj;
}

static JSValue js_connect(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) return JS_UNDEFINED;
    MariLoop *loop = JS_GetContextOpaque(ctx);
    uint32_t port = 0;
    JS_ToUint32(ctx, &port, argv[0]);
    const char *host = JS_ToCString(ctx, argv[1]);
    JSValue cb = argc >= 3 ? argv[2] : JS_UNDEFINED;

    MariTcpSocket *s = mari_tcp_socket_connect(loop, ctx, (int)port, host, cb);
    JS_FreeCString(ctx, host);
    if (!s) return JS_UNDEFINED;

    JSValue sock_obj = JS_NewObjectClass(ctx, g_socket_class_id);
    JS_SetOpaque(sock_obj, s);
    mari_tcp_socket_set_self(s, sock_obj);
    return sock_obj;
}

/* ── init ───────────────────────────────────────────────── */

void mari_net_binding_init(JSRuntime *rt, JSContext *ctx) {
    JS_NewClassID(&g_socket_class_id);
    JS_NewClassID(&g_server_class_id);
    JS_NewClass(rt, g_socket_class_id, &socket_class);
    JS_NewClass(rt, g_server_class_id, &server_class);

    /* socket prototype */
    JSValue sock_proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, sock_proto, "on",
        JS_NewCFunction(ctx, js_socket_on,    "on",    2));
    JS_SetPropertyStr(ctx, sock_proto, "write",
        JS_NewCFunction(ctx, js_socket_write, "write", 1));
    JS_SetPropertyStr(ctx, sock_proto, "end",
        JS_NewCFunction(ctx, js_socket_end,   "end",   0));
    JS_SetClassProto(ctx, g_socket_class_id, sock_proto);

    /* server prototype */
    JSValue srv_proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, srv_proto, "listen",
        JS_NewCFunction(ctx, js_server_listen, "listen", 2));
    JS_SetPropertyStr(ctx, srv_proto, "close",
        JS_NewCFunction(ctx, js_server_close,  "close",  0));
    JS_SetClassProto(ctx, g_server_class_id, srv_proto);

    /* tell tcp.c which class ids to use for new socket objects */
    mari_tcp_set_class_ids(g_socket_class_id, g_server_class_id);

    /* expose net global */
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue net = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, net, "createServer",
        JS_NewCFunction(ctx, js_create_server, "createServer", 1));
    JS_SetPropertyStr(ctx, net, "connect",
        JS_NewCFunction(ctx, js_connect,       "connect",      3));
    JS_SetPropertyStr(ctx, global, "net", net);
    JS_FreeValue(ctx, global);
}
