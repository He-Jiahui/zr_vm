#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"
#include "backend_aot_internal.h"

/* 逻辑 lowering 在可证明的局部值上直接生成判断，其他值交由 runtime 的真值/相等协议。 */

/* 字符串专用比较在生成 C 中读取 boxed 槽，不能从标量缓存推断对象内容。 */
static void backend_aot_c_write_string_logical_operand(FILE *file,
                                                       const char *name,
                                                       TZrUInt32 stackSlot) {
    if (file == ZR_NULL || name == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        const SZrTypeValue *%s = ZrCore_Stack_GetValue(frame.slotBase + %u);\n",
            name,
            (unsigned)stackSlot);
}

/* 相邻 TO_STRING 后可复用引用局部变量，避免重新取 boxed 值参与真值判断。 */
/* TODO: 这里只核对字节码相邻和异常处理器，尚需确认跳转边是否可能直接进入
 * 当前指令而绕过 TO_STRING；核查 CFG 入口与 ref-local 初始化规则。 */
static TZrBool backend_aot_c_to_string_slot_written_immediately_before(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 stringSlot,
        TZrUInt32 execInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *instruction;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL || execInstructionIndex == 0u) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (function->exceptionHandlerCount > 0 ||
        function->instructionsList == ZR_NULL ||
        execInstructionIndex > function->instructionsLength) {
        return ZR_FALSE;
    }

    instruction = &function->instructionsList[execInstructionIndex - 1u];
    return (TZrBool)(instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(TO_STRING) &&
                     instruction->instruction.operandExtra == stringSlot);
}

/* 相邻 TO_OBJECT 的判断使用相同约束；引用局部变量必须确实来自该前驱指令。 */
/* TODO: 与 TO_STRING 相同，需核对非顺序前驱能否越过 TO_OBJECT。 */
static TZrBool backend_aot_c_to_object_slot_written_immediately_before(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 objectSlot,
        TZrUInt32 execInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *instruction;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL || execInstructionIndex == 0u) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (function->exceptionHandlerCount > 0 ||
        function->instructionsList == ZR_NULL ||
        execInstructionIndex > function->instructionsLength) {
        return ZR_FALSE;
    }

    instruction = &function->instructionsList[execInstructionIndex - 1u];
    return (TZrBool)(instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(TO_OBJECT) &&
                     instruction->instruction.operandExtra == objectSlot);
}

/* JUMP_IF 的解释器真值规则将空字符串视为假；LOGICAL_NOT 有独立规则，见下方 BUG。 */
static void backend_aot_c_write_string_slot_truthiness(FILE *file, TZrUInt32 stringSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        if (zr_aot_ref_locals.o%u == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"unsupported AOT string truthiness\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        const SZrString *zr_aot_string_slot_string = ZR_CAST_STRING(state, zr_aot_ref_locals.o%u);\n"
            "        TZrBool zr_aot_string_slot_truthy = (TZrBool)(ZrCore_String_GetByteLength(zr_aot_string_slot_string) > 0u);\n",
            (unsigned)stringSlot,
            (unsigned)stringSlot);
}

/* 对象真值只依赖已建立的引用局部值是否为空，供 NOT 与 JUMP_IF 共用。 */
static void backend_aot_c_write_object_slot_truthiness(FILE *file, TZrUInt32 objectSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        TZrBool zr_aot_object_slot_truthy = (TZrBool)(zr_aot_ref_locals.o%u != ZR_NULL);\n",
            (unsigned)objectSlot);
}

