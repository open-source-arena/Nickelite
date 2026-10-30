#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "action.h"
#include "kfmon.h"
#include "util.h"

NM_ACTION_(dbg_syslog) {
    NM_LOG("dbgsyslog: %s", arg);
    return nm_action_result_silent();
}

NM_ACTION_(dbg_error) {
    NM_ERR_SET("%s", arg);
    return NULL;
}

NM_ACTION_(dbg_msg) {
    return nm_action_result_msg("%s", arg);
}

NM_ACTION_(dbg_toast) {
    return nm_action_result_toast("%s", arg);
}

NM_ACTION_(skip) {
    char *tmp;
    long n = strtol(arg, &tmp, 10);
    NM_CHECK(NULL, *arg && !*tmp && n != 0 && n >= -1 && n < INT_MAX, "invalid count '%s': must be a nonzero integer or -1", arg);

    nm_action_result_t *res = calloc(1, sizeof(nm_action_result_t));
    res->type = NM_ACTION_RESULT_TYPE_SKIP;
    res->skip = (int)(n);
    return res;
}

NM_ACTION_(kfmon_id) {
    // Start by watch ID (simpler, but IDs may not be stable across a single power cycle, given severe KFMon config shuffling)
    int status = nm_kfmon_simple_request("start", arg);
    return nm_kfmon_return_handler(status);
}

NM_ACTION_(kfmon) {
    // Trigger a watch, given its trigger basename. Stable runtime lookup done by KFMon.
    int status = nm_kfmon_simple_request("trigger", arg);

    // Fixup INVALID_ID to INVALID_NAME for slightly clearer feedback (see e8b2588 for details).
    if (status == KFMON_IPC_ERR_INVALID_ID)
        status = KFMON_IPC_ERR_INVALID_NAME;

    return nm_kfmon_return_handler(status);
}

#if NM_QT_MAJOR >= 6
NM_ACTION_(kfmon_list) {
    (void) arg;
    kfmon_watch_list_t list = { 0 };
    const int status = nm_kfmon_list_request("gui-list", &list);
    if (nm_kfmon_error_handler(status)) {
        kfmon_teardown_list(&list);
        return NULL;
    }
    if (!list.count) {
        kfmon_teardown_list(&list);
        nm_action_result_t *empty = calloc(1, sizeof(*empty));
        if (!empty)
            NM_ERR_RET(NULL, "calloc: %m");
        empty->type = NM_ACTION_RESULT_TYPE_LIST;
        return empty;
    }

    nm_action_result_t *res = calloc(1, sizeof(*res));
    if (!res) {
        kfmon_teardown_list(&list);
        NM_ERR_RET(NULL, "calloc: %m");
    }
    res->type = NM_ACTION_RESULT_TYPE_LIST;
    res->list_labels = calloc(list.count, sizeof(*res->list_labels));
    res->list_values = calloc(list.count, sizeof(*res->list_values));
    if (!res->list_labels || !res->list_values) {
        kfmon_teardown_list(&list);
        nm_action_result_free(res);
        NM_ERR_RET(NULL, "calloc: %m");
    }
    for (kfmon_watch_node_t *node = list.head; node; node = node->next) {
        const size_t i = res->list_count++;
        res->list_labels[i] = strdup(node->watch.label ? node->watch.label : node->watch.filename);
        res->list_values[i] = strdup(node->watch.filename);
        if (!res->list_labels[i] || !res->list_values[i]) {
            kfmon_teardown_list(&list);
            nm_action_result_free(res);
            NM_ERR_RET(NULL, "strdup: %m");
        }
    }
    kfmon_teardown_list(&list);
    return res;
}
#endif

NM_ACTION_(uninstall) {
    (void) arg;
    if (remove("/usr/local/Kobo/imageformats/libnm.so")) { // TODO: don't hardcode this
        NM_LOG("uninstall: failed to remove library: %s", strerror(errno));
        NM_LOG("uninstall: falling back to uninstall flag");
        if (mkdir("/mnt/onboard/.adds", 0755) && errno != EEXIST) // TODO: don't hardcode this
            return nm_action_result_msg("failed to create uninstall flag: mkdir .adds: %s", strerror(errno));
        if (mkdir(NM_CONFIG_DIR, 0755) && errno != EEXIST)
            return nm_action_result_msg("failed to create uninstall flag: mkdir %s: %s", NM_CONFIG_DIR, strerror(errno));
        int fd = open(NM_CONFIG_DIR "/uninstall", O_CREAT|O_WRONLY, 0644);
        if (fd == -1) {
            return nm_action_result_msg("failed to create uninstall flag: open %s/uninstall: %s", NM_CONFIG_DIR, strerror(errno));
        }
        NM_LOG("uninstall: created uninstall flag");
        close(fd);
    } else {
        NM_LOG("uninstall: removed lib");
        if (!remove(NM_CONFIG_DIR "/uninstall")) // so if the user created it already, it won't uninstall the next time it gets installed
            NM_LOG("uninstall: removed old uninstall flag");
    }
    NM_LOG("uninstall: rebooting");
    return nm_action_power("reboot");
}
