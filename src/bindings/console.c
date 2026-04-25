#include <stdio.h>
#include "console.h"

static void print_args(JSContext *ctx, FILE *out, int argc, JSValueConst *argv) {
    for (int i = 0; i < argc; i++) {
        if (i > 0) fputc(' ', out);
        const char *str = JS_ToCString(ctx, argv[i]);
        if (str) {
            fputs(str, out);
            JS_FreeCString(ctx, str);
        }
    }
    fputc('\n', out);
}

static JSValue js_console_log(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)this_val;
    print_args(ctx, stdout, argc, argv);
    return JS_UNDEFINED;
}

static JSValue js_console_warn(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val;
    print_args(ctx, stderr, argc, argv);
    return JS_UNDEFINED;
}

static JSValue js_console_error(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    (void)this_val;
    print_args(ctx, stderr, argc, argv);
    return JS_UNDEFINED;
}

static JSValue js_queue_microtask(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
        return JS_UNDEFINED;

    JSValue global   = JS_GetGlobalObject(ctx);
    JSValue Promise  = JS_GetPropertyStr(ctx, global, "Promise");
    JSValue resolve  = JS_GetPropertyStr(ctx, Promise, "resolve");
    JSValue resolved = JS_Call(ctx, resolve, Promise, 0, NULL);
    JSValue then     = JS_GetPropertyStr(ctx, resolved, "then");
    JSValue result   = JS_Call(ctx, then, resolved, 1, &argv[0]);

    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, then);
    JS_FreeValue(ctx, resolved);
    JS_FreeValue(ctx, resolve);
    JS_FreeValue(ctx, Promise);
    JS_FreeValue(ctx, global);
    return JS_UNDEFINED;
}

void mari_console_init(JSContext *ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue console = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, console, "log",
        JS_NewCFunction(ctx, js_console_log, "log", 1));
    JS_SetPropertyStr(ctx, console, "warn",
        JS_NewCFunction(ctx, js_console_warn, "warn", 1));
    JS_SetPropertyStr(ctx, console, "error",
        JS_NewCFunction(ctx, js_console_error, "error", 1));

    JS_SetPropertyStr(ctx, global, "console", console);
    JS_SetPropertyStr(ctx, global, "queueMicrotask",
        JS_NewCFunction(ctx, js_queue_microtask, "queueMicrotask", 1));
    JS_FreeValue(ctx, global);
}
