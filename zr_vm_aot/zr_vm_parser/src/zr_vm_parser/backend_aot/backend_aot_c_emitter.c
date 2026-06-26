#include "backend_aot_c_emitter.h"
#include "backend_aot_c_function_body.h"
#include "backend_aot_c_typed_bool_thunks.h"
#include "backend_aot_c_typed_f64_thunks.h"
#include "backend_aot_c_typed_i64_thunks.h"
#include "backend_aot_c_typed_u64_thunks.h"
#include "backend_aot_c_generic_monomorphization.h"
#include "backend_aot_c_generic_sharing.h"
#include "backend_aot_c_method_metadata.h"
#include "backend_aot_c_runtime_fallback.h"
#include "backend_aot_c_type_layouts.h"
#include "backend_aot_c_zrp_metadata_prune.h"
#include "backend_aot_c_zrp_metadata_size.h"
#include "backend_aot_internal.h"
#include "backend_aot_reachability_function_graph.h"

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <string.h>

static TZrBool backend_aot_string_equals_native(const SZrString *string, const TZrChar *native) {
    const TZrChar *text;

    if (string == ZR_NULL || native == ZR_NULL) {
        return ZR_FALSE;
    }

    text = ZrCore_String_GetNativeString(string);
    return (TZrBool)(text != ZR_NULL && strcmp(text, native) == 0);
}

ZR_PARSER_API TZrBool ZrParser_Writer_ResolveTopLevelCallableFlatIndex(SZrState *state,
                                                                       SZrFunction *function,
                                                                       const TZrChar *callableName,
                                                                       TZrUInt32 *outFlatIndex) {
    SZrAotFunctionTable table;
    TZrBool success = ZR_FALSE;

    if (outFlatIndex != ZR_NULL) {
        *outFlatIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
    }
    if (state == ZR_NULL || function == ZR_NULL || callableName == ZR_NULL || callableName[0] == '\0' ||
        outFlatIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&table, 0, sizeof(table));
    if (!backend_aot_build_function_table(state, function, &table)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 bindingIndex = 0u;
         bindingIndex < function->topLevelCallableBindingLength && !success;
         bindingIndex++) {
        const SZrFunctionTopLevelCallableBinding *binding = &function->topLevelCallableBindings[bindingIndex];
        const SZrFunction *childFunction;
        TZrUInt32 flatIndex;

        if (!backend_aot_string_equals_native(binding->name, callableName)) {
            continue;
        }
        if (binding->callableChildIndex == ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE ||
            function->childFunctionList == ZR_NULL ||
            binding->callableChildIndex >= function->childFunctionLength) {
            break;
        }

        childFunction = &function->childFunctionList[binding->callableChildIndex];
        flatIndex = backend_aot_find_function_table_index(&table, childFunction);
        if (flatIndex != ZR_AOT_INVALID_FUNCTION_INDEX) {
            *outFlatIndex = flatIndex;
            success = ZR_TRUE;
        }
    }

    backend_aot_release_function_table(state, &table);
    return success;
}

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

