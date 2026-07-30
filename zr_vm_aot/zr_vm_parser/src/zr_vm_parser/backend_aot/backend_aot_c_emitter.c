#include "backend_aot_c_emitter.h"
#include "backend_aot_c_function_body.h"
#include "backend_aot_c_annotation_warnings.h"
#include "backend_aot_c_typed_bool_thunks.h"
#include "backend_aot_c_typed_f64_thunks.h"
#include "backend_aot_c_typed_i64_thunks.h"
#include "backend_aot_c_typed_u64_thunks.h"
#include "backend_aot_c_generic_monomorphization.h"
#include "backend_aot_c_generic_sharing.h"
#include "backend_aot_c_method_metadata.h"
#include "backend_aot_c_native_imports.h"
#include "backend_aot_c_reference_locals.h"
#include "backend_aot_c_reflection_invokers.h"
#include "backend_aot_c_runtime_fallback.h"
#include "backend_aot_c_type_layout_reachability.h"
#include "backend_aot_c_type_layouts.h"
#include "backend_aot_c_zrp_metadata_prune.h"
#include "backend_aot_c_zrp_metadata_member_token.h"
#include "backend_aot_c_zrp_metadata_publication.h"
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

static const TZrChar *backend_aot_manifest_export_kind_name(EZrAotManifestExportDeclarationKind kind) {
    switch (kind) {
        case ZR_AOT_MANIFEST_EXPORT_DECLARATION_TYPE:
            return "type";
        case ZR_AOT_MANIFEST_EXPORT_DECLARATION_METHOD:
            return "method";
        case ZR_AOT_MANIFEST_EXPORT_DECLARATION_FIELD:
            return "field";
        default:
            return "unknown";
    }
}

static void backend_aot_write_manifest_export_declarations(FILE *file, const SZrAotWriterOptions *options) {
    TZrUInt32 exportCount = options != ZR_NULL ? options->manifestExportDeclarationCount : 0u;

    if (file == ZR_NULL) {
        return;
    }

    fprintf(file, "/* manifest.exports = %u */\n", (unsigned)exportCount);
    if (exportCount > 0u &&
        (options == ZR_NULL || options->manifestExportDeclarations == ZR_NULL)) {
        return;
    }

    for (TZrUInt32 exportIndex = 0u; exportIndex < exportCount; exportIndex++) {
        const SZrAotManifestExportDeclaration *declaration = &options->manifestExportDeclarations[exportIndex];
        const TZrChar *target = declaration->target != ZR_NULL ? declaration->target : "";

        fprintf(file,
                "/* manifest.export[%u] kind=%s target=%s */\n",
                (unsigned)exportIndex,
                backend_aot_manifest_export_kind_name(declaration->kind),
                target);
        if (declaration->hasTypeTokenBinding) {
            fprintf(file,
                    "/* manifest.export[%u].typeToken = 0x%08x */\n",
                    (unsigned)exportIndex,
                    (unsigned)declaration->typeToken);
        }
        if (declaration->hasMemberTokenBinding) {
            fprintf(file,
                    "/* manifest.export[%u].memberToken = 0x%08x */\n",
                    (unsigned)exportIndex,
                    (unsigned)declaration->memberToken);
        }
    }
}

static TZrUInt32 backend_aot_manifest_export_entry_count(const SZrAotCEmbeddedZrpMetadata *metadata) {
    if (metadata == ZR_NULL || metadata->manifestExportEntries == ZR_NULL) {
        return 0u;
    }
    return metadata->manifestExportCount;
}

static const TZrChar *backend_aot_manifest_export_table_name(const SZrAotCEmbeddedZrpMetadata *metadata) {
    return backend_aot_manifest_export_entry_count(metadata) > 0u ? "zr_aot_manifest_exports" : "ZR_NULL";
}

static void backend_aot_write_c_string_literal(FILE *file, const TZrChar *text) {
    const unsigned char *cursor;

    if (file == ZR_NULL) {
        return;
    }
    if (text == ZR_NULL) {
        fprintf(file, "ZR_NULL");
        return;
    }

    fputc('"', file);
    for (cursor = (const unsigned char *)(const void *)text; *cursor != '\0'; cursor++) {
        switch (*cursor) {
            case '\\':
                fprintf(file, "\\\\");
                break;
            case '"':
                fprintf(file, "\\\"");
                break;
            case '\n':
                fprintf(file, "\\n");
                break;
            case '\r':
                fprintf(file, "\\r");
                break;
            case '\t':
                fprintf(file, "\\t");
                break;
            default:
                if (*cursor >= 32u && *cursor <= 126u) {
                    fputc((int)*cursor, file);
                } else {
                    fprintf(file, "\\%03o", (unsigned)*cursor);
                }
                break;
        }
    }
    fputc('"', file);
}

