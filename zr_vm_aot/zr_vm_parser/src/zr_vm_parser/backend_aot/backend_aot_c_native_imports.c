#include "backend_aot_c_native_imports.h"

#include "backend_aot_reachability.h"

#include "zr_vm_common/zr_ffi_contract.h"

#include <limits.h>

static void backend_aot_c_write_native_string(
        FILE *file,
        const TZrChar *text) {
    const unsigned char *cursor = (const unsigned char *)text;

    fputc('"', file);
    while (cursor != ZR_NULL && *cursor != '\0') {
        switch (*cursor) {
            case '\\': fputs("\\\\", file); break;
            case '"': fputs("\\\"", file); break;
            case '\n': fputs("\\n", file); break;
            case '\r': fputs("\\r", file); break;
            case '\t': fputs("\\t", file); break;
            default:
                if (*cursor >= 0x20u && *cursor <= 0x7eu) {
                    fputc((int)*cursor, file);
                } else {
                    fprintf(file, "\\%03o", (unsigned)*cursor);
                }
                break;
        }
        cursor++;
    }
    fputc('"', file);
}

static void backend_aot_c_write_ffi_type(
        FILE *file,
        const SZrFfiTypeContract *type) {
    fprintf(file,
            "{ .typeKind = %u, .size = %uu, .alignment = %uu, "
            ".canonicalTypeHash = UINT64_C(0x%016llx), "
            ".layoutHash = UINT64_C(0x%016llx), .flags = 0x%08xu, "
            ".aggregateFieldStart = %uu, .aggregateFieldCount = %uu }",
            (unsigned)type->typeKind,
            (unsigned)type->size,
            (unsigned)type->alignment,
            (unsigned long long)type->canonicalTypeHash,
            (unsigned long long)type->layoutHash,
            (unsigned)type->flags,
            (unsigned)type->aggregateFieldStart,
            (unsigned)type->aggregateFieldCount);
}

static void backend_aot_c_write_ffi_callable(
        FILE *file,
        const SZrFfiCallableContract *callable) {
    fprintf(file,
            "{ .parameterCount = %uu, .parameters = {",
            (unsigned)callable->parameterCount);
    for (TZrUInt32 index = 0u; index < callable->parameterCount; index++) {
        const SZrFfiCallableParameterContract *parameter =
                &callable->parameters[index];

        if (index > 0u) {
            fputs(", ", file);
        }
        fprintf(file,
                "{ .canonicalTypeHash = UINT64_C(0x%016llx), "
                ".passingForm = %u, .escapeUpperBound = %u, "
                ".entryInitialization = %u, .exitInitialization = %u, "
                ".acceptsTemporary = %u, .callSiteMarker = %u }",
                (unsigned long long)parameter->canonicalTypeHash,
                (unsigned)parameter->passingForm,
                (unsigned)parameter->escapeUpperBound,
                (unsigned)parameter->entryInitialization,
                (unsigned)parameter->exitInitialization,
                parameter->acceptsTemporary ? 1u : 0u,
                (unsigned)parameter->callSiteMarker);
    }
    fprintf(file,
            "}, .returnTypeHash = UINT64_C(0x%016llx), "
            ".receiverEffect = %u, .effectFlags = 0x%08xu, "
            ".isVariadic = %u, .contractHash = UINT64_C(0x%016llx) }",
            (unsigned long long)callable->returnTypeHash,
            (unsigned)callable->receiverEffect,
            (unsigned)callable->effectFlags,
            callable->isVariadic ? 1u : 0u,
            (unsigned long long)callable->contractHash);
}