static void backend_aot_write_manifest_generic_roots(FILE *file, const SZrAotWriterOptions *options) {
    TZrUInt32 rootCount = options != ZR_NULL ? options->manifestPreserveGenericRootCount : 0u;

    if (file == ZR_NULL) {
        return;
    }

    fprintf(file, "/* manifest.genericRoots = %u */\n", (unsigned)rootCount);
    if (rootCount > 0u &&
        (options == ZR_NULL || options->manifestPreserveGenericRoots == ZR_NULL)) {
        return;
    }
    for (TZrUInt32 rootIndex = 0u; rootIndex < rootCount; rootIndex++) {
        const SZrAotManifestGenericRoot *root = &options->manifestPreserveGenericRoots[rootIndex];
        const TZrChar *target = root->target != ZR_NULL ? root->target : "";

        fprintf(file,
                "/* manifest.genericRoot[%u] target=%s argumentCount=%u */\n",
                (unsigned)rootIndex,
                target,
                (unsigned)root->argumentCount);
        if (root->hasTypeSpecBinding) {
            fprintf(file,
                    "/* manifest.genericRoot[%u].typeSpecToken = 0x%08x */\n",
                    (unsigned)rootIndex,
                    (unsigned)root->typeSpecToken);
            fprintf(file,
                    "/* manifest.genericRoot[%u].signatureToken = 0x%08x */\n",
                    (unsigned)rootIndex,
                    (unsigned)root->signatureToken);
            fprintf(file,
                    "/* manifest.genericRoot[%u].signatureHash = 0x%016llx */\n",
                    (unsigned)rootIndex,
                    (unsigned long long)root->signatureHash);
        }
        if (root->hasMethodSpecBinding) {
            fprintf(file,
                    "/* manifest.genericRoot[%u].methodSpecToken = 0x%08x */\n",
                    (unsigned)rootIndex,
                    (unsigned)root->methodSpecToken);
            fprintf(file,
                    "/* manifest.genericRoot[%u].methodSpec.methodToken = 0x%08x */\n",
                    (unsigned)rootIndex,
                    (unsigned)root->methodSpecMethodToken);
            fprintf(file,
                    "/* manifest.genericRoot[%u].methodSpec.signatureHash = 0x%016llx */\n",
                    (unsigned)rootIndex,
                    (unsigned long long)root->methodSpecSignatureHash);
        }
        if (root->hasGenericInstantiationBinding) {
            fprintf(file,
                    "/* manifest.genericRoot[%u].genericInstance.baseToken = 0x%08x */\n",
                    (unsigned)rootIndex,
                    (unsigned)root->genericInstantiationBaseToken);
            fprintf(file,
                    "/* manifest.genericRoot[%u].genericInstance.id = %u */\n",
                    (unsigned)rootIndex,
                    (unsigned)root->genericInstantiationInstanceId);
            fprintf(file,
                    "/* manifest.genericRoot[%u].genericInstance.shareKind = %u */\n",
                    (unsigned)rootIndex,
                    (unsigned)root->genericInstantiationShareKind);
        }
        for (TZrUInt32 argumentIndex = 0u; argumentIndex < root->argumentCount; argumentIndex++) {
            const TZrChar *argument = root->arguments != ZR_NULL && root->arguments[argumentIndex] != ZR_NULL
                                              ? root->arguments[argumentIndex]
                                              : "";
            fprintf(file,
                    "/* manifest.genericRoot[%u].argument[%u] = %s */\n",
                    (unsigned)rootIndex,
                    (unsigned)argumentIndex,
                    argument);
        }
    }
}

static TZrBool backend_aot_manifest_generic_roots_closed_for_full_aot(const SZrAotWriterOptions *options) {
    TZrUInt32 rootCount;

    if (!backend_aot_option_require_full_aot(options)) {
        return ZR_TRUE;
    }

    rootCount = options != ZR_NULL ? options->manifestPreserveGenericRootCount : 0u;
    if (rootCount == 0u) {
        return ZR_TRUE;
    }
    if (options == ZR_NULL || options->manifestPreserveGenericRoots == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 rootIndex = 0u; rootIndex < rootCount; rootIndex++) {
        const SZrAotManifestGenericRoot *root = &options->manifestPreserveGenericRoots[rootIndex];
        if (root->hasMethodSpecBinding) {
            continue;
        }
        if (!root->hasTypeSpecBinding || !root->hasGenericInstantiationBinding) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
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
        const SZrAotFunctionEntry *entry = &table->entries[index];
        fprintf(file, "static TZrInt64 zr_aot_fn_%u(struct SZrState *state);\n", (unsigned)entry->flatIndex);
    }
}

