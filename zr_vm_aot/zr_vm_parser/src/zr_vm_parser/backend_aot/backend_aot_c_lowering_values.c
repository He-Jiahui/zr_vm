#include "backend_aot_c_emitter.h"

#include "backend_aot_c_scalar_locals.h"
#include "backend_aot_c_scalar_stack_copy.h"

#include "zr_vm_core/closure.h"
/* 所有权指令族统一转交 runtime；生成器只传帧槽，生命周期转换不得在 C 字符串中复制实现。 */
static void backend_aot_write_c_direct_ownership_helper_call(FILE *file,
                                                             const char *helperName,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 sourceSlot) {
    if (file == ZR_NULL || helperName == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_exec_ownership_helper */\n"
            "        ZR_AOT_C_GUARD(%s(state, &frame, %u, %u));\n"
            "    }\n",
            helperName,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

/* UNIQUE/BORROW/LOAN 等包装入口由字节码分派器选择，源/目标槽遵守 runtime 的所有权契约。 */
void backend_aot_write_c_direct_own_unique(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_helper_call(
            file,
            "ZrLibrary_AotRuntime_OwnUnique",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_borrow(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_helper_call(
            file,
            "ZrLibrary_AotRuntime_OwnBorrow",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_loan(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_helper_call(
            file,
            "ZrLibrary_AotRuntime_OwnLoan",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_return_loan(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_helper_call(
            file,
            "ZrLibrary_AotRuntime_OwnReturnLoan",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_share(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_helper_call(
            file,
            "ZrLibrary_AotRuntime_OwnShare",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_degrade(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_helper_call(
            file,
            "ZrLibrary_AotRuntime_OwnDegrade",
            destinationSlot,
            sourceSlot);
}

/* DETACH 的实际资源转移委托 runtime；这里不能假设源槽复制后仍保有资源。 */
void backend_aot_write_c_direct_own_detach(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_helper_call(
            file,
            "ZrLibrary_AotRuntime_OwnDetach",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_into_gc_box(FILE *file,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_helper_call(
            file,
            "ZrLibrary_AotRuntime_OwnIntoGcBox",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_return_to_gc(FILE *file,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_helper_call(
            file,
            "ZrLibrary_AotRuntime_OwnReturnToGc",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_wake(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_helper_call(
            file,
            "ZrLibrary_AotRuntime_OwnWake",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_drop(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_helper_call(
            file,
            "ZrLibrary_AotRuntime_OwnDrop",
            destinationSlot,
            sourceSlot);
}

/* runtime 转换后立即维护引用局部量镜像，供随后不读取通用值槽的路径使用。 */
void backend_aot_write_c_direct_to_string(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_to_string */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToString(state, &frame, %u, %u));\n"
            "        const SZrTypeValue *zr_aot_to_string_value = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_to_string_value == ZR_NULL || !ZR_VALUE_IS_TYPE_STRING(zr_aot_to_string_value->type) ||\n"
            "            zr_aot_to_string_value->value.object == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"unsupported AOT to_string reference local\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_ref_locals.o%u = zr_aot_to_string_value->value.object;\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)destinationSlot);
    backend_aot_write_c_gc_safepoint(file, "        ", "zr_aot_gc_safepoint_reference_local");
    fprintf(file, "    } while (0);\n");
}

/* 常量选择器的公共边界：只返回当前函数有效索引的值，供直写与 thunk 签名分析共用。 */
const SZrTypeValue *backend_aot_c_get_constant_value(const SZrFunction *function, TZrInt32 constantIndex) {
    if (function == ZR_NULL || constantIndex < 0 || (TZrUInt32)constantIndex >= function->constantValueLength ||
        function->constantValueList == ZR_NULL) {
        return ZR_NULL;
    }

    return &function->constantValueList[(TZrUInt32)constantIndex];
}

/* 闭包元数据常量需要生成真实 callable；其余值才可考虑直接字面量路径。 */
TZrBool backend_aot_c_constant_requires_materialization(SZrState *state,
                                                        const SZrFunction *function,
                                                        TZrInt32 constantIndex) {
    const SZrTypeValue *constantValue;

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (state == ZR_NULL || constantValue == ZR_NULL) {
        return ZR_TRUE;
    }

    return ZrCore_Closure_GetMetadataFunctionFromValue(state, constantValue) != ZR_NULL;
}

/* 保留原字节码常量的窄整数/浮点类型标记，避免即时值路径一律扩大类型。 */
static const TZrChar *backend_aot_c_value_type_literal(EZrValueType type) {
    switch (type) {
        case ZR_VALUE_TYPE_BOOL:
            return "ZR_VALUE_TYPE_BOOL";
        case ZR_VALUE_TYPE_INT8:
            return "ZR_VALUE_TYPE_INT8";
        case ZR_VALUE_TYPE_INT16:
            return "ZR_VALUE_TYPE_INT16";
        case ZR_VALUE_TYPE_INT32:
            return "ZR_VALUE_TYPE_INT32";
        case ZR_VALUE_TYPE_INT64:
            return "ZR_VALUE_TYPE_INT64";
        case ZR_VALUE_TYPE_UINT8:
            return "ZR_VALUE_TYPE_UINT8";
        case ZR_VALUE_TYPE_UINT16:
            return "ZR_VALUE_TYPE_UINT16";
        case ZR_VALUE_TYPE_UINT32:
            return "ZR_VALUE_TYPE_UINT32";
        case ZR_VALUE_TYPE_UINT64:
            return "ZR_VALUE_TYPE_UINT64";
        case ZR_VALUE_TYPE_FLOAT:
            return "ZR_VALUE_TYPE_FLOAT";
        case ZR_VALUE_TYPE_DOUBLE:
            return "ZR_VALUE_TYPE_DOUBLE";
        default:
            return "ZR_VALUE_TYPE_UNKNOWN";
    }
}

/* 只有无引用所有权的原始标量可写入生成 C；其他常量仍依赖值槽复制。 */
TZrBool backend_aot_c_constant_can_emit_immediate(const SZrFunction *function, TZrInt32 constantIndex) {
    const SZrTypeValue *constantValue = backend_aot_c_get_constant_value(function, constantIndex);

    if (constantValue == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZR_VALUE_IS_TYPE_NULL(constantValue->type) || ZR_VALUE_IS_TYPE_BOOL(constantValue->type) ||
           ZR_VALUE_IS_TYPE_INT(constantValue->type) || ZR_VALUE_IS_TYPE_FLOAT(constantValue->type);
}

/* 被导出的槽是模块外可观察状态，不可因局部消费者已知而省略真实槽写入。 */
static TZrBool backend_aot_c_function_exports_stack_slot(const SZrFunction *function, TZrUInt32 stackSlot) {
    TZrUInt32 exportIndex;

    if (function == ZR_NULL || function->exportedVariables == ZR_NULL) {
        return ZR_FALSE;
    }

    for (exportIndex = 0u; exportIndex < function->exportedVariableLength; exportIndex++) {
        if (function->exportedVariables[exportIndex].stackSlot == stackSlot) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* TODO: 此处只匹配紧邻跳转等局部条件；跳过 reset 前仍需核查后继槽活性及旧值所有权。 */
TZrBool backend_aot_c_reset_null_consumed_by_local_jump_if(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 resetInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *resetInstruction;
    const TZrInstruction *jumpInstruction;
    TZrUInt32 jumpInstructionIndex;
    TZrInt64 targetInstructionIndex;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    jumpInstructionIndex = resetInstructionIndex + 1u;
    if (function->exceptionHandlerCount > 0 ||
        backend_aot_c_function_exports_stack_slot(function, sourceSlot) ||
        function->instructionsList == ZR_NULL ||
        resetInstructionIndex >= function->instructionsLength ||
        jumpInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    resetInstruction = &function->instructionsList[resetInstructionIndex];
    if (resetInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(RESET_STACK_NULL) ||
        resetInstruction->instruction.operandExtra != sourceSlot) {
        return ZR_FALSE;
    }

    jumpInstruction = &function->instructionsList[jumpInstructionIndex];
    if (jumpInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF) ||
        jumpInstruction->instruction.operandExtra != sourceSlot) {
        return ZR_FALSE;
    }

    targetInstructionIndex = (TZrInt64)jumpInstructionIndex +
                             (TZrInt64)jumpInstruction->instruction.operand.operand2[0] +
                             1;
    return (TZrBool)(targetInstructionIndex >= 0 &&
                     targetInstructionIndex < (TZrInt64)function->instructionsLength);
}

/* TODO: reset->LOGICAL_NOT 只证实相邻及结果镜像；仍需核查 CFG 后继的源槽活性与旧值所有权。 */
TZrBool backend_aot_c_reset_null_consumed_by_local_logical_not(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 resetInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *resetInstruction;
    const TZrInstruction *logicalNotInstruction;
    TZrUInt32 logicalNotInstructionIndex;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    logicalNotInstructionIndex = resetInstructionIndex + 1u;
    if (function->exceptionHandlerCount > 0 ||
        backend_aot_c_function_exports_stack_slot(function, sourceSlot) ||
        function->instructionsList == ZR_NULL ||
        resetInstructionIndex >= function->instructionsLength ||
        logicalNotInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    resetInstruction = &function->instructionsList[resetInstructionIndex];
    if (resetInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(RESET_STACK_NULL) ||
        resetInstruction->instruction.operandExtra != sourceSlot) {
        return ZR_FALSE;
    }

    logicalNotInstruction = &function->instructionsList[logicalNotInstructionIndex];
    if (logicalNotInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(LOGICAL_NOT) ||
        logicalNotInstruction->instruction.operand.operand1[0] != sourceSlot) {
        return ZR_FALSE;
    }

    return backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
            functionIr,
            logicalNotInstruction->instruction.operandExtra,
            logicalNotInstructionIndex);
}

/* 只把已知 SET_STACK/GET_STACK 视为纯槽复制，供线性消费分析使用。 */
static TZrBool backend_aot_c_reset_null_stack_copy_instruction_is_copy(const TZrInstruction *instruction) {
    EZrInstructionCode operationCode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    operationCode = (EZrInstructionCode)instruction->instruction.operationCode;
    return (TZrBool)(operationCode == ZR_INSTRUCTION_ENUM(SET_STACK) ||
                     operationCode == ZR_INSTRUCTION_ENUM(GET_STACK));
}

/* 省略 reset->copy 前验证紧邻关系、异常边界及两端槽都未导出。 */
static TZrBool backend_aot_c_reset_null_stack_copy_candidate(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 copiedSlot,
        TZrUInt32 resetInstructionIndex,
        TZrUInt32 stackCopyInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *resetInstruction;
    const TZrInstruction *stackCopyInstruction;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (function->exceptionHandlerCount > 0 ||
        function->instructionsList == ZR_NULL ||
        resetInstructionIndex >= function->instructionsLength ||
        stackCopyInstructionIndex >= function->instructionsLength ||
        resetInstructionIndex + 1u != stackCopyInstructionIndex ||
        backend_aot_c_function_exports_stack_slot(function, sourceSlot) ||
        backend_aot_c_function_exports_stack_slot(function, copiedSlot)) {
        return ZR_FALSE;
    }

    resetInstruction = &function->instructionsList[resetInstructionIndex];
    if (resetInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(RESET_STACK_NULL) ||
        resetInstruction->instruction.operandExtra != sourceSlot) {
        return ZR_FALSE;
    }

    stackCopyInstruction = &function->instructionsList[stackCopyInstructionIndex];
    if (!backend_aot_c_reset_null_stack_copy_instruction_is_copy(stackCopyInstruction) ||
        stackCopyInstruction->instruction.operandExtra != copiedSlot ||
        stackCopyInstruction->instruction.operand.operand2[0] < 0 ||
        (TZrUInt32)stackCopyInstruction->instruction.operand.operand2[0] != sourceSlot) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* TODO: reset->copy->not 只核对相邻模式；需查 CFG 后继槽活性与被略过的旧值清理。 */
TZrBool backend_aot_c_reset_null_consumed_by_local_stack_copy_logical_not(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 resetInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *stackCopyInstruction;
    const TZrInstruction *logicalNotInstruction;
    TZrUInt32 stackCopyInstructionIndex;
    TZrUInt32 logicalNotInstructionIndex;
    TZrUInt32 copiedSlot;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    stackCopyInstructionIndex = resetInstructionIndex + 1u;
    logicalNotInstructionIndex = stackCopyInstructionIndex + 1u;
    if (function->instructionsList == ZR_NULL ||
        stackCopyInstructionIndex >= function->instructionsLength ||
        logicalNotInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    stackCopyInstruction = &function->instructionsList[stackCopyInstructionIndex];
    if (!backend_aot_c_reset_null_stack_copy_instruction_is_copy(stackCopyInstruction)) {
        return ZR_FALSE;
    }

    copiedSlot = stackCopyInstruction->instruction.operandExtra;
    if (!backend_aot_c_reset_null_stack_copy_candidate(
                functionIr,
                sourceSlot,
                copiedSlot,
                resetInstructionIndex,
                stackCopyInstructionIndex)) {
        return ZR_FALSE;
    }

    logicalNotInstruction = &function->instructionsList[logicalNotInstructionIndex];
    if (logicalNotInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(LOGICAL_NOT) ||
        logicalNotInstruction->instruction.operand.operand1[0] != copiedSlot) {
        return ZR_FALSE;
    }

    return backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
            functionIr,
            logicalNotInstruction->instruction.operandExtra,
            logicalNotInstructionIndex);
}

/* TODO: reset->copy->jump 只证实局部模式及跳转范围；需查 CFG 后继槽活性与旧值所有权。 */
TZrBool backend_aot_c_reset_null_consumed_by_local_stack_copy_jump_if(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 resetInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *stackCopyInstruction;
    const TZrInstruction *jumpInstruction;
    TZrUInt32 stackCopyInstructionIndex;
    TZrUInt32 jumpInstructionIndex;
    TZrUInt32 copiedSlot;
    TZrInt64 targetInstructionIndex;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    stackCopyInstructionIndex = resetInstructionIndex + 1u;
    jumpInstructionIndex = stackCopyInstructionIndex + 1u;
    if (function->instructionsList == ZR_NULL ||
        stackCopyInstructionIndex >= function->instructionsLength ||
        jumpInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    stackCopyInstruction = &function->instructionsList[stackCopyInstructionIndex];
    if (!backend_aot_c_reset_null_stack_copy_instruction_is_copy(stackCopyInstruction)) {
        return ZR_FALSE;
    }

    copiedSlot = stackCopyInstruction->instruction.operandExtra;
    if (!backend_aot_c_reset_null_stack_copy_candidate(
                functionIr,
                sourceSlot,
                copiedSlot,
                resetInstructionIndex,
                stackCopyInstructionIndex)) {
        return ZR_FALSE;
    }

    jumpInstruction = &function->instructionsList[jumpInstructionIndex];
    if (jumpInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF) ||
        jumpInstruction->instruction.operandExtra != copiedSlot) {
        return ZR_FALSE;
    }

    targetInstructionIndex = (TZrInt64)jumpInstructionIndex +
                             (TZrInt64)jumpInstruction->instruction.operand.operand2[0] +
                             1;
    return (TZrBool)(targetInstructionIndex >= 0 &&
                     targetInstructionIndex < (TZrInt64)function->instructionsLength);
}

/* TODO: 复制入口复用 reset->copy->not 局部判断；需查其他 CFG 后继槽活性与旧值所有权。 */
TZrBool backend_aot_c_reset_null_stack_copy_consumed_by_local_logical_not(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 copiedSlot,
        TZrUInt32 stackCopyInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *stackCopyInstruction;

    if (functionIr == ZR_NULL ||
        functionIr->function == ZR_NULL ||
        stackCopyInstructionIndex == 0u) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (function->instructionsList == ZR_NULL ||
        stackCopyInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    stackCopyInstruction = &function->instructionsList[stackCopyInstructionIndex];
    if (!backend_aot_c_reset_null_stack_copy_instruction_is_copy(stackCopyInstruction) ||
        stackCopyInstruction->instruction.operandExtra != copiedSlot ||
        stackCopyInstruction->instruction.operand.operand2[0] < 0) {
        return ZR_FALSE;
    }

    return backend_aot_c_reset_null_consumed_by_local_stack_copy_logical_not(
            functionIr,
            (TZrUInt32)stackCopyInstruction->instruction.operand.operand2[0],
            stackCopyInstructionIndex - 1u);
}

/* TODO: 复制入口复用 reset->copy->jump 局部判断；需查其他 CFG 后继槽活性与旧值所有权。 */
TZrBool backend_aot_c_reset_null_stack_copy_consumed_by_local_jump_if(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 copiedSlot,
        TZrUInt32 stackCopyInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *stackCopyInstruction;

    if (functionIr == ZR_NULL ||
        functionIr->function == ZR_NULL ||
        stackCopyInstructionIndex == 0u) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (function->instructionsList == ZR_NULL ||
        stackCopyInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    stackCopyInstruction = &function->instructionsList[stackCopyInstructionIndex];
    if (!backend_aot_c_reset_null_stack_copy_instruction_is_copy(stackCopyInstruction) ||
        stackCopyInstruction->instruction.operandExtra != copiedSlot ||
        stackCopyInstruction->instruction.operand.operand2[0] < 0) {
        return ZR_FALSE;
    }

    return backend_aot_c_reset_null_consumed_by_local_stack_copy_jump_if(
            functionIr,
            (TZrUInt32)stackCopyInstruction->instruction.operand.operand2[0],
            stackCopyInstructionIndex - 1u);
}

/* 直写原始标量前先释放目标中可能存在的引用所有权，维持值槽替换契约。 */
static void backend_aot_c_write_direct_plain_value_replace_guard(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        if (zr_aot_destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||\n"
            "            zr_aot_destination->isGarbageCollectable) {\n"
            "            ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
            "        }\n");
}

/* NULL 也可能覆盖拥有资源的值槽，仍需走同一释放边界。 */
static void backend_aot_c_write_direct_null_value(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    backend_aot_c_write_direct_plain_value_replace_guard(file);
    fprintf(file, "        ZrCore_Value_ResetAsNull(zr_aot_destination);\n");
}

/* 标量即时值要同时重置 GC/所有权字段，免得复用旧引用元数据。 */
static void backend_aot_c_write_direct_plain_value_scalar_assign(FILE *file,
                                                                 const char *region,
                                                                 const char *dataExpression,
                                                                 const char *typeLiteral) {
    if (file == ZR_NULL || region == ZR_NULL || dataExpression == ZR_NULL || typeLiteral == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        zr_aot_destination->type = %s;\n"
            "        zr_aot_destination->value.nativeObject.%s = %s;\n"
            "        zr_aot_destination->isGarbageCollectable = ZR_FALSE;\n"
            "        zr_aot_destination->isNative = ZR_TRUE;\n"
            "        zr_aot_destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;\n"
            "        zr_aot_destination->ownershipControl = ZR_NULL;\n"
            "        zr_aot_destination->ownershipWeakRef = ZR_NULL;\n",
            typeLiteral,
            region,
            dataExpression);
}

/* 值槽被写入时保持对应标量镜像一致，后继专用路径可能只读镜像。 */
static void backend_aot_c_write_direct_primitive_constant_local_mirror(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        const SZrTypeValue *constantValue,
        const char *dataExpression) {
    if (file == ZR_NULL || constantValue == ZR_NULL || dataExpression == ZR_NULL) {
        return;
    }

    if (ZR_VALUE_IS_TYPE_BOOL(constantValue->type) &&
        backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot)) {
        fprintf(file, "        zr_aot_b%u = %s;\n", (unsigned)destinationSlot, dataExpression);
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type) &&
               backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot)) {
        fprintf(file, "        zr_aot_s%u = %s;\n", (unsigned)destinationSlot, dataExpression);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type) &&
               backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot)) {
        fprintf(file, "        zr_aot_u%u = %s;\n", (unsigned)destinationSlot, dataExpression);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(constantValue->type) &&
               backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot)) {
        fprintf(file, "        zr_aot_f%u = %s;\n", (unsigned)destinationSlot, dataExpression);
    }
}

/* 常量即时路径可在经证明的相邻消费者前略去值槽；否则同时写值槽和镜像。
 * TODO: 核查编译期常量能否包含 NaN/Inf；%.17g 对非有限值输出的 C 字面量未验证。
 */
void backend_aot_write_c_direct_primitive_constant(FILE *file,
                                                   const SZrAotExecIrFunction *functionIr,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 execInstructionIndex,
                                                   const SZrTypeValue *constantValue) {
    char dataExpression[128];
    TZrBool wroteScalarLocal = ZR_FALSE;

    if (file == ZR_NULL || constantValue == ZR_NULL) {
        return;
    }

    if (ZR_VALUE_IS_TYPE_NULL(constantValue->type) &&
        backend_aot_c_null_constant_consumed_by_local_logical_not(
                functionIr, destinationSlot, execInstructionIndex)) {
        fprintf(file,
                "    /* zr_aot_null_constant_local_logical_not_source_skip slot=%u */\n",
                (unsigned)destinationSlot);
        return;
    }
    if (ZR_VALUE_IS_TYPE_NULL(constantValue->type) &&
        backend_aot_c_null_constant_consumed_by_local_jump_if(
                functionIr, destinationSlot, execInstructionIndex)) {
        fprintf(file,
                "    /* zr_aot_null_constant_local_jump_if_source_skip slot=%u */\n",
                (unsigned)destinationSlot);
        return;
    }
    if (ZR_VALUE_IS_TYPE_NULL(constantValue->type) &&
        backend_aot_c_null_constant_consumed_by_local_stack_copy_logical_not(
                functionIr, destinationSlot, execInstructionIndex)) {
        fprintf(file,
                "    /* zr_aot_null_constant_stack_copy_local_logical_not_constant_skip slot=%u */\n",
                (unsigned)destinationSlot);
        return;
    }
    if (ZR_VALUE_IS_TYPE_NULL(constantValue->type) &&
        backend_aot_c_null_constant_consumed_by_local_stack_copy_jump_if(
                functionIr, destinationSlot, execInstructionIndex)) {
        fprintf(file,
                "    /* zr_aot_null_constant_stack_copy_local_jump_if_constant_skip slot=%u */\n",
                (unsigned)destinationSlot);
        return;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(constantValue->type) &&
        backend_aot_c_bool_constant_consumed_by_local_logical_not(
                functionIr, destinationSlot, execInstructionIndex, ZR_NULL)) {
        fprintf(file,
                "    /* zr_aot_bool_constant_local_logical_not_source_skip slot=%u */\n",
                (unsigned)destinationSlot);
        return;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(constantValue->type) &&
        backend_aot_c_bool_constant_consumed_by_local_jump_if(
                functionIr, destinationSlot, execInstructionIndex, ZR_NULL)) {
        fprintf(file,
                "    /* zr_aot_bool_constant_local_jump_if_source_skip slot=%u */\n",
                (unsigned)destinationSlot);
        return;
    }
    if (ZR_VALUE_IS_TYPE_STRING(constantValue->type) &&
        backend_aot_c_string_constant_consumed_by_local_logical_not(
                functionIr, destinationSlot, execInstructionIndex, ZR_NULL)) {
        fprintf(file,
                "    /* zr_aot_string_constant_local_logical_not_source_skip slot=%u */\n",
                (unsigned)destinationSlot);
        return;
    }
    if (ZR_VALUE_IS_TYPE_STRING(constantValue->type) &&
        backend_aot_c_string_constant_consumed_by_local_jump_if(
                functionIr, destinationSlot, execInstructionIndex, ZR_NULL)) {
        fprintf(file,
                "    /* zr_aot_string_constant_local_jump_if_source_skip slot=%u */\n",
                (unsigned)destinationSlot);
        return;
    }
    if (ZR_VALUE_IS_TYPE_STRING(constantValue->type) &&
        backend_aot_c_string_constant_consumed_by_local_stack_copy_logical_not(
                functionIr, destinationSlot, execInstructionIndex, ZR_NULL)) {
        fprintf(file,
                "    /* zr_aot_string_constant_stack_copy_local_logical_not_constant_skip slot=%u */\n",
                (unsigned)destinationSlot);
        return;
    }
    if (ZR_VALUE_IS_TYPE_STRING(constantValue->type) &&
        backend_aot_c_string_constant_consumed_by_local_stack_copy_jump_if(
                functionIr, destinationSlot, execInstructionIndex, ZR_NULL)) {
        fprintf(file,
                "    /* zr_aot_string_constant_stack_copy_local_jump_if_constant_skip slot=%u */\n",
                (unsigned)destinationSlot);
        return;
    }

    if (ZR_VALUE_IS_TYPE_BOOL(constantValue->type) &&
        backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot)) {
        const char *boolExpression = constantValue->value.nativeObject.nativeBool ? "ZR_TRUE" : "ZR_FALSE";
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_constant_bool_local */\n"
                "        zr_aot_b%u = %s;\n"
                "    }\n",
                (unsigned)destinationSlot,
                boolExpression);
        wroteScalarLocal = ZR_TRUE;
        if (backend_aot_c_scalar_locals_bool_constant_can_skip_value_slot(
                    functionIr, destinationSlot, execInstructionIndex)) {
            return;
        }
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type) &&
        backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot)) {
        snprintf(dataExpression,
                 sizeof(dataExpression),
                 "(TZrInt64)%lld",
                 (long long)constantValue->value.nativeObject.nativeInt64);
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_constant_i64_local */\n"
                "        zr_aot_s%u = %s;\n"
                "    }\n",
                (unsigned)destinationSlot,
                dataExpression);
        wroteScalarLocal = ZR_TRUE;
        if (backend_aot_c_scalar_locals_i64_constant_can_skip_value_slot(
                    functionIr, destinationSlot, execInstructionIndex)) {
            return;
        }
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type) &&
               backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot)) {
        snprintf(dataExpression,
                 sizeof(dataExpression),
                 "(TZrUInt64)%lld",
                 (long long)constantValue->value.nativeObject.nativeInt64);
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_constant_u64_from_i64_local */\n"
                "        zr_aot_u%u = %s;\n"
                "    }\n",
                (unsigned)destinationSlot,
                dataExpression);
        wroteScalarLocal = ZR_TRUE;
        if (backend_aot_c_scalar_locals_u64_constant_can_skip_value_slot(
                    functionIr, destinationSlot, execInstructionIndex)) {
            return;
        }
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type) &&
               backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot)) {
        snprintf(dataExpression,
                 sizeof(dataExpression),
                 "(TZrUInt64)%llu",
                 (unsigned long long)constantValue->value.nativeObject.nativeUInt64);
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_constant_u64_local */\n"
                "        zr_aot_u%u = %s;\n"
                "    }\n",
                (unsigned)destinationSlot,
                dataExpression);
        wroteScalarLocal = ZR_TRUE;
        if (backend_aot_c_scalar_locals_u64_constant_can_skip_value_slot(
                    functionIr, destinationSlot, execInstructionIndex)) {
            return;
        }
    } else if (ZR_VALUE_IS_TYPE_FLOAT(constantValue->type) &&
               backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot)) {
        snprintf(dataExpression,
                 sizeof(dataExpression),
                 "(TZrFloat64)%.17g",
                 constantValue->value.nativeObject.nativeDouble);
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_constant_f64_local */\n"
                "        zr_aot_f%u = %s;\n"
                "    }\n",
                (unsigned)destinationSlot,
                dataExpression);
        wroteScalarLocal = ZR_TRUE;
        if (backend_aot_c_scalar_locals_f64_constant_can_skip_value_slot(
                    functionIr, destinationSlot, execInstructionIndex)) {
            return;
        }
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_exec_primitive_constant */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n",
            (unsigned)destinationSlot,
            (unsigned)destinationSlot);

    if (ZR_VALUE_IS_TYPE_NULL(constantValue->type)) {
        backend_aot_c_write_direct_null_value(file);
    } else if (ZR_VALUE_IS_TYPE_BOOL(constantValue->type)) {
        const char *boolExpression = constantValue->value.nativeObject.nativeBool ? "ZR_TRUE" : "ZR_FALSE";
        backend_aot_c_write_direct_plain_value_replace_guard(file);
        backend_aot_c_write_direct_plain_value_scalar_assign(file,
                                                             "nativeBool",
                                                             boolExpression,
                                                             "ZR_VALUE_TYPE_BOOL");
        if (!wroteScalarLocal) {
            backend_aot_c_write_direct_primitive_constant_local_mirror(
                    file, functionIr, destinationSlot, constantValue, boolExpression);
        }
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        backend_aot_c_write_direct_plain_value_replace_guard(file);
        if (!wroteScalarLocal) {
            snprintf(dataExpression,
                     sizeof(dataExpression),
                     "(TZrInt64)%lld",
                     (long long)constantValue->value.nativeObject.nativeInt64);
        }
        backend_aot_c_write_direct_plain_value_scalar_assign(file,
                                                             "nativeInt64",
                                                             dataExpression,
                                                             backend_aot_c_value_type_literal(constantValue->type));
        if (!wroteScalarLocal) {
            backend_aot_c_write_direct_primitive_constant_local_mirror(
                    file, functionIr, destinationSlot, constantValue, dataExpression);
        }
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
        backend_aot_c_write_direct_plain_value_replace_guard(file);
        snprintf(dataExpression,
                 sizeof(dataExpression),
                 "(TZrUInt64)%llu",
                 (unsigned long long)constantValue->value.nativeObject.nativeUInt64);
        backend_aot_c_write_direct_plain_value_scalar_assign(file,
                                                             "nativeUInt64",
                                                             dataExpression,
                                                             backend_aot_c_value_type_literal(constantValue->type));
        backend_aot_c_write_direct_primitive_constant_local_mirror(
                file, functionIr, destinationSlot, constantValue, dataExpression);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(constantValue->type)) {
        backend_aot_c_write_direct_plain_value_replace_guard(file);
        if (!wroteScalarLocal) {
            snprintf(dataExpression,
                     sizeof(dataExpression),
                     "(TZrFloat64)%.17g",
                     constantValue->value.nativeObject.nativeDouble);
        }
        backend_aot_c_write_direct_plain_value_scalar_assign(file,
                                                             "nativeDouble",
                                                             dataExpression,
                                                             backend_aot_c_value_type_literal(constantValue->type));
        if (!wroteScalarLocal) {
            backend_aot_c_write_direct_primitive_constant_local_mirror(
                    file, functionIr, destinationSlot, constantValue, dataExpression);
        }
    } else {
        fprintf(file,
                "        ZrCore_Debug_RunError(state, \"unsupported primitive AOT constant\");\n"
                "        ZR_AOT_C_FAIL();\n");
    }

    fprintf(file, "    }\n");
}

/* SET_CONSTANT 保持解释器的浅赋值语义，修改的是当前函数元数据而非局部常量缓存。 */
void backend_aot_write_c_direct_set_constant(FILE *file, TZrUInt32 sourceSlot, TZrUInt32 constantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_exec_set_constant */\n"
            "        SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrTypeValue *zr_aot_constant = %u < frame.function->constantValueLength\n"
            "                                             ? &frame.function->constantValueList[%u]\n"
            "                                             : ZR_NULL;\n"
            "        if (zr_aot_source == ZR_NULL || zr_aot_constant == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        *zr_aot_constant = *zr_aot_source;\n"
            "    }\n",
            (unsigned)sourceSlot,
            (unsigned)constantIndex,
            (unsigned)constantIndex);
}

/* 非即时常量复制同时照顾布局槽与 dense 值槽，保留后续引用/所有权路径的观察结果。
 * TODO: 确认外部载入函数的常量索引在分派前已验证；生成 C 此处没有局部范围检查。
 */
void backend_aot_write_c_direct_constant_copy(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 constantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_constant_copy */\n"
            "        const SZrFunctionFrameSlotLayout *zr_aot_destination_layout =\n"
            "                ZrCore_Function_FindFrameSlotLayout(frame.function, %u);\n"
            "        const SZrTypeValue *zr_aot_source = &frame.function->constantValueList[%u];\n"
            "        SZrTypeValue *zr_aot_dense_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrStackFramePlace zr_aot_destination_place;\n"
            "        if (frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            zr_aot_dense_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_destination_layout != ZR_NULL &&\n"
            "            zr_aot_destination_layout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE &&\n"
            "            zr_aot_destination_layout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue)) {\n"
            "            if (!ZrCore_Function_MakeFrameSlotPlace(\n"
            "                    state, frame.function, frame.slotBase, %u, &zr_aot_destination_place)) {\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n"
            "            ZrCore_Value_Copy(state, (SZrTypeValue *)zr_aot_destination_place.address, zr_aot_source);\n"
            "            ZrCore_Value_Copy(state, zr_aot_dense_destination, zr_aot_source);\n"
            "        } else {\n"
            "            ZrCore_Value_Copy(state, zr_aot_dense_destination, zr_aot_source);\n"
            "        }\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)constantIndex,
            (unsigned)destinationSlot,
            (unsigned)destinationSlot);
}

/* 解析到当前 AOT 函数表的 callable 常量转为 native closure，并保留原函数作 shim 元数据。 */
void backend_aot_write_c_direct_callable_constant(FILE *file,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 constantIndex,
                                                  TZrUInt32 callableFlatIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        const SZrTypeValue *zr_aot_source = &frame.function->constantValueList[%u];\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrFunction *zr_aot_metadata_function = ZrCore_Closure_GetMetadataFunctionFromValue(state, zr_aot_source);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_metadata_function == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
            "        SZrClosureNative *zr_aot_closure = ZrCore_ClosureNative_New(state, 0);\n"
            "        if (zr_aot_closure == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_closure->nativeFunction = zr_aot_fn_%u;\n"
            "        zr_aot_closure->aotShimFunction = zr_aot_metadata_function;\n"
            "        ZrCore_Value_InitAsRawObject(state, zr_aot_destination, ZR_CAST_RAW_OBJECT_AS_SUPER(zr_aot_closure));\n"
            "        zr_aot_destination->type = ZR_VALUE_TYPE_CLOSURE;\n"
            "        zr_aot_destination->isGarbageCollectable = ZR_TRUE;\n"
            "        zr_aot_destination->isNative = ZR_TRUE;\n"
            "    }\n",
            (unsigned)constantIndex,
            (unsigned)destinationSlot,
            (unsigned)callableFlatIndex);
}

/* 子函数闭包的捕获/身份规则由 runtime 实现；flat index 只负责关联生成函数。 */
void backend_aot_write_c_direct_get_sub_function(FILE *file,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 childFunctionIndex,
                                                 TZrUInt32 callableFlatIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_get_sub_function_native_closure_boundary */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetSubFunctionNativeClosure(state,\n"
            "                                                                        &frame,\n"
            "                                                                        %u,\n"
            "                                                                        %u,\n"
            "                                                                        %u,\n"
            "                                                                        zr_aot_fn_%u));\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)childFunctionIndex,
            (unsigned)callableFlatIndex,
            (unsigned)callableFlatIndex);
}

/* 通用 CREATE_CLOSURE 交给 runtime 处理捕获，不复用零捕获 callable 的直写路径。 */
void backend_aot_write_c_create_closure(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 constantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_create_closure */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CreateClosure(state, &frame, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)constantIndex);
}

