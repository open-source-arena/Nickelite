#define _GNU_SOURCE // RTLD_DEFAULT, Dl_info, dladdr
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <link.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

// --- public api

#include "NickelHook.h"

// --- private api

extern const struct nh NickelHook;
extern void nh_resources_uninstalled(void) __attribute__((weak, visibility("hidden")));

// nh_init is the main entry point for NickelHook which sets up the mod
// according to the information in the external NickelHook struct and handles
// the main setup, failsafe, and other related things. On failure, it leaves
// nickel in the original state. On crash, it removes itself (it may also affect
// other mods if they have a nonzero timeout or are loaded in parallell). On
// success, it calls the mod init function if provided.
__attribute__((visibility("hidden"))) __attribute__((constructor)) void nh_init();

// nh_dlhook takes a lib handle from dlopen and redirects the specified symbol
// to another, returning the previous external hook or the original function.
// Only calls from that library are affected (it replaces that library's GOT).
// This function requires glibc and Linux. It should work on any architecture,
// and it should be resilient to most errors. If it fails, no changes will have
// been made, NULL is returned and the error is logged with nh_log.
__attribute__((visibility("hidden"))) void *nh_dlhook(void *handle, const char *symname, void *target);

// Get the handle for the library whose PLT a hook targets. libnickel is kept
// open for the whole initialization path; other libraries are opened by name.
// Use this for both applying and restoring hooks so rollback edits the same
// library selected by nh_hook.lib.
static void *nh_hook_lib(const char *name, void *libnickel) {
    if (!strcmp(name, "libnickel.so.1.0.0") ||
        !strcmp(name, "/usr/local/Kobo/libnickel.so.1.0.0"))
        return libnickel;
    return dlopen(name, RTLD_LAZY|RTLD_LOCAL);
}

// nh_failsafe_t is a failsafe mechanism for injected shared libraries. It
// works by moving it to a temporary file in the parent directory (so it won't
// get loaded the next time; Qt 6 loads any file in a plugin directory, so a
// new name in the same directory is not enough) and dlopening itself (to
// prevent it from being unloaded if it is dlclose'd by whatever dlopen'd it).
// When it is disarmed, the library is moved back to its original location.
typedef struct nh_failsafe_t nh_failsafe_t;

// nh_failsafe_create allocates and arms a failsafe mechanism for the currently
// dlopen'd or LD_PRELOAD'd library. On error, NULL is returned and the error is
// logged with nh_log.
__attribute__((visibility("hidden"))) nh_failsafe_t *nh_failsafe_create();

// nh_failsafe_self gets the pointer to the loaded library for use with dlsym.
// This pointer will remain valid even after the failsafe is destroyed.
__attribute__((visibility("hidden"))) void *nh_failsafe_self(nh_failsafe_t *fs);

// nh_failsafe_destroy starts a pthread which disarms and frees the failsafe
// after a delay. The nh_failsafe_t must not be used afterwards.
__attribute__((visibility("hidden"))) void nh_failsafe_destroy(nh_failsafe_t *fs, int delay);

// nh_failsafe_uninstall uninstalls the lib. The nh_failsafe_t must not be
// used afterwards.
__attribute__((visibility("hidden"))) void nh_failsafe_uninstall(nh_failsafe_t *fs);

// --- init

