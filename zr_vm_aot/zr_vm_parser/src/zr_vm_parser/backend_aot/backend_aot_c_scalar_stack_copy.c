#include "backend_aot_c_scalar_stack_copy.h"
#include "backend_aot_c_scalar_locals.h"

static EZrStaticCType backend_aot_c_scalar_stack_copy_normalize_static_type(EZrStaticCType staticCType) {
    switch (staticCType) {
        case ZR_STATIC_C_TYPE_BOOL:
            return ZR_STATIC_C_TYPE_BOOL;
        case ZR_STATIC_C_TYPE_I8:
        case ZR_STATIC_C_TYPE_I16:
        case ZR_STATIC_C_TYPE_I32:
        case ZR_STATIC_C_TYPE_I64:
            return ZR_STATIC_C_TYPE_I64;
        case ZR_STATIC_C_TYPE_U8:
        case ZR_STATIC_C_TYPE_U16:
        case ZR_STATIC_C_TYPE_U32:
        case ZR_STATIC_C_TYPE_U64:
            return ZR_STATIC_C_TYPE_U64;
        case ZR_STATIC_C_TYPE_F32:
        case ZR_STATIC_C_TYPE_F64:
            return ZR_STATIC_C_TYPE_F64;
        default:
            return ZR_STATIC_C_TYPE_DYNAMIC;
    }
}

static EZrStaticCType backend_aot_c_scalar_stack_copy_static_type_from_type_ref(
        const SZrFunctionTypedTypeRef *typeRef) {
    EZrStaticCType normalizedType;

    if (typeRef == ZR_NULL) {
        return ZR_STATIC_C_TYPE_DYNAMIC;
    }

    normalizedType = backend_aot_c_scalar_stack_copy_normalize_static_type(typeRef->staticCType);
    if (normalizedType != ZR_STATIC_C_TYPE_DYNAMIC) {
        return normalizedType;
    }

    switch (typeRef->baseType) {
        case ZR_VALUE_TYPE_BOOL:
            return ZR_STATIC_C_TYPE_BOOL;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            return ZR_STATIC_C_TYPE_I64;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            return ZR_STATIC_C_TYPE_U64;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            return ZR_STATIC_C_TYPE_F64;
        default:
            return ZR_STATIC_C_TYPE_DYNAMIC;
    }
}

static EZrStaticCType backend_aot_c_scalar_stack_copy_static_type_for_slot(const SZrFunction *function,
                                                                           TZrUInt32 slot) {
    TZrUInt32 bindingIndex;

    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_STATIC_C_TYPE_DYNAMIC;
    }

    for (bindingIndex = 0u; bindingIndex < function->typedLocalBindingLength; bindingIndex++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[bindingIndex];
        EZrStaticCType staticCType;

        if (binding->stackSlot != slot) {
            continue;
        }

        staticCType = backend_aot_c_scalar_stack_copy_static_type_from_type_ref(&binding->type);
        if (staticCType != ZR_STATIC_C_TYPE_DYNAMIC) {
            return staticCType;
        }
    }

    return ZR_STATIC_C_TYPE_DYNAMIC;
}

static EZrStaticCType backend_aot_c_scalar_stack_copy_static_type_from_locals(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 slot) {
    if (backend_aot_c_scalar_locals_has_bool_slot(functionIr, slot)) {
        return ZR_STATIC_C_TYPE_BOOL;
    }
    if (backend_aot_c_scalar_locals_has_i64_slot(functionIr, slot)) {
        return ZR_STATIC_C_TYPE_I64;
    }
    if (backend_aot_c_scalar_locals_has_u64_slot(functionIr, slot)) {
        return ZR_STATIC_C_TYPE_U64;
    }
    if (backend_aot_c_scalar_locals_has_f64_slot(functionIr, slot)) {
        return ZR_STATIC_C_TYPE_F64;
    }

    return ZR_STATIC_C_TYPE_DYNAMIC;
}