static void backend_aot_c_write_bool_binary_logical(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        if (!ZR_VALUE_IS_TYPE_BOOL(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_BOOL(zr_aot_right->type)) {\n"
            "            ZrCore_Debug_RunError(state, \"unsupported AOT bool logical binary\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        TZrBool zr_aot_left_bool = (TZrBool)(zr_aot_left->value.nativeObject.nativeBool != 0u);\n"
            "        TZrBool zr_aot_right_bool = (TZrBool)(zr_aot_right->value.nativeObject.nativeBool != 0u);\n");
}

/* AND/OR 的布尔快路径要求两源缓存已写入且目标 boxed 槽可跳过。 */
static TZrBool backend_aot_c_write_bool_binary_scalar_local(FILE *file,
                                                            const SZrAotExecIrFunction *functionIr,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 leftSlot,
                                                            TZrUInt32 rightSlot,
                                                            TZrUInt32 execInstructionIndex,
                                                            const char *operatorText) {
    if (file == ZR_NULL || operatorText == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_bool_written_before(functionIr, leftSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_bool_written_before(functionIr, rightSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_bool_binary_scalar_local */\n"
            "        zr_aot_b%u = (TZrBool)((zr_aot_b%u %s zr_aot_b%u) != 0u);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            operatorText,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

/* 字符串专用 EQ/NEQ 与解释器的内容比较对应，不能退化成指针身份比较。 */
static void backend_aot_c_write_string_equality(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        TZrBool zr_aot_equal = ZR_FALSE;\n"
            "        if (!ZR_VALUE_IS_TYPE_STRING(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_STRING(zr_aot_right->type) ||\n"
            "            zr_aot_left->value.object == ZR_NULL || zr_aot_right->value.object == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"unsupported AOT string equality\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        const SZrString *zr_aot_left_string = ZR_CAST_STRING(state, zr_aot_left->value.object);\n"
            "        const SZrString *zr_aot_right_string = ZR_CAST_STRING(state, zr_aot_right->value.object);\n"
            "        if (zr_aot_left_string == zr_aot_right_string) {\n"
            "            zr_aot_equal = ZR_TRUE;\n"
            "        } else {\n"
            "            TZrSize zr_aot_left_length = ZrCore_String_GetByteLength(zr_aot_left_string);\n"
            "            TZrSize zr_aot_right_length = ZrCore_String_GetByteLength(zr_aot_right_string);\n"
            "            const TZrChar *zr_aot_left_bytes = ZrCore_String_GetNativeString(zr_aot_left_string);\n"
            "            const TZrChar *zr_aot_right_bytes = ZrCore_String_GetNativeString(zr_aot_right_string);\n"
            "            if (zr_aot_left_bytes == ZR_NULL || zr_aot_right_bytes == ZR_NULL) {\n"
            "                ZrCore_Debug_RunError(state, \"unsupported AOT string equality\");\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n"
            "            zr_aot_equal = (TZrBool)(zr_aot_left_length == zr_aot_right_length &&\n"
            "                                       (zr_aot_left_length == 0u ||\n"
            "                                        memcmp(zr_aot_left_bytes, zr_aot_right_bytes, zr_aot_left_length) == 0));\n"
            "        }\n");
}

/* 比较结果可留在 bool 局部变量，但字符串操作数仍从 VM 值槽读取。 */
static TZrBool backend_aot_c_write_string_bool_scalar_local(FILE *file,
                                                            const SZrAotExecIrFunction *functionIr,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 leftSlot,
                                                            TZrUInt32 rightSlot,
                                                            TZrUInt32 execInstructionIndex,
                                                            const char *resultExpression,
                                                            const char *markerText) {
    if (file == ZR_NULL || resultExpression == ZR_NULL || markerText == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* %s */\n"
            "        /* zr_aot_string_logical_bool_scalar_local */\n",
            markerText);
    backend_aot_c_write_string_logical_operand(file, "zr_aot_left", leftSlot);
    backend_aot_c_write_string_logical_operand(file, "zr_aot_right", rightSlot);
    fprintf(file,
            "        if (zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n");
    backend_aot_c_write_string_equality(file);
    fprintf(file,
            "        zr_aot_b%u = (TZrBool)((%s) != 0u);\n"
            "    }\n",
            (unsigned)destinationSlot,
            resultExpression);
    return ZR_TRUE;
}

static void backend_aot_c_write_bool_local_sync(FILE *file,
                                                const SZrAotExecIrFunction *functionIr,
                                                TZrUInt32 destinationSlot,
                                                const char *resultExpression) {
    if (file == ZR_NULL || resultExpression == ZR_NULL ||
        !backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot)) {
        return;
    }

    fprintf(file,
            "        zr_aot_b%u = (TZrBool)((%s) != 0u);\n",
            (unsigned)destinationSlot,
            resultExpression);
}

/* runtime 比较写入 boxed bool 后同步缓存，供紧随其后的布尔分支使用。 */
static void backend_aot_c_write_bool_local_sync_from_slot(FILE *file,
                                                          const SZrAotExecIrFunction *functionIr,
                                                          TZrUInt32 destinationSlot) {
    if (file == ZR_NULL || !backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot)) {
        return;
    }

    fprintf(file,
            "        /* zr_aot_generic_logical_sync_bool_local_boundary */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u));\n",
            (unsigned)destinationSlot,
            (unsigned)destinationSlot);
}

/* NOT 按常量来源和已写入的局部类型挑选真值快路径，最后才退至 runtime。 */
/* BUG: 解释器 LOGICAL_NOT 对任意字符串返回 false；此处的字符串常量及相邻 TO_STRING
 * 快路径却对空串写 true。test_aot_c_generic_logical_not_numeric_local_smoke.c 构造了空串指令，
 * 需用相同字节码对照解释器与 C AOT 的结果。 */
static TZrBool backend_aot_c_write_generic_logical_not_scalar_local(FILE *file,
                                                                    const SZrAotExecIrFunction *functionIr,
                                                                    TZrUInt32 destinationSlot,
                                                                    TZrUInt32 sourceSlot,
                                                                    TZrUInt32 execInstructionIndex) {
    TZrBool constantTruthy;
    const char *constantResultExpression;

    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    if (execInstructionIndex > 0u &&
        backend_aot_c_null_constant_consumed_by_local_logical_not(
                functionIr, sourceSlot, execInstructionIndex - 1u)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_logical_not */\n"
                "        /* zr_aot_generic_logical_not_null_constant_local */\n"
                "        zr_aot_b%u = ZR_TRUE;\n"
                "    }\n",
                (unsigned)destinationSlot);
        return ZR_TRUE;
    }

    if (execInstructionIndex > 0u &&
        backend_aot_c_null_constant_stack_copy_consumed_by_local_logical_not(
                functionIr, sourceSlot, execInstructionIndex - 1u)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_logical_not */\n"
                "        /* zr_aot_generic_logical_not_null_stack_copy_local */\n"
                "        zr_aot_b%u = ZR_TRUE;\n"
                "    }\n",
                (unsigned)destinationSlot);
        return ZR_TRUE;
    }

    if (execInstructionIndex > 0u &&
        backend_aot_c_reset_null_consumed_by_local_logical_not(
                functionIr, sourceSlot, execInstructionIndex - 1u)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_logical_not */\n"
                "        /* zr_aot_generic_logical_not_reset_null_local */\n"
                "        zr_aot_b%u = ZR_TRUE;\n"
                "    }\n",
                (unsigned)destinationSlot);
        return ZR_TRUE;
    }

    if (execInstructionIndex > 0u &&
        backend_aot_c_reset_null_stack_copy_consumed_by_local_logical_not(
                functionIr, sourceSlot, execInstructionIndex - 1u)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_logical_not */\n"
                "        /* zr_aot_generic_logical_not_reset_null_stack_copy_local */\n"
                "        zr_aot_b%u = ZR_TRUE;\n"
                "    }\n",
                (unsigned)destinationSlot);
        return ZR_TRUE;
    }

    constantTruthy = ZR_FALSE;
    if (execInstructionIndex > 0u &&
        backend_aot_c_bool_constant_consumed_by_local_logical_not(
                functionIr, sourceSlot, execInstructionIndex - 1u, &constantTruthy)) {
        constantResultExpression = constantTruthy ? "ZR_FALSE" : "ZR_TRUE";
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_logical_not */\n"
                "        /* zr_aot_generic_logical_not_bool_constant_local */\n"
                "        zr_aot_b%u = %s;\n"
                "    }\n",
                (unsigned)destinationSlot,
                constantResultExpression);
        return ZR_TRUE;
    }

    constantTruthy = ZR_FALSE;
    if (execInstructionIndex > 0u &&
        backend_aot_c_string_constant_consumed_by_local_logical_not(
                functionIr, sourceSlot, execInstructionIndex - 1u, &constantTruthy)) {
        constantResultExpression = constantTruthy ? "ZR_FALSE" : "ZR_TRUE";
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_logical_not */\n"
                "        /* zr_aot_generic_logical_not_string_constant_local */\n"
                "        zr_aot_b%u = %s;\n"
                "    }\n",
                (unsigned)destinationSlot,
                constantResultExpression);
        return ZR_TRUE;
    }

    constantTruthy = ZR_FALSE;
    if (execInstructionIndex > 0u &&
        backend_aot_c_string_constant_stack_copy_consumed_by_local_logical_not(
                functionIr, sourceSlot, execInstructionIndex - 1u, &constantTruthy)) {
        constantResultExpression = constantTruthy ? "ZR_FALSE" : "ZR_TRUE";
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_logical_not */\n"
                "        /* zr_aot_generic_logical_not_string_stack_copy_local */\n"
                "        zr_aot_b%u = %s;\n"
                "    }\n",
                (unsigned)destinationSlot,
                constantResultExpression);
        return ZR_TRUE;
    }

    if (backend_aot_c_to_string_slot_written_immediately_before(functionIr, sourceSlot, execInstructionIndex)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_logical_not */\n"
                "        /* zr_aot_generic_logical_not_string_slot_local */\n");
        backend_aot_c_write_string_slot_truthiness(file, sourceSlot);
        fprintf(file,
                "        zr_aot_b%u = (TZrBool)(!zr_aot_string_slot_truthy);\n"
                "    }\n",
                (unsigned)destinationSlot);
        return ZR_TRUE;
    }

    if (backend_aot_c_to_object_slot_written_immediately_before(functionIr, sourceSlot, execInstructionIndex)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_logical_not */\n"
                "        /* zr_aot_generic_logical_not_object_slot_local */\n");
        backend_aot_c_write_object_slot_truthiness(file, sourceSlot);
        fprintf(file,
                "        zr_aot_b%u = (TZrBool)(!zr_aot_object_slot_truthy);\n"
                "    }\n",
                (unsigned)destinationSlot);
        return ZR_TRUE;
    }

    if (backend_aot_c_scalar_locals_bool_value_written_before(functionIr, sourceSlot, execInstructionIndex)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_logical_not */\n"
                "        /* zr_aot_generic_logical_not_scalar_local */\n"
                "        zr_aot_b%u = (TZrBool)(!zr_aot_b%u);\n"
                "    }\n",
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        return ZR_TRUE;
    }

    if (backend_aot_c_scalar_locals_has_i64_slot(functionIr, sourceSlot) &&
        backend_aot_c_scalar_locals_i64_written_before(functionIr, sourceSlot, execInstructionIndex)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_logical_not */\n"
                "        /* zr_aot_generic_logical_not_i64_scalar_local */\n"
                "        zr_aot_b%u = (TZrBool)(zr_aot_s%u == (TZrInt64)0);\n"
                "    }\n",
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        return ZR_TRUE;
    }

    if (backend_aot_c_scalar_locals_has_u64_slot(functionIr, sourceSlot) &&
        backend_aot_c_scalar_locals_u64_written_before(functionIr, sourceSlot, execInstructionIndex)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_logical_not */\n"
                "        /* zr_aot_generic_logical_not_u64_scalar_local */\n"
                "        zr_aot_b%u = (TZrBool)(zr_aot_u%u == (TZrUInt64)0u);\n"
                "    }\n",
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        return ZR_TRUE;
    }

    if (backend_aot_c_scalar_locals_has_f64_slot(functionIr, sourceSlot) &&
        backend_aot_c_scalar_locals_f64_written_before(functionIr, sourceSlot, execInstructionIndex)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_logical_not */\n"
                "        /* zr_aot_generic_logical_not_f64_scalar_local */\n"
                "        zr_aot_b%u = (TZrBool)(zr_aot_f%u == (TZrFloat64)0.0);\n"
                "    }\n",
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_write_generic_bool_compare_scalar_local(FILE *file,
                                                                     const SZrAotExecIrFunction *functionIr,
                                                                     TZrUInt32 destinationSlot,
                                                                     TZrUInt32 leftSlot,
                                                                     TZrUInt32 rightSlot,
                                                                     TZrUInt32 execInstructionIndex,
                                                                     const char *markerText,
                                                                     const char *operatorText) {
    if (file == ZR_NULL || markerText == ZR_NULL || operatorText == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_bool_value_written_before(functionIr, leftSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_bool_value_written_before(functionIr, rightSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* %s */\n"
            "        /* zr_aot_generic_bool_compare_scalar_local */\n"
            "        zr_aot_b%u = (TZrBool)((zr_aot_b%u %s zr_aot_b%u) != 0u);\n"
            "    }\n",
            markerText,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            operatorText,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

static TZrBool backend_aot_c_write_generic_primitive_compare_scalar_local(FILE *file,
                                                                          const SZrAotExecIrFunction *functionIr,
                                                                          TZrUInt32 destinationSlot,
                                                                          TZrUInt32 leftSlot,
                                                                          TZrUInt32 rightSlot,
                                                                          TZrUInt32 execInstructionIndex,
                                                                          const char *markerText,
                                                                          const char *operatorText) {
    const char *compareMarker;
    const char *localPrefix;

    if (file == ZR_NULL || markerText == ZR_NULL || operatorText == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    compareMarker = ZR_NULL;
    localPrefix = ZR_NULL;
    if (backend_aot_c_scalar_locals_has_i64_slot(functionIr, leftSlot) &&
        backend_aot_c_scalar_locals_has_i64_slot(functionIr, rightSlot) &&
        backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, execInstructionIndex) &&
        backend_aot_c_scalar_locals_i64_written_before(functionIr, rightSlot, execInstructionIndex)) {
        compareMarker = "zr_aot_generic_i64_compare_scalar_local";
        localPrefix = "s";
    } else if (backend_aot_c_scalar_locals_has_u64_slot(functionIr, leftSlot) &&
               backend_aot_c_scalar_locals_has_u64_slot(functionIr, rightSlot) &&
               backend_aot_c_scalar_locals_u64_written_before(functionIr, leftSlot, execInstructionIndex) &&
               backend_aot_c_scalar_locals_u64_written_before(functionIr, rightSlot, execInstructionIndex)) {
        compareMarker = "zr_aot_generic_u64_compare_scalar_local";
        localPrefix = "u";
    } else if (backend_aot_c_scalar_locals_has_f64_slot(functionIr, leftSlot) &&
               backend_aot_c_scalar_locals_has_f64_slot(functionIr, rightSlot) &&
               backend_aot_c_scalar_locals_f64_written_before(functionIr, leftSlot, execInstructionIndex) &&
               backend_aot_c_scalar_locals_f64_written_before(functionIr, rightSlot, execInstructionIndex)) {
        compareMarker = "zr_aot_generic_f64_compare_scalar_local";
        localPrefix = "f";
    } else {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* %s */\n"
            "        /* %s */\n"
            "        zr_aot_b%u = (TZrBool)((zr_aot_%s%u %s zr_aot_%s%u) != 0u);\n"
            "    }\n",
            markerText,
            compareMarker,
            (unsigned)destinationSlot,
            localPrefix,
            (unsigned)leftSlot,
            operatorText,
            localPrefix,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

/* 混合基本类型相等比较只在每个槽恰有一种已写缓存时才能常量折叠。 */
static TZrBool backend_aot_c_generic_primitive_local_kind_written_before(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 slot,
        TZrUInt32 execInstructionIndex,
        char *outKindCode) {
    TZrUInt32 kindCount;
    char kindCode;

    if (outKindCode == ZR_NULL) {
        return ZR_FALSE;
    }
    *outKindCode = '\0';
    if (functionIr == ZR_NULL) {
        return ZR_FALSE;
    }

    kindCount = 0u;
    kindCode = '\0';
    if (backend_aot_c_scalar_locals_has_bool_slot(functionIr, slot) &&
        backend_aot_c_scalar_locals_bool_value_written_before(functionIr, slot, execInstructionIndex)) {
        kindCount++;
        kindCode = 'b';
    }
    if (backend_aot_c_scalar_locals_has_i64_slot(functionIr, slot) &&
        backend_aot_c_scalar_locals_i64_written_before(functionIr, slot, execInstructionIndex)) {
        kindCount++;
        kindCode = 's';
    }
    if (backend_aot_c_scalar_locals_has_u64_slot(functionIr, slot) &&
        backend_aot_c_scalar_locals_u64_written_before(functionIr, slot, execInstructionIndex)) {
        kindCount++;
        kindCode = 'u';
    }
    if (backend_aot_c_scalar_locals_has_f64_slot(functionIr, slot) &&
        backend_aot_c_scalar_locals_f64_written_before(functionIr, slot, execInstructionIndex)) {
        kindCount++;
        kindCode = 'f';
    }
    if (kindCount != 1u) {
        return ZR_FALSE;
    }

    *outKindCode = kindCode;
    return ZR_TRUE;
}

/* 与 core 值相等规则一致，类型不同的基本值直接比较为不等。 */
static TZrBool backend_aot_c_write_generic_mixed_primitive_compare_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex,
        const char *markerText,
        const char *resultExpression) {
    char leftKindCode;
    char rightKindCode;

    if (file == ZR_NULL || markerText == ZR_NULL || resultExpression == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, execInstructionIndex) ||
        !backend_aot_c_generic_primitive_local_kind_written_before(
                functionIr, leftSlot, execInstructionIndex, &leftKindCode) ||
        !backend_aot_c_generic_primitive_local_kind_written_before(
                functionIr, rightSlot, execInstructionIndex, &rightKindCode) ||
        leftKindCode == rightKindCode) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* %s */\n"
            "        /* zr_aot_generic_mixed_primitive_compare_scalar_local */\n"
            "        /* leftKind=%c rightKind=%c */\n"
            "        zr_aot_b%u = %s;\n"
            "    }\n",
            markerText,
            leftKindCode,
            rightKindCode,
            (unsigned)destinationSlot,
            resultExpression);
    return ZR_TRUE;
}

/* 只为已证明可用的标量条件生成直接分支，循环回边仍保留 GC 安全点。 */
static TZrBool backend_aot_c_write_generic_jump_if_scalar_local(FILE *file,
                                                                const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 functionIndex,
                                                                TZrUInt32 conditionSlot,
                                                                TZrUInt32 execInstructionIndex,
                                                                TZrUInt32 targetInstructionIndex,
                                                                TZrBool isBackEdge) {
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_c_scalar_locals_bool_value_written_before(functionIr, conditionSlot, execInstructionIndex)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_jump_if */\n"
                "        /* zr_aot_generic_jump_if_bool_scalar_local */\n"
                "        if (!zr_aot_b%u) {\n",
                (unsigned)conditionSlot);
    } else if (backend_aot_c_scalar_locals_has_i64_slot(functionIr, conditionSlot) &&
               backend_aot_c_scalar_locals_i64_written_before(functionIr, conditionSlot, execInstructionIndex)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_jump_if */\n"
                "        /* zr_aot_generic_jump_if_i64_scalar_local */\n"
                "        if (zr_aot_s%u == (TZrInt64)0) {\n",
                (unsigned)conditionSlot);
    } else if (backend_aot_c_scalar_locals_has_u64_slot(functionIr, conditionSlot) &&
               backend_aot_c_scalar_locals_u64_written_before(functionIr, conditionSlot, execInstructionIndex)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_jump_if */\n"
                "        /* zr_aot_generic_jump_if_u64_scalar_local */\n"
                "        if (zr_aot_u%u == (TZrUInt64)0u) {\n",
                (unsigned)conditionSlot);
    } else if (backend_aot_c_scalar_locals_has_f64_slot(functionIr, conditionSlot) &&
               backend_aot_c_scalar_locals_f64_written_before(functionIr, conditionSlot, execInstructionIndex)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_jump_if */\n"
                "        /* zr_aot_generic_jump_if_f64_scalar_local */\n"
                "        if (zr_aot_f%u == (TZrFloat64)0.0) {\n",
                (unsigned)conditionSlot);
    } else {
        return ZR_FALSE;
    }

    if (isBackEdge) {
        backend_aot_write_c_gc_safepoint(file, "            ", "zr_aot_gc_safepoint_back_edge");
    }
    fprintf(file,
            "            goto zr_aot_fn_%u_ins_%u;\n"
            "        }\n"
            "    }\n",
            (unsigned)functionIndex,
            (unsigned)targetInstructionIndex);
    return ZR_TRUE;
}

