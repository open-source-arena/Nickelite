#include "NickelHook.h"
#include <assert.h>
#include <dlfcn.h>

static int (*previous)(int);
int failure_hook(int value) { return previous(value) + 100; }

static int fail_init(void) {
    void *library = dlopen("./libchain.so", RTLD_NOW);
    assert(library);
    int (*call)(int) = dlsym(library, "chain_call");
    assert(call && call(0) == 101);
    dlclose(library);
    return 1;
}

static struct nh_info info = {.name = "failure-check", .failsafe_delay = 1};
static struct nh_hook hooks[] = {
    {.sym = "chain_value", .sym_new = "failure_hook", .lib = "./libchain.so", .out = (void **)&previous},
    {0},
};
NickelHook(.info = &info, .hook = hooks, .init = fail_init)