static void backend_aot_c_scalar_stack_copy_write_release_destination(FILE *file) {
    fprintf(file,
            "        if (zr_aot_destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||\n"
            "            zr_aot_destination->isGarbageCollectable) {\n"
            "            ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
            "        }\n");
}

static void backend_aot_c_scalar_stack_copy_write_plain_tail(FILE *file) {
    fprintf(file,
            "        zr_aot_destination->isGarbageCollectable = ZR_FALSE;\n"
            "        zr_aot_destination->isNative = ZR_TRUE;\n"
            "        zr_aot_destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;\n"
            "        zr_aot_destination->ownershipControl = ZR_NULL;\n"
            "        zr_aot_destination->ownershipWeakRef = ZR_NULL;\n"
            "    }\n");
}

static TZrBool backend_aot_c_scalar_stack_copy_source_local_written_before(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 execInstructionIndex,
        EZrStaticCType staticCType) {
    switch (staticCType) {
        case ZR_STATIC_C_TYPE_BOOL:
            return backend_aot_c_scalar_locals_bool_written_before(functionIr, sourceSlot, execInstructionIndex);
        case ZR_STATIC_C_TYPE_I64:
            return backend_aot_c_scalar_locals_i64_written_before(functionIr, sourceSlot, execInstructionIndex);
        case ZR_STATIC_C_TYPE_U64:
            return backend_aot_c_scalar_locals_u64_written_before(functionIr, sourceSlot, execInstructionIndex);
        case ZR_STATIC_C_TYPE_F64:
            return backend_aot_c_scalar_locals_f64_written_before(functionIr, sourceSlot, execInstructionIndex);
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_stack_copy_source_slot_is_parameter(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot) {
    if (functionIr == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(sourceSlot < functionIr->frameLayout.parameterCount ||
                     (functionIr->function != ZR_NULL && sourceSlot < functionIr->function->parameterCount));
}

static TZrBool backend_aot_c_scalar_stack_copy_source_local_is_available(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 execInstructionIndex,
        EZrStaticCType staticCType) {
    switch (staticCType) {
        case ZR_STATIC_C_TYPE_BOOL:
            return (TZrBool)(backend_aot_c_scalar_locals_has_bool_slot(functionIr, sourceSlot) &&
                             backend_aot_c_scalar_stack_copy_source_local_written_before(
                                     functionIr, sourceSlot, execInstructionIndex, staticCType));
        case ZR_STATIC_C_TYPE_I64:
            return (TZrBool)(backend_aot_c_scalar_locals_has_i64_slot(functionIr, sourceSlot) &&
                             backend_aot_c_scalar_stack_copy_source_local_written_before(
                                     functionIr, sourceSlot, execInstructionIndex, staticCType));
        case ZR_STATIC_C_TYPE_U64:
            return (TZrBool)(backend_aot_c_scalar_locals_has_u64_slot(functionIr, sourceSlot) &&
                             backend_aot_c_scalar_stack_copy_source_local_written_before(
                                     functionIr, sourceSlot, execInstructionIndex, staticCType));
        case ZR_STATIC_C_TYPE_F64:
            return (TZrBool)(backend_aot_c_scalar_locals_has_f64_slot(functionIr, sourceSlot) &&
                             backend_aot_c_scalar_stack_copy_source_local_written_before(
                                     functionIr, sourceSlot, execInstructionIndex, staticCType));
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_stack_copy_destination_local_is_available(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        EZrStaticCType staticCType) {
    switch (staticCType) {
        case ZR_STATIC_C_TYPE_BOOL:
            return backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot);
        case ZR_STATIC_C_TYPE_I64:
            return backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot);
        case ZR_STATIC_C_TYPE_U64:
            return backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot);
        case ZR_STATIC_C_TYPE_F64:
            return backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot);
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_stack_copy_instruction_is_call_result_write(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_scalar_stack_copy_source_is_previous_call_result(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 execInstructionIndex) {
    const TZrInstruction *instruction;

    if (functionIr == ZR_NULL ||
        functionIr->function == ZR_NULL ||
        functionIr->function->instructionsList == ZR_NULL ||
        execInstructionIndex == 0u ||
        execInstructionIndex > functionIr->function->instructionsLength) {
        return ZR_FALSE;
    }

    instruction = &functionIr->function->instructionsList[execInstructionIndex - 1u];
    return (TZrBool)(instruction->instruction.operandExtra == sourceSlot &&
                     backend_aot_c_scalar_stack_copy_instruction_is_call_result_write(
                             (EZrInstructionCode)instruction->instruction.operationCode));
}

static TZrBool backend_aot_c_scalar_stack_copy_try_prefer_available_source_type(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot,
        TZrUInt32 execInstructionIndex,
        EZrStaticCType candidateType,
        TZrBool forceValueSlotWrite,
        EZrStaticCType *ioStaticCType,
        TZrBool *outHasSourceLocal) {
    TZrBool hasSourceLocal;

    if (ioStaticCType == ZR_NULL ||
        outHasSourceLocal == ZR_NULL) {
        return ZR_FALSE;
    }

    hasSourceLocal = backend_aot_c_scalar_stack_copy_source_local_is_available(
            functionIr, sourceSlot, execInstructionIndex, candidateType);
    if (!hasSourceLocal &&
        !(forceValueSlotWrite &&
          candidateType == ZR_STATIC_C_TYPE_BOOL &&
          backend_aot_c_scalar_locals_has_bool_slot(functionIr, sourceSlot) &&
          backend_aot_c_scalar_stack_copy_source_is_previous_call_result(
                  functionIr, sourceSlot, execInstructionIndex))) {
        return ZR_FALSE;
    }

    if (*ioStaticCType == candidateType) {
        *outHasSourceLocal = ZR_TRUE;
        return ZR_TRUE;
    }

    if (!forceValueSlotWrite &&
        !backend_aot_c_scalar_stack_copy_destination_local_is_available(
                functionIr, destinationSlot, candidateType)) {
        return ZR_FALSE;
    }

    *ioStaticCType = candidateType;
    *outHasSourceLocal = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_scalar_stack_copy_prefer_available_source_type(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot,
        TZrUInt32 execInstructionIndex,
        TZrBool forceValueSlotWrite,
        EZrStaticCType *ioStaticCType,
        TZrBool *outHasSourceLocal) {
    if (backend_aot_c_scalar_stack_copy_try_prefer_available_source_type(functionIr,
                                                                         destinationSlot,
                                                                         sourceSlot,
                                                                         execInstructionIndex,
                                                                         ZR_STATIC_C_TYPE_BOOL,
                                                                         forceValueSlotWrite,
                                                                         ioStaticCType,
                                                                         outHasSourceLocal)) {
        return ZR_TRUE;
    }
    if (backend_aot_c_scalar_stack_copy_try_prefer_available_source_type(functionIr,
                                                                         destinationSlot,
                                                                         sourceSlot,
                                                                         execInstructionIndex,
                                                                         ZR_STATIC_C_TYPE_U64,
                                                                         forceValueSlotWrite,
                                                                         ioStaticCType,
                                                                         outHasSourceLocal)) {
        return ZR_TRUE;
    }
    if (backend_aot_c_scalar_stack_copy_try_prefer_available_source_type(functionIr,
                                                                         destinationSlot,
                                                                         sourceSlot,
                                                                         execInstructionIndex,
                                                                         ZR_STATIC_C_TYPE_I64,
                                                                         forceValueSlotWrite,
                                                                         ioStaticCType,
                                                                         outHasSourceLocal)) {
        return ZR_TRUE;
    }
    return backend_aot_c_scalar_stack_copy_try_prefer_available_source_type(functionIr,
                                                                           destinationSlot,
                                                                           sourceSlot,
                                                                           execInstructionIndex,
                                                                           ZR_STATIC_C_TYPE_F64,
                                                                           forceValueSlotWrite,
                                                                           ioStaticCType,
                                                                           outHasSourceLocal);
}

static void backend_aot_write_c_scalar_stack_copy_i64(FILE *file,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 sourceSlot,
                                                       TZrBool hasSourceLocal,
                                                       TZrBool hasDestinationLocal,
                                                       TZrBool forceValueSlotWrite) {
    if (!forceValueSlotWrite && hasSourceLocal && hasDestinationLocal) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_stack_copy_i64 dstSlot=%u srcSlot=%u */\n"
                "        zr_aot_s%u = zr_aot_s%u;\n"
                "    }\n",
                (unsigned)destinationSlot,
                (unsigned)sourceSlot,
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_stack_copy_i64 dstSlot=%u srcSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        const SZrTypeValue *zr_aot_source = ZR_NULL;\n"
            "        TZrInt64 zr_aot_s_value;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n"
            "        zr_aot_source = &frame.slotBase[%u].value;\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    if (!hasSourceLocal) {
        fprintf(file,
                "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n");
    }
    if (hasSourceLocal) {
        fprintf(file, "        zr_aot_s_value = zr_aot_s%u;\n", (unsigned)sourceSlot);
    } else {
        fprintf(file, "        zr_aot_s_value = zr_aot_source->value.nativeObject.nativeInt64;\n");
    }
    backend_aot_c_scalar_stack_copy_write_release_destination(file);
    fprintf(file,
            "        zr_aot_destination->type = ZR_VALUE_TYPE_INT64;\n"
            "        zr_aot_destination->value.nativeObject.nativeInt64 = zr_aot_s_value;\n");
    if (hasDestinationLocal) {
        fprintf(file, "        zr_aot_s%u = zr_aot_s_value;\n", (unsigned)destinationSlot);
    }
    backend_aot_c_scalar_stack_copy_write_plain_tail(file);
}

static void backend_aot_write_c_scalar_stack_copy_u64(FILE *file,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 sourceSlot,
                                                       TZrBool hasSourceLocal,
                                                       TZrBool hasDestinationLocal,
                                                       TZrBool forceValueSlotWrite) {
    if (!forceValueSlotWrite && hasSourceLocal && hasDestinationLocal) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_stack_copy_u64 dstSlot=%u srcSlot=%u */\n"
                "        zr_aot_u%u = zr_aot_u%u;\n"
                "    }\n",
                (unsigned)destinationSlot,
                (unsigned)sourceSlot,
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_stack_copy_u64 dstSlot=%u srcSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        const SZrTypeValue *zr_aot_source = ZR_NULL;\n"
            "        TZrUInt64 zr_aot_u_value;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n"
            "        zr_aot_source = &frame.slotBase[%u].value;\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    if (!hasSourceLocal) {
        fprintf(file,
                "        if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n");
    }
    if (hasSourceLocal) {
        fprintf(file, "        zr_aot_u_value = zr_aot_u%u;\n", (unsigned)sourceSlot);
    } else {
        fprintf(file, "        zr_aot_u_value = zr_aot_source->value.nativeObject.nativeUInt64;\n");
    }
    backend_aot_c_scalar_stack_copy_write_release_destination(file);
    fprintf(file,
            "        zr_aot_destination->type = ZR_VALUE_TYPE_UINT64;\n"
            "        zr_aot_destination->value.nativeObject.nativeUInt64 = zr_aot_u_value;\n");
    if (hasDestinationLocal) {
        fprintf(file, "        zr_aot_u%u = zr_aot_u_value;\n", (unsigned)destinationSlot);
    }
    backend_aot_c_scalar_stack_copy_write_plain_tail(file);
}

static void backend_aot_write_c_scalar_stack_copy_f64(FILE *file,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 sourceSlot,
                                                       TZrBool hasSourceLocal,
                                                       TZrBool hasDestinationLocal,
                                                       TZrBool forceValueSlotWrite) {
    if (!forceValueSlotWrite && hasSourceLocal && hasDestinationLocal) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_stack_copy_f64 dstSlot=%u srcSlot=%u */\n"
                "        zr_aot_f%u = zr_aot_f%u;\n"
                "    }\n",
                (unsigned)destinationSlot,
                (unsigned)sourceSlot,
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_stack_copy_f64 dstSlot=%u srcSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        const SZrTypeValue *zr_aot_source = ZR_NULL;\n"
            "        TZrFloat64 zr_aot_f_value;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n"
            "        zr_aot_source = &frame.slotBase[%u].value;\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    if (!hasSourceLocal) {
        fprintf(file,
                "        if (!ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n");
    }
    if (hasSourceLocal) {
        fprintf(file, "        zr_aot_f_value = zr_aot_f%u;\n", (unsigned)sourceSlot);
    } else {
        fprintf(file, "        zr_aot_f_value = zr_aot_source->value.nativeObject.nativeDouble;\n");
    }
    backend_aot_c_scalar_stack_copy_write_release_destination(file);
    fprintf(file,
            "        zr_aot_destination->type = ZR_VALUE_TYPE_DOUBLE;\n"
            "        zr_aot_destination->value.nativeObject.nativeDouble = zr_aot_f_value;\n");
    if (hasDestinationLocal) {
        fprintf(file, "        zr_aot_f%u = zr_aot_f_value;\n", (unsigned)destinationSlot);
    }
    backend_aot_c_scalar_stack_copy_write_plain_tail(file);
}

static void backend_aot_write_c_scalar_stack_copy_bool(FILE *file,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 sourceSlot,
                                                        TZrBool hasSourceLocal,
                                                        TZrBool hasDestinationLocal,
                                                        TZrBool forceValueSlotWrite) {
    if (!forceValueSlotWrite && hasSourceLocal && hasDestinationLocal) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_scalar_stack_copy_bool dstSlot=%u srcSlot=%u */\n"
                "        zr_aot_b%u = (TZrBool)(zr_aot_b%u != 0u);\n"
                "    }\n",
                (unsigned)destinationSlot,
                (unsigned)sourceSlot,
                (unsigned)destinationSlot,
                (unsigned)sourceSlot);
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scalar_stack_copy_bool dstSlot=%u srcSlot=%u */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        const SZrTypeValue *zr_aot_source = ZR_NULL;\n"
            "        TZrBool zr_aot_b_value;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = &frame.slotBase[%u].value;\n"
            "        zr_aot_source = &frame.slotBase[%u].value;\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    if (!hasSourceLocal) {
        fprintf(file,
                "        if (!ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type)) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n");
    }
    if (hasSourceLocal) {
        fprintf(file, "        zr_aot_b_value = zr_aot_b%u;\n", (unsigned)sourceSlot);
    } else {
        fprintf(file, "        zr_aot_b_value = zr_aot_source->value.nativeObject.nativeBool;\n");
    }
    backend_aot_c_scalar_stack_copy_write_release_destination(file);
    fprintf(file,
            "        zr_aot_destination->type = ZR_VALUE_TYPE_BOOL;\n"
            "        zr_aot_destination->value.nativeObject.nativeBool = zr_aot_b_value;\n");
    if (hasDestinationLocal) {
        fprintf(file,
                "        zr_aot_b%u = (TZrBool)(zr_aot_b_value != 0u);\n",
                (unsigned)destinationSlot);
    }
    backend_aot_c_scalar_stack_copy_write_plain_tail(file);
}