static const SZrAotFunctionEntry *backend_aot_c_find_function_entry_by_flat_index(const SZrAotFunctionTable *table,
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

static void backend_aot_write_c_function_table(FILE *file,
                                               const SZrAotFunctionTable *table,
                                               TZrUInt32 functionIndexSpace) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL) {
        return;
    }

    fprintf(file, "static const FZrAotEntryThunk zr_aot_function_thunks[] = {\n");
    for (index = 0u; index < functionIndexSpace; index++) {
        const SZrAotFunctionEntry *entry = backend_aot_c_find_function_entry_by_flat_index(table, index);
        if (entry != ZR_NULL) {
            fprintf(file, "    zr_aot_fn_%u,\n", (unsigned)entry->flatIndex);
        } else {
            fprintf(file, "    ZR_NULL,\n");
        }
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

static TZrBool backend_aot_apply_code_stripping(SZrState *state,
                                                SZrAotFunctionTable *functionTable,
                                                const SZrAotWriterOptions *options) {
    SZrAotReachabilityMark *marks = ZR_NULL;
    SZrAotReachabilityEdge *edges = ZR_NULL;
    TZrUInt32 *queue = ZR_NULL;
    TZrUInt32 *roots = ZR_NULL;
    EZrAotReachabilityReason *rootReasons = ZR_NULL;
    const TZrUInt32 *manifestRoots = ZR_NULL;
    TZrUInt32 indexSpace;
    TZrUInt32 edgeCapacity = 0u;
    TZrUInt32 manifestRootCount = 0u;
    TZrUInt32 markedCount = 0u;
    TZrUInt32 edgeCount = 0u;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || state->global == ZR_NULL || functionTable == ZR_NULL) {
        return ZR_FALSE;
    }

    indexSpace = backend_aot_function_table_index_space(functionTable);
    if (indexSpace == 0u) {
        return ZR_FALSE;
    }
    if (options != ZR_NULL) {
        manifestRoots = options->manifestPreserveFunctionFlatIndices;
        manifestRootCount = options->manifestPreserveFunctionFlatIndexCount;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < functionTable->count; entryIndex++) {
        const SZrFunction *function = functionTable->entries[entryIndex].function;
        if (function == ZR_NULL) {
            return ZR_FALSE;
        }
        edgeCapacity += function->instructionsLength;
    }
    if (edgeCapacity == 0u) {
        edgeCapacity = 1u;
    }

    marks = (SZrAotReachabilityMark *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrAotReachabilityMark) * indexSpace,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    queue = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrUInt32) * indexSpace,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    roots = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrUInt32) * indexSpace,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    rootReasons = (EZrAotReachabilityReason *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(EZrAotReachabilityReason) * indexSpace,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    edges = (SZrAotReachabilityEdge *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrAotReachabilityEdge) * edgeCapacity,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);

    if (marks != ZR_NULL && queue != ZR_NULL && roots != ZR_NULL && rootReasons != ZR_NULL && edges != ZR_NULL &&
        backend_aot_compute_static_callable_reachability(state,
                                                         functionTable,
                                                         manifestRoots,
                                                         manifestRootCount,
                                                         roots,
                                                         rootReasons,
                                                         indexSpace,
                                                         marks,
                                                         indexSpace,
                                                         edges,
                                                         edgeCapacity,
                                                         queue,
                                                         indexSpace,
                                                         &markedCount,
                                                         &edgeCount) &&
        markedCount > 0u) {
        success = backend_aot_filter_function_table_by_reachability(functionTable, marks, indexSpace);
    }

    if (rootReasons != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      rootReasons,
                                      sizeof(EZrAotReachabilityReason) * indexSpace,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (roots != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      roots,
                                      sizeof(TZrUInt32) * indexSpace,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (edges != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      edges,
                                      sizeof(SZrAotReachabilityEdge) * edgeCapacity,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (queue != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      queue,
                                      sizeof(TZrUInt32) * indexSpace,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (marks != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      marks,
                                      sizeof(SZrAotReachabilityMark) * indexSpace,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    ZR_UNUSED_PARAMETER(edgeCount);
    return success;
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
    TZrUInt32 functionIndexSpace;
    TZrUInt32 typeLayoutIndexSpace;
    TZrUInt32 gcDescriptorIndexSpace;
    TZrUInt32 functionCountBeforeStripping;
    TZrUInt32 functionCountAfterStripping;
    TZrUInt32 functionCountRemovedByStripping;
    TZrUInt32 typeLayoutCountBeforeStripping;
    TZrUInt32 typeLayoutCountAfterStripping;
    TZrUInt32 typeLayoutCountRemovedByStripping;
    TZrUInt32 trimRuntimeFallbackWarningCount;
    TZrUInt32 trimRuntimeFallbackSuppressedCount;
    SZrAotCEmbeddedZrpMetadata embeddedZrpMetadata;
    SZrAotZrpMetadataSizeStats zrpMetadataSizeBeforeStripping;
    SZrAotZrpMetadataSizeStats zrpMetadataSizeAfterStripping;
    unsigned long long typeLayoutBytesBeforeStripping;
    unsigned long long typeLayoutBytesAfterStripping;
    unsigned long long typeLayoutBytesRemovedByStripping;
    unsigned long long typeLayoutGeneratedBytesBeforeStripping;
    unsigned long long typeLayoutGeneratedBytesAfterStripping;
    unsigned long long typeLayoutGeneratedBytesRemovedByStripping;
    unsigned long long methodMetadataGeneratedBytesBeforeStripping;
    unsigned long long methodMetadataGeneratedBytesAfterStripping;
    unsigned long long methodMetadataGeneratedBytesRemovedByStripping;
    unsigned long long retainedFunctionBodyBytesTotal = 0u;
    TZrBool requireExecutableLowering;
    TZrBool requireFullAot;
    TZrBool enableCodeStripping;
    TZrBool stripGeneratedSymbols;
    TZrUInt8 reflectionMetadataLevel;
    TZrUInt32 suppressedRuntimeFallbackWarningReasonMask;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || function == ZR_NULL || filename == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_manifest_generic_roots_closed_for_full_aot(options)) {
        return ZR_FALSE;
    }

    memset(&module, 0, sizeof(module));
    memset(&functionTable, 0, sizeof(functionTable));
    memset(&embeddedZrpMetadata, 0, sizeof(embeddedZrpMetadata));

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
    requireFullAot = backend_aot_option_require_full_aot(options);
    enableCodeStripping = backend_aot_option_enable_code_stripping(options);
    stripGeneratedSymbols = backend_aot_option_strip_generated_symbols(options);
    reflectionMetadataLevel = backend_aot_option_reflection_metadata_level(options);
    suppressedRuntimeFallbackWarningReasonMask =
            backend_aot_option_runtime_fallback_warning_suppression_mask(options);
    functionCountBeforeStripping = functionTable.count;
    typeLayoutCountBeforeStripping = backend_aot_c_type_layout_count_referenced(&functionTable);
    typeLayoutBytesBeforeStripping = backend_aot_c_type_layout_payload_bytes_referenced(&functionTable);
    typeLayoutGeneratedBytesBeforeStripping =
            backend_aot_c_type_layout_generated_bytes_referenced(state, &functionTable);
    methodMetadataGeneratedBytesBeforeStripping =
            backend_aot_c_method_metadata_generated_bytes_referenced(state,
                                                                     &functionTable,
                                                                     &module,
                                                                     reflectionMetadataLevel);
    backend_aot_collect_zrp_metadata_size_stats(options, &zrpMetadataSizeBeforeStripping);
    if (enableCodeStripping && !backend_aot_apply_code_stripping(state, &functionTable, options)) {
        fclose(file);
        remove(filename);
        backend_aot_release_function_table(state, &functionTable);
        backend_aot_exec_ir_release_module(state, &module);
        return ZR_FALSE;
    }
    functionCountAfterStripping = functionTable.count;
    functionCountRemovedByStripping =
            functionCountBeforeStripping >= functionCountAfterStripping
                    ? functionCountBeforeStripping - functionCountAfterStripping
                    : 0u;
    typeLayoutCountAfterStripping = backend_aot_c_type_layout_count_referenced(&functionTable);
    typeLayoutBytesAfterStripping = backend_aot_c_type_layout_payload_bytes_referenced(&functionTable);
    typeLayoutGeneratedBytesAfterStripping =
            backend_aot_c_type_layout_generated_bytes_referenced(state, &functionTable);
    methodMetadataGeneratedBytesAfterStripping =
            backend_aot_c_method_metadata_generated_bytes_referenced(state,
                                                                     &functionTable,
                                                                     &module,
                                                                     reflectionMetadataLevel);
    if (!backend_aot_c_prepare_embedded_zrp_metadata(options,
                                                     enableCodeStripping,
                                                     &functionTable,
                                                     &embeddedZrpMetadata)) {
        fclose(file);
        remove(filename);
        backend_aot_release_function_table(state, &functionTable);
        backend_aot_exec_ir_release_module(state, &module);
        return ZR_FALSE;
    }
    backend_aot_collect_zrp_metadata_size_stats_from_blob(embeddedZrpMetadata.blob,
                                                          embeddedZrpMetadata.length,
                                                          &zrpMetadataSizeAfterStripping);
    typeLayoutCountRemovedByStripping =
            typeLayoutCountBeforeStripping >= typeLayoutCountAfterStripping
                    ? typeLayoutCountBeforeStripping - typeLayoutCountAfterStripping
                    : 0u;
    typeLayoutBytesRemovedByStripping =
            typeLayoutBytesBeforeStripping >= typeLayoutBytesAfterStripping
                    ? typeLayoutBytesBeforeStripping - typeLayoutBytesAfterStripping
                    : 0u;
    typeLayoutGeneratedBytesRemovedByStripping =
            typeLayoutGeneratedBytesBeforeStripping >= typeLayoutGeneratedBytesAfterStripping
                    ? typeLayoutGeneratedBytesBeforeStripping - typeLayoutGeneratedBytesAfterStripping
                    : 0u;
    methodMetadataGeneratedBytesRemovedByStripping =
            methodMetadataGeneratedBytesBeforeStripping >= methodMetadataGeneratedBytesAfterStripping
                    ? methodMetadataGeneratedBytesBeforeStripping - methodMetadataGeneratedBytesAfterStripping
                    : 0u;
    functionIndexSpace = backend_aot_function_table_index_space(&functionTable);
    if (functionIndexSpace == 0u) {
        fclose(file);
        remove(filename);
        backend_aot_c_release_embedded_zrp_metadata(&embeddedZrpMetadata);
        backend_aot_release_function_table(state, &functionTable);
        backend_aot_exec_ir_release_module(state, &module);
        return ZR_FALSE;
    }
    typeLayoutIndexSpace = backend_aot_c_type_layout_index_space(state, &functionTable);
    gcDescriptorIndexSpace = backend_aot_c_type_layout_gc_descriptor_index_space(state, &functionTable);

    if (requireFullAot && !backend_aot_c_validate_full_aot_runtime_closure(state, &functionTable, &module)) {
        fclose(file);
        remove(filename);
        backend_aot_c_release_embedded_zrp_metadata(&embeddedZrpMetadata);
        backend_aot_release_function_table(state, &functionTable);
        backend_aot_exec_ir_release_module(state, &module);
        return ZR_FALSE;
    }
    trimRuntimeFallbackWarningCount =
            backend_aot_c_count_runtime_fallback_warnings(state,
                                                          &functionTable,
                                                          &module,
                                                          suppressedRuntimeFallbackWarningReasonMask);
    trimRuntimeFallbackSuppressedCount =
            backend_aot_c_count_suppressed_runtime_fallback_warnings(state,
                                                                     &functionTable,
                                                                     &module,
                                                                     suppressedRuntimeFallbackWarningReasonMask);

    if (requireExecutableLowering || backend_aot_report_first_unsupported_instruction("aot_c", moduleName, &functionTable)) {
        for (TZrUInt32 functionIndex = 0; functionIndex < functionTable.count; functionIndex++) {
            const SZrAotFunctionEntry *entry = &functionTable.entries[functionIndex];
            if (entry->function != ZR_NULL && !backend_aot_function_is_executable_subset(entry->function)) {
                backend_aot_report_first_unsupported_instruction("aot_c", moduleName, &functionTable);
                fclose(file);
                remove(filename);
                backend_aot_c_release_embedded_zrp_metadata(&embeddedZrpMetadata);
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
    fprintf(file, "/* code_stripping.enabled = %u */\n", enableCodeStripping ? 1u : 0u);
    fprintf(file, "/* symbol_stripping.generatedSymbols = %u */\n", stripGeneratedSymbols ? 1u : 0u);
    fprintf(file, "/* metadata_policy.reflectionLevel = %u */\n", (unsigned)reflectionMetadataLevel);
    fprintf(file, "/* code_stripping.functionsBefore = %u */\n", (unsigned)functionCountBeforeStripping);
    fprintf(file, "/* code_stripping.functionsAfter = %u */\n", (unsigned)functionCountAfterStripping);
    fprintf(file, "/* code_stripping.functionsRemoved = %u */\n", (unsigned)functionCountRemovedByStripping);
    fprintf(file, "/* code_stripping.typeLayoutsBefore = %u */\n", (unsigned)typeLayoutCountBeforeStripping);
    fprintf(file, "/* code_stripping.typeLayoutsAfter = %u */\n", (unsigned)typeLayoutCountAfterStripping);
    fprintf(file, "/* code_stripping.typeLayoutsRemoved = %u */\n", (unsigned)typeLayoutCountRemovedByStripping);
    fprintf(file,
            "/* code_stripping.typeLayoutPayloadBytesBefore = %llu */\n",
            typeLayoutBytesBeforeStripping);
    fprintf(file,
            "/* code_stripping.typeLayoutPayloadBytesAfter = %llu */\n",
            typeLayoutBytesAfterStripping);
    fprintf(file,
            "/* code_stripping.typeLayoutPayloadBytesRemoved = %llu */\n",
            typeLayoutBytesRemovedByStripping);
    fprintf(file,
            "/* code_stripping.typeLayoutGeneratedBytesBefore = %llu */\n",
            typeLayoutGeneratedBytesBeforeStripping);
    fprintf(file,
            "/* code_stripping.typeLayoutGeneratedBytesAfter = %llu */\n",
            typeLayoutGeneratedBytesAfterStripping);
    fprintf(file,
            "/* code_stripping.typeLayoutGeneratedBytesRemoved = %llu */\n",
            typeLayoutGeneratedBytesRemovedByStripping);
    fprintf(file,
            "/* code_stripping.methodMetadataGeneratedBytesBefore = %llu */\n",
            methodMetadataGeneratedBytesBeforeStripping);
    fprintf(file,
            "/* code_stripping.methodMetadataGeneratedBytesAfter = %llu */\n",
            methodMetadataGeneratedBytesAfterStripping);
    fprintf(file,
            "/* code_stripping.methodMetadataGeneratedBytesRemoved = %llu */\n",
            methodMetadataGeneratedBytesRemovedByStripping);
    backend_aot_write_code_stripping_zrp_metadata_size_deltas(file,
                                                              &zrpMetadataSizeBeforeStripping,
                                                              &zrpMetadataSizeAfterStripping);
    backend_aot_write_manifest_generic_roots(file, options);
    fprintf(file,
            "/* trim_warnings.runtimeFallbackCount = %u */\n",
            (unsigned)trimRuntimeFallbackWarningCount);
    fprintf(file,
            "/* trim_warnings.runtimeFallbackSuppressedCount = %u */\n",
            (unsigned)trimRuntimeFallbackSuppressedCount);
    backend_aot_write_c_trim_warnings(file,
                                      state,
                                      &functionTable,
                                      &module,
                                      suppressedRuntimeFallbackWarningReasonMask);
    fprintf(file, "/* descriptor.embeddedModuleBlobLength = %llu */\n",
            (unsigned long long)embeddedZrpMetadata.length);
    fprintf(file, "/* aot_size.embeddedModuleBytes = %llu */\n",
            (unsigned long long)embeddedZrpMetadata.length);
    backend_aot_write_zrp_metadata_size_stats(file, &zrpMetadataSizeAfterStripping);
    fprintf(file, "#include \"zr_vm_common/zr_aot_abi.h\"\n");
    fprintf(file, "#include \"zr_vm_common/zr_ast_constants.h\"\n");
    fprintf(file, "#include \"zr_vm_core/call_info.h\"\n");
    fprintf(file, "#include \"zr_vm_core/closure.h\"\n");
    fprintf(file, "#include \"zr_vm_core/debug.h\"\n");
    fprintf(file, "#include \"zr_vm_core/exception.h\"\n");
    fprintf(file, "#include \"zr_vm_core/execution.h\"\n");
    fprintf(file, "#include \"zr_vm_core/execution_control.h\"\n");
    fprintf(file, "#include \"zr_vm_core/function.h\"\n");
    fprintf(file, "#include \"zr_vm_core/gc.h\"\n");
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
    backend_aot_write_c_generic_dictionary_macros(file);
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
                                      embeddedZrpMetadata.blob,
                                      embeddedZrpMetadata.length);
    fprintf(file, "\n");
    backend_aot_write_c_type_layout_declarations(file, state, &functionTable);
    backend_aot_write_c_type_layout_gc_descriptor_table(file, state, &functionTable, gcDescriptorIndexSpace);
    backend_aot_write_c_type_layout_registration_table(file, state, &functionTable, typeLayoutIndexSpace);
    backend_aot_write_c_type_layout_token_table(file, state, &functionTable, typeLayoutIndexSpace);
    backend_aot_write_c_generic_monomorphization_layouts(file, state, &functionTable);
    fprintf(file, "\n");
    backend_aot_write_c_function_forward_decls(file, &functionTable);
    fprintf(file, "\n");
    backend_aot_write_c_generic_monomorphization_entries(file, &functionTable, stripGeneratedSymbols);
    backend_aot_write_c_generic_sharing_entries(file, &functionTable, stripGeneratedSymbols);
    backend_aot_write_c_typed_bool_thunk_forward_decls(file, &functionTable);
    backend_aot_write_c_typed_f64_thunk_forward_decls(file, &functionTable);
    backend_aot_write_c_typed_i64_thunk_forward_decls(file, &functionTable);
    backend_aot_write_c_typed_u64_thunk_forward_decls(file, &functionTable);
    fprintf(file, "\n");
    backend_aot_write_c_reflection_invokers(file);
    fprintf(file, "\n");
    backend_aot_write_c_method_infos(file, state, &functionTable, &module, reflectionMetadataLevel);
    fprintf(file, "\n");
    backend_aot_write_c_method_info_table(file, &functionTable, functionIndexSpace);
    fprintf(file, "\n");
    backend_aot_write_c_function_table(file, &functionTable, functionIndexSpace);
    fprintf(file, "\n");
    backend_aot_write_c_typed_bool_thunks(file, &functionTable);
    backend_aot_write_c_typed_f64_thunks(file, &functionTable);
    backend_aot_write_c_typed_i64_thunks(file, &functionTable);
    backend_aot_write_c_typed_u64_thunks(file, &functionTable);
    fprintf(file, "\n");
    for (TZrUInt32 functionIndex = 0; functionIndex < functionTable.count; functionIndex++) {
        const SZrAotFunctionEntry *entry = &functionTable.entries[functionIndex];
        long functionBodyStart = -1;
        long functionBodyEnd = -1;

        if (enableCodeStripping) {
            functionBodyStart = ftell(file);
        }
        backend_aot_write_c_function_body(file,
                                          state,
                                          &functionTable,
                                          &module,
                                          entry,
                                          options);
        if (enableCodeStripping) {
            functionBodyEnd = ftell(file);
            if (functionBodyStart >= 0 && functionBodyEnd >= functionBodyStart) {
                retainedFunctionBodyBytesTotal += (unsigned long long)(functionBodyEnd - functionBodyStart);
            }
            fprintf(file,
                    "/* code_stripping.functionBodyBytes[%u] = %llu */\n",
                    (unsigned)entry->flatIndex,
                    (unsigned long long)((functionBodyStart >= 0 &&
                                          functionBodyEnd >= functionBodyStart)
                                                 ? functionBodyEnd - functionBodyStart
                                                 : 0));
        }
        fprintf(file, "\n");
    }
    if (enableCodeStripping) {
        fprintf(file,
                "/* code_stripping.functionBodyBytesTotal = %llu */\n\n",
                retainedFunctionBodyBytesTotal);
    }
    fprintf(file, "static const SZrAotCodeRegistration zr_aot_code_registration = {\n");
    fprintf(file, "    .functionCount = %u,\n", (unsigned)functionIndexSpace);
    fprintf(file, "    .functionPointers = zr_aot_function_thunks,\n");
    fprintf(file, "    .methodInfos = zr_aot_method_infos,\n");
    fprintf(file, "    .methodInfoCount = %u,\n", (unsigned)functionIndexSpace);
    fprintf(file, "    .invokers = zr_aot_reflection_invokers,\n");
    fprintf(file, "    .invokerCount = 1u,\n");
    fprintf(file, "    .typeLayouts = %s,\n", typeLayoutIndexSpace > 0u ? "zr_aot_type_layouts" : "ZR_NULL");
    fprintf(file, "    .typeLayoutCount = %u,\n", (unsigned)typeLayoutIndexSpace);
    fprintf(file, "    .typeLayoutTokens = %s,\n", typeLayoutIndexSpace > 0u ? "zr_aot_type_layout_tokens" : "ZR_NULL");
    fprintf(file, "    .typeLayoutTokenCount = %u,\n", (unsigned)typeLayoutIndexSpace);
    fprintf(file, "    .gcDescriptors = %s,\n", gcDescriptorIndexSpace > 0u ? "zr_aot_gc_descriptors" : "ZR_NULL");
    fprintf(file, "    .gcDescriptorCount = %u,\n", (unsigned)gcDescriptorIndexSpace);
    fprintf(file, "};\n");
    fprintf(file, "\n");
    fprintf(file, "static const ZrAotCompiledModule zr_aot_module = {\n");
    fprintf(file, "    .abiVersion = ZR_VM_AOT_ABI_VERSION,\n");
    fprintf(file, "    .backendKind = ZR_AOT_BACKEND_KIND_C,\n");
    fprintf(file, "    .moduleName = \"%s\",\n", moduleName);
    fprintf(file, "    .inputKind = %u,\n", (unsigned)inputKind);
    fprintf(file, "    .inputHash = \"%s\",\n", inputHash);
    fprintf(file, "    .runtimeContracts = zr_aot_runtime_contracts,\n");
    fprintf(file,
            "    .embeddedModuleBlob = %s,\n",
            (embeddedZrpMetadata.blob != ZR_NULL && embeddedZrpMetadata.length > 0u)
                    ? "zr_aot_embedded_module_blob"
                    : "ZR_NULL");
    fprintf(file, "    .embeddedModuleBlobLength = %llu,\n",
            (unsigned long long)embeddedZrpMetadata.length);
    fprintf(file, "    .functionThunks = zr_aot_function_thunks,\n");
    fprintf(file, "    .functionThunkCount = %u,\n", (unsigned)functionIndexSpace);
    if (backend_aot_c_find_function_entry_by_flat_index(&functionTable, ZR_AOT_FUNCTION_TREE_ROOT_INDEX) != ZR_NULL) {
        fprintf(file, "    .entryThunk = zr_aot_fn_%u,\n", (unsigned)ZR_AOT_FUNCTION_TREE_ROOT_INDEX);
    } else {
        fprintf(file, "    .entryThunk = ZR_NULL,\n");
    }
    fprintf(file, "    .methodInfos = zr_aot_method_infos,\n");
    fprintf(file, "    .methodInfoCount = %u,\n", (unsigned)functionIndexSpace);
    fprintf(file, "    .typeLayouts = %s,\n", typeLayoutIndexSpace > 0u ? "zr_aot_type_layouts" : "ZR_NULL");
    fprintf(file, "    .typeLayoutCount = %u,\n", (unsigned)typeLayoutIndexSpace);
    fprintf(file, "    .typeLayoutTokens = %s,\n", typeLayoutIndexSpace > 0u ? "zr_aot_type_layout_tokens" : "ZR_NULL");
    fprintf(file, "    .typeLayoutTokenCount = %u,\n", (unsigned)typeLayoutIndexSpace);
    fprintf(file, "    .gcDescriptors = %s,\n", gcDescriptorIndexSpace > 0u ? "zr_aot_gc_descriptors" : "ZR_NULL");
    fprintf(file, "    .gcDescriptorCount = %u,\n", (unsigned)gcDescriptorIndexSpace);
    fprintf(file, "    .codeRegistration = &zr_aot_code_registration,\n");
    fprintf(file, "};\n");
    fprintf(file, "\n");
    fprintf(file, "ZR_VM_AOT_EXPORT const ZrAotCompiledModule *ZrVm_GetAotCompiledModule(void) {\n");
    fprintf(file, "    return &zr_aot_module;\n");
    fprintf(file, "}\n");

    fclose(file);
    success = ZR_TRUE;
    backend_aot_c_release_embedded_zrp_metadata(&embeddedZrpMetadata);
    backend_aot_release_function_table(state, &functionTable);
    backend_aot_exec_ir_release_module(state, &module);
    return success;
}

ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFile(SZrState *state,
                                                    SZrFunction *function,
                                                    const TZrChar *filename) {
    return ZrParser_Writer_WriteAotCFileWithOptions(state, function, filename, ZR_NULL);
}