void nh_init() {
    const struct nh *nh = &NickelHook;

    if (!nh->info) {
        syslog(LOG_DEBUG, "(NickelHook) fatal: NickelHook->info is NULL");
        return;
    }

    nh_log("(NickelHook) initializing '%s' (version: %s)", nh->info->name, NH_VERSION);

    if (nh->info->desc)
        nh_log("(NickelHook) ... desc: %s", nh->info->desc);

    nh_log("(NickelHook) creating failsafe");

    nh_failsafe_t *fs;
    if (!(fs = nh_failsafe_create())) {
        nh_log("(NickelHook) ... error creating failsafe, returning");
        goto nh_init_return_no_fs;
    }

    nh_log("(NickelHook) checking config");

    for (struct nh_dlsym *v = nh->dlsym; v && v->name; v++) {
        if (!v->out) {
            nh_log("(NickelHook) ... error in mod: nh_dlsym missing symbol name");
            goto nh_init_return_err;
        }
    }

    for (struct nh_hook *v = nh->hook; v && v->sym; v++) {
        if (!v->sym || !v->sym_new || !v->lib || !v->out) {
            nh_log("(NickelHook) ... error in mod: nh_hook missing sym, sym_new, lib, or out");
            goto nh_init_return_err;
        }
    }

    if (nh->info->uninstall_flag) {
        nh_log("(NickelHook) checking for uninstall flag '%s'", nh->info->uninstall_flag);
        if (!access(nh->info->uninstall_flag, F_OK)) {
            nh_log("(NickelHook) ... info: flag found, uninstalling");
            unlink(nh->info->uninstall_flag);
            nh_failsafe_uninstall(fs);
            if (nh->uninstall && !nh->uninstall()) {
                nh_dump_log();
            }
            goto nh_init_return_no_fs;
        }
    }

    if (nh->info->uninstall_xflag) {
        nh_log("(NickelHook) checking for uninstall !flag '%s'", nh->info->uninstall_xflag);
        if (access(nh->info->uninstall_xflag, F_OK) && errno == ENOENT) {
            nh_log("(NickelHook) ... info: flag removed, uninstalling");
            nh_failsafe_uninstall(fs);
            if (nh->uninstall && !nh->uninstall()) {
                nh_dump_log();
            }
            goto nh_init_return_no_fs;
        }
    }

    nh_log("(NickelHook) loading libraries");

    void *self = nh_failsafe_self(fs);
    if (!self) {
        nh_log("(NickelHook) ... fatal: could not get own dlopen handle from failsafe");
        goto nh_init_return_err;
    }

    void *libnickel = dlopen("libnickel.so.1.0.0", RTLD_LAZY|RTLD_NODELETE);
    if (!libnickel) {
        nh_log("(NickelHook) ... fatal: dlopen libnickel: %s", dlerror());
        goto nh_init_return_err;
    }

    nh_log("(NickelHook) resolving symbols");

    for (struct nh_dlsym *v = nh->dlsym; v && v->name; v++) {
        nh_log("(NickelHook) info: nh_dlsym: loading symbol '%s' from RTLD_DEFAULT to %p%s%s%s%s", v->name, v->out, v->desc ? " (desc: " : "", v->desc ?: "", v->desc ? ")" : "", v->optional ? " (optional)" : "");
        if (!(*v->out = dlsym(RTLD_DEFAULT, v->name))) {
            if (!v->optional) {
                nh_log("(NickelHook) ... fatal: could not load symbol: %s", dlerror());
                goto nh_init_return_err;
            }
            nh_log("(NickelHook) ... info: could not load symbol: %s, is optional so ignoring", dlerror());
        }
    }

    nh_log("(NickelHook) applying hooks");

    struct nh_hook *e = NULL;

    for (struct nh_hook *v = nh->hook; v && v->sym; v++) {
        nh_log("(NickelHook) info: nh_hook: setting hook on ('%s', '%s') to (self, '%s')%s%s%s%s", v->lib, v->sym, v->sym_new, v->desc ? " (desc: " : "", v->desc ?: "", v->desc ? ")" : "", v->optional ? " (optional)" : "");

        *v->out = NULL;

        void *lib = nh_hook_lib(v->lib, libnickel);
        if (!lib) {
            if (!v->optional) {
                nh_log("(NickelHook) ... fatal: could not load lib: %s", dlerror());
                goto nh_init_return_err_rhook;
            }
            nh_log("(NickelHook) ... info: could not load lib: %s, is optional so ignoring", dlerror());
            continue;
        }

        void *target = dlsym(self, v->sym_new);
        if (!target) {
            if (!v->optional) {
                nh_log("(NickelHook) ... fatal: could not load new symbol: %s", dlerror());
                goto nh_init_return_err_rhook;
            }
            nh_log("(NickelHook) ... info: could not load new symbol: %s, is optional so ignoring", dlerror());
            continue;
        }

        if (!(*v->out = nh_dlhook(lib, v->sym, target))) {
            if (!v->optional) {
                nh_log("(NickelHook) ... fatal: could not hook symbol");
                goto nh_init_return_err_rhook;
            }
            nh_log("(NickelHook) ... info: could not hook symbol, is optional so ignoring");
            continue;
        }

        nh_log("(NickelHook) ... info: redirected ('%s'=%p, '%s'=%p) to (self=%p, '%s'=%p)", v->lib, lib, v->sym, *v->out, self, v->sym_new, target);
        e = v;

        continue;
    }

    if (nh->init) {
        nh_log("(NickelHook) calling next init");
        if (nh->init()) {
            nh_log("(NickelHook) ... error: init returned error");
            goto nh_init_return_err_rhook;
        }
    }

    goto nh_init_return;

nh_init_return_err_rhook:
    if (e) {
        nh_log("(NickelHook) info: restoring hooks");
        for (struct nh_hook *v = e; v >= nh->hook; v--) {
            nh_log("(NickelHook) info: restoring hook on ('%s', '%s') from (self, '%s') to %p", v->lib, v->sym, v->sym_new, *v->out); // out will have been previously set to the original func
            if (*v->out) {
                void *lib = nh_hook_lib(v->lib, libnickel);
                if (!lib) {
                    nh_log("(NickelHook) ... warning: could not load '%s' to restore '%s': %s", v->lib, v->sym, dlerror());
                } else if (!nh_dlhook(lib, v->sym, *v->out)) {
                    nh_log("(NickelHook) ... warning: failed to restore hook on ('%s', '%s')", v->lib, v->sym);
                }
            } else {
                nh_log("(NickelHook) ... warning: failed to restore hook: original should have been set, but it was null");
            }
        }
    }

nh_init_return_err:
    nh_log("(NickelHook) fatal error");
    nh_dump_log();
    // Keep an incompatible plugin outside Qt's scan directory. Configuration
    // and logs remain available, and installing a new package retries setup.
    nh_log("(NickelHook) compatibility failure: leaving plugin parked");
    free(fs);
    goto nh_init_return_no_fs;

nh_init_return:
    if (nh->info->failsafe_delay) {
        nh_log("(NickelHook) destroying failsafe after %ds", nh->info->failsafe_delay);
    } else {
        nh_log("(NickelHook) destroying failsafe");
    }
    nh_failsafe_destroy(fs, nh->info->failsafe_delay);

nh_init_return_no_fs:
    nh_log("(NickelHook) done init");
    return;
}

