#include "backend_aot_c_typed_return.h"

#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"

/* 导出发布必须发生在直接 return 前，由调用方决定该返回点是否需要发布。 */
static void backend_aot_write_c_typed_return_prefix(FILE *file, TZrBool publishExports) {
    if (publishExports) {
        backend_aot_write_c_publish_exports(file);
    }
}

/* 只有槽的标量类型在当前指令点可证明时才绕开通用值槽返回。 */
TZrBool backend_aot_try_write_c_typed_return(FILE *file,
                                             const SZrAotExecIrFunction *functionIr,
                                             TZrUInt32 sourceSlot,
                                             TZrUInt32 execInstructionIndex,
                                             TZrBool publishExports) {
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_c_scalar_locals_can_direct_return_i64_local(
                functionIr, sourceSlot, execInstructionIndex)) {
        backend_aot_write_c_typed_return_prefix(file, publishExports);
        backend_aot_write_c_direct_return_i64_local(file, sourceSlot);
        return ZR_TRUE;
    }

    if (backend_aot_c_scalar_locals_can_direct_return_bool_local(
                functionIr, sourceSlot, execInstructionIndex) ||
        backend_aot_c_scalar_locals_can_infer_return_bool_local(
                functionIr, sourceSlot, execInstructionIndex)) {
        backend_aot_write_c_typed_return_prefix(file, publishExports);
        backend_aot_write_c_direct_return_bool_local(file, sourceSlot);
        return ZR_TRUE;
    }

    if (backend_aot_c_scalar_locals_can_direct_return_u64_local(
                functionIr, sourceSlot, execInstructionIndex) ||
        backend_aot_c_scalar_locals_can_infer_return_u64_local(
                functionIr, sourceSlot, execInstructionIndex)) {
        backend_aot_write_c_typed_return_prefix(file, publishExports);
        backend_aot_write_c_direct_return_u64_local(file, sourceSlot);
        return ZR_TRUE;
    }

    if (backend_aot_c_scalar_locals_can_direct_return_f64_local(
                functionIr, sourceSlot, execInstructionIndex) ||
        backend_aot_c_scalar_locals_can_infer_return_f64_local(
                functionIr, sourceSlot, execInstructionIndex)) {
        backend_aot_write_c_typed_return_prefix(file, publishExports);
        backend_aot_write_c_direct_return_f64_local(file, sourceSlot);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
