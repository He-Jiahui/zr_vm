#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"

/* 泛型数值 opcode 先尝试已证明可用的标量局部变量，再通过 VM 值槽进入运行时类型分派。 */

/* 运行时退路写回 boxed 目标后，刷新该槽的各类标量缓存供后续专用 lowering 复用。 */
static void backend_aot_write_c_generic_numeric_sync_locals(FILE *file,
                                                            const SZrAotExecIrFunction *functionIr,
                                                            TZrUInt32 destinationSlot) {
    TZrBool syncI64;
    TZrBool syncU64;
    TZrBool syncF64;

    if (file == ZR_NULL) {
        return;
    }

    syncI64 = backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot);
    syncU64 = backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot);
    syncF64 = backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot);

    if (syncI64) {
        fprintf(file,
                "        /* zr_aot_generic_numeric_sync_i64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncU64) {
        fprintf(file,
                "        /* zr_aot_generic_numeric_sync_u64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
    if (syncF64) {
        fprintf(file,
                "        /* zr_aot_generic_numeric_sync_f64_local_boundary */\n"
                "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u));\n",
                (unsigned)destinationSlot,
                (unsigned)destinationSlot);
    }
}

/* 二元退路保留运行时类型检查；调用者只有无法安全跳过值槽时才选此边界。 */
static void backend_aot_write_c_generic_numeric_binary_boundary(FILE *file,
                                                                const SZrAotExecIrFunction *functionIr,
                                                                const char *runtimeHelper,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 leftSlot,
                                                                TZrUInt32 rightSlot) {
    if (file == ZR_NULL || runtimeHelper == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary_boundary */\n"
            "        ZR_AOT_C_GUARD(%s(state, &frame, %u, %u, %u));\n",
            runtimeHelper,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    backend_aot_write_c_generic_numeric_sync_locals(file, functionIr, destinationSlot);
    fprintf(file, "    }\n");
}

/* 一元退路与二元退路共享“先 VM 运算、后同步缓存”的值表示协议。 */
static void backend_aot_write_c_generic_numeric_unary_boundary(FILE *file,
                                                               const SZrAotExecIrFunction *functionIr,
                                                               const char *runtimeHelper,
                                                               TZrUInt32 destinationSlot,
                                                               TZrUInt32 sourceSlot) {
    if (file == ZR_NULL || runtimeHelper == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_unary_boundary */\n"
            "        ZR_AOT_C_GUARD(%s(state, &frame, %u, %u));\n",
            runtimeHelper,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    backend_aot_write_c_generic_numeric_sync_locals(file, functionIr, destinationSlot);
    fprintf(file, "    }\n");
}

/* 记录已写入局部缓存的数值种类；NONE 强制回到 boxed 运行时路径。 */
typedef enum EZrAotGenericNumericScalarLocalKind {
    ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_NONE,
    ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_I64,
    ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_U64,
    ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_F64
} EZrAotGenericNumericScalarLocalKind;

/* 混合类型快路径必须先确认操作数缓存已在当前指令前写入。 */
static EZrAotGenericNumericScalarLocalKind backend_aot_c_generic_numeric_written_scalar_kind(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 slot,
        TZrUInt32 execInstructionIndex) {
    if (backend_aot_c_scalar_locals_has_f64_slot(functionIr, slot) &&
        backend_aot_c_scalar_locals_f64_written_before(functionIr, slot, execInstructionIndex)) {
        return ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_F64;
    }
    if (backend_aot_c_scalar_locals_has_i64_slot(functionIr, slot) &&
        backend_aot_c_scalar_locals_i64_written_before(functionIr, slot, execInstructionIndex)) {
        return ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_I64;
    }
    if (backend_aot_c_scalar_locals_has_u64_slot(functionIr, slot) &&
        backend_aot_c_scalar_locals_u64_written_before(functionIr, slot, execInstructionIndex)) {
        return ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_U64;
    }
    return ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_NONE;
}

static TZrBool backend_aot_c_generic_numeric_scalar_kind_is_integer(
        EZrAotGenericNumericScalarLocalKind kind) {
    return (TZrBool)(kind == ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_I64 ||
                     kind == ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_U64);
}

static TZrBool backend_aot_c_generic_numeric_scalar_kinds_form_mixed_i64_u64(
        EZrAotGenericNumericScalarLocalKind leftKind,
        EZrAotGenericNumericScalarLocalKind rightKind) {
    return (TZrBool)((leftKind == ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_I64 &&
                      rightKind == ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_U64) ||
                     (leftKind == ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_U64 &&
                      rightKind == ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_I64));
}

/* 混合整数结果沿 signed 槽传播，表达式前缀映射与运行时 int64 提取规则耦合。 */
/* TODO: unsigned 高位值的 signed 转换是实现定义；需与 aot_runtime_values.c
 * 的泛型数值提取和解释器混合数值规则作极值对照。 */
static const char *backend_aot_c_generic_numeric_i64_expression_prefix(
        EZrAotGenericNumericScalarLocalKind kind) {
    switch (kind) {
        case ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_I64:
            return "zr_aot_s";
        case ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_U64:
            return "(TZrInt64)zr_aot_u";
        default:
            return ZR_NULL;
    }
}

static const char *backend_aot_c_generic_numeric_f64_expression_prefix(
        EZrAotGenericNumericScalarLocalKind kind) {
    switch (kind) {
        case ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_I64:
            return "(TZrFloat64)zr_aot_s";
        case ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_U64:
            return "(TZrFloat64)zr_aot_u";
        case ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_F64:
            return "zr_aot_f";
        default:
            return ZR_NULL;
    }
}

/* 同类 float 缓存直接计算；目标值槽可跳过是避免 boxed 状态失配的必要条件。 */
static TZrBool backend_aot_c_write_generic_numeric_f64_binary_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex,
        const char *marker,
        const char *operatorToken) {
    if (file == ZR_NULL || marker == ZR_NULL || operatorToken == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_f64_slot(functionIr, leftSlot) ||
        !backend_aot_c_scalar_locals_has_f64_slot(functionIr, rightSlot) ||
        !backend_aot_c_scalar_locals_f64_written_before(functionIr, leftSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_f64_written_before(functionIr, rightSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary */\n"
            "        /* %s */\n"
            "        zr_aot_f%u = zr_aot_f%u %s zr_aot_f%u;\n"
            "    }\n",
            marker,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            operatorToken,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

/* mixed i64/u64 使用 signed 结果槽，只有两个缓存都可证明已写入才短路运行时。 */
/* BUG: mixed ADD/SUB/MUL 最终执行 signed C 运算；转型后结果越界会触发
 * 未定义行为，需与纯 i64 路径一起补极值与 UBSan 回归。 */
static TZrBool backend_aot_c_write_generic_numeric_mixed_i64_u64_binary_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex,
        const char *marker,
        const char *operatorToken) {
    EZrAotGenericNumericScalarLocalKind leftKind;
    EZrAotGenericNumericScalarLocalKind rightKind;
    const char *leftPrefix;
    const char *rightPrefix;

    if (file == ZR_NULL || marker == ZR_NULL || operatorToken == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    leftKind = backend_aot_c_generic_numeric_written_scalar_kind(functionIr, leftSlot, execInstructionIndex);
    rightKind = backend_aot_c_generic_numeric_written_scalar_kind(functionIr, rightSlot, execInstructionIndex);
    if (!backend_aot_c_generic_numeric_scalar_kinds_form_mixed_i64_u64(leftKind, rightKind)) {
        return ZR_FALSE;
    }

    leftPrefix = backend_aot_c_generic_numeric_i64_expression_prefix(leftKind);
    rightPrefix = backend_aot_c_generic_numeric_i64_expression_prefix(rightKind);
    if (leftPrefix == ZR_NULL || rightPrefix == ZR_NULL) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary */\n"
            "        /* %s */\n"
            "        zr_aot_s%u = %s%u %s %s%u;\n"
            "    }\n",
            marker,
            (unsigned)destinationSlot,
            leftPrefix,
            (unsigned)leftSlot,
            operatorToken,
            rightPrefix,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

/* mixed 整数除/模仍需在生成代码中保留零除诊断，语义须与 runtime 退路一致。 */
/* BUG: 仅检查零除；INT64_MIN / -1 或取模仍进入 C signed 运算，触发未定义行为。 */
static TZrBool backend_aot_c_write_generic_numeric_mixed_i64_u64_guarded_binary_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex,
        const char *marker,
        const char *operatorToken,
        const char *errorMessage) {
    EZrAotGenericNumericScalarLocalKind leftKind;
    EZrAotGenericNumericScalarLocalKind rightKind;
    const char *leftPrefix;
    const char *rightPrefix;

    if (file == ZR_NULL || marker == ZR_NULL || operatorToken == ZR_NULL || errorMessage == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    leftKind = backend_aot_c_generic_numeric_written_scalar_kind(functionIr, leftSlot, execInstructionIndex);
    rightKind = backend_aot_c_generic_numeric_written_scalar_kind(functionIr, rightSlot, execInstructionIndex);
    if (!backend_aot_c_generic_numeric_scalar_kinds_form_mixed_i64_u64(leftKind, rightKind)) {
        return ZR_FALSE;
    }

    leftPrefix = backend_aot_c_generic_numeric_i64_expression_prefix(leftKind);
    rightPrefix = backend_aot_c_generic_numeric_i64_expression_prefix(rightKind);
    if (leftPrefix == ZR_NULL || rightPrefix == ZR_NULL) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary */\n"
            "        /* %s */\n"
            "        if (%s%u == (TZrInt64)0) {\n"
            "            ZrCore_Debug_RunError(state, \"%s\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_s%u = %s%u %s %s%u;\n"
            "    }\n",
            marker,
            rightPrefix,
            (unsigned)rightSlot,
            errorMessage,
            (unsigned)destinationSlot,
            leftPrefix,
            (unsigned)leftSlot,
            operatorToken,
            rightPrefix,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

static TZrBool backend_aot_c_write_generic_numeric_mixed_i64_u64_div_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex) {
    return backend_aot_c_write_generic_numeric_mixed_i64_u64_guarded_binary_scalar_local(
            file,
            functionIr,
            destinationSlot,
            leftSlot,
            rightSlot,
            execInstructionIndex,
            "zr_aot_generic_numeric_mixed_i64_u64_div_scalar_local",
            "/",
            "divide by zero");
}

static TZrBool backend_aot_c_write_generic_numeric_mixed_i64_u64_mod_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex) {
    return backend_aot_c_write_generic_numeric_mixed_i64_u64_guarded_binary_scalar_local(
            file,
            functionIr,
            destinationSlot,
            leftSlot,
            rightSlot,
            execInstructionIndex,
            "zr_aot_generic_numeric_mixed_i64_u64_mod_scalar_local",
            "%",
            "modulo by zero");
}

/* 混合 float 与整数时结果采用 float 缓存，保持与运行时数字提升方向一致。 */
static TZrBool backend_aot_c_write_generic_numeric_mixed_f64_binary_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex,
        const char *marker,
        const char *operatorToken) {
    EZrAotGenericNumericScalarLocalKind leftKind;
    EZrAotGenericNumericScalarLocalKind rightKind;
    const char *leftPrefix;
    const char *rightPrefix;

    if (file == ZR_NULL || marker == ZR_NULL || operatorToken == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    leftKind = backend_aot_c_generic_numeric_written_scalar_kind(functionIr, leftSlot, execInstructionIndex);
    rightKind = backend_aot_c_generic_numeric_written_scalar_kind(functionIr, rightSlot, execInstructionIndex);
    if (!((leftKind == ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_F64 &&
           backend_aot_c_generic_numeric_scalar_kind_is_integer(rightKind)) ||
          (rightKind == ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_F64 &&
           backend_aot_c_generic_numeric_scalar_kind_is_integer(leftKind)))) {
        return ZR_FALSE;
    }

    leftPrefix = backend_aot_c_generic_numeric_f64_expression_prefix(leftKind);
    rightPrefix = backend_aot_c_generic_numeric_f64_expression_prefix(rightKind);
    if (leftPrefix == ZR_NULL || rightPrefix == ZR_NULL) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary */\n"
            "        /* %s */\n"
            "        zr_aot_f%u = %s%u %s %s%u;\n"
            "    }\n",
            marker,
            (unsigned)destinationSlot,
            leftPrefix,
            (unsigned)leftSlot,
            operatorToken,
            rightPrefix,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

static TZrBool backend_aot_c_write_generic_numeric_mixed_f64_div_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex) {
    EZrAotGenericNumericScalarLocalKind leftKind;
    EZrAotGenericNumericScalarLocalKind rightKind;
    const char *leftPrefix;
    const char *rightPrefix;

    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    leftKind = backend_aot_c_generic_numeric_written_scalar_kind(functionIr, leftSlot, execInstructionIndex);
    rightKind = backend_aot_c_generic_numeric_written_scalar_kind(functionIr, rightSlot, execInstructionIndex);
    if (!((leftKind == ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_F64 &&
           backend_aot_c_generic_numeric_scalar_kind_is_integer(rightKind)) ||
          (rightKind == ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_F64 &&
           backend_aot_c_generic_numeric_scalar_kind_is_integer(leftKind)))) {
        return ZR_FALSE;
    }

    leftPrefix = backend_aot_c_generic_numeric_f64_expression_prefix(leftKind);
    rightPrefix = backend_aot_c_generic_numeric_f64_expression_prefix(rightKind);
    if (leftPrefix == ZR_NULL || rightPrefix == ZR_NULL) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary */\n"
            "        /* zr_aot_generic_numeric_mixed_f64_div_scalar_local */\n"
            "        if (%s%u == (TZrFloat64)0.0) {\n"
            "            ZrCore_Debug_RunError(state, \"divide by zero\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_f%u = %s%u / %s%u;\n"
            "    }\n",
            rightPrefix,
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            leftPrefix,
            (unsigned)leftSlot,
            rightPrefix,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

static TZrBool backend_aot_c_write_generic_numeric_mixed_f64_mod_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex) {
    EZrAotGenericNumericScalarLocalKind leftKind;
    EZrAotGenericNumericScalarLocalKind rightKind;
    const char *leftPrefix;
    const char *rightPrefix;

    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    leftKind = backend_aot_c_generic_numeric_written_scalar_kind(functionIr, leftSlot, execInstructionIndex);
    rightKind = backend_aot_c_generic_numeric_written_scalar_kind(functionIr, rightSlot, execInstructionIndex);
    if (!((leftKind == ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_F64 &&
           backend_aot_c_generic_numeric_scalar_kind_is_integer(rightKind)) ||
          (rightKind == ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_F64 &&
           backend_aot_c_generic_numeric_scalar_kind_is_integer(leftKind)))) {
        return ZR_FALSE;
    }

    leftPrefix = backend_aot_c_generic_numeric_f64_expression_prefix(leftKind);
    rightPrefix = backend_aot_c_generic_numeric_f64_expression_prefix(rightKind);
    if (leftPrefix == ZR_NULL || rightPrefix == ZR_NULL) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary */\n"
            "        /* zr_aot_generic_numeric_mixed_f64_mod_scalar_local */\n"
            "        if (%s%u == (TZrFloat64)0.0) {\n"
            "            ZrCore_Debug_RunError(state, \"modulo by zero\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_f%u = fmod(%s%u, %s%u);\n"
            "    }\n",
            rightPrefix,
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            leftPrefix,
            (unsigned)leftSlot,
            rightPrefix,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

/* signed 快路径绕过 runtime；结果及输入仅在静态缓存活性条件下成立。 */
/* BUG: ADD/SUB/MUL 的 signed 溢出未经检查直接进入生成 C 表达式，触发
 * 未定义行为；运行时退路也有同类问题，需统一极值语义与 UBSan 回归。 */
static TZrBool backend_aot_c_write_generic_numeric_i64_binary_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex,
        const char *marker,
        const char *operatorToken) {
    if (file == ZR_NULL || marker == ZR_NULL || operatorToken == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_i64_slot(functionIr, leftSlot) ||
        !backend_aot_c_scalar_locals_has_i64_slot(functionIr, rightSlot) ||
        !backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_i64_written_before(functionIr, rightSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary */\n"
            "        /* %s */\n"
            "        zr_aot_s%u = zr_aot_s%u %s zr_aot_s%u;\n"
            "    }\n",
            marker,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            operatorToken,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

/* unsigned 快路径可采用 C 模数算术，仍受目标值槽跳过判定约束。 */
static TZrBool backend_aot_c_write_generic_numeric_u64_binary_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex,
        const char *marker,
        const char *operatorToken) {
    if (file == ZR_NULL || marker == ZR_NULL || operatorToken == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, leftSlot) ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, rightSlot) ||
        !backend_aot_c_scalar_locals_u64_written_before(functionIr, leftSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_u64_written_before(functionIr, rightSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary */\n"
            "        /* %s */\n"
            "        zr_aot_u%u = zr_aot_u%u %s zr_aot_u%u;\n"
            "    }\n",
            marker,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            operatorToken,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

/* signed 取负保留在标量局部变量中以继续传播至下一个数值操作。 */
/* BUG: 源为 INT64_MIN 时生成 C 取负溢出；此快路径和 runtime 退路均未防护。 */
static TZrBool backend_aot_c_write_generic_numeric_i64_neg_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot,
        TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_i64_slot(functionIr, sourceSlot) ||
        !backend_aot_c_scalar_locals_i64_written_before(functionIr, sourceSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_unary */\n"
            "        /* zr_aot_generic_numeric_i64_neg_scalar_local */\n"
            "        zr_aot_s%u = -zr_aot_s%u;\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    return ZR_TRUE;
}

/* unsigned 负号的目标改用 signed 缓存，匹配 runtime 的结果类型。 */
/* TODO: UINT64 高位值先强转 signed 再取负；需确认语言期望的边界结果，
 * 并与解释器及 runtime 的同一路径做极值对照。 */
static TZrBool backend_aot_c_write_generic_numeric_u64_neg_to_i64_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot,
        TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, sourceSlot) ||
        !backend_aot_c_scalar_locals_u64_written_before(functionIr, sourceSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_unary */\n"
            "        /* zr_aot_generic_numeric_u64_neg_to_i64_scalar_local */\n"
            "        zr_aot_s%u = -(TZrInt64)zr_aot_u%u;\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    return ZR_TRUE;
}

/* signed 除法快路径在生成代码中负责零除诊断。 */
/* BUG: INT64_MIN / -1 通过零除检查后进入 C 除法，触发未定义行为。 */
static TZrBool backend_aot_c_write_generic_numeric_i64_div_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_i64_slot(functionIr, leftSlot) ||
        !backend_aot_c_scalar_locals_has_i64_slot(functionIr, rightSlot) ||
        !backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_i64_written_before(functionIr, rightSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary */\n"
            "        /* zr_aot_generic_numeric_i64_div_scalar_local */\n"
            "        if (zr_aot_s%u == (TZrInt64)0) {\n"
            "            ZrCore_Debug_RunError(state, \"divide by zero\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_s%u = zr_aot_s%u / zr_aot_s%u;\n"
            "    }\n",
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

/* signed 模运算与除法共享运行期值域前提。 */
/* BUG: INT64_MIN % -1 通过零除检查后进入 C 取模，触发未定义行为。 */
static TZrBool backend_aot_c_write_generic_numeric_i64_mod_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_i64_slot(functionIr, leftSlot) ||
        !backend_aot_c_scalar_locals_has_i64_slot(functionIr, rightSlot) ||
        !backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_i64_written_before(functionIr, rightSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary */\n"
            "        /* zr_aot_generic_numeric_i64_mod_scalar_local */\n"
            "        if (zr_aot_s%u == (TZrInt64)0) {\n"
            "            ZrCore_Debug_RunError(state, \"modulo by zero\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_s%u = zr_aot_s%u %% zr_aot_s%u;\n"
            "    }\n",
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

static TZrBool backend_aot_c_write_generic_numeric_u64_div_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, leftSlot) ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, rightSlot) ||
        !backend_aot_c_scalar_locals_u64_written_before(functionIr, leftSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_u64_written_before(functionIr, rightSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary */\n"
            "        /* zr_aot_generic_numeric_u64_div_scalar_local */\n"
            "        if (zr_aot_u%u == (TZrUInt64)0u) {\n"
            "            ZrCore_Debug_RunError(state, \"divide by zero\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_u%u = zr_aot_u%u / zr_aot_u%u;\n"
            "    }\n",
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

static TZrBool backend_aot_c_write_generic_numeric_u64_mod_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, leftSlot) ||
        !backend_aot_c_scalar_locals_has_u64_slot(functionIr, rightSlot) ||
        !backend_aot_c_scalar_locals_u64_written_before(functionIr, leftSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_u64_written_before(functionIr, rightSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary */\n"
            "        /* zr_aot_generic_numeric_u64_mod_scalar_local */\n"
            "        if (zr_aot_u%u == (TZrUInt64)0u) {\n"
            "            ZrCore_Debug_RunError(state, \"modulo by zero\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_u%u = zr_aot_u%u %% zr_aot_u%u;\n"
            "    }\n",
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

static TZrBool backend_aot_c_write_generic_numeric_f64_mod_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_f64_slot(functionIr, leftSlot) ||
        !backend_aot_c_scalar_locals_has_f64_slot(functionIr, rightSlot) ||
        !backend_aot_c_scalar_locals_f64_written_before(functionIr, leftSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_f64_written_before(functionIr, rightSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary */\n"
            "        /* zr_aot_generic_numeric_f64_mod_scalar_local */\n"
            "        if (zr_aot_f%u == (TZrFloat64)0.0) {\n"
            "            ZrCore_Debug_RunError(state, \"modulo by zero\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_f%u = fmod(zr_aot_f%u, zr_aot_f%u);\n"
            "    }\n",
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

static TZrBool backend_aot_c_write_generic_numeric_f64_div_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_f64_slot(functionIr, leftSlot) ||
        !backend_aot_c_scalar_locals_has_f64_slot(functionIr, rightSlot) ||
        !backend_aot_c_scalar_locals_f64_written_before(functionIr, leftSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_f64_written_before(functionIr, rightSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary */\n"
            "        /* zr_aot_generic_numeric_f64_div_scalar_local */\n"
            "        if (zr_aot_f%u == (TZrFloat64)0.0) {\n"
            "            ZrCore_Debug_RunError(state, \"divide by zero\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_f%u = zr_aot_f%u / zr_aot_f%u;\n"
            "    }\n",
            (unsigned)rightSlot,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    return ZR_TRUE;
}

static TZrBool backend_aot_c_write_generic_numeric_f64_neg_scalar_local(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot,
        TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot) ||
        !backend_aot_c_scalar_locals_has_f64_slot(functionIr, sourceSlot) ||
        !backend_aot_c_scalar_locals_f64_written_before(functionIr, sourceSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_unary */\n"
            "        /* zr_aot_generic_numeric_f64_neg_scalar_local */\n"
            "        zr_aot_f%u = -zr_aot_f%u;\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    return ZR_TRUE;
}

/* ADD 与 quickened ADD_STRING 共用此分派；数值快路径只适用于数值操作数。 */
/* BUG: ADD_STRING 的字符串值在这里落到 GenericNumericAdd，而 runtime 要求
 * 两侧均为 number 并报错；解释器会拼接字符串，模板字符串编译器会发出此 opcode。 */
/* BUG: 泛型 ADD 的对象元方法也无法经数值退路执行；解释器有 ZR_META_ADD
 * 分派，AOT 会以 unsupported generic numeric arithmetic 失败。 */
void backend_aot_write_c_direct_add(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot,
                                    TZrUInt32 execInstructionIndex) {
    if (backend_aot_c_write_generic_numeric_i64_binary_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_numeric_i64_add_scalar_local",
                "+")) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_u64_binary_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_numeric_u64_add_scalar_local",
                "+")) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_mixed_i64_u64_binary_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_numeric_mixed_i64_u64_add_scalar_local",
                "+")) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_f64_binary_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_numeric_f64_add_scalar_local",
                "+")) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_mixed_f64_binary_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_numeric_mixed_f64_add_scalar_local",
                "+")) {
        return;
    }

    backend_aot_write_c_generic_numeric_binary_boundary(file,
                                                        functionIr,
                                                        "ZrLibrary_AotRuntime_GenericNumericAdd",
                                                        destinationSlot,
                                                        leftSlot,
                                                        rightSlot);
}

/* SUB 对已知数值选择标量局部路径，未知类型由运行时边界判断。 */
/* BUG: 对象的 ZR_META_SUB 在解释器可调用，此退路只接受 number，AOT 报错。 */
void backend_aot_write_c_direct_sub(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot,
                                    TZrUInt32 execInstructionIndex) {
    if (backend_aot_c_write_generic_numeric_i64_binary_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_numeric_i64_sub_scalar_local",
                "-")) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_u64_binary_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_numeric_u64_sub_scalar_local",
                "-")) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_mixed_i64_u64_binary_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_numeric_mixed_i64_u64_sub_scalar_local",
                "-")) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_f64_binary_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_numeric_f64_sub_scalar_local",
                "-")) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_mixed_f64_binary_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_numeric_mixed_f64_sub_scalar_local",
                "-")) {
        return;
    }

    backend_aot_write_c_generic_numeric_binary_boundary(file,
                                                        functionIr,
                                                        "ZrLibrary_AotRuntime_GenericNumericSub",
                                                        destinationSlot,
                                                        leftSlot,
                                                        rightSlot);
}