static void backend_aot_c_write_native_import(
        FILE *file,
        const SZrNativeImportContract *contract) {
    fprintf(file, "    {\n        .schemaVersion = %uu,\n        .libraryLocator = ",
            (unsigned)contract->schemaVersion);
    backend_aot_c_write_native_string(file, contract->libraryLocator);
    fputs(",\n        .entryPoint = ", file);
    backend_aot_c_write_native_string(file, contract->entryPoint);
    fprintf(file,
            ",\n        .symbolId = UINT64_C(0x%016llx),\n"
            "        .declaringModuleId = UINT64_C(0x%016llx),\n"
            "        .callable = ",
            (unsigned long long)contract->symbolId,
            (unsigned long long)contract->declaringModuleId);
    backend_aot_c_write_ffi_callable(file, &contract->callable);
    fprintf(file,
            ",\n        .availability = 0x%08xu,\n"
            "        .requiredCapabilities = UINT64_C(0x%016llx),\n"
            "        .sourceMapping = {\n"
            "            .document = ",
            (unsigned)contract->availability,
            (unsigned long long)contract->requiredCapabilities);
    backend_aot_c_write_native_string(file, contract->sourceMapping.document);
    fprintf(file,
            ",\n            .startOffset = UINT64_C(%llu),\n"
            "            .endOffset = UINT64_C(%llu),\n"
            "            .startLine = %d,\n"
            "            .startColumn = %d,\n"
            "            .endLine = %d,\n"
            "            .endColumn = %d\n"
            "        },\n"
            "        .signature = {\n"
            "            .abi = %u,\n"
            "            .targetPointerSize = %uu,\n"
            "            .targetEndianness = %u,\n"
            "            .targetTriple = ",
            (unsigned long long)contract->sourceMapping.startOffset,
            (unsigned long long)contract->sourceMapping.endOffset,
            (int)contract->sourceMapping.startLine,
            (int)contract->sourceMapping.startColumn,
            (int)contract->sourceMapping.endLine,
            (int)contract->sourceMapping.endColumn,
            (unsigned)contract->signature.abi,
            (unsigned)contract->signature.targetPointerSize,
            (unsigned)contract->signature.targetEndianness);
    backend_aot_c_write_native_string(file, contract->signature.targetTriple);
    fprintf(file,
            ",\n            .targetAbiHash = UINT64_C(0x%016llx),\n"
            "            .charset = %u,\n"
            "            .errorPolicy = %u,\n"
            "            .cleanupPolicy = %u,\n"
            "            .callbackLifetime = %u,\n"
            "            .callbackThreadPolicy = %u,\n"
            "            .callbackExceptionPolicy = %u,\n"
            "            .isVariadic = %s,\n"
            "            .parameterCount = %uu,\n"
            "            .returnType = ",
            (unsigned long long)contract->signature.targetAbiHash,
            (unsigned)contract->signature.charset,
            (unsigned)contract->signature.errorPolicy,
            (unsigned)contract->signature.cleanupPolicy,
            (unsigned)contract->signature.callbackLifetime,
            (unsigned)contract->signature.callbackThreadPolicy,
            (unsigned)contract->signature.callbackExceptionPolicy,
            contract->signature.isVariadic ? "ZR_TRUE" : "ZR_FALSE",
            (unsigned)contract->signature.parameterCount);
    backend_aot_c_write_ffi_type(file, &contract->signature.returnType);
    fputs(",\n            .parameters = {\n", file);
    for (TZrUInt32 index = 0u;
         index < contract->signature.parameterCount;
         index++) {
        const SZrFfiParameterContract *parameter =
                &contract->signature.parameters[index];

        fputs("                { .type = ", file);
        backend_aot_c_write_ffi_type(file, &parameter->type);
        fprintf(file,
                ", .direction = %u, .marshalling = %u, .ownership = %u, "
                ".isNullable = %s, .flags = 0x%08xu },\n",
                (unsigned)parameter->direction,
                (unsigned)parameter->marshalling,
                (unsigned)parameter->ownership,
                parameter->isNullable ? "ZR_TRUE" : "ZR_FALSE",
                (unsigned)parameter->flags);
    }
    fprintf(file,
            "            },\n"
            "            .aggregateFieldCount = %uu,\n"
            "            .aggregateFields = {\n",
            (unsigned)contract->signature.aggregateFieldCount);
    for (TZrUInt32 index = 0u;
         index < contract->signature.aggregateFieldCount;
         index++) {
        const SZrFfiAggregateFieldContract *field =
                &contract->signature.aggregateFields[index];

        fputs("                { .name = ", file);
        backend_aot_c_write_native_string(file, field->name);
        fprintf(file,
                ", .typeKind = %u, .size = %uu, .alignment = %uu, "
                ".offset = %uu },\n",
                (unsigned)field->typeKind,
                (unsigned)field->size,
                (unsigned)field->alignment,
                (unsigned)field->offset);
    }
    fprintf(file,
            "            },\n"
            "            .signatureHash = UINT64_C(0x%016llx)\n"
            "        }\n"
            "    },\n",
            (unsigned long long)contract->signature.signatureHash);
}

