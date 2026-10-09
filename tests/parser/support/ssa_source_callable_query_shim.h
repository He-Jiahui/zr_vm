#ifndef ZR_TEST_SOURCE_CALLABLE_QUERY_SHIM_H
#define ZR_TEST_SOURCE_CALLABLE_QUERY_SHIM_H
#include <stdlib.h>
void *ZrTests_SourceCallable_Malloc(size_t bytes);
void *ZrTests_SourceCallable_Calloc(size_t count, size_t width);
void *ZrTests_SourceCallable_Realloc(void *pointer, size_t bytes);
void ZrTests_SourceCallable_Free(void *pointer);
#ifndef ZR_TEST_SOURCE_CALLABLE_NO_REDIRECT
#define malloc ZrTests_SourceCallable_Malloc
#define calloc ZrTests_SourceCallable_Calloc
#define realloc ZrTests_SourceCallable_Realloc
#define free ZrTests_SourceCallable_Free
#endif
#endif
