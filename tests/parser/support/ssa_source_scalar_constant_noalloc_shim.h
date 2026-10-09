#ifndef ZR_TEST_SSA_SOURCE_SCALAR_CONSTANT_NOALLOC_SHIM_H
#define ZR_TEST_SSA_SOURCE_SCALAR_CONSTANT_NOALLOC_SHIM_H

/* Forced into the actual private bridge OBJECT only; CRT declarations must
 * precede redirection. Ordinary Parser objects are not instrumented here. */
#include <stddef.h>
#include <stdlib.h>
void *ZrTests_SourceScalarConstant_Malloc(size_t bytes);
void *ZrTests_SourceScalarConstant_Calloc(size_t count, size_t bytes);
void *ZrTests_SourceScalarConstant_Realloc(void *pointer, size_t bytes);
void ZrTests_SourceScalarConstant_Free(void *pointer);

#ifndef ZR_TEST_SSA_SOURCE_SCALAR_CONSTANT_NO_REDIRECT
#define malloc ZrTests_SourceScalarConstant_Malloc
#define calloc ZrTests_SourceScalarConstant_Calloc
#define realloc ZrTests_SourceScalarConstant_Realloc
#define free ZrTests_SourceScalarConstant_Free
#endif
#endif