/* 函数常量不能安全绑定到本模块 AOT 函数时显式报错，避免产生错误 callable 身份。 */
void backend_aot_write_c_unsupported_callable_constant_materialization(FILE *file,
                                                                       TZrUInt32 destinationSlot,
                                                                       TZrUInt32 constantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_unsupported_callable_constant_materialization */\n"
            "        const SZrTypeValue *zr_aot_source = %u < frame.function->constantValueLength\n"
            "                                             ? &frame.function->constantValueList[%u]\n"
            "                                             : ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrFunction *zr_aot_metadata_function = ZrCore_Closure_GetMetadataFunctionFromValue(state, zr_aot_source);\n"
            "        if (zr_aot_source == ZR_NULL || zr_aot_destination == ZR_NULL ||\n"
            "            zr_aot_metadata_function == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Debug_RunError(state, \"unsupported AOT callable constant materialization\");\n"
            "        ZR_AOT_C_FAIL();\n"
            "    }\n",
            (unsigned)constantIndex,
            (unsigned)constantIndex,
            (unsigned)destinationSlot);
}

/* 子函数映射未解析的恢复路径保留原字节码诊断，不能随意造一个闭包。 */
void backend_aot_write_c_unsupported_get_sub_function_materialization(FILE *file,
                                                                      TZrUInt32 destinationSlot,
                                                                      TZrUInt32 childFunctionIndex,
                                                                      TZrUInt32 captureCount) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_unsupported_get_sub_function_materialization */\n"
            "        const TZrUInt32 zr_aot_capture_count = %u;\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrFunction *zr_aot_metadata_function = %u < frame.function->childFunctionLength\n"
            "                                             ? &frame.function->childFunctionList[%u]\n"
            "                                             : ZR_NULL;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_metadata_function == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_capture_count == 0) {\n"
            "            ZrCore_Debug_RunError(state, \"unsupported AOT GET_SUB_FUNCTION materialization\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Debug_RunError(state, \"unsupported AOT GET_SUB_FUNCTION materialization\");\n"
            "        ZR_AOT_C_FAIL();\n"
            "    }\n",
            (unsigned)captureCount,
            (unsigned)destinationSlot,
            (unsigned)childFunctionIndex,
            (unsigned)childFunctionIndex);
}