/* 通用 EQ 保留不同基本类型的快速不等判断，未知值转交运行时。 */
/* BUG: 两个动态字符串或对象进入 GenericPrimitiveLogicalEqual 时 runtime
 * 报 unsupported；解释器的 ZrCore_Value_Equal 支持字符串内容与对象身份比较。 */
void backend_aot_write_c_direct_logical_equal(FILE *file,
                                              const SZrAotExecIrFunction *functionIr,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 rightSlot,
                                              TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return;
    }
    if (backend_aot_c_write_generic_bool_compare_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_logical_equal",
                "==")) {
        return;
    }
    if (backend_aot_c_write_generic_primitive_compare_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_logical_equal",
                "==")) {
        return;
    }
    if (backend_aot_c_write_generic_mixed_primitive_compare_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_logical_equal",
                "ZR_FALSE")) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_generic_logical_equal */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    backend_aot_c_write_bool_local_sync_from_slot(file, functionIr, destinationSlot);
    fprintf(file, "    }\n");
}

/* 通用 NEQ 与 EQ 共用运行时相等协议，取反后再同步 bool 缓存。 */
/* BUG: 两个动态字符串或对象的 NEQ 仍会在 primitive runtime 退路报错，
 * 而解释器按 ZrCore_Value_Equal 的反值返回布尔结果。 */
