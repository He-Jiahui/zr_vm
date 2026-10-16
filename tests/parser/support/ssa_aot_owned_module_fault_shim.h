#ifndef ZR_TEST_SSA_AOT_OWNED_MODULE_FAULT_SHIM_H
#define ZR_TEST_SSA_AOT_OWNED_MODULE_FAULT_SHIM_H

/* Forced include for the independent owner OBJECT only. Read real declarations
 * before redirecting this translation unit's direct allocator calls. */
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include "aot/aot_ir_owned_module.h"

void *ZrTests_AotOwnedModule_Malloc(size_t bytes);
void ZrTests_AotOwnedModule_Free(void *pointer);
TZrBool ZrTests_AotOwnedModule_CheckedSpan(
        size_t *cursor, size_t count, size_t width, size_t alignment,
        size_t *offset);
size_t ZrTests_AotOwnedModule_AlignmentBound(void);

/* The test TU defines this switch so its allocator wrappers call the CRT. */
#ifndef ZR_TEST_SSA_AOT_OWNED_MODULE_NO_REDIRECT
#define malloc ZrTests_AotOwnedModule_Malloc
#define free ZrTests_AotOwnedModule_Free
#endif

#endif /* ZR_TEST_SSA_AOT_OWNED_MODULE_FAULT_SHIM_H */
