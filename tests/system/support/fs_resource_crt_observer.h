#ifndef ZR_TEST_FS_RESOURCE_CRT_OBSERVER_H
#define ZR_TEST_FS_RESOURCE_CRT_OBSERVER_H
#include <stddef.h>
#include <stdlib.h>

/* Forced into only the two actual production translation units under test.
 * The observer implementation and fixture do not define ZR_FS_CRT_REDIRECT. */
enum FsCrtTarget { FS_CRT_TEXT, FS_CRT_READ_BYTES, FS_CRT_WRITE_BYTES, FS_CRT_LIST };
void fs_crt_begin(enum FsCrtTarget target, size_t size);
void fs_crt_end(void);
size_t fs_crt_live(void);
size_t fs_crt_seen(void);
int fs_crt_valid(void);
void *fs_crt_malloc(size_t size, const char *source);
void *fs_crt_realloc(void *pointer, size_t size, const char *source);
void fs_crt_free(void *pointer);

#ifdef ZR_FS_CRT_REDIRECT
#define malloc(size) fs_crt_malloc((size), __FILE__)
#define realloc(pointer, size) fs_crt_realloc((pointer), (size), __FILE__)
#define free(pointer) fs_crt_free((pointer))
#endif
#endif
