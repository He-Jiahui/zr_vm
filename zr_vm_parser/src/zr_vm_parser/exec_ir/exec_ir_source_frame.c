#include "exec_ir_source_frame_internal.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/canonical_type.h"

#include <stdlib.h>
#include <string.h>

TZrBool ZrParser_ExecIr_AttachPrimitiveSourceFrame(SZrExecIrFunction *function, const SZrSemanticContext *context,
                                                   const SZrExecIrLayout *layouts, TZrUInt32 layoutCount,
                                                   TZrUInt32 frameByteLimit, SZrExecIrDiagnostic *diagnostic) {
    SZrSourceFrameStorage storage = {0};
    SZrExecIrPackedFrameLayout packed;
    SZrExecIrPackedFrameRequest request = {0};
    SZrExecIrPackedValue *values = ZR_NULL;
    SZrExecIrFrameLayout *header = ZR_NULL;
    SZrExecIrFunction view;
    TZrBool success = ZR_FALSE;
    if (function == ZR_NULL || context == ZR_NULL || (layoutCount != 0u && layouts == ZR_NULL))
        return source_frame_fail(function, diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, ZR_NULL);
    if (!source_frame_storage(function, context, layouts, layoutCount, diagnostic, &storage))
        return source_frame_fail(function, diagnostic, storage.error, ZR_NULL);
    /* Unconditional guards also cover SIZE_MAX == UINT32_MAX. */
    if (!source_frame_count_fits(function->valueCount, sizeof(SZrExecIrPackedValue)) ||
        !source_frame_count_fits(function->valueCount, sizeof(TZrUInt32)) ||
        !source_frame_count_fits(function->valueCount, sizeof(TZrExecIrValueId)) ||
        !source_frame_count_fits(function->valueCount, sizeof(SZrExecIrFrameSlot)) ||
        !source_frame_count_fits(function->valueCount, sizeof(EZrExecIrPackedSlotClass)))
        return source_frame_fail(function, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, ZR_NULL);
    if (diagnostic != ZR_NULL)
        memset(diagnostic, 0, sizeof(*diagnostic));
    if (!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, diagnostic))
        return ZR_FALSE;
    if (function->sealed)
        return source_frame_fail(function, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_SEALED, ZR_NULL);
    if (function->frameLayout != ZR_NULL || function->contract.layoutHash != 0u ||
        !source_frame_body_metadata(function) || !source_frame_rows(layouts, layoutCount))
        return source_frame_fail(function, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, ZR_NULL);
    if (!source_frame_admit_values(function, context, layouts, layoutCount, diagnostic))
        return ZR_FALSE;
    if (!source_frame_prepare_request(function, layouts, layoutCount, frameByteLimit, &values, &request, diagnostic))
        return ZR_FALSE;
    ZrParser_ExecIr_PackedFrameLayoutInit(&packed);
    if (!ZrParser_ExecIr_LayoutPackedFrame(&request, &packed, diagnostic))
        goto cleanup;
    if (!source_frame_geometry(function, values, &packed, frameByteLimit)) {
        source_frame_fail(function, diagnostic, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, ZR_NULL);
        goto cleanup;
    }
    header = (SZrExecIrFrameLayout *) malloc(sizeof(*header));
    if (header == ZR_NULL) {
        source_frame_fail(function, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, ZR_NULL);
        goto cleanup;
    }
    *header = packed.frame;
    /* Every other owner stays borrowed. Never FreeFunction on this view. */
    view = *function;
    view.frameLayout = header;
    view.contract.layoutHash = header->layoutHash;
    if (!ZrCore_ExecIr_VerifyFunction(&view, ZR_EXEC_IR_VERIFY_ALL, diagnostic))
        goto cleanup;
    function->frameLayout = header;
    function->contract.layoutHash = header->layoutHash;
    header = ZR_NULL;
    memset(&packed.frame, 0, sizeof(packed.frame));
    success = ZR_TRUE;
cleanup:
    /* Until publication packed owns slots; header is a shallow copy only. */
    free(header);
    ZrParser_ExecIr_PackedFrameLayoutFree(&packed);
    free(values);
    return success;
}