/* 捕获闭包无法匹配 AOT callable 时拒绝物化，防止绕过捕获语义。 */
void backend_aot_write_c_unsupported_create_closure_materialization(FILE *file,
                                                                    TZrUInt32 destinationSlot,
                                                                    TZrUInt32 constantIndex,
                                                                    TZrUInt32 captureCount) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_unsupported_create_closure_materialization */\n"
            "        const TZrUInt32 zr_aot_capture_count = %u;\n"
            "        const SZrTypeValue *zr_aot_source = %u < frame.function->constantValueLength\n"
            "                                             ? &frame.function->constantValueList[%u]\n"
            "                                             : ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrFunction *zr_aot_metadata_function = ZrCore_Closure_GetMetadataFunctionFromValue(state, zr_aot_source);\n"
            "        if (zr_aot_source == ZR_NULL || zr_aot_destination == ZR_NULL ||\n"
            "            zr_aot_metadata_function == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_capture_count == 0) {\n"
            "            ZrCore_Debug_RunError(state, \"unsupported AOT CREATE_CLOSURE materialization\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Debug_RunError(state, \"unsupported AOT CREATE_CLOSURE materialization\");\n"
            "        ZR_AOT_C_FAIL();\n"
            "    }\n",
            (unsigned)captureCount,
            (unsigned)constantIndex,
            (unsigned)constantIndex,
            (unsigned)destinationSlot);
}

