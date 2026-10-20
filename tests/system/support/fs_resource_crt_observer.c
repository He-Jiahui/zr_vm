#include "fs_resource_crt_observer.h"
#include <string.h>

/* Fixed storage: observing an allocation never allocates. Rows describe exact
 * returned pointers, not process-wide heap totals or Core allocator counters. */
typedef struct FsCrtRow { void *pointer; unsigned frees; } FsCrtRow;
static FsCrtRow rows[128];
static size_t count, targetSize;
static enum FsCrtTarget targetKind;
static int enabled, overflow;

void fs_crt_begin(enum FsCrtTarget target, size_t size) {
    /* Call only once per process: preserve failure rows during successful retry. */
    targetKind = target; targetSize = size; enabled = 1;
}
void fs_crt_end(void) { enabled = 0; }
size_t fs_crt_seen(void) { return count; }
size_t fs_crt_live(void) {
    size_t live = 0;
    for (size_t i = 0; i < count; ++i) if (rows[i].frees == 0) ++live;
    return live;
}
int fs_crt_valid(void) {
    if (overflow) return 0;
    for (size_t i = 0; i < count; ++i) if (rows[i].frees != 1) return 0;
    return 1;
}
static int selected(size_t size, const char *source, int isRealloc) {
    return enabled && size == targetSize &&
        (targetKind == FS_CRT_LIST
            ? isRealloc && strstr(source, "file.c") != NULL
            : !isRealloc && strstr(source, "fs_stream.c") != NULL);
}
static void record(void *pointer) {
    if (pointer == NULL) return;
    if (count == sizeof(rows) / sizeof(rows[0])) { overflow = 1; return; }
    rows[count++] = (FsCrtRow){pointer, 0};
}
void *fs_crt_malloc(size_t size, const char *source) {
    void *pointer = malloc(size);
    if (selected(size, source, 0)) record(pointer);
    return pointer;
}
void *fs_crt_realloc(void *pointer, size_t size, const char *source) {
    size_t active = count;
    for (size_t i = count; i > 0; --i)
        if (rows[i - 1].pointer == pointer && rows[i - 1].frees == 0) { active = i - 1; break; }
    void *next = realloc(pointer, size);
    if (next != NULL) {
        if (active != count) rows[active].pointer = next;
        else if (selected(size, source, 1)) record(next);
    }
    return next;
}
void fs_crt_free(void *pointer) {
    if (pointer != NULL)
        for (size_t i = count; i > 0; --i)
            if (rows[i - 1].pointer == pointer && rows[i - 1].frees == 0) {
                ++rows[i - 1].frees; break;
            }
    free(pointer);
}