// --- log

#ifndef NH_NO_LOGGING
void nh_log(const char *fmt, ...) {
    static __thread char buf[256] = {0};
    int n;

    if ((n = snprintf(buf, sizeof(buf), "(%s) ", NickelHook.info->name ?: "???")) < 0)
        n = 0;

    if (fmt) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(&buf[n], sizeof(buf)-n, fmt, args);
        va_end(args);
    }

    buf[sizeof(buf)-1] = '\0';
    syslog(LOG_DEBUG, "%s", buf);
}

#endif

void nh_dump_log() {
#ifndef NH_NO_LOGGING
    char cmd[PATH_MAX];
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    if (!l || snprintf(cmd, sizeof(cmd), "logread > '/mnt/onboard/%s_%d-%02d-%02d_%02d-%02d-%02d.log'", NickelHook.info->name ?: "NickelHook", l->tm_year + 1900, l->tm_mon + 1, l->tm_mday, l->tm_hour, l->tm_min, l->tm_sec) < 0)
        strcpy(cmd, "logread > '/mnt/onboard/NickelHook.log'");
    if (system(cmd))
        nh_log("(NickelHook) warning: could not dump the log with logread");
#endif
}

// --- dlhook

#ifndef ELFW
#define ELFW(type) _ElfW(ELF, __ELF_NATIVE_CLASS, type)
#endif