/* 栈槽复制后按来源可达标量种类同步目标镜像，避免下一条优化指令读到旧值。 */
static void backend_aot_write_c_direct_stack_copy_scalar_local_sync(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot,
        TZrUInt32 execInstructionIndex) {
    TZrBool syncBool;
    TZrBool syncI64;
    TZrBool syncU64;
    TZrBool syncF64;
    TZrUInt32 sourceKindMask;

    if (file == ZR_NULL || functionIr == ZR_NULL) {
        return;
    }
    sourceKindMask = backend_aot_c_scalar_stack_copy_source_reaching_runtime_scalar_kind_mask_before(
            functionIr, sourceSlot, execInstructionIndex);
    if (sourceKindMask == ZR_AOT_RUNTIME_SCALAR_KIND_NONE) {
        return;
    }
    syncBool = (TZrBool)((sourceKindMask & ZR_AOT_RUNTIME_SCALAR_KIND_BOOL) != 0u &&
                         backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot));
    syncI64 = (TZrBool)((sourceKindMask & ZR_AOT_RUNTIME_SCALAR_KIND_I64) != 0u &&
                        backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot));
    syncU64 = (TZrBool)((sourceKindMask & ZR_AOT_RUNTIME_SCALAR_KIND_U64) != 0u &&
                        backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot));
    syncF64 = (TZrBool)((sourceKindMask & ZR_AOT_RUNTIME_SCALAR_KIND_F64) != 0u &&
                        backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot));
    if (!syncBool && !syncI64 && !syncU64 && !syncF64) {
        return;
    }

    if (syncBool) {
        fprintf(file,
                "        /* zr_aot_direct_stack_copy_sync_bool_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncI64) {
        fprintf(file,
                "        /* zr_aot_direct_stack_copy_sync_i64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncU64) {
        fprintf(file,
                "        /* zr_aot_direct_stack_copy_sync_u64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncF64) {
        fprintf(file,
                "        /* zr_aot_direct_stack_copy_sync_f64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
}

/* GET_STACK 保留源槽，SET_STACK 沿用复制语义；跳过镜像同步只在调用方已证明可行时使用。 */
void backend_aot_write_c_direct_stack_copy(FILE *file,
                                           const SZrAotExecIrFunction *functionIr,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 sourceSlot,
                                           TZrUInt32 execInstructionIndex,
                                           TZrBool preserveSource,
                                           TZrBool skipScalarLocalSync) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_copy_stack */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_%s(state, &frame, %u, %u));\n",
            preserveSource ? "GetStack" : "CopyStack",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    if (!skipScalarLocalSync) {
        backend_aot_write_c_direct_stack_copy_scalar_local_sync(
                file, functionIr, destinationSlot, sourceSlot, execInstructionIndex);
    }
    fprintf(file, "    } while (0);\n");
}

/* 以下 skip 发射器只留下审计标记；分派器依据相邻模式谓词选择入口。 */
void backend_aot_write_c_string_constant_stack_copy_local_logical_not_skip(FILE *file,
                                                                           TZrUInt32 destinationSlot,
                                                                           TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    /* zr_aot_string_constant_stack_copy_local_logical_not_source_skip dstSlot=%u srcSlot=%u */\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_null_constant_stack_copy_local_logical_not_skip(FILE *file,
                                                                         TZrUInt32 destinationSlot,
                                                                         TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    /* zr_aot_null_constant_stack_copy_local_logical_not_source_skip dstSlot=%u srcSlot=%u */\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_string_constant_stack_copy_local_jump_if_skip(FILE *file,
                                                                       TZrUInt32 destinationSlot,
                                                                       TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    /* zr_aot_string_constant_stack_copy_local_jump_if_source_skip dstSlot=%u srcSlot=%u */\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_null_constant_stack_copy_local_jump_if_skip(FILE *file,
                                                                     TZrUInt32 destinationSlot,
                                                                     TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    /* zr_aot_null_constant_stack_copy_local_jump_if_source_skip dstSlot=%u srcSlot=%u */\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

/* 闭包捕获槽的实际位置和生命周期由 runtime 解析，生成 C 只传捕获索引。 */
void backend_aot_write_c_get_closure_value(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 closureIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_get_closure_value */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetClosureValue(state, &frame, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)closureIndex);
}

void backend_aot_write_c_set_closure_value(FILE *file, TZrUInt32 sourceSlot, TZrUInt32 closureIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_set_closure_value */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SetClosureValue(state, &frame, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)sourceSlot,
            (unsigned)closureIndex);
}

/* 一般 RESET_STACK_NULL 交给 runtime 清理旧值；相邻模式谓词命中时分派器会跳过此调用。 */
void backend_aot_write_c_direct_reset_stack_null(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_reset_stack_null */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ResetStackNull(state, &frame, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot);
}

/* reset-skip 家族记录省略理由；调用方须先排除导出槽、异常边界和非线性消费者。 */
void backend_aot_write_c_reset_stack_null_scalar_local_skip(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    /* zr_aot_reset_stack_null_scalar_local_skip slot=%u */\n",
            (unsigned)destinationSlot);
}

void backend_aot_write_c_reset_stack_null_local_logical_not_skip(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    /* zr_aot_reset_stack_null_local_logical_not_skip slot=%u */\n",
            (unsigned)destinationSlot);
}

void backend_aot_write_c_reset_stack_null_local_jump_if_skip(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    /* zr_aot_reset_null_local_jump_if_source_skip slot=%u */\n",
            (unsigned)destinationSlot);
}

void backend_aot_write_c_reset_null_stack_copy_local_logical_not_reset_skip(FILE *file,
                                                                            TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    /* zr_aot_reset_null_stack_copy_local_logical_not_reset_skip slot=%u */\n",
            (unsigned)destinationSlot);
}

