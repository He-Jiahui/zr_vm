#ifndef ZR_TEST_SSA_SOURCE_RETURNED_OWNER_SHIM_H
#define ZR_TEST_SSA_SOURCE_RETURNED_OWNER_SHIM_H
#include <stddef.h>
#include <stdlib.h>
#include "aot/aot_ir_owned_module.h"

void *ZrTests_ReturnedOwner_Malloc(size_t bytes);
void ZrTests_ReturnedOwner_Free(void *pointer);

/* Forced into only the real Core owned-arena translation unit of this test. */
#define malloc ZrTests_ReturnedOwner_Malloc
#define free ZrTests_ReturnedOwner_Free
#endif
