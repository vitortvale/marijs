#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"

int mari_is_module(const char *filename, const char *src) {
    const char *ext = strrchr(filename, '.');
    if (ext && strcmp(ext, ".mjs") == 0) return 1;
    while (*src == ' ' || *src == '\t' || *src == '\n' || *src == '\r') src++;
    return strncmp(src, "import ", 7)       == 0 ||
           strncmp(src, "import{", 7)       == 0 ||
           strncmp(src, "export ", 7)       == 0 ||
           strncmp(src, "export{", 7)       == 0 ||
           strncmp(src, "export default", 14) == 0;
}

static char *module_normalizer(JSContext *ctx, const char *base_name,
                                const char *name, void *opaque) {
    (void)opaque;
    if (name[0] != '.') return js_strdup(ctx, name);

    const char *p = strrchr(base_name, '/');
    int dir_len = p ? (int)(p - base_name) : 0;

    char *out = js_malloc(ctx, dir_len + 1 + strlen(name) + 1);
    if (!out) return NULL;

    memcpy(out, base_name, dir_len);
    out[dir_len] = '\0';

    const char *r = name;
    while (*r) {
        const char *e = strchr(r, '/');
        if (!e) e = r + strlen(r);
        size_t seg = (size_t)(e - r);

        if (seg == 1 && r[0] == '.') {
            /* current dir — skip */
        } else if (seg == 2 && r[0] == '.' && r[1] == '.') {
            char *last = strrchr(out, '/');
            if (last) *last = '\0';
            else      out[0] = '\0';
        } else {
            size_t cur = strlen(out);
            if (cur > 0) out[cur++] = '/';
            memcpy(out + cur, r, seg);
            out[cur + seg] = '\0';
        }
        if (*e == '\0') break;
        r = e + 1;
    }
    return out;
}

static JSModuleDef *module_loader(JSContext *ctx, const char *module_name,
                                   void *opaque) {
    (void)opaque;
    FILE *f = fopen(module_name, "rb");
    if (!f) {
        JS_ThrowReferenceError(ctx, "cannot load module '%s'", module_name);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *src = malloc(len + 1);
    if (!src) { fclose(f); return NULL; }
    fread(src, 1, len, f);
    src[len] = '\0';
    fclose(f);

    JSValue val = JS_Eval(ctx, src, len, module_name,
                          JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    free(src);
    if (JS_IsException(val)) return NULL;

    JSModuleDef *m = JS_VALUE_GET_PTR(val);
    JS_FreeValue(ctx, val);
    return m;
}

void mari_module_loader_init(JSRuntime *rt) {
    JS_SetModuleLoaderFunc(rt, module_normalizer, module_loader, NULL);
}