void backend_aot_write_c_reset_null_stack_copy_local_jump_if_reset_skip(FILE *file,
                                                                        TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    /* zr_aot_reset_null_stack_copy_local_jump_if_reset_skip slot=%u */\n",
            (unsigned)destinationSlot);
}

void backend_aot_write_c_reset_null_stack_copy_local_logical_not_skip(FILE *file,
                                                                      TZrUInt32 destinationSlot,
                                                                      TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    /* zr_aot_reset_null_stack_copy_local_logical_not_source_skip dstSlot=%u srcSlot=%u */\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_reset_null_stack_copy_local_jump_if_skip(FILE *file,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    /* zr_aot_reset_null_stack_copy_local_jump_if_source_skip dstSlot=%u srcSlot=%u */\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

/* 双槽重置是单条字节码的原子清理边界；实际释放顺序保留给 runtime。 */
void backend_aot_write_c_direct_reset_stack_null2(FILE *file, TZrUInt32 firstSlot, TZrUInt32 secondSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_reset_stack_null2 */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ResetStackNull2(state, &frame, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)firstSlot,
            (unsigned)secondSlot);
}

void backend_aot_write_c_reset_stack_null2_scalar_local_skip(FILE *file,
                                                             TZrUInt32 firstSlot,
                                                             TZrUInt32 secondSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    /* zr_aot_reset_stack_null2_scalar_local_skip slots=%u,%u */\n",
            (unsigned)firstSlot,
            (unsigned)secondSlot);
}