void backend_aot_write_c_direct_logical_not_equal(FILE *file,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot,
                                                  TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return;
    }
    if (backend_aot_c_write_generic_bool_compare_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_logical_not_equal",
                "!=")) {
        return;
    }
    if (backend_aot_c_write_generic_primitive_compare_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_logical_not_equal",
                "!=")) {
        return;
    }
    if (backend_aot_c_write_generic_mixed_primitive_compare_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_logical_not_equal",
                "ZR_TRUE")) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_generic_logical_not_equal */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GenericPrimitiveLogicalNotEqual(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    backend_aot_c_write_bool_local_sync_from_slot(file, functionIr, destinationSlot);
    fprintf(file, "    }\n");
}

/* 已 quicken 为 STRING 的 EQ 独立走内容比较，避免通用 primitive 退路。 */
void backend_aot_write_c_direct_logical_equal_string(FILE *file,
                                                     const SZrAotExecIrFunction *functionIr,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot,
                                                     TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return;
    }
    if (backend_aot_c_write_string_bool_scalar_local(file,
                                                     functionIr,
                                                     destinationSlot,
                                                     leftSlot,
                                                     rightSlot,
                                                     execInstructionIndex,
                                                     "zr_aot_equal",
                                                     "zr_aot_string_logical_equal")) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_string_logical_equal */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n",
            (unsigned)destinationSlot);
    backend_aot_c_write_string_logical_operand(file, "zr_aot_left", leftSlot);
    backend_aot_c_write_string_logical_operand(file, "zr_aot_right", rightSlot);
    fprintf(file,
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n");
    backend_aot_c_write_string_equality(file);
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, zr_aot_equal, ZR_VALUE_TYPE_BOOL);\n");
    backend_aot_c_write_bool_local_sync(file, functionIr, destinationSlot, "zr_aot_equal");
    fprintf(file, "    }\n");
}

