#ifndef ZR_TEST_SSA_SOURCE_RETURNED_DESCRIPTOR_SHIM_H
#define ZR_TEST_SSA_SOURCE_RETURNED_DESCRIPTOR_SHIM_H
#include <stddef.h>
#include <stdlib.h>
#include "exec_ir/aot_ir_projection_descriptor.h"

void *ZrTests_ReturnedDescriptor_Calloc(size_t count, size_t width);
void ZrTests_ReturnedDescriptor_Free(void *pointer);

/* Forced into only the real descriptor translation unit of this test. */
#define calloc ZrTests_ReturnedDescriptor_Calloc
#define free ZrTests_ReturnedDescriptor_Free
#endif
