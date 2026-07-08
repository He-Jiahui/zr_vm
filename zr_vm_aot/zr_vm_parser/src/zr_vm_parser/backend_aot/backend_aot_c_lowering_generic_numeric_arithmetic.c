#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"

/* backend_aot_c_lowering_generic_numeric_arithmetic.c */

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

typedef enum EZrAotGenericNumericScalarLocalKind {
    ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_NONE,
    ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_I64,
    ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_U64,
    ZR_AOT_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_F64
} EZrAotGenericNumericScalarLocalKind;

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