/* 已 quicken 为 STRING 的 NEQ 复用同一比较，再反转布尔结果。 */
void backend_aot_write_c_direct_logical_not_equal_string(FILE *file,
                                                         const SZrAotExecIrFunction *functionIr,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot,
                                                         TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return;
    }
    if (backend_aot_c_write_string_bool_scalar_local(file,
                                                     functionIr,
                                                     destinationSlot,
                                                     leftSlot,
                                                     rightSlot,
                                                     execInstructionIndex,
                                                     "!zr_aot_equal",
                                                     "zr_aot_string_logical_not_equal")) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_string_logical_not_equal */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n",
            (unsigned)destinationSlot);
    backend_aot_c_write_string_logical_operand(file, "zr_aot_left", leftSlot);
    backend_aot_c_write_string_logical_operand(file, "zr_aot_right", rightSlot);
    fprintf(file,
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n");
    backend_aot_c_write_string_equality(file);
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, !zr_aot_equal, ZR_VALUE_TYPE_BOOL);\n");
    backend_aot_c_write_bool_local_sync(file, functionIr, destinationSlot, "!zr_aot_equal");
    fprintf(file, "    }\n");
}

/* AND 的操作数按指令契约均为 bool；快路径和 boxed 路径都保持布尔结果。 */
void backend_aot_write_c_direct_logical_and(FILE *file,
                                            const SZrAotExecIrFunction *functionIr,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 rightSlot,
                                            TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return;
    }
    if (backend_aot_c_write_bool_binary_scalar_local(
                file, functionIr, destinationSlot, leftSlot, rightSlot, execInstructionIndex, "&&")) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_bool_logical_and */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    backend_aot_c_write_bool_binary_logical(file);
    fprintf(file,
            "        TZrBool zr_aot_result = (TZrBool)(zr_aot_left_bool && zr_aot_right_bool);\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, zr_aot_result, ZR_VALUE_TYPE_BOOL);\n");
    backend_aot_c_write_bool_local_sync(file, functionIr, destinationSlot, "zr_aot_result");
    fprintf(file, "    }\n");
}

