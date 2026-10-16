#ifndef ZR_TEST_RETURNED_EXEC_FRAME_SHIM_H
#define ZR_TEST_RETURNED_EXEC_FRAME_SHIM_H
#include <stdlib.h>
#include "tests/core/support/ssa_exec_ir_owned_module_allocator.h"
#include "exec_ir/exec_ir_frame_layout.h"
#include "exec_ir/source/exec_ir_source_frame.h"

/* Only the two real Source-frame objects receive this forced include. All heap
 * operations still belong to the common ExecOwner ledger, including Core frees.
 * The separate site observer is active only around the genuine producer Attach. */
void *ZrTests_ReturnedExec_FrameMalloc(size_t bytes);
void *ZrTests_ReturnedExec_FrameCalloc(size_t count, size_t width);
void ZrTests_ReturnedExec_FrameFree(void *pointer);
void ZrTests_ReturnedExec_PackedFree(SZrExecIrPackedFrameLayout *packed);
#ifndef ZR_TEST_RETURNED_EXEC_FRAME_NO_REDIRECT
#define malloc ZrTests_ReturnedExec_FrameMalloc
#define calloc ZrTests_ReturnedExec_FrameCalloc
#define realloc ZrTests_ExecOwner_Realloc
#define free ZrTests_ReturnedExec_FrameFree
#define ZrParser_ExecIr_PackedFrameLayoutFree ZrTests_ReturnedExec_PackedFree
#endif
#endif
