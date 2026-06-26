#include "backend_aot_c_method_metadata.h"

#include "backend_aot_c_generic_sharing.h"
#include "backend_aot_c_scalar_locals.h"
#include "backend_aot_internal.h"

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"

#include <string.h>

static const SZrAotFunctionEntry *backend_aot_c_method_metadata_find_entry_by_flat_index(
        const SZrAotFunctionTable *table,
        TZrUInt32 flatIndex) {
    if (table == ZR_NULL || table->entries == ZR_NULL || flatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (entry->flatIndex == flatIndex) {
            return entry;
        }
    }

    return ZR_NULL;
}

static const TZrChar *backend_aot_c_reflection_metadata_level_name(TZrUInt8 reflectionMetadataLevel) {
    switch (reflectionMetadataLevel) {
        case ZR_AOT_REFLECTION_METADATA_NONE:
            return "ZR_AOT_REFLECTION_METADATA_NONE";
        case ZR_AOT_REFLECTION_METADATA_DESCRIPTION:
            return "ZR_AOT_REFLECTION_METADATA_DESCRIPTION";
        case ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING:
        default:
            return "ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING";
    }
}

static void backend_aot_write_c_signature_type(FILE *file,
                                               const SZrFunctionTypedTypeRef *typeRef) {
    TZrUInt32 baseType = 0u;
    TZrUInt32 staticCType = 0u;
    TZrUInt32 staticCTypeId = 0u;
    TZrUInt32 ownershipQualifier = 0u;
    TZrUInt32 elementBaseType = 0u;
    TZrUInt32 isNullable = 0u;
    TZrUInt32 isArray = 0u;

    if (file == ZR_NULL) {
        return;
    }

    if (typeRef != ZR_NULL) {
        baseType = (TZrUInt32)typeRef->baseType;
        staticCType = (TZrUInt32)typeRef->staticCType;
        staticCTypeId = typeRef->staticCTypeId;
        ownershipQualifier = typeRef->ownershipQualifier;
        elementBaseType = (TZrUInt32)typeRef->elementBaseType;
        isNullable = typeRef->isNullable ? 1u : 0u;
        isArray = typeRef->isArray ? 1u : 0u;
    }

    fprintf(file,
            "    {\n"
            "        .baseType = (TZrUInt16)%uu,\n"
            "        .staticCType = (TZrUInt16)%uu,\n"
            "        .staticCTypeId = %uu,\n"
            "        .ownershipQualifier = %uu,\n"
            "        .elementBaseType = (TZrUInt16)%uu,\n"
            "        .isNullable = (TZrUInt8)%uu,\n"
            "        .isArray = (TZrUInt8)%uu,\n"
            "    },\n",
            (unsigned)baseType,
            (unsigned)staticCType,
            (unsigned)staticCTypeId,
            (unsigned)ownershipQualifier,
            (unsigned)elementBaseType,
            (unsigned)isNullable,
            (unsigned)isArray);
}

static const SZrFunctionTypedTypeRef *backend_aot_c_signature_parameter_type(
        const SZrFunction *function,
        TZrUInt32 parameterIndex) {
    if (function == ZR_NULL ||
        function->parameterMetadata == ZR_NULL ||
        parameterIndex >= function->parameterMetadataCount) {
        return ZR_NULL;
    }

    return &function->parameterMetadata[parameterIndex].type;
}

typedef TZrBool (*FZrAotCSignatureReturnProof)(const SZrAotExecIrFunction *functionIr,
                                               TZrUInt32 slot,
                                               TZrUInt32 execInstructionIndex);

static void backend_aot_c_signature_init_scalar_return_type(SZrFunctionTypedTypeRef *returnType,
                                                            EZrValueType baseType,
                                                            EZrStaticCType staticCType) {
    if (returnType == ZR_NULL) {
        return;
    }

    memset(returnType, 0, sizeof(*returnType));
    returnType->baseType = baseType;
    returnType->elementBaseType = ZR_VALUE_TYPE_OBJECT;
    returnType->staticCType = staticCType;
    returnType->staticCTypeId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
}

