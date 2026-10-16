#ifndef ZR_TEST_SSA_SOURCE_INTEGER_EXPRESSION_FAULTS_FRAME_SHIM_H
#define ZR_TEST_SSA_SOURCE_INTEGER_EXPRESSION_FAULTS_FRAME_SHIM_H

#include <stdlib.h>
#include "tests/core/support/ssa_exec_ir_owned_module_allocator.h"
#include "exec_ir/exec_ir_frame_layout.h"

void *ZrTests_ReturnedExec_FrameMalloc(size_t bytes);
void *ZrTests_ReturnedExec_FrameCalloc(size_t count, size_t width);
void ZrTests_ReturnedExec_FrameFree(void *pointer);
void ZrTests_ReturnedExec_PackedFree(SZrExecIrPackedFrameLayout *packed);

/* Only the two real Source Frame TUs use this shim. It supports their actual
 * value counts, including validation inside finalization; there is no literal
 * one-value geometry or local fault schedule. TryArm controls the common ledger. */
#ifndef ZR_TEST_INTEGER_EXPR_NO_REDIRECT
#ifdef malloc
#undef malloc
#endif
#ifdef calloc
#undef calloc
#endif
#ifdef realloc
#undef realloc
#endif
#ifdef free
#undef free
#endif
#define malloc ZrTests_ReturnedExec_FrameMalloc
#define calloc ZrTests_ReturnedExec_FrameCalloc
#define realloc ZrTests_ExecOwner_Realloc
#define free ZrTests_ReturnedExec_FrameFree
#define ZrParser_ExecIr_PackedFrameLayoutFree ZrTests_ReturnedExec_PackedFree
#endif
#endif