#if __x86_64__
#define R_JUMP_SLOT R_X86_64_JUMP_SLOT
#else
#if __arm__
#define R_JUMP_SLOT R_ARM_JUMP_SLOT
#else
#error Please define the architecture-specific R_*_JUMP_SLOT type.
#endif
#endif

struct nh_range {
    uintptr_t address;
    size_t size;
    uintptr_t base;
    unsigned flags;
    bool found;
};

static int nh_find_range(struct dl_phdr_info *info, size_t size, void *opaque) {
    (void)size;
    struct nh_range *range = opaque;
    if (range->base != (uintptr_t)info->dlpi_addr) return 0;
    for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr) *p = &info->dlpi_phdr[i];
        if (p->p_type != PT_LOAD || (p->p_flags & range->flags) != range->flags) continue;
        uintptr_t start = info->dlpi_addr + p->p_vaddr;
        if (range->address >= start && range->address - start < p->p_memsz
                && range->size <= p->p_memsz - (range->address - start)) {
            range->found = true;
            return 1;
        }
    }
    return 0;
}

static bool nh_mapped(struct link_map *lm, const void *address, size_t size, unsigned flags) {
    struct nh_range range = {(uintptr_t)address, size, lm->l_addr, flags, false};
    if (!address || !size) return false;
    dl_iterate_phdr(nh_find_range, &range);
    return range.found;
}

static bool nh_executable(void *address) {
    Dl_info info;
    if (!dladdr(address, &info)) return false;
    struct link_map lm = {.l_addr = (uintptr_t)info.dli_fbase};
#if __arm__
    address = (void *)((uintptr_t)address & ~(uintptr_t)1);
#endif
    return nh_mapped(&lm, address, 1, PF_X);
}

static int nh_page_protection(void *address, size_t size) {
    FILE *maps = fopen("/proc/self/maps", "re");
    if (!maps) return -1;
    char line[1024], permissions[5];
    unsigned long start, end;
    const uintptr_t page = (uintptr_t)address;
    int protection = -1;
    while (fgets(line, sizeof(line), maps)) {
        if (sscanf(line, "%lx-%lx %4s", &start, &end, permissions) == 3 &&
                page >= start && page < end && size <= end - page) {
            protection = (permissions[0] == 'r' ? PROT_READ : 0) |
                (permissions[1] == 'w' ? PROT_WRITE : 0) |
                (permissions[2] == 'x' ? PROT_EXEC : 0);
            break;
        }
    }
    fclose(maps);
    return protection;
}