static void backend_aot_c_signature_init_i64_return_type(SZrFunctionTypedTypeRef *returnType) {
    backend_aot_c_signature_init_scalar_return_type(returnType, ZR_VALUE_TYPE_INT64, ZR_STATIC_C_TYPE_I64);
}

static void backend_aot_c_signature_init_bool_return_type(SZrFunctionTypedTypeRef *returnType) {
    backend_aot_c_signature_init_scalar_return_type(returnType, ZR_VALUE_TYPE_BOOL, ZR_STATIC_C_TYPE_BOOL);
}

static void backend_aot_c_signature_init_u64_return_type(SZrFunctionTypedTypeRef *returnType) {
    backend_aot_c_signature_init_scalar_return_type(returnType, ZR_VALUE_TYPE_UINT64, ZR_STATIC_C_TYPE_U64);
}

static void backend_aot_c_signature_init_f64_return_type(SZrFunctionTypedTypeRef *returnType) {
    backend_aot_c_signature_init_scalar_return_type(returnType, ZR_VALUE_TYPE_DOUBLE, ZR_STATIC_C_TYPE_F64);
}

static TZrBool backend_aot_c_signature_try_infer_scalar_return(
        const SZrAotExecIrFunction *functionIr,
        SZrFunctionTypedTypeRef *returnType,
        FZrAotCSignatureReturnProof returnProof,
        void (*initReturnType)(SZrFunctionTypedTypeRef *returnType)) {
    const SZrFunction *function;
    TZrUInt32 instructionIndex;
    TZrBool foundReturn = ZR_FALSE;

    if (functionIr == ZR_NULL ||
        functionIr->function == ZR_NULL ||
        functionIr->function->instructionsList == ZR_NULL ||
        returnType == ZR_NULL ||
        returnProof == ZR_NULL ||
        initReturnType == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    for (instructionIndex = 0u; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];

        if (instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
            continue;
        }

        foundReturn = ZR_TRUE;
        if (!returnProof(functionIr, instruction->instruction.operand.operand1[0], instructionIndex)) {
            return ZR_FALSE;
        }
    }

    if (!foundReturn) {
        return ZR_FALSE;
    }

    initReturnType(returnType);
    return ZR_TRUE;
}

static TZrBool backend_aot_c_signature_try_infer_i64_return(
        const SZrAotExecIrFunction *functionIr,
        SZrFunctionTypedTypeRef *returnType) {
    return backend_aot_c_signature_try_infer_scalar_return(
            functionIr,
            returnType,
            backend_aot_c_scalar_locals_can_direct_return_i64_local,
            backend_aot_c_signature_init_i64_return_type);
}

static TZrBool backend_aot_c_signature_try_infer_bool_return(
        const SZrAotExecIrFunction *functionIr,
        SZrFunctionTypedTypeRef *returnType) {
    return backend_aot_c_signature_try_infer_scalar_return(
            functionIr,
            returnType,
            backend_aot_c_scalar_locals_can_infer_return_bool_local,
            backend_aot_c_signature_init_bool_return_type);
}

static TZrBool backend_aot_c_signature_try_infer_u64_return(
        const SZrAotExecIrFunction *functionIr,
        SZrFunctionTypedTypeRef *returnType) {
    return backend_aot_c_signature_try_infer_scalar_return(
            functionIr,
            returnType,
            backend_aot_c_scalar_locals_can_infer_return_u64_local,
            backend_aot_c_signature_init_u64_return_type);
}

static TZrBool backend_aot_c_signature_try_infer_f64_return(
        const SZrAotExecIrFunction *functionIr,
        SZrFunctionTypedTypeRef *returnType) {
    return backend_aot_c_signature_try_infer_scalar_return(
            functionIr,
            returnType,
            backend_aot_c_scalar_locals_can_infer_return_f64_local,
            backend_aot_c_signature_init_f64_return_type);
}

