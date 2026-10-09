#ifndef ZR_TEST_SSA_SOURCE_DESCRIPTOR_OOM_SHIM_H
#define ZR_TEST_SSA_SOURCE_DESCRIPTOR_OOM_SHIM_H
#include <stddef.h>
#include <stdlib.h>
#include "zr_vm_parser/aot_ir_projection_descriptor.h"

void *ZrTests_SourceDescriptor_Calloc(size_t count, size_t width);
void ZrTests_SourceDescriptor_Free(void *pointer);

/* Forced into only the actual descriptor OBJECT by the registration owner. */
#ifndef ZR_TEST_SSA_SOURCE_DESCRIPTOR_OOM_NO_REDIRECT
#define calloc ZrTests_SourceDescriptor_Calloc
#define free ZrTests_SourceDescriptor_Free
#endif
#endif