/* OR 与 AND 共用 bool 快路径资格，未知缓存状态返回 boxed 值槽。 */
void backend_aot_write_c_direct_logical_or(FILE *file,
                                           const SZrAotExecIrFunction *functionIr,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot,
                                           TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return;
    }
    if (backend_aot_c_write_bool_binary_scalar_local(
                file, functionIr, destinationSlot, leftSlot, rightSlot, execInstructionIndex, "||")) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_bool_logical_or */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    backend_aot_c_write_bool_binary_logical(file);
    fprintf(file,
            "        TZrBool zr_aot_result = (TZrBool)(zr_aot_left_bool || zr_aot_right_bool);\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, zr_aot_result, ZR_VALUE_TYPE_BOOL);\n");
    backend_aot_c_write_bool_local_sync(file, functionIr, destinationSlot, "zr_aot_result");
    fprintf(file, "    }\n");
}

/* 通用 NOT 可利用常量与相邻转换的局部来源，其他来源由 runtime 判断真值。 */
/* BUG: 非相邻来源的字符串或对象落到 GenericPrimitiveLogicalNot 后报错；
 * 解释器 LOGICAL_NOT 对字符串/对象有定义的真值结果。 */
void backend_aot_write_c_direct_logical_not(FILE *file,
                                            const SZrAotExecIrFunction *functionIr,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 sourceSlot,
                                            TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return;
    }
    if (backend_aot_c_write_generic_logical_not_scalar_local(
                file, functionIr, destinationSlot, sourceSlot, execInstructionIndex)) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_generic_logical_not */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    backend_aot_c_write_bool_local_sync_from_slot(file, functionIr, destinationSlot);
    fprintf(file, "    }\n");
}

