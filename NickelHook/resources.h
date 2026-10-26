#ifndef NH_RESOURCES_H
#define NH_RESOURCES_H
#include <stddef.h>

struct nh_resource {
    const char *name;
    const unsigned char *data;
    size_t size;
};

// Embedded files are created on user storage once. Existing user files survive.
struct nh_resources {
    const char *config_dir;
    const char *uninstall_file;
    const struct nh_resource *files;
};

extern const struct nh_resources nh_resources;
// NickelHook calls this after removing the library, so reinstall can seed again.
__attribute__((visibility("hidden"))) void nh_resources_uninstalled(void);
#endif
