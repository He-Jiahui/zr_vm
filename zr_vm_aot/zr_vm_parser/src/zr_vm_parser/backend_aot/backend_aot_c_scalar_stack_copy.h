#ifndef ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_STACK_COPY_H
#define ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_STACK_COPY_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

/** @brief 以位掩码标记栈复制源在当前位置可能具有的运行时标量种类。 */
typedef enum EZrAotRuntimeScalarKindMask {
    ZR_AOT_RUNTIME_SCALAR_KIND_NONE = 0,
    ZR_AOT_RUNTIME_SCALAR_KIND_BOOL = 1u << 0,
    ZR_AOT_RUNTIME_SCALAR_KIND_I64 = 1u << 1,
    ZR_AOT_RUNTIME_SCALAR_KIND_U64 = 1u << 2,
    ZR_AOT_RUNTIME_SCALAR_KIND_F64 = 1u << 3
} EZrAotRuntimeScalarKindMask;

/** @brief 尝试生成标量栈复制；forceValueSlotWrite 禁止仅写 C 局部变量。 */
TZrBool backend_aot_try_write_c_scalar_stack_copy(FILE *file,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 sourceSlot,
                                                  TZrUInt32 execInstructionIndex,
                                                  TZrBool forceValueSlotWrite);
/** @brief 判定目的槽后续用途是否允许省去 SZrTypeValue 的物化。 */
TZrBool backend_aot_c_scalar_stack_copy_can_use_local_only(const SZrAotExecIrFunction *functionIr,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 sourceSlot,
                                                           TZrUInt32 execInstructionIndex);
/** @brief 仅在源槽的标量局部值已定义且类型匹配时直接读取该局部值。 */
TZrBool backend_aot_c_scalar_stack_copy_source_can_use_local(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 execInstructionIndex,
        EZrStaticCType staticCType);
/** @brief 向前追踪当前块内源槽的原始标量写入，所有权写入会切断该证明。 */
TZrBool backend_aot_c_scalar_stack_copy_has_scalar_provenance_before(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 slot,
        TZrUInt32 execInstructionIndex);
/** @brief 汇总源槽到达此指令的运行时标量种类，供复制路径选择。 */
TZrUInt32 backend_aot_c_scalar_stack_copy_source_reaching_runtime_scalar_kind_mask_before(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 execInstructionIndex);
/** @brief 检查目的槽是否紧接着成为所有权操作的输入，避免跳过其值槽写入。 */
TZrBool backend_aot_c_scalar_stack_copy_destination_is_next_ownership_source(
        const SZrFunction *function,
        TZrUInt32 instructionIndex,
        TZrUInt32 destinationSlot);

#endif
