#include <stdio.h>
#include <stdlib.h>
#include "../deps/quickjs/quickjs.h"

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    rewind(f);
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    *out_len = len;
    return buf;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: mari <file.js>\n");
        return 1;
    }

    size_t src_len;
    char *src = read_file(argv[1], &src_len);
    if (!src) {
        fprintf(stderr, "mari: cannot read '%s'\n", argv[1]);
        return 1;
    }

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);

    JSValue result = JS_Eval(ctx, src, src_len, argv[1], JS_EVAL_TYPE_GLOBAL);
    free(src);

    int exit_code = 0;
    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(ctx);
        const char *msg = JS_ToCString(ctx, exc);
        fprintf(stderr, "Uncaught exception: %s\n", msg);
        JS_FreeCString(ctx, msg);
        JS_FreeValue(ctx, exc);
        exit_code = 1;
    }

    JS_FreeValue(ctx, result);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return exit_code;
}
