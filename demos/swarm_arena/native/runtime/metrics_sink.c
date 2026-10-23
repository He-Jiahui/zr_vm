#include "runtime.h"

#include <stdio.h>

static struct { FILE *file; int64_t handle; } sinks[ZRG_SINK_CAPACITY];
static int64_t next_handle = 1;

int64_t zrg_sink_open(const char *path) {
    if (!path || !*path || next_handle == INT64_MAX) return 0;
    for (uint32_t i = 0; i < ZRG_SINK_CAPACITY; ++i) {
        if (sinks[i].file) continue;
        FILE *file = fopen(path, "wb");
        if (!file) return 0;
        sinks[i].file = file;
        sinks[i].handle = next_handle++;
        return sinks[i].handle;
    }
    return 0;
}

bool zrg_sink_write(int64_t handle, const char *bytes, size_t length) {
    if (!bytes) return false;
    for (uint32_t i = 0; i < ZRG_SINK_CAPACITY; ++i) {
        if (!sinks[i].file || sinks[i].handle != handle) continue;
        FILE *file = sinks[i].file;
        return fwrite(bytes, 1, length, file) == length && fputc('\n', file) != EOF && fflush(file) == 0;
    }
    return false;
}

bool zrg_sink_close(int64_t handle) {
    for (uint32_t i = 0; i < ZRG_SINK_CAPACITY; ++i) {
        if (!sinks[i].file || sinks[i].handle != handle) continue;
        FILE *file = sinks[i].file;
        sinks[i].file = NULL;
        sinks[i].handle = 0;
        return fclose(file) == 0;
    }
    return true;
}
