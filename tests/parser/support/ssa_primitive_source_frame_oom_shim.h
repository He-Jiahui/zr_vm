#ifndef ZR_TEST_SSA_PRIMITIVE_SOURCE_FRAME_OOM_SHIM_H
#define ZR_TEST_SSA_PRIMITIVE_SOURCE_FRAME_OOM_SHIM_H

/* Forced include for the independent source-frame OBJECT target only. Read
 * the real CRT and packed-helper declarations before redirecting call sites. */
#include <stdlib.h>
#include "exec_ir/exec_ir_frame_layout.h"

void *ZrTests_PrimitiveSourceFrame_Calloc(size_t count, size_t width);
void *ZrTests_PrimitiveSourceFrame_Malloc(size_t size);
void ZrTests_PrimitiveSourceFrame_Free(void *pointer);
void ZrTests_PrimitiveSourceFrame_PackedFree(SZrExecIrPackedFrameLayout *packed);

/* The test TU defines this switch so its wrappers call the original functions.
 * No other TU or normal parser target should receive this forced include. */
#ifndef ZR_TEST_SSA_PRIMITIVE_SOURCE_FRAME_OOM_NO_REDIRECT
#define calloc ZrTests_PrimitiveSourceFrame_Calloc
#define malloc ZrTests_PrimitiveSourceFrame_Malloc
#define free ZrTests_PrimitiveSourceFrame_Free
#define ZrParser_ExecIr_PackedFrameLayoutFree ZrTests_PrimitiveSourceFrame_PackedFree
#endif

#endif
