#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    void *library = dlopen("./libchain.so", RTLD_NOW | RTLD_LOCAL);
    assert(library);
    int (*call)(int) = dlsym(library, "chain_call");
    assert(call && call(0) == 1);
    assert(dlopen("./imageformats/libfailure.so", RTLD_NOW | RTLD_LOCAL));
    // Initialization failed after patching a library other than libnickel.
    assert(call(0) == 1);
    assert(access("./imageformats/libfailure.so", F_OK) != 0);
    assert(access("./libfailure.so.failsafe", F_OK) == 0);
    sleep(2);
    assert(access("./imageformats/libfailure.so", F_OK) != 0);
    assert(call(0) == 1);
    puts("PASS: initialization rollback restores the other library and keeps the plugin parked");
}