static TZrBool backend_aot_c_signature_try_infer_static_return(
        const SZrAotExecIrFunction *functionIr,
        SZrFunctionTypedTypeRef *returnType) {
    if (backend_aot_c_signature_try_infer_i64_return(functionIr, returnType)) {
        return ZR_TRUE;
    }
    if (backend_aot_c_signature_try_infer_bool_return(functionIr, returnType)) {
        return ZR_TRUE;
    }
    if (backend_aot_c_signature_try_infer_u64_return(functionIr, returnType)) {
        return ZR_TRUE;
    }
    if (backend_aot_c_signature_try_infer_f64_return(functionIr, returnType)) {
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static const SZrFunctionTypedTypeRef *backend_aot_c_signature_return_type(
        const SZrFunction *function,
        const SZrAotExecIrFunction *functionIr,
        SZrFunctionTypedTypeRef *inferredReturnType) {
    if (function != ZR_NULL && function->hasCallableReturnType) {
        return &function->callableReturnType;
    }

    if (backend_aot_c_signature_try_infer_static_return(functionIr, inferredReturnType)) {
        return inferredReturnType;
    }

    return ZR_NULL;
}

static void backend_aot_write_c_signature(FILE *file,
                                          TZrUInt32 functionIndex,
                                          const SZrFunction *function,
                                          const SZrAotExecIrFunction *functionIr) {
    TZrUInt32 parameterCount;
    TZrUInt32 parameterIndex;
    TZrBool hasReturnValue;
    SZrFunctionTypedTypeRef inferredReturnType;
    const SZrFunctionTypedTypeRef *returnType;

    if (file == ZR_NULL) {
        return;
    }

    parameterCount = function != ZR_NULL ? (TZrUInt32)function->parameterCount : 0u;
    memset(&inferredReturnType, 0, sizeof(inferredReturnType));
    returnType = backend_aot_c_signature_return_type(function, functionIr, &inferredReturnType);
    hasReturnValue = (TZrBool)(returnType != ZR_NULL);

    fprintf(file, "static const SZrAotSignatureType zr_aot_signature_%u_types[] = {\n",
            (unsigned)functionIndex);
    backend_aot_write_c_signature_type(file, returnType);
    for (parameterIndex = 0u; parameterIndex < parameterCount; parameterIndex++) {
        backend_aot_write_c_signature_type(file,
                                           backend_aot_c_signature_parameter_type(function, parameterIndex));
    }
    fprintf(file, "};\n");
    fprintf(file, "static const SZrAotSignature zr_aot_signature_%u = {\n", (unsigned)functionIndex);
    fprintf(file, "    .parameterCount = %uu,\n", (unsigned)parameterCount);
    if (hasReturnValue) {
        fprintf(file, "    .returnType = &zr_aot_signature_%u_types[0],\n", (unsigned)functionIndex);
    } else {
        fprintf(file, "    .returnType = ZR_NULL,\n");
    }
    if (parameterCount > 0u) {
        fprintf(file, "    .parameterTypes = &zr_aot_signature_%u_types[1],\n", (unsigned)functionIndex);
    } else {
        fprintf(file, "    .parameterTypes = ZR_NULL,\n");
    }
    fprintf(file, "    .hasReturnValue = (TZrUInt8)%uu,\n", hasReturnValue ? 1u : 0u);
    fprintf(file,
            "    .hasVarArgs = (TZrUInt8)%uu,\n",
            function != ZR_NULL && function->hasVariableArguments ? 1u : 0u);
    fprintf(file, "};\n");
}

void backend_aot_write_c_reflection_invokers(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "static void zr_aot_invoker_entry_thunk(struct SZrState *state,\n"
            "                                      FZrAotEntryThunk target,\n"
            "                                      const SZrAotMethodInfo *method,\n"
            "                                      SZrTypeValue *self,\n"
            "                                      SZrTypeValue *args,\n"
            "                                      SZrTypeValue *outReturn) {\n"
            "    (void)method;\n"
            "    (void)self;\n"
            "    (void)args;\n"
            "    (void)outReturn;\n"
            "    if (target != ZR_NULL) {\n"
            "        (void)target(state);\n"
            "    }\n"
            "}\n"
            "static const FZrAotReflectionInvoker zr_aot_reflection_invokers[] = {\n"
            "    zr_aot_invoker_entry_thunk,\n"
            "};\n");
}

static TZrUInt32 backend_aot_c_method_info_register_frame_bytes(const SZrAotExecIrFunction *functionIr) {
    TZrUInt32 layoutIndex;
    TZrUInt32 registerFrameBytes = 0u;

    if (functionIr == ZR_NULL ||
        functionIr->frameLayout.slotLayouts == ZR_NULL) {
        return 0u;
    }

    for (layoutIndex = 0u; layoutIndex < functionIr->frameLayout.slotLayoutCount; layoutIndex++) {
        const SZrAotExecIrFrameSlotLayout *layout = &functionIr->frameLayout.slotLayouts[layoutIndex];
        TZrUInt32 slotEnd;

        if (layout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
            layout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
            layout->byteSize == 0u) {
            continue;
        }

        slotEnd = layout->byteOffset + layout->byteSize;
        if (slotEnd > registerFrameBytes) {
            registerFrameBytes = slotEnd;
        }
    }

    return registerFrameBytes;
}

static TZrBool backend_aot_c_gc_root_map_try_get_gc_offset(const SZrTypeLayout *typeLayout,
                                                           TZrUInt32 gcFieldIndex,
                                                           TZrUInt32 *outOffset) {
    TZrUInt32 matchedIndex = 0u;

    if (outOffset == ZR_NULL) {
        return ZR_FALSE;
    }
    *outOffset = 0u;
    if (typeLayout == ZR_NULL || gcFieldIndex >= typeLayout->gcFieldCount) {
        return ZR_FALSE;
    }

    if (typeLayout->gcFieldOffsets != ZR_NULL) {
        *outOffset = typeLayout->gcFieldOffsets[gcFieldIndex];
        return ZR_TRUE;
    }

    if (typeLayout->fields == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 fieldIndex = 0u; fieldIndex < typeLayout->fieldCount; fieldIndex++) {
        const SZrTypeLayoutField *field = &typeLayout->fields[fieldIndex];

        if ((field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) == 0u ||
            (field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE) == 0u ||
            field->byteSize < sizeof(SZrTypeValue)) {
            continue;
        }

        if (matchedIndex == gcFieldIndex) {
            *outOffset = field->byteOffset;
            return ZR_TRUE;
        }
        matchedIndex++;
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_gc_root_map_can_emit_field(const SZrTypeLayout *typeLayout,
                                                        TZrUInt32 gcFieldIndex,
                                                        TZrUInt32 *outFieldOffset) {
    TZrUInt32 fieldOffset = 0u;

    if (!backend_aot_c_gc_root_map_try_get_gc_offset(typeLayout, gcFieldIndex, &fieldOffset) ||
        fieldOffset > typeLayout->byteSize ||
        sizeof(SZrTypeValue) > typeLayout->byteSize - fieldOffset) {
        return ZR_FALSE;
    }

    if (outFieldOffset != ZR_NULL) {
        *outFieldOffset = fieldOffset;
    }
    return ZR_TRUE;
}

static const SZrTypeLayout *backend_aot_c_gc_root_map_resolve_layout(
        SZrState *state,
        const SZrAotExecIrFunction *functionIr,
        const SZrAotExecIrFrameSlotLayout *slotLayout) {
    if (state == ZR_NULL ||
        functionIr == ZR_NULL ||
        functionIr->function == ZR_NULL ||
        slotLayout == ZR_NULL ||
        slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
        slotLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        slotLayout->byteSize == 0u) {
        return ZR_NULL;
    }

    return ZrCore_Function_ResolvePrototypeFrameTypeLayout(functionIr->function,
                                                           slotLayout->typeLayoutId,
                                                           state);
}

TZrUInt32 backend_aot_c_method_metadata_count_gc_roots(SZrState *state,
                                                       const SZrAotExecIrFunction *functionIr) {
    TZrUInt32 rootCount = 0u;

    if (functionIr == ZR_NULL || functionIr->frameLayout.slotLayouts == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 layoutIndex = 0u; layoutIndex < functionIr->frameLayout.slotLayoutCount; layoutIndex++) {
        const SZrAotExecIrFrameSlotLayout *slotLayout = &functionIr->frameLayout.slotLayouts[layoutIndex];
        const SZrTypeLayout *typeLayout = backend_aot_c_gc_root_map_resolve_layout(state, functionIr, slotLayout);

        if (typeLayout == ZR_NULL || typeLayout->gcFieldCount == 0u) {
            continue;
        }

        for (TZrUInt32 gcFieldIndex = 0u; gcFieldIndex < typeLayout->gcFieldCount; gcFieldIndex++) {
            if (backend_aot_c_gc_root_map_can_emit_field(typeLayout, gcFieldIndex, ZR_NULL)) {
                rootCount++;
            }
        }
    }

    return rootCount;
}

static void backend_aot_write_c_gc_root_map(FILE *file,
                                            SZrState *state,
                                            TZrUInt32 functionIndex,
                                            const SZrAotExecIrFunction *functionIr) {
    TZrUInt32 rootCount = backend_aot_c_method_metadata_count_gc_roots(state, functionIr);

    if (file == ZR_NULL || rootCount == 0u) {
        return;
    }

    fprintf(file, "static const SZrAotGcRootSlot zr_aot_gc_root_slots_%u[] = {\n",
            (unsigned)functionIndex);
    for (TZrUInt32 layoutIndex = 0u; layoutIndex < functionIr->frameLayout.slotLayoutCount; layoutIndex++) {
        const SZrAotExecIrFrameSlotLayout *slotLayout = &functionIr->frameLayout.slotLayouts[layoutIndex];
        const SZrTypeLayout *typeLayout = backend_aot_c_gc_root_map_resolve_layout(state, functionIr, slotLayout);

        if (typeLayout == ZR_NULL || typeLayout->gcFieldCount == 0u) {
            continue;
        }

        for (TZrUInt32 gcFieldIndex = 0u; gcFieldIndex < typeLayout->gcFieldCount; gcFieldIndex++) {
            TZrUInt32 fieldOffset = 0u;

            if (!backend_aot_c_gc_root_map_can_emit_field(typeLayout, gcFieldIndex, &fieldOffset)) {
                continue;
            }

            fprintf(file,
                    "    {\n"
                    "        .stackSlot = %uu,\n"
                    "        .frameByteOffset = %uu,\n"
                    "        .typeLayoutId = %uu,\n"
                    "        .fieldByteOffset = %uu,\n"
                    "        .locationKind = (TZrUInt8)ZR_AOT_GC_ROOT_LOCATION_FRAME_BYTE_OFFSET,\n"
                    "    },\n",
                    (unsigned)slotLayout->stackSlot,
                    (unsigned)(slotLayout->byteOffset + fieldOffset),
                    (unsigned)slotLayout->typeLayoutId,
                    (unsigned)fieldOffset);
        }
    }
    fprintf(file, "};\n");
    fprintf(file, "static const SZrAotGcRootMap zr_aot_gc_root_map_%u = {\n", (unsigned)functionIndex);
    fprintf(file, "    %uu,\n", (unsigned)rootCount);
    fprintf(file, "    zr_aot_gc_root_slots_%u,\n", (unsigned)functionIndex);
    fprintf(file, "};\n");
}

unsigned long long backend_aot_write_c_method_infos(FILE *file,
                                                    SZrState *state,
                                                    const SZrAotFunctionTable *table,
                                                    const SZrAotExecIrModule *module,
                                                    TZrUInt8 reflectionMetadataLevel) {
    TZrUInt32 index;
    unsigned long long methodMetadataBytesTotal = 0u;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL || module == ZR_NULL) {
        return 0u;
    }

    for (index = 0; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        const SZrAotExecIrFunction *functionIr =
                backend_aot_exec_ir_find_function(module, entry->flatIndex);
        TZrUInt32 registerFrameBytes = backend_aot_c_method_info_register_frame_bytes(functionIr);
        TZrUInt32 gcRootCount = backend_aot_c_method_metadata_count_gc_roots(state, functionIr);
        TZrUInt32 genericDictionaryId =
                backend_aot_c_generic_sharing_dictionary_id_for_function(table, entry->function);
        long methodMetadataStart = ftell(file);
        long methodMetadataEnd;
        unsigned long long methodMetadataBytes = 0u;

        backend_aot_write_c_gc_root_map(file, state, entry->flatIndex, functionIr);
        backend_aot_write_c_signature(file, entry->flatIndex, entry->function, functionIr);
        fprintf(file, "static const SZrAotMethodInfo zr_aot_method_info_%u = {\n", (unsigned)entry->flatIndex);
        fprintf(file, "    .functionIndex = %uu,\n", (unsigned)entry->flatIndex);
        fprintf(file, "    .metadataFunction = ZR_NULL,\n");
        fprintf(file, "    .registerFrameBytes = %uu,\n", (unsigned)registerFrameBytes);
        if (gcRootCount > 0u) {
            fprintf(file, "    .gcRootMap = &zr_aot_gc_root_map_%u,\n", (unsigned)entry->flatIndex);
        } else {
            fprintf(file, "    .gcRootMap = ZR_NULL,\n");
        }
        fprintf(file, "    .signature = &zr_aot_signature_%u,\n", (unsigned)entry->flatIndex);
        if (genericDictionaryId != 0u) {
            fprintf(file, "    .genericDictionary = &zr_aot_generic_dict_%u,\n", (unsigned)genericDictionaryId);
        } else {
            fprintf(file, "    .genericDictionary = ZR_NULL,\n");
        }
        fprintf(file, "    .invoker = zr_aot_invoker_entry_thunk,\n");
        fprintf(file, "    .observationPolicy = 0u,\n");
        fprintf(file,
                "    .reflectionMetadataLevel = %s,\n",
                backend_aot_c_reflection_metadata_level_name(reflectionMetadataLevel));
        fprintf(file, "    .reserved0 = 0u,\n");
        fprintf(file, "    .reserved1 = 0u,\n");
        fprintf(file, "};\n");
        methodMetadataEnd = ftell(file);
        if (methodMetadataStart >= 0 && methodMetadataEnd >= methodMetadataStart) {
            methodMetadataBytes = (unsigned long long)(methodMetadataEnd - methodMetadataStart);
            methodMetadataBytesTotal += methodMetadataBytes;
        }
        fprintf(file,
                "/* aot_size.methodMetadataBytes[%u] = %llu */\n",
                (unsigned)entry->flatIndex,
                methodMetadataBytes);
    }
    fprintf(file, "/* aot_size.methodMetadataBytesTotal = %llu */\n", methodMetadataBytesTotal);
    return methodMetadataBytesTotal;
}

unsigned long long backend_aot_c_method_metadata_generated_bytes_referenced(
        SZrState *state,
        const SZrAotFunctionTable *table,
        const SZrAotExecIrModule *module,
        TZrUInt8 reflectionMetadataLevel) {
    FILE *scratchFile;
    unsigned long long methodMetadataBytesTotal;

    if (state == ZR_NULL || table == ZR_NULL || module == ZR_NULL) {
        return 0u;
    }

    scratchFile = tmpfile();
    if (scratchFile == ZR_NULL) {
        return 0u;
    }

    methodMetadataBytesTotal =
            backend_aot_write_c_method_infos(scratchFile, state, table, module, reflectionMetadataLevel);
    fclose(scratchFile);
    return methodMetadataBytesTotal;
}

void backend_aot_write_c_method_info_table(FILE *file,
                                           const SZrAotFunctionTable *table,
                                           TZrUInt32 functionIndexSpace) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL) {
        return;
    }

    fprintf(file, "static const SZrAotMethodInfo *const zr_aot_method_infos[] = {\n");
    for (index = 0u; index < functionIndexSpace; index++) {
        const SZrAotFunctionEntry *entry =
                backend_aot_c_method_metadata_find_entry_by_flat_index(table, index);
        if (entry != ZR_NULL) {
            fprintf(file, "    &zr_aot_method_info_%u,\n", (unsigned)entry->flatIndex);
        } else {
            fprintf(file, "    ZR_NULL,\n");
        }
    }
    fprintf(file, "};\n");
}