static void backend_aot_write_manifest_export_table_markers(FILE *file,
                                                            const SZrAotCEmbeddedZrpMetadata *metadata) {
    TZrUInt32 count;

    if (file == ZR_NULL) {
        return;
    }

    count = backend_aot_manifest_export_entry_count(metadata);
    fprintf(file, "/* manifest.exportTableEntries = %u */\n", (unsigned)count);
    for (TZrUInt32 index = 0u; index < count; index++) {
        const SZrAotManifestExportEntry *entry = &metadata->manifestExportEntries[index];
        fprintf(file,
                "/* manifest.exportTable[%u] kind=%u target=%s */\n",
                (unsigned)index,
                (unsigned)entry->kind,
                entry->target != ZR_NULL ? entry->target : "");
        if ((entry->flags & ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_TYPE_TOKEN) != 0u) {
            fprintf(file,
                    "/* manifest.exportTable[%u].typeToken = 0x%08x */\n",
                    (unsigned)index,
                    (unsigned)entry->typeToken);
        }
        if ((entry->flags & ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN) != 0u) {
            fprintf(file,
                    "/* manifest.exportTable[%u].memberToken = 0x%08x */\n",
                    (unsigned)index,
                    (unsigned)entry->memberToken);
        }
    }
}

static void backend_aot_write_manifest_export_table(FILE *file,
                                                    const SZrAotCEmbeddedZrpMetadata *metadata) {
    TZrUInt32 count;

    if (file == ZR_NULL) {
        return;
    }

    count = backend_aot_manifest_export_entry_count(metadata);
    if (count == 0u) {
        return;
    }

    fprintf(file, "static const SZrAotManifestExportEntry zr_aot_manifest_exports[] = {\n");
    for (TZrUInt32 index = 0u; index < count; index++) {
        const SZrAotManifestExportEntry *entry = &metadata->manifestExportEntries[index];
        fprintf(file,
                "    { .kind = %uu, .flags = %uu, .target = ",
                (unsigned)entry->kind,
                (unsigned)entry->flags);
        backend_aot_write_c_string_literal(file, entry->target);
        fprintf(file,
                ", .typeToken = 0x%08xu, .memberToken = 0x%08xu },\n",
                (unsigned)entry->typeToken,
                (unsigned)entry->memberToken);
    }
    fprintf(file, "};\n");
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

static TZrUInt32 backend_aot_member_token_remap_count(const SZrAotCEmbeddedZrpMetadata *metadata) {
    if (metadata == ZR_NULL || metadata->memberTokenRemapEntries == ZR_NULL) {
        return 0u;
    }
    return metadata->memberTokenRemapCount;
}

static const TZrChar *backend_aot_member_token_remap_table_name(const SZrAotCEmbeddedZrpMetadata *metadata) {
    return backend_aot_member_token_remap_count(metadata) > 0u ? "zr_aot_member_token_remaps" : "ZR_NULL";
}

static void backend_aot_write_member_token_remap_markers(FILE *file,
                                                         const SZrAotCEmbeddedZrpMetadata *metadata) {
    TZrUInt32 count;

    if (file == ZR_NULL) {
        return;
    }

    count = backend_aot_member_token_remap_count(metadata);
    fprintf(file, "/* code_stripping.memberTokenRemaps = %u */\n", (unsigned)count);
    for (TZrUInt32 index = 0u; index < count; index++) {
        const SZrAotCZrpMemberTokenRemapEntry *entry = &metadata->memberTokenRemapEntries[index];
        fprintf(file,
                "/* code_stripping.memberTokenRemap[%u].sourceToken = 0x%08x */\n",
                (unsigned)index,
                (unsigned)entry->sourceToken);
        fprintf(file,
                "/* code_stripping.memberTokenRemap[%u].targetToken = 0x%08x */\n",
                (unsigned)index,
                (unsigned)entry->targetToken);
    }
}

static void backend_aot_write_member_token_remap_table(FILE *file,
                                                       const SZrAotCEmbeddedZrpMetadata *metadata) {
    TZrUInt32 count;

    if (file == ZR_NULL) {
        return;
    }

    count = backend_aot_member_token_remap_count(metadata);
    if (count == 0u) {
        return;
    }

    fprintf(file, "static const SZrAotMemberTokenRemap zr_aot_member_token_remaps[] = {\n");
    for (TZrUInt32 index = 0u; index < count; index++) {
        const SZrAotCZrpMemberTokenRemapEntry *entry = &metadata->memberTokenRemapEntries[index];
        fprintf(file,
                "    { .sourceToken = 0x%08xu, .targetToken = 0x%08xu },\n",
                (unsigned)entry->sourceToken,
                (unsigned)entry->targetToken);
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

static void backend_aot_release_annotation_roots(SZrState *state,
                                                 TZrUInt32 *annotationRoots,
                                                 TZrUInt32 annotationRootCapacity) {
    if (state == ZR_NULL || state->global == ZR_NULL || annotationRoots == ZR_NULL ||
        annotationRootCapacity == 0u) {
        return;
    }
    ZrCore_Memory_RawFreeWithType(state->global,
                                  annotationRoots,
                                  sizeof(TZrUInt32) * annotationRootCapacity,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
}

static TZrBool backend_aot_apply_code_stripping(FILE *file,
                                                SZrState *state,
                                                SZrAotFunctionTable *functionTable,
                                                const SZrAotWriterOptions *options,
                                                const TZrUInt32 *annotationRoots,
                                                TZrUInt32 annotationRootCount) {
    SZrAotReachabilityMark *marks = ZR_NULL;
    SZrAotReachabilityEdge *edges = ZR_NULL;
    TZrUInt32 *queue = ZR_NULL;
    TZrUInt32 *roots = ZR_NULL;
    EZrAotReachabilityReason *rootReasons = ZR_NULL;
    const TZrUInt32 *manifestRoots = ZR_NULL;
    const SZrAotManifestGenericRoot *genericRoots = ZR_NULL;
    const SZrAotManifestExportDeclaration *manifestExports = ZR_NULL;
    TZrUInt32 indexSpace;
    TZrUInt32 edgeCapacity = 0u;
    TZrUInt32 manifestRootCount = 0u;
    TZrUInt32 genericRootCount = 0u;
    TZrUInt32 manifestExportCount = 0u;
    TZrUInt32 markedCount = 0u;
    TZrUInt32 edgeCount = 0u;
    TZrBool success = ZR_FALSE;

    if (file == ZR_NULL || state == ZR_NULL || state->global == ZR_NULL || functionTable == ZR_NULL) {
        return ZR_FALSE;
    }

    indexSpace = backend_aot_function_table_index_space(functionTable);
    if (indexSpace == 0u) {
        return ZR_FALSE;
    }
    if (options != ZR_NULL) {
        manifestRoots = options->manifestPreserveFunctionFlatIndices;
        manifestRootCount = options->manifestPreserveFunctionFlatIndexCount;
        genericRoots = options->manifestPreserveGenericRoots;
        genericRootCount = options->manifestPreserveGenericRootCount;
        manifestExports = options->manifestExportDeclarations;
        manifestExportCount = options->manifestExportDeclarationCount;
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
        backend_aot_compute_static_callable_reachability_with_preserve_roots(
                state,
                functionTable,
                annotationRoots,
                annotationRootCount,
                manifestRoots,
                manifestRootCount,
                genericRoots,
                genericRootCount,
                manifestExports,
                manifestExportCount,
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
        markedCount > 0u &&
        backend_aot_reachability_write_function_manifest(file, marks, indexSpace)) {
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
    TZrUInt32 nativeImportContractCount;
    TZrUInt32 functionCountBeforeStripping;
    TZrUInt32 functionCountAfterStripping;
    TZrUInt32 functionCountRemovedByStripping;
    TZrUInt32 typeLayoutCountBeforeStripping;
    TZrUInt32 typeLayoutCountAfterStripping;
    TZrUInt32 typeLayoutCountRemovedByStripping;
    TZrUInt32 trimRuntimeFallbackWarningCount;
    TZrUInt32 trimRuntimeFallbackSuppressedCount;
    TZrUInt32 trimRuntimeFallbackWarningReasonMask;
    TZrUInt32 trimRuntimeFallbackSuppressedReasonMask;
    TZrUInt32 trimAnnotationWarningCount;
    TZrUInt32 trimAnnotationSuppressedCount;
    TZrUInt32 trimAnnotationWarningTotal;
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
    TZrUInt32 *annotationRoots = ZR_NULL;
    TZrUInt32 annotationRootCount = 0u;
    TZrUInt32 annotationRootCapacity = 0u;
    TZrUInt32 *annotationTypeLayoutRoots = ZR_NULL;
    TZrUInt32 annotationTypeLayoutRootCount = 0u;
    TZrUInt32 annotationTypeLayoutRootCapacity = 0u;
    TZrBool requireExecutableLowering;
    TZrBool requireFullAot;
    TZrBool enableCodeStripping;
    TZrBool stripGeneratedSymbols;
    TZrBool suppressAnnotationWarnings;
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
    suppressAnnotationWarnings = backend_aot_option_suppress_annotation_warnings(options);
    reflectionMetadataLevel = backend_aot_option_reflection_metadata_level(options);
    suppressedRuntimeFallbackWarningReasonMask =
            backend_aot_option_runtime_fallback_warning_suppression_mask(options);
    functionCountBeforeStripping = functionTable.count;
    annotationTypeLayoutRootCapacity = backend_aot_function_table_index_space(&functionTable);
    if (annotationTypeLayoutRootCapacity > 0u) {
        annotationTypeLayoutRoots = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(TZrUInt32) * annotationTypeLayoutRootCapacity,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (annotationTypeLayoutRoots == ZR_NULL ||
            !backend_aot_c_type_layout_collect_dynamic_dependency_roots(state,
                                                                        &functionTable,
                                                                        options != ZR_NULL
                                                                                ? options->embeddedModuleBlob
                                                                                : ZR_NULL,
                                                                        options != ZR_NULL
                                                                                ? options->embeddedModuleBlobLength
                                                                                : 0u,
                                                                        annotationTypeLayoutRoots,
                                                                        annotationTypeLayoutRootCapacity,
                                                                        &annotationTypeLayoutRootCount)) {
            fclose(file);
            remove(filename);
            backend_aot_release_annotation_roots(state,
                                                 annotationTypeLayoutRoots,
                                                 annotationTypeLayoutRootCapacity);
            backend_aot_release_function_table(state, &functionTable);
            backend_aot_exec_ir_release_module(state, &module);
            return ZR_FALSE;
        }
    }
    typeLayoutCountBeforeStripping = backend_aot_c_type_layout_count_referenced(state,
                                                                               &functionTable,
                                                                               annotationTypeLayoutRoots,
                                                                               annotationTypeLayoutRootCount);
    typeLayoutBytesBeforeStripping = backend_aot_c_type_layout_payload_bytes_referenced(state,
                                                                                       &functionTable,
                                                                                       annotationTypeLayoutRoots,
                                                                                       annotationTypeLayoutRootCount);
    typeLayoutGeneratedBytesBeforeStripping =
            backend_aot_c_type_layout_generated_bytes_referenced(state,
                                                                 &functionTable,
                                                                 annotationTypeLayoutRoots,
                                                                 annotationTypeLayoutRootCount);
    methodMetadataGeneratedBytesBeforeStripping =
            backend_aot_c_method_metadata_generated_bytes_referenced(state,
                                                                     &functionTable,
                                                                     &module,
                                                                     reflectionMetadataLevel);
    annotationRootCapacity = backend_aot_function_table_index_space(&functionTable);
    if (annotationRootCapacity > 0u) {
        annotationRoots = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                       sizeof(TZrUInt32) * annotationRootCapacity,
                                                                       ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (annotationRoots == ZR_NULL ||
            !backend_aot_collect_reflection_annotation_roots(state,
                                                             &functionTable,
                                                             annotationRoots,
                                                             annotationRootCapacity,
                                                             &annotationRootCount)) {
            fclose(file);
            remove(filename);
            backend_aot_release_annotation_roots(state,
                                                 annotationTypeLayoutRoots,
                                                 annotationTypeLayoutRootCapacity);
            backend_aot_release_annotation_roots(state, annotationRoots, annotationRootCapacity);
            backend_aot_release_function_table(state, &functionTable);
            backend_aot_exec_ir_release_module(state, &module);
            return ZR_FALSE;
        }
    }
    backend_aot_collect_zrp_metadata_size_stats(options, &zrpMetadataSizeBeforeStripping);
    if (enableCodeStripping &&
        !backend_aot_apply_code_stripping(file,
                                          state,
                                          &functionTable,
                                          options,
                                          annotationRoots,
                                          annotationRootCount)) {
        fclose(file);
        remove(filename);
        backend_aot_release_annotation_roots(state,
                                             annotationTypeLayoutRoots,
                                             annotationTypeLayoutRootCapacity);
        backend_aot_release_annotation_roots(state, annotationRoots, annotationRootCapacity);
        backend_aot_release_function_table(state, &functionTable);
        backend_aot_exec_ir_release_module(state, &module);
        return ZR_FALSE;
    }
    functionCountAfterStripping = functionTable.count;
    functionCountRemovedByStripping =
            functionCountBeforeStripping >= functionCountAfterStripping
                    ? functionCountBeforeStripping - functionCountAfterStripping
                    : 0u;
    if (!backend_aot_c_native_import_count(
                &functionTable, &nativeImportContractCount)) {
        fclose(file);
        remove(filename);
        backend_aot_release_annotation_roots(state,
                                             annotationTypeLayoutRoots,
                                             annotationTypeLayoutRootCapacity);
        backend_aot_release_annotation_roots(state, annotationRoots, annotationRootCapacity);
        backend_aot_release_function_table(state, &functionTable);
        backend_aot_exec_ir_release_module(state, &module);
        return ZR_FALSE;
    }
    typeLayoutCountAfterStripping = backend_aot_c_type_layout_count_referenced(state,
                                                                              &functionTable,
                                                                              annotationTypeLayoutRoots,
                                                                              annotationTypeLayoutRootCount);
    typeLayoutBytesAfterStripping = backend_aot_c_type_layout_payload_bytes_referenced(state,
                                                                                      &functionTable,
                                                                                      annotationTypeLayoutRoots,
                                                                                      annotationTypeLayoutRootCount);
    typeLayoutGeneratedBytesAfterStripping =
            backend_aot_c_type_layout_generated_bytes_referenced(state,
                                                                 &functionTable,
                                                                 annotationTypeLayoutRoots,
                                                                 annotationTypeLayoutRootCount);
    if (enableCodeStripping &&
        !backend_aot_c_type_layout_reachability_write_manifest(file,
                                                                state,
                                                                &functionTable,
                                                                annotationTypeLayoutRoots,
                                                                annotationTypeLayoutRootCount,
                                                                typeLayoutCountAfterStripping)) {
        fclose(file);
        remove(filename);
        backend_aot_release_annotation_roots(state,
                                             annotationTypeLayoutRoots,
                                             annotationTypeLayoutRootCapacity);
        backend_aot_release_annotation_roots(state, annotationRoots, annotationRootCapacity);
        backend_aot_release_function_table(state, &functionTable);
        backend_aot_exec_ir_release_module(state, &module);
        return ZR_FALSE;
    }
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
        backend_aot_release_annotation_roots(state,
                                             annotationTypeLayoutRoots,
                                             annotationTypeLayoutRootCapacity);
        backend_aot_release_annotation_roots(state, annotationRoots, annotationRootCapacity);
        backend_aot_release_function_table(state, &functionTable);
        backend_aot_exec_ir_release_module(state, &module);
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_manifest_export_table_build(
                &embeddedZrpMetadata,
                options != ZR_NULL ? options->manifestExportDeclarations : ZR_NULL,
                options != ZR_NULL ? options->manifestExportDeclarationCount : 0u)) {
        fclose(file);
        remove(filename);
        backend_aot_release_annotation_roots(state,
                                             annotationTypeLayoutRoots,
                                             annotationTypeLayoutRootCapacity);
        backend_aot_release_annotation_roots(state, annotationRoots, annotationRootCapacity);
        backend_aot_c_release_embedded_zrp_metadata(&embeddedZrpMetadata);
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
        backend_aot_release_annotation_roots(state,
                                             annotationTypeLayoutRoots,
                                             annotationTypeLayoutRootCapacity);
        backend_aot_release_annotation_roots(state, annotationRoots, annotationRootCapacity);
        backend_aot_c_release_embedded_zrp_metadata(&embeddedZrpMetadata);
        backend_aot_release_function_table(state, &functionTable);
        backend_aot_exec_ir_release_module(state, &module);
        return ZR_FALSE;
    }
    typeLayoutIndexSpace = backend_aot_c_type_layout_index_space(state,
                                                                 &functionTable,
                                                                 annotationTypeLayoutRoots,
                                                                 annotationTypeLayoutRootCount);
    gcDescriptorIndexSpace = backend_aot_c_type_layout_gc_descriptor_index_space(state,
                                                                                &functionTable,
                                                                                annotationTypeLayoutRoots,
                                                                                annotationTypeLayoutRootCount);

    if (requireFullAot && !backend_aot_c_validate_full_aot_runtime_closure(state, &functionTable, &module)) {
        fclose(file);
        remove(filename);
        backend_aot_release_annotation_roots(state,
                                             annotationTypeLayoutRoots,
                                             annotationTypeLayoutRootCapacity);
        backend_aot_release_annotation_roots(state, annotationRoots, annotationRootCapacity);
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
    trimRuntimeFallbackWarningReasonMask =
            backend_aot_c_runtime_fallback_warning_reason_mask(state,
                                                               &functionTable,
                                                               &module,
                                                               suppressedRuntimeFallbackWarningReasonMask);
    trimRuntimeFallbackSuppressedReasonMask =
            backend_aot_c_suppressed_runtime_fallback_warning_reason_mask(state,
                                                                          &functionTable,
                                                                          &module,
                                                                          suppressedRuntimeFallbackWarningReasonMask);
    trimAnnotationWarningCount =
            enableCodeStripping
                    ? backend_aot_c_count_annotation_warnings(state, &functionTable)
                    : 0u;
    trimAnnotationSuppressedCount =
            enableCodeStripping
                    ? backend_aot_c_count_suppressed_annotation_warnings(state, &functionTable)
                    : 0u;
    trimAnnotationWarningTotal = trimAnnotationWarningCount + trimAnnotationSuppressedCount;
    if (suppressAnnotationWarnings) {
        trimAnnotationWarningCount = 0u;
        trimAnnotationSuppressedCount = trimAnnotationWarningTotal;
    }

    if (requireExecutableLowering || backend_aot_report_first_unsupported_instruction("aot_c", moduleName, &functionTable)) {
        for (TZrUInt32 functionIndex = 0; functionIndex < functionTable.count; functionIndex++) {
            const SZrAotFunctionEntry *entry = &functionTable.entries[functionIndex];
            if (entry->function != ZR_NULL && !backend_aot_function_is_executable_subset(entry->function)) {
                backend_aot_report_first_unsupported_instruction("aot_c", moduleName, &functionTable);
                fclose(file);
                remove(filename);
                backend_aot_release_annotation_roots(state,
                                                     annotationTypeLayoutRoots,
                                                     annotationTypeLayoutRootCapacity);
                backend_aot_release_annotation_roots(state, annotationRoots, annotationRootCapacity);
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
    fprintf(file, "/* code_stripping.annotationRoots = %u */\n", (unsigned)annotationRootCount);
    for (TZrUInt32 annotationRootIndex = 0u; annotationRootIndex < annotationRootCount; annotationRootIndex++) {
        fprintf(file,
                "/* code_stripping.annotationRoot[%u] = %u */\n",
                (unsigned)annotationRootIndex,
                (unsigned)annotationRoots[annotationRootIndex]);
    }
    fprintf(file,
            "/* code_stripping.annotationTypeLayoutRoots = %u */\n",
            (unsigned)annotationTypeLayoutRootCount);
    for (TZrUInt32 annotationTypeLayoutRootIndex = 0u;
         annotationTypeLayoutRootIndex < annotationTypeLayoutRootCount;
         annotationTypeLayoutRootIndex++) {
        fprintf(file,
                "/* code_stripping.annotationTypeLayoutRoot[%u] = %u */\n",
                (unsigned)annotationTypeLayoutRootIndex,
                (unsigned)annotationTypeLayoutRoots[annotationTypeLayoutRootIndex]);
    }
    backend_aot_write_code_stripping_zrp_metadata_size_deltas(file,
                                                              &zrpMetadataSizeBeforeStripping,
                                                              &zrpMetadataSizeAfterStripping);
    backend_aot_write_manifest_generic_roots(file, options);
    backend_aot_write_manifest_export_declarations(file, options);
    backend_aot_write_manifest_export_table_markers(file, &embeddedZrpMetadata);
    fprintf(file,
            "/* trim_warnings.annotationCount = %u */\n",
            (unsigned)trimAnnotationWarningCount);
    fprintf(file,
            "/* trim_warnings.annotationSuppressedCount = %u */\n",
            (unsigned)trimAnnotationSuppressedCount);
    if (enableCodeStripping && !suppressAnnotationWarnings) {
        backend_aot_write_c_annotation_warnings(file, state, &functionTable);
    }
    fprintf(file,
            "/* trim_warnings.runtimeFallbackCount = %u */\n",
            (unsigned)trimRuntimeFallbackWarningCount);
    fprintf(file,
            "/* trim_warnings.runtimeFallbackSuppressedCount = %u */\n",
            (unsigned)trimRuntimeFallbackSuppressedCount);
    fprintf(file,
            "/* trim_warnings.runtimeFallbackReasonMask = %u */\n",
            (unsigned)trimRuntimeFallbackWarningReasonMask);
    fprintf(file,
            "/* trim_warnings.runtimeFallbackSuppressedReasonMask = %u */\n",
            (unsigned)trimRuntimeFallbackSuppressedReasonMask);
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
    backend_aot_write_member_token_remap_markers(file, &embeddedZrpMetadata);
    fprintf(file, "#include \"zr_vm_common/zr_aot_abi.h\"\n");
    fprintf(file, "#include \"zr_vm_common/zr_ast_constants.h\"\n");
    fprintf(file, "#include \"zr_vm_common/zr_ffi_contract.h\"\n");
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
    fprintf(file, "#include \"zr_vm_core/metadata_runtime.h\"\n");
    fprintf(file, "#include \"zr_vm_core/module.h\"\n");
    fprintf(file, "#include \"zr_vm_core/object.h\"\n");
    fprintf(file, "#include \"zr_vm_core/ownership.h\"\n");
    fprintf(file, "#include \"zr_vm_core/reflection.h\"\n");
    fprintf(file, "#include \"zr_vm_core/string.h\"\n");
    fprintf(file, "#include \"zr_vm_core/type_layout.h\"\n");
    fprintf(file, "#include \"zr_vm_core/value.h\"\n");
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
    backend_aot_write_c_type_layout_declarations(file,
                                                 state,
                                                 &functionTable,
                                                 annotationTypeLayoutRoots,
                                                 annotationTypeLayoutRootCount);
    backend_aot_write_c_type_layout_gc_descriptor_table(file,
                                                        state,
                                                        &functionTable,
                                                        annotationTypeLayoutRoots,
                                                        annotationTypeLayoutRootCount,
                                                        gcDescriptorIndexSpace);
    backend_aot_write_c_type_layout_registration_table(file,
                                                       state,
                                                       &functionTable,
                                                       annotationTypeLayoutRoots,
                                                       annotationTypeLayoutRootCount,
                                                       typeLayoutIndexSpace);
    backend_aot_write_c_type_layout_token_table(file,
                                                state,
                                                &functionTable,
                                                annotationTypeLayoutRoots,
                                                annotationTypeLayoutRootCount,
                                                embeddedZrpMetadata.blob,
                                                embeddedZrpMetadata.length,
                                                typeLayoutIndexSpace);
    backend_aot_write_c_generic_monomorphization_layouts(file, state, &functionTable);
    fprintf(file, "\n");
    backend_aot_write_c_function_forward_decls(file, &functionTable);
    backend_aot_write_c_reference_local_structs(file, &functionTable);
    backend_aot_write_c_reference_local_root_maps(file, &functionTable);
    fprintf(file, "\n");
    backend_aot_write_c_generic_monomorphization_entries(file, &functionTable, stripGeneratedSymbols);
    backend_aot_write_c_generic_sharing_entries(file, &functionTable, stripGeneratedSymbols);
    backend_aot_write_c_typed_bool_thunk_forward_decls(file, &functionTable);
    backend_aot_write_c_typed_f64_thunk_forward_decls(file, &functionTable);
    backend_aot_write_c_typed_i64_thunk_forward_decls(file, &functionTable);
    backend_aot_write_c_typed_u64_thunk_forward_decls(file, &functionTable);
    fprintf(file, "\n");
    backend_aot_write_c_reflection_invokers(file, &functionTable);
    fprintf(file, "\n");
    backend_aot_write_c_method_infos(file, state, &functionTable, &module, reflectionMetadataLevel);
    fprintf(file, "\n");
    backend_aot_write_c_method_info_table(file, &functionTable, functionIndexSpace);
    fprintf(file, "\n");
    backend_aot_write_c_method_token_table(file,
                                           &functionTable,
                                           &embeddedZrpMetadata,
                                           functionIndexSpace);
    fprintf(file, "\n");
    backend_aot_write_member_token_remap_table(file, &embeddedZrpMetadata);
    fprintf(file, "\n");
    backend_aot_write_manifest_export_table(file, &embeddedZrpMetadata);
    fprintf(file, "\n");
    backend_aot_write_c_function_table(file, &functionTable, functionIndexSpace);
    fprintf(file, "\n");
    backend_aot_c_write_native_import_table(file, &functionTable);
    if (nativeImportContractCount > 0u) {
        fprintf(file, "\n");
    }
    backend_aot_c_write_native_import_range_table(
            file, &functionTable, functionIndexSpace);
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
    fprintf(file, "    .methodTokens = zr_aot_method_tokens,\n");
    fprintf(file, "    .methodTokenCount = %u,\n", (unsigned)functionIndexSpace);
    fprintf(file, "    .memberTokenRemaps = %s,\n", backend_aot_member_token_remap_table_name(&embeddedZrpMetadata));
    fprintf(file, "    .memberTokenRemapCount = %uu,\n",
            (unsigned)backend_aot_member_token_remap_count(&embeddedZrpMetadata));
    fprintf(file, "    .manifestExports = %s,\n", backend_aot_manifest_export_table_name(&embeddedZrpMetadata));
    fprintf(file, "    .manifestExportCount = %uu,\n",
            (unsigned)backend_aot_manifest_export_entry_count(&embeddedZrpMetadata));
    fprintf(file, "    .invokers = zr_aot_reflection_invokers,\n");
    fprintf(file, "    .invokerCount = 1u,\n");
    fprintf(file, "    .typeLayouts = %s,\n", typeLayoutIndexSpace > 0u ? "zr_aot_type_layouts" : "ZR_NULL");
    fprintf(file, "    .typeLayoutCount = %u,\n", (unsigned)typeLayoutIndexSpace);
    fprintf(file, "    .typeLayoutTokens = %s,\n", typeLayoutIndexSpace > 0u ? "zr_aot_type_layout_tokens" : "ZR_NULL");
    fprintf(file, "    .typeLayoutTokenCount = %u,\n", (unsigned)typeLayoutIndexSpace);
    fprintf(file, "    .gcDescriptors = %s,\n", gcDescriptorIndexSpace > 0u ? "zr_aot_gc_descriptors" : "ZR_NULL");
    fprintf(file, "    .gcDescriptorCount = %u,\n", (unsigned)gcDescriptorIndexSpace);
    fprintf(file,
            "    .nativeImportContracts = %s,\n",
            nativeImportContractCount > 0u
                    ? "zr_aot_native_import_contracts"
                    : "ZR_NULL");
    fprintf(file,
            "    .nativeImportContractCount = %uu,\n",
            (unsigned)nativeImportContractCount);
    fprintf(file, "    .nativeImportRanges = zr_aot_native_import_ranges,\n");
    fprintf(file,
            "    .nativeImportRangeCount = %uu,\n",
            (unsigned)functionIndexSpace);
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
    fprintf(file, "    .methodTokens = zr_aot_method_tokens,\n");
    fprintf(file, "    .methodTokenCount = %u,\n", (unsigned)functionIndexSpace);
    fprintf(file, "    .memberTokenRemaps = %s,\n", backend_aot_member_token_remap_table_name(&embeddedZrpMetadata));
    fprintf(file, "    .memberTokenRemapCount = %uu,\n",
            (unsigned)backend_aot_member_token_remap_count(&embeddedZrpMetadata));
    fprintf(file, "    .manifestExports = %s,\n", backend_aot_manifest_export_table_name(&embeddedZrpMetadata));
    fprintf(file, "    .manifestExportCount = %uu,\n",
            (unsigned)backend_aot_manifest_export_entry_count(&embeddedZrpMetadata));
    fprintf(file, "    .typeLayouts = %s,\n", typeLayoutIndexSpace > 0u ? "zr_aot_type_layouts" : "ZR_NULL");
    fprintf(file, "    .typeLayoutCount = %u,\n", (unsigned)typeLayoutIndexSpace);
    fprintf(file, "    .typeLayoutTokens = %s,\n", typeLayoutIndexSpace > 0u ? "zr_aot_type_layout_tokens" : "ZR_NULL");
    fprintf(file, "    .typeLayoutTokenCount = %u,\n", (unsigned)typeLayoutIndexSpace);
    fprintf(file, "    .gcDescriptors = %s,\n", gcDescriptorIndexSpace > 0u ? "zr_aot_gc_descriptors" : "ZR_NULL");
    fprintf(file, "    .gcDescriptorCount = %u,\n", (unsigned)gcDescriptorIndexSpace);
    fprintf(file,
            "    .nativeImportContracts = %s,\n",
            nativeImportContractCount > 0u
                    ? "zr_aot_native_import_contracts"
                    : "ZR_NULL");
    fprintf(file,
            "    .nativeImportContractCount = %uu,\n",
            (unsigned)nativeImportContractCount);
    fprintf(file, "    .nativeImportRanges = zr_aot_native_import_ranges,\n");
    fprintf(file,
            "    .nativeImportRangeCount = %uu,\n",
            (unsigned)functionIndexSpace);
    fprintf(file, "    .codeRegistration = &zr_aot_code_registration,\n");
    fprintf(file, "};\n");
    fprintf(file, "\n");
    fprintf(file, "ZR_VM_AOT_EXPORT const ZrAotCompiledModule *ZrVm_GetAotCompiledModule(void) {\n");
    fprintf(file, "    return &zr_aot_module;\n");
    fprintf(file, "}\n");

    if (fclose(file) != 0 ||
        !backend_aot_c_publish_compacted_zrp_metadata(options, &embeddedZrpMetadata)) {
        remove(filename);
        backend_aot_release_annotation_roots(state,
                                             annotationTypeLayoutRoots,
                                             annotationTypeLayoutRootCapacity);
        backend_aot_release_annotation_roots(state, annotationRoots, annotationRootCapacity);
        backend_aot_c_release_embedded_zrp_metadata(&embeddedZrpMetadata);
        backend_aot_release_function_table(state, &functionTable);
        backend_aot_exec_ir_release_module(state, &module);
        return ZR_FALSE;
    }
    success = ZR_TRUE;
    backend_aot_release_annotation_roots(state,
                                         annotationTypeLayoutRoots,
                                         annotationTypeLayoutRootCapacity);
    backend_aot_release_annotation_roots(state, annotationRoots, annotationRootCapacity);
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
