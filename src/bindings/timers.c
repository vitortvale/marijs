#include "../loop/timers.h"
#include "timers.h"

static JSValue js_set_timeout(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
        return JS_UNDEFINED;
    MariLoop *loop = JS_GetContextOpaque(ctx);
    uint32_t delay = 0;
    if (argc >= 2) JS_ToUint32(ctx, &delay, argv[1]);
    return JS_NewInt32(ctx, mari_timer_set(loop, ctx, argv[0], delay, 0));
}

static JSValue js_clear_timeout(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_UNDEFINED;
    MariLoop *loop = JS_GetContextOpaque(ctx);
    uint32_t id;
    JS_ToUint32(ctx, &id, argv[0]);
    mari_timer_clear(loop, (int)id);
    return JS_UNDEFINED;
}

static JSValue js_set_interval(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
        return JS_UNDEFINED;
    MariLoop *loop = JS_GetContextOpaque(ctx);
    uint32_t delay = 0;
    if (argc >= 2) JS_ToUint32(ctx, &delay, argv[1]);
    return JS_NewInt32(ctx, mari_timer_set(loop, ctx, argv[0], delay, 1));
}

static JSValue js_clear_interval(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_UNDEFINED;
    MariLoop *loop = JS_GetContextOpaque(ctx);
    uint32_t id;
    JS_ToUint32(ctx, &id, argv[0]);
    mari_timer_clear(loop, (int)id);
    return JS_UNDEFINED;
}

void mari_timers_binding_init(JSContext *ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "setTimeout",
        JS_NewCFunction(ctx, js_set_timeout,   "setTimeout",   2));
    JS_SetPropertyStr(ctx, global, "clearTimeout",
        JS_NewCFunction(ctx, js_clear_timeout, "clearTimeout", 1));
    JS_SetPropertyStr(ctx, global, "setInterval",
        JS_NewCFunction(ctx, js_set_interval,  "setInterval",  2));
    JS_SetPropertyStr(ctx, global, "clearInterval",
        JS_NewCFunction(ctx, js_clear_interval,"clearInterval",1));
    JS_FreeValue(ctx, global);
}
