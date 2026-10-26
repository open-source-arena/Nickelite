#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void **slot(void *library) {
    struct link_map *map;
    assert(!dlinfo(library, RTLD_DI_LINKMAP, &map));
    ElfW(Sym) *symbols = NULL;
    const char *strings = NULL;
    void *relocations = NULL;
    size_t size = 0, entry_size = 0;
    int rela = 0;
    for (ElfW(Dyn) *d = map->l_ld; d->d_tag != DT_NULL; ++d) {
        if (d->d_tag == DT_SYMTAB) symbols = (void *)d->d_un.d_ptr;
        if (d->d_tag == DT_STRTAB) strings = (void *)d->d_un.d_ptr;
        if (d->d_tag == DT_JMPREL) relocations = (void *)d->d_un.d_ptr;
        if (d->d_tag == DT_PLTRELSZ) size = d->d_un.d_val;
        if (d->d_tag == DT_PLTREL) rela = d->d_un.d_val == DT_RELA;
    }
    entry_size = rela ? sizeof(ElfW(Rela)) : sizeof(ElfW(Rel));
    assert(symbols && strings && relocations);
    for (size_t i = 0; i < size; i += entry_size) {
        ElfW(Rel) *relocation = (void *)((char *)relocations + i);
#if __SIZEOF_POINTER__ == 8
        size_t index = ELF64_R_SYM(relocation->r_info);
#else
        size_t index = ELF32_R_SYM(relocation->r_info);
#endif
        if (!strcmp(strings + symbols[index].st_name, "chain_value"))
            return (void **)(map->l_addr + relocation->r_offset);
    }
    abort();
}

static int writable(void *address) {
    FILE *maps = fopen("/proc/self/maps", "re");
    assert(maps);
    char line[1024], permissions[5];
    unsigned long start, end;
    while (fgets(line, sizeof(line), maps)) {
        if (sscanf(line, "%lx-%lx %4s", &start, &end, permissions) == 3 &&
                (uintptr_t)address >= start && (uintptr_t)address < end) {
            fclose(maps);
            return permissions[1] == 'w';
        }
    }
    abort();
}

struct hook {
    void *(*install)(void *);
    void *(*remove)(void *);
    int (*calls)(void);
    void *function;
    void *(*replace)(void *, void *);
    void (*fail_restore)(void);
};

static struct hook load(int digit) {
    char path[32];
    snprintf(path, sizeof(path), "./libhook%d.so", digit);
    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    assert(handle);
    struct hook result = {
        dlsym(handle, "chain_install"), dlsym(handle, "chain_remove"),
        dlsym(handle, "chain_calls"), dlsym(handle, "chain_hook"),
        dlsym(handle, "chain_replace"),
        dlsym(handle, "chain_fail_restore"),
    };
    assert(result.install && result.remove && result.calls && result.function && result.replace && result.fail_restore);
    return result;
}

int main(int argc, char **argv) {
    assert(argc == 6);
    int warm = atoi(argv[2]), order = atoi(argv[3]), preload = atoi(argv[4]);
    int relro = atoi(argv[5]);
    // GLOBAL lets the preload's RTLD_NEXT find the fixture's stock function.
    void *library = dlopen(relro ? "./libchain-now.so" : "./libchain.so", (preload ? RTLD_GLOBAL : RTLD_LOCAL) | (atoi(argv[1]) ? RTLD_NOW : RTLD_LAZY));
    assert(library);
    void **target_slot = slot(library);
    assert(writable(target_slot) == !relro);
    int (*call)(int) = dlsym(library, "chain_call");
    int (*stock_calls)(void) = dlsym(library, "chain_stock_calls");
    void *stock = dlsym(preload ? RTLD_DEFAULT : library, "chain_value");
    assert(call && stock && stock_calls);
    int expected = preload ? 101 : 1;
    if (warm) assert(call(0) == expected);
    struct hook hooks[3];
    int digits[3] = {order / 100, order / 10 % 10, order % 10};
    for (int i = 0; i < 3; i++) {
        hooks[i] = load(digits[i]);
        if (!i) assert(!hooks[i].replace(library, stock));
        if (!i && relro) {
            void *previous = *target_slot;
            hooks[i].fail_restore();
            assert(!hooks[i].install(library));
            assert(*target_slot == previous && !writable(target_slot));
        }
        assert(hooks[i].install(library) == (i ? hooks[i-1].function : stock));
        assert(writable(target_slot) == !relro);
        expected = expected * 10 + digits[i];
    }
    assert(call(0) == expected);
    for (int i = 0; i < 3; i++) assert(hooks[i].calls() == 1);
    assert(stock_calls() == warm + 1);
    // A duplicate must not make a hook call itself.
    assert(!hooks[2].install(library));
    assert(call(0) == expected);
    for (int i = 0; i < 3; i++) assert(hooks[i].calls() == 2);
    assert(stock_calls() == warm + 2);
    // Undo in reverse order, as initialization rollback does.
    for (int i = 2; i >= 0; i--) {
        assert(hooks[i].remove(library) == hooks[i].function);
        assert(writable(target_slot) == !relro);
        expected /= 10;
        assert(call(0) == expected);
    }
    assert(stock_calls() == warm + 5);
    printf("PASS binding=%s warm=%d order=%d preload=%d\n", atoi(argv[1]) ? "now" : "lazy", warm, order, preload);
}