/* MUL 与 ADD/SUB 共用值槽和 scalar-local 活性前提。 */
/* BUG: 对象的 ZR_META_MUL 在解释器可调用，此退路只接受 number，AOT 报错。 */
void backend_aot_write_c_direct_mul(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot,
                                    TZrUInt32 execInstructionIndex) {
    if (backend_aot_c_write_generic_numeric_i64_binary_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_numeric_i64_mul_scalar_local",
                "*")) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_u64_binary_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_numeric_u64_mul_scalar_local",
                "*")) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_mixed_i64_u64_binary_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_numeric_mixed_i64_u64_mul_scalar_local",
                "*")) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_f64_binary_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_numeric_f64_mul_scalar_local",
                "*")) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_mixed_f64_binary_scalar_local(
                file,
                functionIr,
                destinationSlot,
                leftSlot,
                rightSlot,
                execInstructionIndex,
                "zr_aot_generic_numeric_mixed_f64_mul_scalar_local",
                "*")) {
        return;
    }

    backend_aot_write_c_generic_numeric_binary_boundary(file,
                                                        functionIr,
                                                        "ZrLibrary_AotRuntime_GenericNumericMul",
                                                        destinationSlot,
                                                        leftSlot,
                                                        rightSlot);
}

/* DIV 将零除检查留在选中的快路径或 runtime，避免无诊断的生成 C 计算。 */
/* BUG: 对象的 ZR_META_DIV 在解释器可调用，此退路只接受 number，AOT 报错。 */
void backend_aot_write_c_direct_div(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot,
                                    TZrUInt32 execInstructionIndex) {
    if (backend_aot_c_write_generic_numeric_i64_div_scalar_local(file,
                                                                functionIr,
                                                                destinationSlot,
                                                                leftSlot,
                                                                 rightSlot,
                                                                 execInstructionIndex)) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_u64_div_scalar_local(file,
                                                                functionIr,
                                                                destinationSlot,
                                                                leftSlot,
                                                                rightSlot,
                                                                execInstructionIndex)) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_mixed_i64_u64_div_scalar_local(file,
                                                                           functionIr,
                                                                           destinationSlot,
                                                                           leftSlot,
                                                                           rightSlot,
                                                                           execInstructionIndex)) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_f64_div_scalar_local(file,
                                                                functionIr,
                                                                destinationSlot,
                                                                leftSlot,
                                                                 rightSlot,
                                                                 execInstructionIndex)) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_mixed_f64_div_scalar_local(file,
                                                                       functionIr,
                                                                       destinationSlot,
                                                                       leftSlot,
                                                                       rightSlot,
                                                                       execInstructionIndex)) {
        return;
    }

    backend_aot_write_c_generic_numeric_binary_boundary(file,
                                                        functionIr,
                                                        "ZrLibrary_AotRuntime_GenericNumericDiv",
                                                        destinationSlot,
                                                        leftSlot,
                                                        rightSlot);
}

