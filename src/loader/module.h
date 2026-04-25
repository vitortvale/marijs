#ifndef MARI_MODULE_LOADER_H
#define MARI_MODULE_LOADER_H

#include "../../deps/quickjs/quickjs.h"

void mari_module_loader_init(JSRuntime *rt);
int  mari_is_module(const char *filename, const char *src);

#endif
