#include "backend_aot_c_emitter.h"

#include "backend_aot_c_scalar_locals.h"

#include "zr_vm_core/closure.h"

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

void backend_aot_write_c_direct_own_weak(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_helper_call(
            file,
            "ZrLibrary_AotRuntime_OwnWeak",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_detach(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_helper_call(
            file,
            "ZrLibrary_AotRuntime_OwnDetach",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_upgrade(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_helper_call(
            file,
            "ZrLibrary_AotRuntime_OwnUpgrade",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_own_release(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_ownership_helper_call(
            file,
            "ZrLibrary_AotRuntime_OwnRelease",
            destinationSlot,
            sourceSlot);
}

void backend_aot_write_c_direct_to_string(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_to_string */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToString(state, &frame, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
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

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type) &&
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
        backend_aot_c_write_direct_primitive_constant_local_mirror(
                file, functionIr, destinationSlot, constantValue, boolExpression);
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

static void backend_aot_write_c_direct_stack_copy_scalar_local_sync(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot) {
    TZrBool syncBool;
    TZrBool syncI64;
    TZrBool syncU64;
    TZrBool syncF64;

    if (file == ZR_NULL || functionIr == ZR_NULL) {
        return;
    }

    syncBool = backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot);
    syncI64 = backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot);
    syncU64 = backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot);
    syncF64 = backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot);
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

void backend_aot_write_c_direct_stack_copy(FILE *file,
                                           const SZrAotExecIrFunction *functionIr,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_value_exec_copy_stack */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CopyStack(state, &frame, %u, %u));\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    backend_aot_write_c_direct_stack_copy_scalar_local_sync(file, functionIr, destinationSlot);
    fprintf(file, "    } while (0);\n");
}

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

void backend_aot_write_c_reset_stack_null_scalar_local_skip(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    /* zr_aot_reset_stack_null_scalar_local_skip slot=%u */\n",
            (unsigned)destinationSlot);
}

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
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
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
            "    do {\n"
            "        /* zr_aot_value_exec_to_struct */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToStruct(state, &frame, %u, %u, %u));\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)typeNameConstantIndex);
}
