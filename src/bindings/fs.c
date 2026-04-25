#include "../loop/fs.h"
#include "fs.h"

static JSValue js_read_file(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2 || !JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;
    MariLoop *loop = JS_GetContextOpaque(ctx);
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_UNDEFINED;
    mari_fs_read(loop, ctx, path, argv[1]);
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

static JSValue js_write_file(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3 || !JS_IsFunction(ctx, argv[2])) return JS_UNDEFINED;
    MariLoop *loop = JS_GetContextOpaque(ctx);
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_UNDEFINED;
    size_t len;
    const char *data = JS_ToCStringLen(ctx, &len, argv[1]);
    if (!data) { JS_FreeCString(ctx, path); return JS_UNDEFINED; }
    mari_fs_write(loop, ctx, path, data, len, argv[2]);
    JS_FreeCString(ctx, data);
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

void mari_fs_binding_init(JSContext *ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fs = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, fs, "readFile",
        JS_NewCFunction(ctx, js_read_file,  "readFile",  2));
    JS_SetPropertyStr(ctx, fs, "writeFile",
        JS_NewCFunction(ctx, js_write_file, "writeFile", 3));
    JS_SetPropertyStr(ctx, global, "fs", fs);
    JS_FreeValue(ctx, global);
}