static const SZrAotFunctionEntry *backend_aot_c_native_import_find_function(
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 flatIndex) {
    if (functionTable == ZR_NULL || functionTable->entries == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrUInt32 index = 0u; index < functionTable->count; index++) {
        if (functionTable->entries[index].flatIndex == flatIndex) {
            return &functionTable->entries[index];
        }
    }
    return ZR_NULL;
}

static TZrBool backend_aot_c_native_import_validate_function(
        const SZrFunction *function) {
    if (function == ZR_NULL ||
        function->nativeImportContractLength >
                ZR_FFI_CONTRACT_MAX_IMPORTS_PER_FUNCTION ||
        (function->nativeImportContractLength > 0u &&
         function->nativeImportContracts == ZR_NULL)) {
        return ZR_FALSE;
    }
    for (TZrUInt32 contractIndex = 0u;
         contractIndex < function->nativeImportContractLength;
         contractIndex++) {
        if (!ZrCommon_NativeImportContract_Validate(
                    &function->nativeImportContracts[contractIndex])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool backend_aot_c_native_import_validate_function_tree(
        const SZrFunction *function) {
    if (!backend_aot_c_native_import_validate_function(function) ||
        (function->childFunctionLength > 0u &&
         function->childFunctionList == ZR_NULL)) {
        return ZR_FALSE;
    }
    for (TZrUInt32 childIndex = 0u;
         childIndex < function->childFunctionLength;
         childIndex++) {
        if (!backend_aot_c_native_import_validate_function_tree(
                    &function->childFunctionList[childIndex])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool backend_aot_c_native_import_count(
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 *outCount) {
    TZrUInt64 count = 0u;

    if (outCount != ZR_NULL) {
        *outCount = 0u;
    }
    if (functionTable == ZR_NULL || outCount == ZR_NULL ||
        functionTable->count > functionTable->capacity ||
        functionTable->indexSpace > functionTable->capacity ||
        (functionTable->count > 0u && functionTable->entries == ZR_NULL)) {
        return ZR_FALSE;
    }
    for (TZrUInt32 functionIndex = 0u;
         functionIndex < functionTable->count;
         functionIndex++) {
        const SZrFunction *function = functionTable->entries[functionIndex].function;

        if (!backend_aot_c_native_import_validate_function(function)) {
            return ZR_FALSE;
        }
        count += function->nativeImportContractLength;
        if (count > UINT32_MAX) {
            return ZR_FALSE;
        }
    }
    *outCount = (TZrUInt32)count;
    return ZR_TRUE;
}

TZrBool backend_aot_c_native_import_write_reachability_manifest(
        FILE *file,
        const SZrAotFunctionTable *functionTable) {
    const TZrChar *reasonName = backend_aot_reachability_reason_name(
            ZR_AOT_REACHABILITY_REASON_NATIVE_IMPORT);
    TZrUInt32 count = 0u;
    TZrUInt32 nodeIndex = 0u;
    TZrUInt32 functionIndexSpace;

    if (file == ZR_NULL || reasonName == ZR_NULL ||
        !backend_aot_c_native_import_count(functionTable, &count)) {
        return ZR_FALSE;
    }
    if (fprintf(file,
                "/* reachability.nativeImportManifest.version = 1 */\n") < 0 ||
        fprintf(file,
                "/* reachability.nativeImportManifest.count = %u */\n",
                (unsigned)count) < 0) {
        return ZR_FALSE;
    }

    functionIndexSpace = backend_aot_function_table_index_space(functionTable);
    for (TZrUInt32 functionIndex = 0u;
         functionIndex < functionIndexSpace;
         functionIndex++) {
        const SZrAotFunctionEntry *entry =
                backend_aot_c_native_import_find_function(
                        functionTable, functionIndex);
        const SZrFunction *function = entry != ZR_NULL ? entry->function : ZR_NULL;

        for (TZrUInt32 contractIndex = 0u;
             function != ZR_NULL &&
             contractIndex < function->nativeImportContractLength;
             contractIndex++) {
            const SZrNativeImportContract *contract =
                    &function->nativeImportContracts[contractIndex];

            if (fprintf(file,
                        "/* reachability.nativeImportManifest.node[%u] = "
                        "ownerFunction=%u contractIndex=%u "
                        "symbolId=0x%016llx callableHash=0x%016llx "
                        "reason=%s predecessor=%u */\n",
                        (unsigned)nodeIndex,
                        (unsigned)entry->flatIndex,
                        (unsigned)contractIndex,
                        (unsigned long long)contract->symbolId,
                        (unsigned long long)contract->callable.contractHash,
                        reasonName,
                        (unsigned)entry->flatIndex) < 0) {
                return ZR_FALSE;
            }
            nodeIndex++;
        }
    }

    return (TZrBool)(nodeIndex == count && ferror(file) == 0);
}

void backend_aot_c_write_native_import_table(
        FILE *file,
        const SZrAotFunctionTable *functionTable) {
    TZrUInt32 count = 0u;
    TZrUInt32 functionIndexSpace;

    if (file == ZR_NULL || functionTable == ZR_NULL ||
        !backend_aot_c_native_import_count(functionTable, &count) || count == 0u) {
        return;
    }
    functionIndexSpace = backend_aot_function_table_index_space(functionTable);
    fputs("static const SZrNativeImportContract zr_aot_native_import_contracts[] = {\n", file);
    for (TZrUInt32 functionIndex = 0u;
         functionIndex < functionIndexSpace;
         functionIndex++) {
        const SZrAotFunctionEntry *entry =
                backend_aot_c_native_import_find_function(
                        functionTable, functionIndex);
        const SZrFunction *function = entry != ZR_NULL ? entry->function : ZR_NULL;

        for (TZrUInt32 contractIndex = 0u;
             function != ZR_NULL &&
             contractIndex < function->nativeImportContractLength;
             contractIndex++) {
            backend_aot_c_write_native_import(
                    file, &function->nativeImportContracts[contractIndex]);
        }
    }
    fputs("};\n", file);
}

void backend_aot_c_write_native_import_range_table(
        FILE *file,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 functionIndexSpace) {
    TZrUInt32 contractStart = 0u;

    if (file == ZR_NULL || functionTable == ZR_NULL ||
        functionIndexSpace == 0u) {
        return;
    }
    fputs("static const SZrAotNativeImportRange zr_aot_native_import_ranges[] = {\n", file);
    for (TZrUInt32 functionIndex = 0u;
         functionIndex < functionIndexSpace;
         functionIndex++) {
        const SZrAotFunctionEntry *entry =
                backend_aot_c_native_import_find_function(
                        functionTable, functionIndex);
        TZrUInt32 contractCount = entry != ZR_NULL && entry->function != ZR_NULL
                ? entry->function->nativeImportContractLength
                : 0u;

        fprintf(file,
                "    { .contractStart = %uu, .contractCount = %uu },\n",
                (unsigned)contractStart,
                (unsigned)contractCount);
        contractStart += contractCount;
    }
    fputs("};\n", file);
}
