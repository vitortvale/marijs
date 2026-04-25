#include "../loop/child.h"
#include "process.h"

static JSValue js_exec(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2 || !JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;
    MariLoop *loop = JS_GetContextOpaque(ctx);
    const char *cmd = JS_ToCString(ctx, argv[0]);
    if (!cmd) return JS_UNDEFINED;
    mari_child_exec(loop, ctx, cmd, argv[1]);
    JS_FreeCString(ctx, cmd);
    return JS_UNDEFINED;
}

void mari_process_binding_init(JSContext *ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue proc = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, proc, "exec",
        JS_NewCFunction(ctx, js_exec, "exec", 2));
    JS_SetPropertyStr(ctx, global, "process", proc);
    JS_FreeValue(ctx, global);
}
