#include "backend_aot_c_emitter.h"

#include "zr_vm_core/closure.h"

static void backend_aot_write_c_direct_ownership_core_call(FILE *file,
                                                           const char *operationExpression,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 sourceSlot) {
    if (file == ZR_NULL || operationExpression == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_ownership_core */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_source = ZR_NULL;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            %u >= frame.generatedFrameSlotCount || %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!%s) {\n"
            "            ZrCore_Value_ResetAsNull(zr_aot_destination);\n"
            "        }\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            operationExpression);
}

void backend_aot_write_c_direct_own_unique(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_core_call(
            file,
            "ZrCore_Ownership_UniqueValue(state, zr_aot_destination, zr_aot_source)",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_borrow(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_core_call(
            file,
            "ZrCore_Ownership_BorrowValue(state, zr_aot_destination, zr_aot_source)",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_loan(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_core_call(
            file,
            "ZrCore_Ownership_LoanValue(state, zr_aot_destination, zr_aot_source)",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_return_loan(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_core_call(
            file,
            "ZrCore_Ownership_ReturnLoanValue(state, zr_aot_destination, zr_aot_source)",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_share(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_core_call(
            file,
            "ZrCore_Ownership_ShareValue(state, zr_aot_destination, zr_aot_source)",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_weak(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_core_call(
            file,
            "ZrCore_Ownership_WeakValue(state, zr_aot_destination, zr_aot_source)",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_detach(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_core_call(
            file,
            "ZrCore_Ownership_DetachValue(state, zr_aot_destination, zr_aot_source)",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_upgrade(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_core_call(
            file,
            "ZrCore_Ownership_UpgradeValue(state, zr_aot_destination, zr_aot_source)",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_release(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_ownership_release */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_source = ZR_NULL;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            %u >= frame.generatedFrameSlotCount || %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Ownership_ReleaseValue(state, zr_aot_source);\n"
            "        ZrCore_Value_ResetAsNull(zr_aot_destination);\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_unsupported_meta_value_access(FILE *file,
                                                       const char *opcodeName,
                                                       TZrUInt32 primarySlot,
                                                       TZrUInt32 secondarySlot,
                                                       TZrUInt32 memberOrCacheIndex) {
    const char *safeOpcodeName;

    if (file == ZR_NULL) {
        return;
    }

    safeOpcodeName = opcodeName != ZR_NULL ? opcodeName : "META_VALUE_ACCESS";
    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_unsupported_meta_value_access */\n"
            "        const char *zr_aot_opcode_name = \"%s\";\n"
            "        SZrTypeValue *zr_aot_primary = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrTypeValue *zr_aot_secondary = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const TZrUInt32 zr_aot_member_or_cache_index = %u;\n"
            "        (void)zr_aot_primary;\n"
            "        (void)zr_aot_secondary;\n"
            "        (void)zr_aot_member_or_cache_index;\n"
            "        ZrCore_Debug_RunError(state, \"unsupported AOT meta value access: %%s\", zr_aot_opcode_name);\n"
            "        ZR_AOT_C_FAIL();\n"
            "    } while (0);\n",
            safeOpcodeName,
            (unsigned)primarySlot,
            (unsigned)secondarySlot,
            (unsigned)memberOrCacheIndex);
}

void backend_aot_write_c_unsupported_dynamic_value_access(FILE *file,
                                                          const char *opcodeName,
                                                          TZrUInt32 primarySlot,
                                                          TZrUInt32 secondarySlot,
                                                          TZrUInt32 operandIndex) {
    const char *safeOpcodeName;

    if (file == ZR_NULL) {
        return;
    }

    safeOpcodeName = opcodeName != ZR_NULL ? opcodeName : "DYNAMIC_VALUE_ACCESS";
    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_unsupported_dynamic_value_access */\n"
            "        const char *zr_aot_opcode_name = \"%s\";\n"
            "        SZrTypeValue *zr_aot_primary = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrTypeValue *zr_aot_secondary = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const TZrUInt32 zr_aot_operand_index = %u;\n"
            "        (void)zr_aot_primary;\n"
            "        (void)zr_aot_secondary;\n"
            "        (void)zr_aot_operand_index;\n"
            "        ZrCore_Debug_RunError(state, \"unsupported AOT dynamic value access: %%s\", zr_aot_opcode_name);\n"
            "        ZR_AOT_C_FAIL();\n"
            "    } while (0);\n",
            safeOpcodeName,
            (unsigned)primarySlot,
            (unsigned)secondarySlot,
            (unsigned)operandIndex);
}

void backend_aot_write_c_direct_to_string(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_exec_to_string */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_source = ZR_NULL;\n"
            "        SZrString *zr_aot_result_string = ZR_NULL;\n"
            "        SZrCallInfo *zr_aot_call_info = ZR_NULL;\n"
            "        if (state == ZR_NULL || frame.slotBase == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_source == ZR_NULL || zr_aot_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_result_string = ZrCore_Value_ConvertToString(state, zr_aot_source);\n"
            "        zr_aot_call_info = frame.callInfo != ZR_NULL ? frame.callInfo : state->callInfoList;\n"
            "        if (zr_aot_call_info != ZR_NULL && zr_aot_call_info->functionBase.valuePointer != ZR_NULL) {\n"
            "            frame.callInfo = zr_aot_call_info;\n"
            "            frame.slotBase = zr_aot_call_info->functionBase.valuePointer + 1;\n"
            "            state->callInfoList = zr_aot_call_info;\n"
            "            state->stackTop.valuePointer = zr_aot_call_info->functionTop.valuePointer;\n"
            "        }\n"
            "        zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_result_string != ZR_NULL) {\n"
            "            ZrCore_Value_InitAsRawObject(state, zr_aot_destination, ZR_CAST_RAW_OBJECT_AS_SUPER(zr_aot_result_string));\n"
            "            zr_aot_destination->type = ZR_VALUE_TYPE_STRING;\n"
            "        } else {\n"
            "            ZrCore_Value_ResetAsNull(zr_aot_destination);\n"
            "        }\n"
            "    }\n",
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)destinationSlot);
}

const SZrTypeValue *backend_aot_c_get_constant_value(const SZrFunction *function, TZrInt32 constantIndex) {
    if (function == ZR_NULL || constantIndex < 0 || (TZrUInt32)constantIndex >= function->constantValueLength ||
        function->constantValueList == ZR_NULL) {
        return ZR_NULL;
    }

    return &function->constantValueList[(TZrUInt32)constantIndex];
}

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

TZrBool backend_aot_c_constant_can_emit_immediate(const SZrFunction *function, TZrInt32 constantIndex) {
    const SZrTypeValue *constantValue = backend_aot_c_get_constant_value(function, constantIndex);

    if (constantValue == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZR_VALUE_IS_TYPE_NULL(constantValue->type) || ZR_VALUE_IS_TYPE_BOOL(constantValue->type) ||
           ZR_VALUE_IS_TYPE_INT(constantValue->type) || ZR_VALUE_IS_TYPE_FLOAT(constantValue->type);
}

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

static void backend_aot_c_write_direct_null_value(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    backend_aot_c_write_direct_plain_value_replace_guard(file);
    fprintf(file, "        ZrCore_Value_ResetAsNull(zr_aot_destination);\n");
}

void backend_aot_write_c_direct_primitive_constant(FILE *file,
                                                   TZrUInt32 destinationSlot,
                                                   const SZrTypeValue *constantValue) {
    if (file == ZR_NULL || constantValue == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_exec_primitive_constant */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n",
            (unsigned)destinationSlot);
    fprintf(file,
            "        if (zr_aot_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n");

    if (ZR_VALUE_IS_TYPE_NULL(constantValue->type)) {
        backend_aot_c_write_direct_null_value(file);
    } else if (ZR_VALUE_IS_TYPE_BOOL(constantValue->type)) {
        backend_aot_c_write_direct_plain_value_replace_guard(file);
        fprintf(file,
                "        ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, %s, ZR_VALUE_TYPE_BOOL);\n",
                constantValue->value.nativeObject.nativeBool ? "ZR_TRUE" : "ZR_FALSE");
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        backend_aot_c_write_direct_plain_value_replace_guard(file);
        fprintf(file,
                "        ZR_VALUE_FAST_SET(zr_aot_destination, nativeInt64, (TZrInt64)%lld, %s);\n",
                (long long)constantValue->value.nativeObject.nativeInt64,
                backend_aot_c_value_type_literal(constantValue->type));
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
        backend_aot_c_write_direct_plain_value_replace_guard(file);
        fprintf(file,
                "        ZR_VALUE_FAST_SET(zr_aot_destination, nativeUInt64, (TZrUInt64)%llu, %s);\n",
                (unsigned long long)constantValue->value.nativeObject.nativeUInt64,
                backend_aot_c_value_type_literal(constantValue->type));
    } else if (ZR_VALUE_IS_TYPE_FLOAT(constantValue->type)) {
        backend_aot_c_write_direct_plain_value_replace_guard(file);
        fprintf(file,
                "        ZR_VALUE_FAST_SET(zr_aot_destination, nativeDouble, (TZrFloat64)%.17g, %s);\n",
                constantValue->value.nativeObject.nativeDouble,
                backend_aot_c_value_type_literal(constantValue->type));
    } else {
        fprintf(file,
                "        ZrCore_Debug_RunError(state, \"unsupported primitive AOT constant\");\n"
                "        ZR_AOT_C_FAIL();\n");
    }

    fprintf(file, "    }\n");
}

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

void backend_aot_write_c_direct_get_sub_function(FILE *file,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 childFunctionIndex,
                                                 TZrUInt32 callableFlatIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_exec_get_sub_function_native_closure */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrFunction *zr_aot_metadata_function = %u < frame.function->childFunctionLength\n"
            "                                             ? &frame.function->childFunctionList[%u]\n"
            "                                             : ZR_NULL;\n"
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
            (unsigned)destinationSlot,
            (unsigned)childFunctionIndex,
            (unsigned)childFunctionIndex,
            (unsigned)callableFlatIndex);
}

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

void backend_aot_write_c_direct_stack_copy(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_direct_stack_copy */\n"
            "        const SZrFunctionFrameSlotLayout *zr_aot_destination_layout =\n"
            "                ZrCore_Function_FindFrameSlotLayout(frame.function, %u);\n"
            "        const SZrFunctionFrameSlotLayout *zr_aot_source_layout =\n"
            "                ZrCore_Function_FindFrameSlotLayout(frame.function, %u);\n"
            "        const SZrTypeValue *zr_aot_dense_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = zr_aot_dense_source;\n"
            "        SZrTypeValue *zr_aot_dense_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrStackFramePlace zr_aot_source_place;\n"
            "        SZrStackFramePlace zr_aot_destination_place;\n"
            "        if (frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            zr_aot_dense_source == ZR_NULL || zr_aot_dense_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_source_layout != ZR_NULL &&\n"
            "            zr_aot_source_layout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE &&\n"
            "            zr_aot_source_layout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue)) {\n"
            "            if (!ZrCore_Function_MakeFrameSlotPlace(\n"
            "                    state, frame.function, frame.slotBase, %u, &zr_aot_source_place)) {\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n"
            "            zr_aot_source = (const SZrTypeValue *)zr_aot_source_place.address;\n"
            "            if (ZR_VALUE_IS_TYPE_NULL(zr_aot_source->type) &&\n"
            "                !ZR_VALUE_IS_TYPE_NULL(zr_aot_dense_source->type)) {\n"
            "                zr_aot_source = zr_aot_dense_source;\n"
            "            }\n"
            "        }\n"
            "        if (zr_aot_destination_layout != ZR_NULL &&\n"
            "            zr_aot_destination_layout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {\n"
            "            ZR_AOT_C_GUARD(ZrCore_Function_CopyObjectValueToFrameSlotInline(\n"
            "                    state, frame.function, frame.slotBase, %u, zr_aot_source));\n"
            "        } else if (zr_aot_destination_layout != ZR_NULL &&\n"
            "                   zr_aot_destination_layout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE &&\n"
            "                   zr_aot_destination_layout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue)) {\n"
            "            if (!ZrCore_Function_MakeFrameSlotPlace(\n"
            "                    state, frame.function, frame.slotBase, %u, &zr_aot_destination_place)) {\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n"
            "            ZrCore_Value_Copy(state, (SZrTypeValue *)zr_aot_destination_place.address, zr_aot_source);\n"
            "            ZrCore_Value_Copy(state, zr_aot_dense_destination, zr_aot_source);\n"
            "        } else {\n"
            "            ZrCore_Value_AssignMaterializedStackValue(\n"
            "                    state, zr_aot_dense_destination, (SZrTypeValue *)zr_aot_source);\n"
            "        }\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)destinationSlot);
}

void backend_aot_write_c_get_closure_value(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 closureIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_exec_get_closure_value */\n"
            "        const TZrUInt32 zr_aot_closure_index = %u;\n"
            "        const SZrTypeValue *zr_aot_current_closure_value = ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_closure_value = ZR_NULL;\n"
            "        if (state == ZR_NULL || frame.slotBase == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_current_closure_value = ZrCore_Stack_GetValue(frame.slotBase - 1);\n"
            "        zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_current_closure_value == ZR_NULL ||\n"
            "            zr_aot_current_closure_value->type != ZR_VALUE_TYPE_CLOSURE ||\n"
            "            zr_aot_current_closure_value->value.object == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_current_closure_value->isNative) {\n"
            "            SZrClosureNative *zr_aot_native_closure =\n"
            "                    ZR_CAST_NATIVE_CLOSURE(state, zr_aot_current_closure_value->value.object);\n"
            "            if (zr_aot_native_closure == ZR_NULL ||\n"
            "                zr_aot_closure_index >= zr_aot_native_closure->closureValueCount) {\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n"
            "            zr_aot_closure_value = ZrCore_ClosureNative_GetCaptureValue(zr_aot_native_closure,\n"
            "                                                                        zr_aot_closure_index);\n"
            "        } else {\n"
            "            SZrClosure *zr_aot_vm_closure =\n"
            "                    ZR_CAST_VM_CLOSURE(state, zr_aot_current_closure_value->value.object);\n"
            "            SZrClosureValue *zr_aot_vm_closure_value = ZR_NULL;\n"
            "            if (zr_aot_vm_closure == ZR_NULL ||\n"
            "                zr_aot_closure_index >= zr_aot_vm_closure->closureValueCount) {\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n"
            "            zr_aot_vm_closure_value = zr_aot_vm_closure->closureValuesExtend[zr_aot_closure_index];\n"
            "            if (zr_aot_vm_closure_value == ZR_NULL) {\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n"
            "            zr_aot_closure_value = ZrCore_ClosureValue_GetValue(zr_aot_vm_closure_value);\n"
            "        }\n"
            "        if (zr_aot_closure_value == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Value_Copy(state, zr_aot_destination, zr_aot_closure_value);\n"
            "    }\n",
            (unsigned)closureIndex,
            (unsigned)destinationSlot);
}

void backend_aot_write_c_set_closure_value(FILE *file, TZrUInt32 sourceSlot, TZrUInt32 closureIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_exec_set_closure_value */\n"
            "        const TZrUInt32 zr_aot_closure_index = %u;\n"
            "        const SZrTypeValue *zr_aot_current_closure_value = ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_source = ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_closure_value = ZR_NULL;\n"
            "        SZrRawObject *zr_aot_barrier_object = ZR_NULL;\n"
            "        if (state == ZR_NULL || frame.slotBase == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_current_closure_value = ZrCore_Stack_GetValue(frame.slotBase - 1);\n"
            "        zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_source == ZR_NULL || zr_aot_current_closure_value == ZR_NULL ||\n"
            "            zr_aot_current_closure_value->type != ZR_VALUE_TYPE_CLOSURE ||\n"
            "            zr_aot_current_closure_value->value.object == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_current_closure_value->isNative) {\n"
            "            SZrClosureNative *zr_aot_native_closure =\n"
            "                    ZR_CAST_NATIVE_CLOSURE(state, zr_aot_current_closure_value->value.object);\n"
            "            if (zr_aot_native_closure == ZR_NULL ||\n"
            "                zr_aot_closure_index >= zr_aot_native_closure->closureValueCount) {\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n"
            "            zr_aot_closure_value = ZrCore_ClosureNative_GetCaptureValue(zr_aot_native_closure,\n"
            "                                                                        zr_aot_closure_index);\n"
            "            zr_aot_barrier_object = ZrCore_ClosureNative_GetCaptureOwner(zr_aot_native_closure,\n"
            "                                                                        zr_aot_closure_index);\n"
            "            if (zr_aot_barrier_object == ZR_NULL) {\n"
            "                zr_aot_barrier_object = ZR_CAST_RAW_OBJECT_AS_SUPER(zr_aot_native_closure);\n"
            "            }\n"
            "        } else {\n"
            "            SZrClosure *zr_aot_vm_closure =\n"
            "                    ZR_CAST_VM_CLOSURE(state, zr_aot_current_closure_value->value.object);\n"
            "            SZrClosureValue *zr_aot_vm_closure_value = ZR_NULL;\n"
            "            if (zr_aot_vm_closure == ZR_NULL ||\n"
            "                zr_aot_closure_index >= zr_aot_vm_closure->closureValueCount) {\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n"
            "            zr_aot_vm_closure_value = zr_aot_vm_closure->closureValuesExtend[zr_aot_closure_index];\n"
            "            if (zr_aot_vm_closure_value == ZR_NULL) {\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n"
            "            zr_aot_closure_value = ZrCore_ClosureValue_GetValue(zr_aot_vm_closure_value);\n"
            "            zr_aot_barrier_object = ZR_CAST_RAW_OBJECT_AS_SUPER(zr_aot_vm_closure_value);\n"
            "        }\n"
            "        if (zr_aot_closure_value == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Value_Copy(state, zr_aot_closure_value, zr_aot_source);\n"
            "        if (zr_aot_barrier_object != ZR_NULL) {\n"
            "            ZrCore_Value_Barrier(state, zr_aot_barrier_object, zr_aot_source);\n"
            "        }\n"
            "    }\n",
            (unsigned)closureIndex,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_direct_reset_stack_null(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_reset_stack_null */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Value_ResetAsNull(zr_aot_destination);\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)destinationSlot);
}

void backend_aot_write_c_direct_reset_stack_null2(FILE *file, TZrUInt32 firstSlot, TZrUInt32 secondSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_reset_stack_null2 */\n"
            "        SZrTypeValue *zr_aot_first = ZR_NULL;\n"
            "        SZrTypeValue *zr_aot_second = ZR_NULL;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_first = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        zr_aot_second = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_first == ZR_NULL || zr_aot_second == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Value_ResetAsNull(zr_aot_first);\n"
            "        ZrCore_Value_ResetAsNull(zr_aot_second);\n"
            "    } while (0);\n",
            (unsigned)firstSlot,
            (unsigned)secondSlot,
            (unsigned)firstSlot,
            (unsigned)secondSlot);
}

void backend_aot_write_c_direct_get_global(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_global_object =\n"
            "                (state != ZR_NULL && state->global != ZR_NULL &&\n"
            "                 state->global->zrObject.type == ZR_VALUE_TYPE_OBJECT)\n"
            "                        ? &state->global->zrObject\n"
            "                        : ZR_NULL;\n"
            "        if (zr_aot_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_global_object != ZR_NULL) {\n"
            "            ZrCore_Value_Copy(state, zr_aot_destination, &state->global->zrObject);\n"
            "        } else {\n"
            "            ZrCore_Value_ResetAsNull(zr_aot_destination);\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot);
}

void backend_aot_write_c_direct_create_object(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrObject *zr_aot_object = ZrCore_Object_New(state, ZR_NULL);\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
            "        if (zr_aot_object != ZR_NULL) {\n"
            "            ZrCore_Object_Init(state, zr_aot_object);\n"
            "            ZrCore_Value_InitAsRawObject(state,\n"
            "                                         zr_aot_destination,\n"
            "                                         ZR_CAST_RAW_OBJECT_AS_SUPER(zr_aot_object));\n"
            "        } else {\n"
            "            ZrCore_Value_ResetAsNull(zr_aot_destination);\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot);
}

void backend_aot_write_c_direct_create_array(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrObject *zr_aot_array = ZrCore_Object_NewCustomized(state,\n"
            "                                                             sizeof(SZrObject),\n"
            "                                                             ZR_OBJECT_INTERNAL_TYPE_ARRAY);\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
            "        if (zr_aot_array != ZR_NULL) {\n"
            "            ZrCore_Object_Init(state, zr_aot_array);\n"
            "            ZrCore_Value_InitAsRawObject(state,\n"
            "                                         zr_aot_destination,\n"
            "                                         ZR_CAST_RAW_OBJECT_AS_SUPER(zr_aot_array));\n"
            "            zr_aot_destination->type = ZR_VALUE_TYPE_ARRAY;\n"
            "        } else {\n"
            "            ZrCore_Value_ResetAsNull(zr_aot_destination);\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot);
}

void backend_aot_write_c_direct_typeof(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_exec_typeof */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL ||\n"
            "            !ZrCore_Reflection_TypeOfValue(state, zr_aot_source, zr_aot_destination)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_direct_to_object(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 sourceSlot,
                                          TZrUInt32 typeNameConstantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_exec_to_object */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_type_name = %u < frame.function->constantValueLength\n"
            "                                             ? &frame.function->constantValueList[%u]\n"
            "                                             : ZR_NULL;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL || zr_aot_type_name == ZR_NULL ||\n"
            "            !ZrCore_Execution_ToObject(state,\n"
            "                                      frame.callInfo,\n"
            "                                      zr_aot_destination,\n"
            "                                      zr_aot_source,\n"
            "                                      zr_aot_type_name)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)typeNameConstantIndex,
            (unsigned)typeNameConstantIndex);
}

void backend_aot_write_c_direct_to_struct(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 sourceSlot,
                                          TZrUInt32 typeNameConstantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_value_exec_to_struct */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_type_name = %u < frame.function->constantValueLength\n"
            "                                             ? &frame.function->constantValueList[%u]\n"
            "                                             : ZR_NULL;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL || zr_aot_type_name == ZR_NULL ||\n"
            "            !ZrCore_Execution_ToStruct(state,\n"
            "                                      frame.callInfo,\n"
            "                                      zr_aot_destination,\n"
            "                                      zr_aot_source,\n"
            "                                      zr_aot_type_name)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)typeNameConstantIndex,
            (unsigned)typeNameConstantIndex);
}
