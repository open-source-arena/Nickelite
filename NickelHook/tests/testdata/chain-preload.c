#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>

int chain_value(int value) {
    int (*next)(int) = dlsym(RTLD_NEXT, "chain_value");
    assert(next);
    return next(value) + 100;
}