void *nh_dlhook(void *handle, const char *symname, void *target) {
    #define NH_DLHOOK_CHECK(cond, fmt, ...) do {                                                        \
        if (!(cond)) {                                                                                  \
            nh_log("(NickelHook) ... dlhook: error: " fmt " (check failed: %s)", ##__VA_ARGS__, #cond); \
            return NULL;                                                                                \
        }                                                                                               \
    } while (0)                                                                                         \

    NH_DLHOOK_CHECK(handle && symname && target, "BUG: required arguments are null");
    NH_DLHOOK_CHECK(nh_executable(target), "replacement is outside executable library memory");

    // the link_map conveniently gives use the base address without /proc/maps, and it gives us a pointer to dyn
    struct link_map *lm;
    NH_DLHOOK_CHECK(!dlinfo(handle, RTLD_DI_LINKMAP, &lm), "could not get link_map for lib");
    nh_log("(NickelHook) ... dlhook: info: lib %s is mapped at %lx", lm->l_name, (unsigned long)(lm->l_addr));

    // stuff extracted from DT_DYNAMIC
    struct {
        bool plt_is_rela;
        union {
            ElfW(Addr) _;
            ElfW(Rel)  *rel;
            ElfW(Rela) *rela;
        } plt;
        ElfW(Xword) plt_sz;
        ElfW(Xword) plt_ent_sz;
        ElfW(Sym)   *sym;
        const char  *str;
        size_t str_sz;
    } dyn = {0};

    // parse DT_DYNAMIC
    for (size_t i = 0; lm->l_ld[i].d_tag != DT_NULL; i++) {
        switch (lm->l_ld[i].d_tag) {
        case DT_PLTREL:   dyn.plt_is_rela = lm->l_ld[i].d_un.d_val == DT_RELA;          break; // .rel.plt - is Rel or Rela
        case DT_JMPREL:   dyn.plt._       = lm->l_ld[i].d_un.d_ptr;                     break; // .rel.plt - offset from base
        case DT_PLTRELSZ: dyn.plt_sz      = lm->l_ld[i].d_un.d_val;                     break; // .rel.plt - size in bytes
        case DT_RELENT:   if (!dyn.plt_ent_sz) dyn.plt_ent_sz = lm->l_ld[i].d_un.d_val; break; // .rel.plt - entry size if Rel
        case DT_RELAENT:  if (!dyn.plt_ent_sz) dyn.plt_ent_sz = lm->l_ld[i].d_un.d_val; break; // .rel.plt - entry size if Rela
        case DT_SYMTAB:   dyn.sym         = (ElfW(Sym)*)(lm->l_ld[i].d_un.d_val);       break; // .dynsym  - offset
        case DT_STRTAB:   dyn.str         = (const char*)(lm->l_ld[i].d_un.d_val);      break; // .dynstr  - offset
        case DT_STRSZ:    dyn.str_sz      = lm->l_ld[i].d_un.d_val;                    break;
        }
    }
    nh_log("(NickelHook) ... dlhook: info: DT_DYNAMIC: plt_is_rela=%d plt=%p plt_sz=%lu plt_ent_sz=%lu sym=%p str=%p", dyn.plt_is_rela, (void*)(dyn.plt)._, (unsigned long)(dyn.plt_sz), (unsigned long)(dyn.plt_ent_sz), (void*)(dyn.sym), (const void*)(dyn.str));
    NH_DLHOOK_CHECK(dyn.plt_ent_sz, "plt_ent_sz is zero");
    NH_DLHOOK_CHECK(dyn.plt_sz%dyn.plt_ent_sz == 0, ".rel.plt length is not a multiple of plt_ent_sz");
    NH_DLHOOK_CHECK((dyn.plt_is_rela ? sizeof(*dyn.plt.rela) : sizeof(*dyn.plt.rel)) == dyn.plt_ent_sz, "size mismatch (%lu != %lu)", (unsigned long)(dyn.plt_is_rela ? sizeof(*dyn.plt.rela) : sizeof(*dyn.plt.rel)), (unsigned long)(dyn.plt_ent_sz));
    NH_DLHOOK_CHECK(nh_mapped(lm, (void *)dyn.plt._, dyn.plt_sz, PF_R), "relocation table is outside readable library memory");
    NH_DLHOOK_CHECK(nh_mapped(lm, dyn.str, dyn.str_sz, PF_R), "string table is outside readable library memory");

    // parse the dynamic symbol table, resolve symbols to relocations, then GOT entries
    for (size_t i = 0; i < dyn.plt_sz/dyn.plt_ent_sz; i++) {
        // note: Rela is a superset of Rel
        ElfW(Rel) *rel = dyn.plt_is_rela
            ? (ElfW(Rel)*)(&dyn.plt.rela[i])
            : &dyn.plt.rel[i];
        NH_DLHOOK_CHECK(ELFW(R_TYPE)(rel->r_info) == R_JUMP_SLOT, "not a jump slot relocation (R_TYPE=%lu)", (unsigned long)(ELFW(R_TYPE)(rel->r_info)));

        size_t index = ELFW(R_SYM)(rel->r_info);
        NH_DLHOOK_CHECK(index <= (UINTPTR_MAX - (uintptr_t)dyn.sym) / sizeof(*dyn.sym), "symbol address overflow");
        ElfW(Sym) *sym = (ElfW(Sym) *)((uintptr_t)dyn.sym + index * sizeof(*dyn.sym));
        NH_DLHOOK_CHECK(nh_mapped(lm, sym, sizeof(*sym), PF_R), "symbol is outside readable library memory");
        NH_DLHOOK_CHECK(sym->st_name < dyn.str_sz, "symbol name is outside string table");
        const char *str = &dyn.str[sym->st_name];
        NH_DLHOOK_CHECK(memchr(str, 0, dyn.str_sz - sym->st_name), "unterminated symbol name");
        if (strcmp(str, symname))
            continue;

        NH_DLHOOK_CHECK(rel->r_offset <= UINTPTR_MAX - lm->l_addr, "GOT address overflow");
        void **gotoff = (void**)(lm->l_addr + rel->r_offset);
        NH_DLHOOK_CHECK((uintptr_t)gotoff % sizeof(void *) == 0
            && nh_mapped(lm, gotoff, sizeof(*gotoff), PF_R | PF_W), "GOT slot is outside writable library segment");
        nh_log("(NickelHook) ... dlhook: info: found symbol %s (gotoff=%p [mapped=%p])", str, (void*)(rel->r_offset), gotoff);

        NH_DLHOOK_CHECK(ELFW(ST_TYPE)(sym->st_info) != STT_GNU_IFUNC, "STT_GNU_IFUNC not implemented (gotoff=%p)", (void*)(rel->r_offset));
        NH_DLHOOK_CHECK(ELFW(ST_TYPE)(sym->st_info) == STT_FUNC, "not a function symbol (ST_TYPE=%d) (gotoff=%p)", (int)(ELFW(ST_TYPE)(sym->st_info)), (void*)(rel->r_offset));
        NH_DLHOOK_CHECK(ELFW(ST_BIND)(sym->st_info) == STB_GLOBAL, "not a globally bound symbol (ST_BIND=%d) (gotoff=%p)", (int)(ELFW(ST_BIND)(sym->st_info)), (void*)(rel->r_offset));

        nh_log("(NickelHook) ... dlhook: info: resolving the original symbol");
        void *orig = dlsym(handle, symname);
        NH_DLHOOK_CHECK(orig, "could not dlsym symbol");
        NH_DLHOOK_CHECK(*gotoff != target, "target is already installed");

        // dlsym does not resolve a lazy GOT slot or report another mod's hook.
        // ARM and x86-64 lazy slots point into the calling library's PLT.
        // Resolve lazy slots in the global scope first to retain LD_PRELOAD
        // interposers. A handle-only lookup can skip those. Already resolved
        // external slots must keep their current target, including other mods.
        Dl_info previous;
        if (*gotoff != orig && dladdr(*gotoff, &previous)) {
            if (previous.dli_fbase != (void *)lm->l_addr) {
                orig = *gotoff;
                nh_log("(NickelHook) ... dlhook: info: preserving previous hook %p in %s", orig, previous.dli_fname);
            } else {
                void *global = dlsym(RTLD_DEFAULT, symname);
                if (global) orig = global;
            }
        }
        NH_DLHOOK_CHECK(orig != target, "target would call itself");
        NH_DLHOOK_CHECK(nh_executable(orig), "previous function is outside executable library memory");

        long pagesize = sysconf(_SC_PAGESIZE);
        NH_DLHOOK_CHECK(pagesize > 0 && !(pagesize & (pagesize - 1)), "could not get memory page size");
        void *gotpage = (void*)((size_t)(gotoff) & ~(pagesize-1));
        int protection = nh_page_protection(gotpage, pagesize);
        NH_DLHOOK_CHECK(protection >= 0 && (protection & PROT_READ) && !(protection & PROT_EXEC), "could not determine non-executable GOT page protection");
        if (!(protection & PROT_WRITE))
            NH_DLHOOK_CHECK(!mprotect(gotpage, pagesize, protection | PROT_WRITE), "could not make GOT page writable");

        void *previous_target = *gotoff;
        __atomic_store_n(gotoff, target, __ATOMIC_RELEASE);
        // Restore RELRO, while leaving lazy-binding pages writable as found.
        if (!(protection & PROT_WRITE) && mprotect(gotpage, pagesize, protection)) {
            __atomic_store_n(gotoff, previous_target, __ATOMIC_RELEASE);
            if (mprotect(gotpage, pagesize, protection))
                nh_log("(NickelHook) ... dlhook: error: GOT protection restore failed twice; reboot required");
            NH_DLHOOK_CHECK(false, "GOT protection restore failed; hook rolled back");
        }
        nh_log("(NickelHook) ... dlhook: info: successfully patched symbol %s (orig=%p, new=%p)", str, orig, target);

        return orig;
    }

    nh_log("(NickelHook) ... dlhook: error: could not find symbol");
    return NULL;

    #undef NH_DLHOOK_CHECK
}

