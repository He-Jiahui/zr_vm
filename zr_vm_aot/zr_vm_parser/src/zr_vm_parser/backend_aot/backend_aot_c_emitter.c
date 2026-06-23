#include "backend_aot_c_emitter.h"
#include "backend_aot_c_function_body.h"
#include "backend_aot_c_typed_bool_thunks.h"
#include "backend_aot_c_typed_f64_thunks.h"
#include "backend_aot_c_typed_i64_thunks.h"
#include "backend_aot_c_typed_u64_thunks.h"
#include "backend_aot_c_scalar_locals.h"
#include "backend_aot_c_type_layouts.h"
#include "backend_aot_internal.h"

#include "zr_vm_common/zr_aot_abi.h"

#include <string.h>

static void backend_aot_write_c_contracts(FILE *file, TZrUInt32 runtimeContracts) {
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_REFLECTION_TYPEOF) {
        fprintf(file, "/* runtime contract: ZrCore_Reflection_TypeOfValue */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_FUNCTION_PRECALL) {
        fprintf(file, "/* runtime contract: ZrCore_Function_PreCall */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_BORROW) {
        fprintf(file, "/* runtime contract: ZrCore_Ownership_BorrowValue */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_LOAN) {
        fprintf(file, "/* runtime contract: ZrCore_Ownership_LoanValue */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_SHARE) {
        fprintf(file, "/* runtime contract: ZrCore_Ownership_NativeShared */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_WEAK) {
        fprintf(file, "/* runtime contract: ZrCore_Ownership_NativeWeak */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_DETACH) {
        fprintf(file, "/* runtime contract: ZrCore_Ownership_DetachValue */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_UPGRADE) {
        fprintf(file, "/* runtime contract: ZrCore_Ownership_UpgradeValue */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE) {
        fprintf(file, "/* runtime contract: ZrCore_Ownership_ReleaseValue */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RETURN_LOAN) {
        fprintf(file, "/* runtime contract: ZrCore_Ownership_ReturnLoanValue */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_ITER_INIT) {
        fprintf(file, "/* runtime contract: ZrCore_Object_IterInit */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_ITER_MOVE_NEXT) {
        fprintf(file, "/* runtime contract: ZrCore_Object_IterMoveNext */\n");
    }
}

static void backend_aot_write_runtime_contract_array_c(FILE *file, TZrUInt32 runtimeContracts) {
    TZrUInt32 contractBit;

    fprintf(file, "static const TZrChar *const zr_aot_runtime_contracts[] = {\n");
    for (contractBit = 1; contractBit <= ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RETURN_LOAN; contractBit <<= 1) {
        if ((runtimeContracts & contractBit) == 0) {
            continue;
        }
        fprintf(file, "    \"%s\",\n", backend_aot_exec_ir_runtime_contract_name(contractBit));
    }
    fprintf(file, "    ZR_NULL,\n");
    fprintf(file, "};\n");
}

static void backend_aot_write_embedded_blob_c(FILE *file, const TZrByte *blob, TZrSize blobLength) {
    TZrSize index;

    if (file == ZR_NULL) {
        return;
    }

    fprintf(file, "static const TZrByte zr_aot_embedded_module_blob[] = {\n");
    if (blob != ZR_NULL && blobLength > 0) {
        for (index = 0; index < blobLength; index++) {
            if ((index % 12) == 0) {
                fprintf(file, "    ");
            }
            fprintf(file, "0x%02x", blob[index]);
            if (index + 1 < blobLength) {
                fprintf(file, ", ");
            }
            if ((index % 12) == 11 || index + 1 == blobLength) {
                fprintf(file, "\n");
            }
        }
    } else {
        fprintf(file, "    0x%02x\n", 0x00u);
    }
    fprintf(file, "};\n");
}

static void backend_aot_write_c_function_forward_decls(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL) {
        return;
    }

    for (index = 0; index < table->count; index++) {
        fprintf(file, "static TZrInt64 zr_aot_fn_%u(struct SZrState *state);\n", (unsigned)index);
    }
}

static void backend_aot_write_c_function_table(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL) {
        return;
    }

    fprintf(file, "static const FZrAotEntryThunk zr_aot_function_thunks[] = {\n");
    for (index = 0; index < table->count; index++) {
        fprintf(file, "    zr_aot_fn_%u,\n", (unsigned)index);
    }
    fprintf(file, "};\n");
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

static void backend_aot_write_c_method_infos(FILE *file,
                                             const SZrAotFunctionTable *table,
                                             const SZrAotExecIrModule *module) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL || module == ZR_NULL) {
        return;
    }

    for (index = 0; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        const SZrAotExecIrFunction *functionIr =
                backend_aot_exec_ir_find_function(module, entry->flatIndex);
        TZrUInt32 registerFrameBytes = backend_aot_c_method_info_register_frame_bytes(functionIr);

        backend_aot_write_c_signature(file, entry->flatIndex, entry->function, functionIr);
        fprintf(file, "static const SZrAotMethodInfo zr_aot_method_info_%u = {\n", (unsigned)entry->flatIndex);
        fprintf(file, "    .functionIndex = %uu,\n", (unsigned)entry->flatIndex);
        fprintf(file, "    .metadataFunction = ZR_NULL,\n");
        fprintf(file, "    .registerFrameBytes = %uu,\n", (unsigned)registerFrameBytes);
        fprintf(file, "    .gcRootMap = ZR_NULL,\n");
        fprintf(file, "    .signature = &zr_aot_signature_%u,\n", (unsigned)entry->flatIndex);
        fprintf(file, "    .observationPolicy = 0u,\n");
        fprintf(file, "};\n");
    }
}

static void backend_aot_write_c_method_info_table(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL) {
        return;
    }

    fprintf(file, "static const SZrAotMethodInfo *const zr_aot_method_infos[] = {\n");
    for (index = 0u; index < table->count; index++) {
        fprintf(file, "    &zr_aot_method_info_%u,\n", (unsigned)index);
    }
    fprintf(file, "};\n");
}

static void backend_aot_write_c_guard_macro(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "#define ZR_AOT_C_RETURN(expr)            \\\n"
            "    do {                                  \\\n"
            "        zr_aot_return_value = (expr);      \\\n"
            "        goto zr_aot_function_exit;         \\\n"
            "    } while (0)\n"
            "#define ZR_AOT_C_FAIL()                                                             \\\n"
            "    do {                                                                              \\\n"
            "        ZrCore_Debug_RunError(state,                                                   \\\n"
            "                              \"generated AOT function failed: functionIndex=%%u instructionIndex=%%u\", \\\n"
            "                              (unsigned)zr_aot_function_index,                       \\\n"
            "                              UINT32_MAX);                                           \\\n"
            "        ZR_AOT_C_RETURN(0);                                                           \\\n"
            "    } while (0)\n"
            "#define ZR_AOT_C_GUARD(expr)            \\\n"
            "    do {                                 \\\n"
            "        if (!(expr)) {                   \\\n"
            "            ZR_AOT_C_FAIL();             \\\n"
            "        }                                \\\n"
            "    } while (0)\n");
}

ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFileWithOptions(SZrState *state,
                                                               SZrFunction *function,
                                                               const TZrChar *filename,
                                                               const SZrAotWriterOptions *options) {
    SZrAotExecIrModule module;
    SZrAotFunctionTable functionTable;
    FILE *file;
    const TZrChar *moduleName;
    const TZrChar *sourceHash;
    const TZrChar *zroHash;
    const TZrChar *inputHash;
    TZrUInt32 inputKind;
    TZrBool requireExecutableLowering;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || function == ZR_NULL || filename == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&module, 0, sizeof(module));
    memset(&functionTable, 0, sizeof(functionTable));

    if (!backend_aot_exec_ir_build_module(state, function, &module)) {
        return ZR_FALSE;
    }

    if (!backend_aot_build_function_table(state, function, &functionTable)) {
        backend_aot_exec_ir_release_module(state, &module);
        return ZR_FALSE;
    }

    file = fopen(filename, "wb");
    if (file == ZR_NULL) {
        backend_aot_release_function_table(state, &functionTable);
        backend_aot_exec_ir_release_module(state, &module);
        return ZR_FALSE;
    }

    moduleName = backend_aot_option_text(options, options != ZR_NULL ? options->moduleName : ZR_NULL, "__entry__");
    sourceHash = backend_aot_option_text(options, options != ZR_NULL ? options->sourceHash : ZR_NULL, "unknown");
    zroHash = backend_aot_option_text(options, options != ZR_NULL ? options->zroHash : ZR_NULL, "unknown");
    inputKind = backend_aot_option_input_kind(options);
    inputHash = backend_aot_option_input_hash(options, sourceHash, zroHash);
    requireExecutableLowering = options != ZR_NULL && options->requireExecutableLowering;

    if (requireExecutableLowering || backend_aot_report_first_unsupported_instruction("aot_c", moduleName, &functionTable)) {
        for (TZrUInt32 functionIndex = 0; functionIndex < functionTable.count; functionIndex++) {
            const SZrAotFunctionEntry *entry = &functionTable.entries[functionIndex];
            if (entry->function != ZR_NULL && !backend_aot_function_is_executable_subset(entry->function)) {
                backend_aot_report_first_unsupported_instruction("aot_c", moduleName, &functionTable);
                fclose(file);
                remove(filename);
                backend_aot_release_function_table(state, &functionTable);
                backend_aot_exec_ir_release_module(state, &module);
                return ZR_FALSE;
            }
        }
    }

    fprintf(file, "/* ZR AOT C Backend */\n");
    fprintf(file, "/* SemIR overlay + generated exec thunks. */\n");
    fprintf(file, "/* descriptor.moduleName = %s */\n", moduleName);
    fprintf(file, "/* descriptor.inputKind = %u */\n", (unsigned)inputKind);
    fprintf(file, "/* descriptor.inputHash = %s */\n", inputHash);
    fprintf(file, "/* descriptor.embeddedModuleBlobLength = %llu */\n",
            (unsigned long long)((options != ZR_NULL) ? options->embeddedModuleBlobLength : 0));
    fprintf(file, "#include \"zr_vm_common/zr_aot_abi.h\"\n");
    fprintf(file, "#include \"zr_vm_common/zr_ast_constants.h\"\n");
    fprintf(file, "#include \"zr_vm_core/call_info.h\"\n");
    fprintf(file, "#include \"zr_vm_core/closure.h\"\n");
    fprintf(file, "#include \"zr_vm_core/debug.h\"\n");
    fprintf(file, "#include \"zr_vm_core/exception.h\"\n");
    fprintf(file, "#include \"zr_vm_core/execution.h\"\n");
    fprintf(file, "#include \"zr_vm_core/execution_control.h\"\n");
    fprintf(file, "#include \"zr_vm_core/function.h\"\n");
    fprintf(file, "#include \"zr_vm_core/global.h\"\n");
    fprintf(file, "#include \"zr_vm_core/meta.h\"\n");
    fprintf(file, "#include \"zr_vm_core/module.h\"\n");
    fprintf(file, "#include \"zr_vm_core/object.h\"\n");
    fprintf(file, "#include \"zr_vm_core/ownership.h\"\n");
    fprintf(file, "#include \"zr_vm_core/reflection.h\"\n");
    fprintf(file, "#include \"zr_vm_core/string.h\"\n");
    fprintf(file, "#include \"zr_vm_core/type_layout.h\"\n");
    fprintf(file, "#include \"zr_vm_library/aot_runtime.h\"\n");
    fprintf(file, "#include <math.h>\n");
    fprintf(file, "#include <stddef.h>\n");
    fprintf(file, "#include <string.h>\n");
    fprintf(file, "\n");
    backend_aot_write_c_guard_macro(file);
    fprintf(file, "\n");
    backend_aot_write_c_contracts(file, module.runtimeContracts);
    fprintf(file, "\n");
    fprintf(file, "/*\n");
    backend_aot_write_instruction_listing(file, " * ", &module);
    fprintf(file, " */\n");
    fprintf(file, "\n");
    backend_aot_write_runtime_contract_array_c(file, module.runtimeContracts);
    fprintf(file, "\n");
    backend_aot_write_embedded_blob_c(file,
                                      options != ZR_NULL ? options->embeddedModuleBlob : ZR_NULL,
                                      options != ZR_NULL ? options->embeddedModuleBlobLength : 0);
    fprintf(file, "\n");
    backend_aot_write_c_type_layout_declarations(file, state, &functionTable);
    fprintf(file, "\n");
    backend_aot_write_c_function_forward_decls(file, &functionTable);
    fprintf(file, "\n");
    backend_aot_write_c_typed_bool_thunk_forward_decls(file, &functionTable);
    backend_aot_write_c_typed_f64_thunk_forward_decls(file, &functionTable);
    backend_aot_write_c_typed_i64_thunk_forward_decls(file, &functionTable);
    backend_aot_write_c_typed_u64_thunk_forward_decls(file, &functionTable);
    fprintf(file, "\n");
    backend_aot_write_c_method_infos(file, &functionTable, &module);
    fprintf(file, "\n");
    backend_aot_write_c_method_info_table(file, &functionTable);
    fprintf(file, "\n");
    backend_aot_write_c_function_table(file, &functionTable);
    fprintf(file, "\n");
    backend_aot_write_c_typed_bool_thunks(file, &functionTable);
    backend_aot_write_c_typed_f64_thunks(file, &functionTable);
    backend_aot_write_c_typed_i64_thunks(file, &functionTable);
    backend_aot_write_c_typed_u64_thunks(file, &functionTable);
    fprintf(file, "\n");
    for (TZrUInt32 functionIndex = 0; functionIndex < functionTable.count; functionIndex++) {
        backend_aot_write_c_function_body(file, state, &functionTable, &module, &functionTable.entries[functionIndex]);
        fprintf(file, "\n");
    }
    fprintf(file, "static const ZrAotCompiledModule zr_aot_module = {\n");
    fprintf(file, "    ZR_VM_AOT_ABI_VERSION,\n");
    fprintf(file, "    ZR_AOT_BACKEND_KIND_C,\n");
    fprintf(file, "    \"%s\",\n", moduleName);
    fprintf(file, "    %u,\n", (unsigned)inputKind);
    fprintf(file, "    \"%s\",\n", inputHash);
    fprintf(file, "    zr_aot_runtime_contracts,\n");
    fprintf(file,
            "    %s,\n",
            (options != ZR_NULL && options->embeddedModuleBlob != ZR_NULL && options->embeddedModuleBlobLength > 0)
                    ? "zr_aot_embedded_module_blob"
                    : "ZR_NULL");
    fprintf(file, "    %llu,\n",
            (unsigned long long)((options != ZR_NULL) ? options->embeddedModuleBlobLength : 0));
    fprintf(file, "    zr_aot_function_thunks,\n");
    fprintf(file, "    %u,\n", (unsigned)functionTable.count);
    if (functionTable.count > ZR_AOT_COUNT_NONE) {
        fprintf(file, "    zr_aot_fn_%u,\n", (unsigned)ZR_AOT_FUNCTION_TREE_ROOT_INDEX);
    } else {
        fprintf(file, "    ZR_NULL,\n");
    }
    fprintf(file, "    zr_aot_method_infos,\n");
    fprintf(file, "    %u,\n", (unsigned)functionTable.count);
    fprintf(file, "};\n");
    fprintf(file, "\n");
    fprintf(file, "ZR_VM_AOT_EXPORT const ZrAotCompiledModule *ZrVm_GetAotCompiledModule(void) {\n");
    fprintf(file, "    return &zr_aot_module;\n");
    fprintf(file, "}\n");

    fclose(file);
    success = ZR_TRUE;
    backend_aot_release_function_table(state, &functionTable);
    backend_aot_exec_ir_release_module(state, &module);
    return success;
}

ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFile(SZrState *state,
                                                    SZrFunction *function,
                                                    const TZrChar *filename) {
    return ZrParser_Writer_WriteAotCFileWithOptions(state, function, filename, ZR_NULL);
}