/* 全局对象来自当前执行状态，不能在生成时烘焙为编译期对象指针。 */
void backend_aot_write_c_direct_get_global(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_get_global */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetGlobal(state, &frame, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot);
}

/* 对象/数组分配族在 runtime 成功写入值槽后设置 GC 安全点。 */
void backend_aot_write_c_direct_create_object(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_create_object */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CreateObject(state, &frame, %u));\n",
            (unsigned)destinationSlot);
    backend_aot_write_c_gc_safepoint(file, "        ", "zr_aot_gc_safepoint_allocation");
    fprintf(file, "    } while (0);\n");
}

void backend_aot_write_c_direct_create_array(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_create_array */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CreateArray(state, &frame, %u));\n",
            (unsigned)destinationSlot);
    backend_aot_write_c_gc_safepoint(file, "        ", "zr_aot_gc_safepoint_allocation");
    fprintf(file, "    } while (0);\n");
}

/* 内联数组布局 ID 与长度来自当前字节码，分配后仍保留安全点。 */
void backend_aot_write_c_direct_create_inline_array(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 elementTypeLayoutId,
                                                    TZrUInt32 length) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_create_inline_array */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CreateInlineArray(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)elementTypeLayoutId,
            (unsigned)length);
    backend_aot_write_c_gc_safepoint(file, "        ", "zr_aot_gc_safepoint_allocation");
    fprintf(file, "    } while (0);\n");
}