// --- failsafe

struct nh_failsafe_t {
    char orig[PATH_MAX];
    char tmp[PATH_MAX];
    int  delay;
    void *self;
};

nh_failsafe_t *nh_failsafe_create() {
    #define NH_FAILSAFE_CHECK(cond, fmt, ...) do {                                                        \
        if (!(cond)) {                                                                                    \
            nh_log("(NickelHook) ... failsafe: error: " fmt " (check failed: %s)", ##__VA_ARGS__, #cond); \
            return NULL;                                                                                  \
        }                                                                                                 \
    } while (0)                                                                                           \

    nh_log("(NickelHook) ... failsafe: info: allocating memory");
    nh_failsafe_t *fs;
    NH_FAILSAFE_CHECK((fs = calloc(1, sizeof(nh_failsafe_t))), "could not allocate memory");

    nh_log("(NickelHook) ... failsafe: info: finding filenames");
    Dl_info info;
    NH_FAILSAFE_CHECK(dladdr(nh_failsafe_create, &info), "could not find own path");
    NH_FAILSAFE_CHECK(info.dli_fname, "dladdr did not return a filename");
    const char *d = strrchr(info.dli_fname, '.');
    NH_FAILSAFE_CHECK(!(d && !strcmp(d, ".failsafe")), "lib was loaded from the failsafe for some reason");
    NH_FAILSAFE_CHECK(realpath(info.dli_fname, fs->orig), "could not resolve %s", info.dli_fname);

    // /usr/local/Kobo/imageformats/libnm.so becomes /usr/local/Kobo/libnm.so.failsafe:
    // one directory up, where no plugin loader looks, on the same filesystem so rename works.
    const char *base = strrchr(fs->orig, '/');
    NH_FAILSAFE_CHECK(base && base != fs->orig, "own path %s has no parent directory", fs->orig);
    const char *parent = memrchr(fs->orig, '/', base - fs->orig);
    NH_FAILSAFE_CHECK(parent, "own path %s has no parent directory", fs->orig);
    NH_FAILSAFE_CHECK(snprintf(fs->tmp, sizeof(fs->tmp), "%.*s%s.failsafe", (int)(parent - fs->orig + 1), fs->orig, base + 1) < (int)(sizeof(fs->tmp)), "could not generate temp filename");

    nh_log("(NickelHook) ... failsafe: info: ensuring own lib remains in memory even if it is dlclosed after being loaded with a dlopen");
    NH_FAILSAFE_CHECK((fs->self = dlopen(fs->orig, RTLD_LAZY|RTLD_NODELETE)), "could not dlopen self");

    nh_log("(NickelHook) ... failsafe: info: renaming %s to %s", fs->orig, fs->tmp);
    NH_FAILSAFE_CHECK(!rename(fs->orig, fs->tmp), "could not rename lib");

    return fs;

    #undef NH_FAILSAFE_CHECK
}

