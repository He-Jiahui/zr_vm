#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"
#include "backend_aot_internal.h"

/* backend_aot_c_lowering_generic_logical.c */

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

static TZrBool backend_aot_c_generic_logical_not_destination_is_next_bool_branch(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 execInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *nextInstruction;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (function->instructionsList == ZR_NULL ||
        execInstructionIndex + 1u >= function->instructionsLength) {
        return ZR_FALSE;
    }

    nextInstruction = &function->instructionsList[execInstructionIndex + 1u];
    return (TZrBool)(nextInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE) &&
                     nextInstruction->instruction.operandExtra == destinationSlot);
}

static TZrBool backend_aot_c_write_generic_logical_not_scalar_local(FILE *file,
                                                                    const SZrAotExecIrFunction *functionIr,
                                                                    TZrUInt32 destinationSlot,
                                                                    TZrUInt32 sourceSlot,
                                                                    TZrUInt32 execInstructionIndex) {
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_generic_logical_not_destination_is_next_bool_branch(
                functionIr, destinationSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, execInstructionIndex) ||
        !backend_aot_c_scalar_locals_bool_value_written_before(functionIr, sourceSlot, execInstructionIndex)) {
        return ZR_FALSE;
    }

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

void backend_aot_write_c_direct_jump_if(FILE *file,
                                        const SZrAotExecIrFunction *functionIr,
                                        TZrUInt32 functionIndex,
                                        TZrUInt32 conditionSlot,
                                        TZrUInt32 execInstructionIndex,
                                        TZrUInt32 targetInstructionIndex,
                                        TZrBool isBackEdge) {
    if (file == ZR_NULL) {
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