/* JUMP_IF 在假值边跳转；循环回边的 GC 安全点须在实际跳转之前发出。 */
/* BUG: 非相邻来源的字符串或对象条件落到 GenericPrimitiveIsTruthy 后报错；
 * 解释器 execution_is_truthy 支持空字符串与对象条件。 */
void backend_aot_write_c_direct_jump_if(FILE *file,
                                        const SZrAotExecIrFunction *functionIr,
                                        TZrUInt32 functionIndex,
                                        TZrUInt32 conditionSlot,
                                        TZrUInt32 execInstructionIndex,
                                        TZrUInt32 targetInstructionIndex,
                                        TZrBool isBackEdge) {
    TZrBool constantTruthy;

    if (file == ZR_NULL) {
        return;
    }
    constantTruthy = ZR_FALSE;
    if (execInstructionIndex > 0u &&
        backend_aot_c_null_constant_consumed_by_local_jump_if(
                functionIr, conditionSlot, execInstructionIndex - 1u)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_jump_if */\n"
                "        /* zr_aot_generic_jump_if_null_constant_false */\n");
        if (isBackEdge) {
            backend_aot_write_c_gc_safepoint(file, "        ", "zr_aot_gc_safepoint_back_edge");
        }
        fprintf(file,
                "        goto zr_aot_fn_%u_ins_%u;\n"
                "    }\n",
                (unsigned)functionIndex,
                (unsigned)targetInstructionIndex);
        return;
    }
    if (execInstructionIndex > 0u &&
        backend_aot_c_null_constant_stack_copy_consumed_by_local_jump_if(
                functionIr, conditionSlot, execInstructionIndex - 1u)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_jump_if */\n"
                "        /* zr_aot_generic_jump_if_null_stack_copy_false */\n");
        if (isBackEdge) {
            backend_aot_write_c_gc_safepoint(file, "        ", "zr_aot_gc_safepoint_back_edge");
        }
        fprintf(file,
                "        goto zr_aot_fn_%u_ins_%u;\n"
                "    }\n",
                (unsigned)functionIndex,
                (unsigned)targetInstructionIndex);
        return;
    }
    if (execInstructionIndex > 0u &&
        backend_aot_c_reset_null_stack_copy_consumed_by_local_jump_if(
                functionIr, conditionSlot, execInstructionIndex - 1u)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_jump_if */\n"
                "        /* zr_aot_generic_jump_if_reset_null_stack_copy_false */\n");
        if (isBackEdge) {
            backend_aot_write_c_gc_safepoint(file, "        ", "zr_aot_gc_safepoint_back_edge");
        }
        fprintf(file,
                "        goto zr_aot_fn_%u_ins_%u;\n"
                "    }\n",
                (unsigned)functionIndex,
                (unsigned)targetInstructionIndex);
        return;
    }
    if (execInstructionIndex > 0u &&
        backend_aot_c_reset_null_consumed_by_local_jump_if(
                functionIr, conditionSlot, execInstructionIndex - 1u)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_jump_if */\n"
                "        /* zr_aot_generic_jump_if_reset_null_false */\n");
        if (isBackEdge) {
            backend_aot_write_c_gc_safepoint(file, "        ", "zr_aot_gc_safepoint_back_edge");
        }
        fprintf(file,
                "        goto zr_aot_fn_%u_ins_%u;\n"
                "    }\n",
                (unsigned)functionIndex,
                (unsigned)targetInstructionIndex);
        return;
    }
    if (execInstructionIndex > 0u &&
        backend_aot_c_bool_constant_consumed_by_local_jump_if(
                functionIr, conditionSlot, execInstructionIndex - 1u, &constantTruthy)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_jump_if */\n");
        if (constantTruthy) {
            fprintf(file,
                    "        /* zr_aot_generic_jump_if_bool_constant_true */\n"
                    "    }\n");
            return;
        }
        fprintf(file,
                "        /* zr_aot_generic_jump_if_bool_constant_false */\n");
        if (isBackEdge) {
            backend_aot_write_c_gc_safepoint(file, "        ", "zr_aot_gc_safepoint_back_edge");
        }
        fprintf(file,
                "        goto zr_aot_fn_%u_ins_%u;\n"
                "    }\n",
                (unsigned)functionIndex,
                (unsigned)targetInstructionIndex);
        return;
    }
    constantTruthy = ZR_FALSE;
    if (execInstructionIndex > 0u &&
        backend_aot_c_string_constant_consumed_by_local_jump_if(
                functionIr, conditionSlot, execInstructionIndex - 1u, &constantTruthy)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_jump_if */\n");
        if (constantTruthy) {
            fprintf(file,
                    "        /* zr_aot_generic_jump_if_string_constant_true */\n"
                    "    }\n");
            return;
        }
        fprintf(file,
                "        /* zr_aot_generic_jump_if_string_constant_false */\n");
        if (isBackEdge) {
            backend_aot_write_c_gc_safepoint(file, "        ", "zr_aot_gc_safepoint_back_edge");
        }
        fprintf(file,
                "        goto zr_aot_fn_%u_ins_%u;\n"
                "    }\n",
                (unsigned)functionIndex,
                (unsigned)targetInstructionIndex);
        return;
    }
    constantTruthy = ZR_FALSE;
    if (execInstructionIndex > 0u &&
        backend_aot_c_string_constant_stack_copy_consumed_by_local_jump_if(
                functionIr, conditionSlot, execInstructionIndex - 1u, &constantTruthy)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_jump_if */\n");
        if (constantTruthy) {
            fprintf(file,
                    "        /* zr_aot_generic_jump_if_string_stack_copy_true */\n"
                    "    }\n");
            return;
        }
        fprintf(file,
                "        /* zr_aot_generic_jump_if_string_stack_copy_false */\n");
        if (isBackEdge) {
            backend_aot_write_c_gc_safepoint(file, "        ", "zr_aot_gc_safepoint_back_edge");
        }
        fprintf(file,
                "        goto zr_aot_fn_%u_ins_%u;\n"
                "    }\n",
                (unsigned)functionIndex,
                (unsigned)targetInstructionIndex);
        return;
    }
    if (backend_aot_c_to_string_slot_written_immediately_before(functionIr, conditionSlot, execInstructionIndex)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_jump_if */\n"
                "        /* zr_aot_generic_jump_if_string_slot_local */\n");
        backend_aot_c_write_string_slot_truthiness(file, conditionSlot);
        fprintf(file,
                "        if (!zr_aot_string_slot_truthy) {\n");
        if (isBackEdge) {
            backend_aot_write_c_gc_safepoint(file, "            ", "zr_aot_gc_safepoint_back_edge");
        }
        fprintf(file,
                "            goto zr_aot_fn_%u_ins_%u;\n"
                "        }\n"
                "    }\n",
                (unsigned)functionIndex,
                (unsigned)targetInstructionIndex);
        return;
    }
    if (backend_aot_c_to_object_slot_written_immediately_before(functionIr, conditionSlot, execInstructionIndex)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_generic_jump_if */\n"
                "        /* zr_aot_generic_jump_if_object_slot_local */\n");
        backend_aot_c_write_object_slot_truthiness(file, conditionSlot);
        fprintf(file,
                "        if (!zr_aot_object_slot_truthy) {\n");
        if (isBackEdge) {
            backend_aot_write_c_gc_safepoint(file, "            ", "zr_aot_gc_safepoint_back_edge");
        }
        fprintf(file,
                "            goto zr_aot_fn_%u_ins_%u;\n"
                "        }\n"
                "    }\n",
                (unsigned)functionIndex,
                (unsigned)targetInstructionIndex);
        return;
    }
    if (backend_aot_c_write_generic_jump_if_scalar_local(file,
                                                         functionIr,
                                                         functionIndex,
                                                         conditionSlot,
                                                         execInstructionIndex,
                                                         targetInstructionIndex,
                                                         isBackEdge)) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_generic_jump_if */\n"
            "        TZrBool zr_aot_truthy = ZR_FALSE;\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(state, &frame, %u, &zr_aot_truthy));\n",
            (unsigned)conditionSlot);
    fprintf(file,
            "        if (!zr_aot_truthy) {\n");
    if (isBackEdge) {
        backend_aot_write_c_gc_safepoint(file, "            ", "zr_aot_gc_safepoint_back_edge");
    }
    fprintf(file,
            "            goto zr_aot_fn_%u_ins_%u;\n"
            "        }\n"
            "    }\n",
            (unsigned)functionIndex,
            (unsigned)targetInstructionIndex);
}