/* 元素 place 是数组布局上的可写位置，绑定由 runtime 校验索引与所有权。 */
void backend_aot_write_c_direct_bind_inline_array_element_place(FILE *file,
                                                               TZrUInt32 destinationSlot,
                                                               TZrUInt32 arraySlot,
                                                               TZrUInt32 indexSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_bind_inline_array_element_place */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_BindInlineArrayElementPlace("
            "state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)arraySlot,
            (unsigned)indexSlot);
}

/* 类型查询交给 runtime 以保留动态值类别及错误语义。 */
void backend_aot_write_c_direct_typeof(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_typeof */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_TypeOf(state, &frame, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

/* 转对象后立刻刷新引用局部量，后续专用 member 路径依赖该镜像仍然有效。 */
void backend_aot_write_c_direct_to_object(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 sourceSlot,
                                          TZrUInt32 typeNameConstantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_to_object */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToObject(state, &frame, %u, %u, %u));\n"
            "        const SZrTypeValue *zr_aot_to_object_value = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_to_object_value == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_VALUE_IS_TYPE_NULL(zr_aot_to_object_value->type)) {\n"
            "            zr_aot_ref_locals.o%u = ZR_NULL;\n"
            "        } else if (ZR_VALUE_IS_TYPE_OBJECT(zr_aot_to_object_value->type)) {\n"
            "            zr_aot_ref_locals.o%u = zr_aot_to_object_value->value.object;\n"
            "        } else {\n"
            "            ZrCore_Debug_RunError(state, \"unsupported AOT to_object reference local\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)typeNameConstantIndex,
            (unsigned)destinationSlot,
            (unsigned)destinationSlot,
            (unsigned)destinationSlot);
    backend_aot_write_c_gc_safepoint(file, "        ", "zr_aot_gc_safepoint_reference_local");
    fprintf(file, "    } while (0);\n");
}

/* struct 转换需 runtime 按类型名常量校验布局，不能直接重解释对象槽。 */
void backend_aot_write_c_direct_to_struct(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 sourceSlot,
                                          TZrUInt32 typeNameConstantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_to_struct */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToStruct(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)typeNameConstantIndex);
}