TZrBool backend_aot_c_scalar_stack_copy_can_use_local_only(const SZrAotExecIrFunction *functionIr,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 sourceSlot,
                                                           TZrUInt32 execInstructionIndex) {
    EZrStaticCType staticCType;
    EZrStaticCType sourceStaticCType;
    EZrStaticCType sourceLocalStaticCType;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceStaticCType = backend_aot_c_scalar_stack_copy_static_type_for_slot(functionIr->function, sourceSlot);
    sourceLocalStaticCType = backend_aot_c_scalar_stack_copy_static_type_from_locals(functionIr, sourceSlot);
    staticCType = backend_aot_c_scalar_stack_copy_static_type_for_slot(functionIr->function, destinationSlot);
    if (staticCType == ZR_STATIC_C_TYPE_DYNAMIC) {
        staticCType = sourceStaticCType;
    }
    if (staticCType == ZR_STATIC_C_TYPE_DYNAMIC) {
        staticCType = backend_aot_c_scalar_stack_copy_static_type_from_locals(functionIr, destinationSlot);
    }
    if (staticCType == ZR_STATIC_C_TYPE_DYNAMIC) {
        staticCType = sourceLocalStaticCType;
    }

    switch (staticCType) {
        case ZR_STATIC_C_TYPE_BOOL:
        case ZR_STATIC_C_TYPE_I64:
        case ZR_STATIC_C_TYPE_U64:
        case ZR_STATIC_C_TYPE_F64:
            if (backend_aot_c_scalar_stack_copy_source_local_is_available(
                        functionIr, sourceSlot, execInstructionIndex, staticCType) &&
                backend_aot_c_scalar_stack_copy_destination_local_is_available(
                        functionIr, destinationSlot, staticCType)) {
                return ZR_TRUE;
            }
            break;
        default:
            break;
    }

    if (sourceStaticCType != ZR_STATIC_C_TYPE_DYNAMIC &&
        sourceStaticCType != staticCType &&
        backend_aot_c_scalar_stack_copy_source_local_is_available(functionIr,
                                                                  sourceSlot,
                                                                  execInstructionIndex,
                                                                  sourceStaticCType) &&
        backend_aot_c_scalar_stack_copy_destination_local_is_available(
                functionIr, destinationSlot, sourceStaticCType)) {
        return ZR_TRUE;
    }

    if (sourceLocalStaticCType != ZR_STATIC_C_TYPE_DYNAMIC &&
        sourceLocalStaticCType != staticCType &&
        backend_aot_c_scalar_stack_copy_source_local_is_available(functionIr,
                                                                  sourceSlot,
                                                                  execInstructionIndex,
                                                                  sourceLocalStaticCType) &&
        backend_aot_c_scalar_stack_copy_destination_local_is_available(
                functionIr, destinationSlot, sourceLocalStaticCType)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool backend_aot_try_write_c_scalar_stack_copy(FILE *file,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 sourceSlot,
                                                  TZrUInt32 execInstructionIndex,
                                                  TZrBool forceValueSlotWrite) {
    EZrStaticCType staticCType;
    EZrStaticCType sourceStaticCType;
    EZrStaticCType sourceLocalStaticCType;
    TZrBool hasSourceLocal;
    TZrBool sourceIsParameter;

    if (file == ZR_NULL || functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceStaticCType = backend_aot_c_scalar_stack_copy_static_type_for_slot(functionIr->function, sourceSlot);
    sourceLocalStaticCType = backend_aot_c_scalar_stack_copy_static_type_from_locals(functionIr, sourceSlot);
    staticCType = backend_aot_c_scalar_stack_copy_static_type_for_slot(functionIr->function, destinationSlot);
    if (staticCType == ZR_STATIC_C_TYPE_DYNAMIC) {
        staticCType = sourceStaticCType;
    }
    if (staticCType == ZR_STATIC_C_TYPE_DYNAMIC) {
        staticCType = backend_aot_c_scalar_stack_copy_static_type_from_locals(functionIr, destinationSlot);
    }
    if (staticCType == ZR_STATIC_C_TYPE_DYNAMIC) {
        staticCType = backend_aot_c_scalar_stack_copy_static_type_from_locals(functionIr, sourceSlot);
    }
    hasSourceLocal = backend_aot_c_scalar_stack_copy_source_local_is_available(functionIr,
                                                                              sourceSlot,
                                                                              execInstructionIndex,
                                                                              staticCType);
    if (forceValueSlotWrite) {
        (void)backend_aot_c_scalar_stack_copy_prefer_available_source_type(functionIr,
                                                                          destinationSlot,
                                                                          sourceSlot,
                                                                          execInstructionIndex,
                                                                          forceValueSlotWrite,
                                                                          &staticCType,
                                                                          &hasSourceLocal);
    }
    sourceIsParameter = backend_aot_c_scalar_stack_copy_source_slot_is_parameter(functionIr, sourceSlot);
    if (!hasSourceLocal &&
        sourceStaticCType != ZR_STATIC_C_TYPE_DYNAMIC &&
        sourceStaticCType != staticCType &&
        backend_aot_c_scalar_stack_copy_source_local_is_available(functionIr,
                                                                  sourceSlot,
                                                                  execInstructionIndex,
                                                                  sourceStaticCType) &&
        backend_aot_c_scalar_stack_copy_destination_local_is_available(
                functionIr, destinationSlot, sourceStaticCType)) {
        staticCType = sourceStaticCType;
        hasSourceLocal = ZR_TRUE;
    }
    if (!hasSourceLocal &&
        sourceLocalStaticCType != ZR_STATIC_C_TYPE_DYNAMIC &&
        sourceLocalStaticCType != staticCType &&
        backend_aot_c_scalar_stack_copy_source_local_is_available(functionIr,
                                                                  sourceSlot,
                                                                  execInstructionIndex,
                                                                  sourceLocalStaticCType) &&
        backend_aot_c_scalar_stack_copy_destination_local_is_available(
                functionIr, destinationSlot, sourceLocalStaticCType)) {
        staticCType = sourceLocalStaticCType;
        hasSourceLocal = ZR_TRUE;
    }
    if (!hasSourceLocal && !sourceIsParameter) {
        return ZR_FALSE;
    }

    switch (staticCType) {
        case ZR_STATIC_C_TYPE_BOOL:
            backend_aot_write_c_scalar_stack_copy_bool(
                    file,
                    destinationSlot,
                    sourceSlot,
                    hasSourceLocal,
                    backend_aot_c_scalar_stack_copy_destination_local_is_available(
                            functionIr, destinationSlot, staticCType),
                    forceValueSlotWrite);
            return ZR_TRUE;
        case ZR_STATIC_C_TYPE_I64:
            backend_aot_write_c_scalar_stack_copy_i64(
                    file,
                    destinationSlot,
                    sourceSlot,
                    hasSourceLocal,
                    backend_aot_c_scalar_stack_copy_destination_local_is_available(
                            functionIr, destinationSlot, staticCType),
                    forceValueSlotWrite);
            return ZR_TRUE;
        case ZR_STATIC_C_TYPE_U64:
            backend_aot_write_c_scalar_stack_copy_u64(
                    file,
                    destinationSlot,
                    sourceSlot,
                    hasSourceLocal,
                    backend_aot_c_scalar_stack_copy_destination_local_is_available(
                            functionIr, destinationSlot, staticCType),
                    forceValueSlotWrite);
            return ZR_TRUE;
        case ZR_STATIC_C_TYPE_F64:
            backend_aot_write_c_scalar_stack_copy_f64(
                    file,
                    destinationSlot,
                    sourceSlot,
                    hasSourceLocal,
                    backend_aot_c_scalar_stack_copy_destination_local_is_available(
                            functionIr, destinationSlot, staticCType),
                    forceValueSlotWrite);
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}
