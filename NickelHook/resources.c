#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <NickelHook.h>
#include "resources.h"

static bool paths(char *parent, size_t parent_size, char *marker, size_t marker_size) {
    if (snprintf(parent, parent_size, "%s", nh_resources.config_dir) >= (int)parent_size)
        return false;
    char *slash = strrchr(parent, '/');
    if (!slash || slash == parent || !slash[1]) return false;
    *slash = '\0';
    return snprintf(marker, marker_size, "%s/.%s.initialized", parent, slash + 1) < (int)marker_size;
}

static bool write_file(const char *target, const unsigned char *data, size_t size) {
    char temporary[1024];
    if (snprintf(temporary, sizeof(temporary), "%s.install.XXXXXX", target) >= (int)sizeof(temporary))
        return false;
    int fd = mkstemp(temporary);
    if (fd < 0) return false;
    FILE *output = fdopen(fd, "wb");
    if (!output) { close(fd); unlink(temporary); return false; }
    bool ok = fwrite(data, 1, size, output) == size;
    if (ok && fflush(output) != 0) ok = false;
    if (ok && fchmod(fd, 0644) != 0) ok = false;
    if (ok && fsync(fd) != 0) ok = false;
    if (fclose(output) != 0) ok = false;
    if (ok && rename(temporary, target) != 0) ok = false;
    if (!ok) unlink(temporary);
    return ok;
}

// The receipt sits beside the config directory on user storage. Removing the
// directory still requests uninstall; only actual plugin removal clears it.
__attribute__((constructor(200))) static void install_resources(void) {
    char parent[1024], marker[1024];
    if (!paths(parent, sizeof(parent), marker, sizeof(marker))) return;
    if (access(nh_resources.uninstall_file, F_OK) == 0) return;
    struct stat status;
    if (lstat(marker, &status) == 0 || errno != ENOENT) return;
    if ((mkdir(parent, 0755) != 0 && errno != EEXIST) ||
        (mkdir(nh_resources.config_dir, 0755) != 0 && errno != EEXIST)) {
        nh_log("resource installation: cannot create config directory");
        return;
    }
    for (const struct nh_resource *file = nh_resources.files; file->name; ++file) {
        char target[1024];
        if (snprintf(target, sizeof(target), "%s/%s", nh_resources.config_dir, file->name) >= (int)sizeof(target))
            return;
        if (lstat(target, &status) == 0) {
            if (S_ISREG(status.st_mode)) continue;
            nh_log("resource installation: target is not a regular file");
            return;
        }
        if (errno != ENOENT || !write_file(target, file->data, file->size)) {
            nh_log("resource installation failed; retrying on next load");
            return;
        }
    }
    // Publish completion only after all resource files have reached storage.
    sync();
    if (!write_file(marker, (const unsigned char *)"1\n", 2)) {
        nh_log("resource installation: cannot record completion");
        return;
    }
    sync();
    nh_log("installed embedded support files on user storage");
}

void nh_resources_uninstalled(void) {
    char parent[1024], marker[1024];
    if (paths(parent, sizeof(parent), marker, sizeof(marker)) && unlink(marker) != 0 && errno != ENOENT)
        nh_log("resource uninstall: cannot clear initialization receipt");
}