void *nh_failsafe_self(nh_failsafe_t *fs) {
    return fs->self;
}

static void *_nh_failsafe_destroy(void *_fs) {
    nh_failsafe_t *fs = (nh_failsafe_t*)(_fs);

    nh_log("(NickelHook) ... failsafe: info: restoring after %d seconds", fs->delay);
    sleep(fs->delay);

    nh_log("(NickelHook) ... failsafe: info: renaming %s to %s", fs->tmp, fs->orig);
    if (rename(fs->tmp, fs->orig))
        nh_log("(NickelHook) ... failsafe: warning: could not rename lib");

    nh_log("(NickelHook) ... failsafe: info: freeing memory");
    free(fs);

    return NULL;
}

void nh_failsafe_destroy(nh_failsafe_t *fs, int delay) {
    fs->delay = delay;
    nh_log("(NickelHook) ... failsafe: info: scheduling restore");
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t t;
    pthread_create(&t, &attr, _nh_failsafe_destroy, fs);
    const char name[16] = "nh_failsafe";
    pthread_setname_np(t, name);
    pthread_attr_destroy(&attr);
}

void nh_failsafe_uninstall(nh_failsafe_t *fs) {
    nh_log("(NickelHook) ... failsafe: info: deleting %s", fs->tmp);
    if (unlink(fs->tmp) == 0 && nh_resources_uninstalled) nh_resources_uninstalled();
}