/* MOD 保留整数与浮点两类标量路径，其他值经 runtime 再做动态类型判定。 */
/* BUG: 对象的 ZR_META_MOD 在解释器可调用，此退路只接受 number，AOT 报错。 */
void backend_aot_write_c_direct_mod(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot,
                                    TZrUInt32 execInstructionIndex) {
    if (backend_aot_c_write_generic_numeric_i64_mod_scalar_local(file,
                                                                functionIr,
                                                                destinationSlot,
                                                                leftSlot,
                                                                 rightSlot,
                                                                 execInstructionIndex)) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_u64_mod_scalar_local(file,
                                                                functionIr,
                                                                destinationSlot,
                                                                leftSlot,
                                                                rightSlot,
                                                                execInstructionIndex)) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_mixed_i64_u64_mod_scalar_local(file,
                                                                           functionIr,
                                                                           destinationSlot,
                                                                           leftSlot,
                                                                           rightSlot,
                                                                           execInstructionIndex)) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_f64_mod_scalar_local(file,
                                                                functionIr,
                                                                destinationSlot,
                                                                leftSlot,
                                                                 rightSlot,
                                                                 execInstructionIndex)) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_mixed_f64_mod_scalar_local(file,
                                                                       functionIr,
                                                                       destinationSlot,
                                                                       leftSlot,
                                                                       rightSlot,
                                                                       execInstructionIndex)) {
        return;
    }

    backend_aot_write_c_generic_numeric_binary_boundary(file,
                                                        functionIr,
                                                        "ZrLibrary_AotRuntime_GenericNumericMod",
                                                        destinationSlot,
                                                        leftSlot,
                                                        rightSlot);
}

/* NEG 的输出种类由输入动态决定，静态可判时才选 i64/f64 缓存。 */
/* BUG: 对象的 ZR_META_NEG 在解释器可调用，此退路只接受数值，AOT 报错。 */
void backend_aot_write_c_direct_neg(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 sourceSlot,
                                    TZrUInt32 execInstructionIndex) {
    if (backend_aot_c_write_generic_numeric_i64_neg_scalar_local(file,
                                                                 functionIr,
                                                                 destinationSlot,
                                                                 sourceSlot,
                                                                 execInstructionIndex)) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_u64_neg_to_i64_scalar_local(file,
                                                                        functionIr,
                                                                        destinationSlot,
                                                                        sourceSlot,
                                                                        execInstructionIndex)) {
        return;
    }

    if (backend_aot_c_write_generic_numeric_f64_neg_scalar_local(file,
                                                                 functionIr,
                                                                 destinationSlot,
                                                                 sourceSlot,
                                                                 execInstructionIndex)) {
        return;
    }

    backend_aot_write_c_generic_numeric_unary_boundary(file,
                                                       functionIr,
                                                       "ZrLibrary_AotRuntime_GenericNumericNeg",
                                                       destinationSlot,
                                                       sourceSlot);
}