// --- File/directory deletion

// The following blacklist helps prevent any whoopsies when deleting files or
// directories during uninstall.

// Let's not allow deleting any files in /bin, /sbin, /etc/init.d or u-boot
// stuff.
static const char *delete_prefix_blacklist[] = {
    "/bin",
    "/sbin",
    // note, '/etc/u-boot/*/u-boot.recovery' is a critical file for recovery
    "/etc/u-boot",
    "/etc/init.d",
    "/usr/local/Kobo/pickel",
};

// nh_delete_path deletes the provided path if it exists. It performs some
// checks to detect and avoid deleting critical system files
//
// This is for the schmuck who forgot to terminate his array of paths and
// accidentally deleted '/bin/sh' due to an out-of-bounds read.
static bool nh_delete_path(const char *path, bool is_dir) {
    if (!path) {
        nh_log("(NickelHook) no path supplied");
        return false;
    }
    char fn[PATH_MAX] = {0};
    if (!realpath(path, fn)) {
        nh_log("(NickelHook) unable to get canonical path for %s : %m", path);
        return false;
    }
    for (size_t i = 0; i < (sizeof(delete_prefix_blacklist) / sizeof(*delete_prefix_blacklist)); i++) {
        if (!strncmp(delete_prefix_blacklist[i], fn, strlen(delete_prefix_blacklist[i]))) {
            nh_log("(NickelHook) not deleting %s with blacklisted prefix %s", fn, delete_prefix_blacklist[i]);
            return false;
        }
    }
    nh_log("(NickelHook) ... deleting %s %s", (is_dir ? "directory" : "file"), fn);
    int res = is_dir 
        ? rmdir(fn) 
        : unlink(fn);
    if (res == -1 && errno != ENOENT) {
        nh_log("(NickelHook) failed to delete %s, with error: %m", fn);
        return false;
    }
    return true;
}

bool nh_delete_file(const char *path) {
    return nh_delete_path(path, false);
}

bool nh_delete_dir(const char *path) {
    return nh_delete_path(path, true);
}
