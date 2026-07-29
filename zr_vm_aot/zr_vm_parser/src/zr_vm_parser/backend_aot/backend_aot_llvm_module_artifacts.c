#include "backend_aot_llvm_module_artifacts.h"

#include "backend_aot_c_native_imports.h"
#include "backend_aot_function_table.h"
#include "backend_aot_llvm_text_emit.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_common/zr_ffi_contract.h"

#include <string.h>

static void backend_aot_llvm_write_fixed_string(
        FILE *file,
        const TZrChar *text,
        TZrSize capacity) {
    TZrSize length = text != ZR_NULL ? strlen(text) : 0u;

    if (length >= capacity) {
        length = capacity - 1u;
    }
    fputs("c\"", file);
    for (TZrSize index = 0u; index < capacity; index++) {
        unsigned char value = index < length
                ? (unsigned char)text[index]
                : 0u;

        if (value >= 0x20u && value <= 0x7eu &&
            value != '"' && value != '\\') {
            fputc((int)value, file);
        } else {
            fprintf(file, "\\%02X", (unsigned)value);
        }
    }
    fputc('"', file);
}

static void backend_aot_llvm_write_ffi_type(
        FILE *file,
        const SZrFfiTypeContract *type) {
    fprintf(
            file,
            "%%SZrFfiTypeContract { i32 %u, i32 %u, i32 %u, "
            "i64 %llu, i64 %llu, i32 %u, i32 %u, i32 %u }",
            (unsigned)type->typeKind,
            (unsigned)type->size,
            (unsigned)type->alignment,
            (unsigned long long)type->canonicalTypeHash,
            (unsigned long long)type->layoutHash,
            (unsigned)type->flags,
            (unsigned)type->aggregateFieldStart,
            (unsigned)type->aggregateFieldCount);
}

static void backend_aot_llvm_write_ffi_parameter(
        FILE *file,
        const SZrFfiParameterContract *parameter) {
    fputs("%SZrFfiParameterContract { ", file);
    backend_aot_llvm_write_ffi_type(file, &parameter->type);
    fprintf(
            file,
            ", i32 %u, i32 %u, i32 %u, i8 %u, i32 %u }",
            (unsigned)parameter->direction,
            (unsigned)parameter->marshalling,
            (unsigned)parameter->ownership,
            parameter->isNullable ? 1u : 0u,
            (unsigned)parameter->flags);
}

static void backend_aot_llvm_write_ffi_aggregate_field(
        FILE *file,
        const SZrFfiAggregateFieldContract *field) {
    fputs("%SZrFfiAggregateFieldContract { [32 x i8] ", file);
    backend_aot_llvm_write_fixed_string(
            file, field->name, ZR_FFI_CONTRACT_FIELD_NAME_CAPACITY);
    fprintf(
            file,
            ", i32 %u, i32 %u, i32 %u, i32 %u }",
            (unsigned)field->typeKind,
            (unsigned)field->size,
            (unsigned)field->alignment,
            (unsigned)field->offset);
}

static void backend_aot_llvm_write_ffi_signature(
        FILE *file,
        const SZrFfiSignatureContract *signature) {
    fprintf(
            file,
            "%%SZrFfiSignatureContract { i32 %u, i32 %u, i32 %u, "
            "[64 x i8] ",
            (unsigned)signature->abi,
            (unsigned)signature->targetPointerSize,
            (unsigned)signature->targetEndianness);
    backend_aot_llvm_write_fixed_string(
            file,
            signature->targetTriple,
            ZR_FFI_CONTRACT_TARGET_TRIPLE_CAPACITY);
    fprintf(
            file,
            ", i64 %llu, i32 %u, i32 %u, i32 %u, i32 %u, i32 %u, "
            "i32 %u, i8 %u, i32 %u, ",
            (unsigned long long)signature->targetAbiHash,
            (unsigned)signature->charset,
            (unsigned)signature->errorPolicy,
            (unsigned)signature->cleanupPolicy,
            (unsigned)signature->callbackLifetime,
            (unsigned)signature->callbackThreadPolicy,
            (unsigned)signature->callbackExceptionPolicy,
            signature->isVariadic ? 1u : 0u,
            (unsigned)signature->parameterCount);
    backend_aot_llvm_write_ffi_type(file, &signature->returnType);
    fputs(", [32 x %SZrFfiParameterContract] [", file);
    for (TZrUInt32 index = 0u;
         index < ZR_FFI_CONTRACT_MAX_PARAMETERS;
         index++) {
        if (index > 0u) {
            fputs(", ", file);
        }
        if (index < signature->parameterCount) {
            backend_aot_llvm_write_ffi_parameter(
                    file, &signature->parameters[index]);
        } else {
            fputs("%SZrFfiParameterContract zeroinitializer", file);
        }
    }
    fprintf(
            file,
            "], i32 %u, [64 x %%SZrFfiAggregateFieldContract] [",
            (unsigned)signature->aggregateFieldCount);
    for (TZrUInt32 index = 0u;
         index < ZR_FFI_CONTRACT_MAX_AGGREGATE_FIELDS;
         index++) {
        if (index > 0u) {
            fputs(", ", file);
        }
        if (index < signature->aggregateFieldCount) {
            backend_aot_llvm_write_ffi_aggregate_field(
                    file, &signature->aggregateFields[index]);
        } else {
            fputs("%SZrFfiAggregateFieldContract zeroinitializer", file);
        }
    }
    fprintf(
            file,
            "], i64 %llu }",
            (unsigned long long)signature->signatureHash);
}

static void backend_aot_llvm_write_native_import(
        FILE *file,
        const SZrNativeImportContract *contract) {
    fprintf(
            file,
            "%%SZrNativeImportContract { i32 %u, [512 x i8] ",
            (unsigned)contract->schemaVersion);
    backend_aot_llvm_write_fixed_string(
            file,
            contract->libraryLocator,
            ZR_FFI_CONTRACT_LIBRARY_CAPACITY);
    fputs(", [128 x i8] ", file);
    backend_aot_llvm_write_fixed_string(
            file, contract->entryPoint, ZR_FFI_CONTRACT_ENTRY_CAPACITY);
    fprintf(
            file,
            ", i64 %llu, i64 %llu, i64 %llu, i32 %u, i64 %llu, "
            "%%SZrFfiSourceMapping { [512 x i8] ",
            (unsigned long long)contract->symbolId,
            (unsigned long long)contract->declaringModuleId,
            (unsigned long long)contract->callableContractHash,
            (unsigned)contract->availability,
            (unsigned long long)contract->requiredCapabilities);
    backend_aot_llvm_write_fixed_string(
            file,
            contract->sourceMapping.document,
            ZR_FFI_CONTRACT_SOURCE_DOCUMENT_CAPACITY);
    fprintf(
            file,
            ", i64 %llu, i64 %llu, i32 %d, i32 %d, i32 %d, i32 %d }, ",
            (unsigned long long)contract->sourceMapping.startOffset,
            (unsigned long long)contract->sourceMapping.endOffset,
            (int)contract->sourceMapping.startLine,
            (int)contract->sourceMapping.startColumn,
            (int)contract->sourceMapping.endLine,
            (int)contract->sourceMapping.endColumn);
    backend_aot_llvm_write_ffi_signature(file, &contract->signature);
    fputs(" }", file);
}

static const SZrFunction *backend_aot_llvm_find_function(
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 flatIndex) {
    for (TZrUInt32 index = 0u;
         functionTable != ZR_NULL && index < functionTable->count;
         index++) {
        if (functionTable->entries[index].flatIndex == flatIndex) {
            return functionTable->entries[index].function;
        }
    }
    return ZR_NULL;
}

static TZrUInt32 backend_aot_llvm_write_native_import_tables(
        FILE *file,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 functionIndexSpace) {
    TZrUInt32 contractCount = 0u;
    TZrUInt32 contractStart = 0u;

    if (!backend_aot_c_native_import_count(functionTable, &contractCount)) {
        return 0u;
    }
    if (contractCount > 0u) {
        fprintf(
                file,
                "@zr_aot_native_import_contracts = private constant [%u x %%SZrNativeImportContract] [",
                (unsigned)contractCount);
        TZrUInt32 emitted = 0u;
        for (TZrUInt32 functionIndex = 0u;
             functionIndex < functionIndexSpace;
             functionIndex++) {
            const SZrFunction *function = backend_aot_llvm_find_function(
                    functionTable, functionIndex);

            for (TZrUInt32 contractIndex = 0u;
                 function != ZR_NULL &&
                 contractIndex < function->nativeImportContractLength;
                 contractIndex++) {
                if (emitted > 0u) {
                    fputs(", ", file);
                }
                backend_aot_llvm_write_native_import(
                        file,
                        &function->nativeImportContracts[contractIndex]);
                emitted++;
            }
        }
        fputs("]\n", file);
    }
    fprintf(
            file,
            "@zr_aot_native_import_ranges = private constant [%u x %%SZrAotNativeImportRange] [",
            (unsigned)functionIndexSpace);
    for (TZrUInt32 functionIndex = 0u;
         functionIndex < functionIndexSpace;
         functionIndex++) {
        const SZrFunction *function = backend_aot_llvm_find_function(
                functionTable, functionIndex);
        TZrUInt32 count = function != ZR_NULL
                ? function->nativeImportContractLength
                : 0u;

        if (functionIndex > 0u) {
            fputs(", ", file);
        }
        fprintf(
                file,
                "%%SZrAotNativeImportRange { i32 %u, i32 %u }",
                (unsigned)contractStart,
                (unsigned)count);
        contractStart += count;
    }
    fputs("]\n", file);
    return contractCount;
}

void backend_aot_llvm_write_function_thunk_table(FILE *file,
                                                 const SZrAotFunctionTable *functionTable,
                                                 TZrBool stripGeneratedSymbols) {
    TZrUInt32 functionIndex;

    if (file == ZR_NULL || functionTable == ZR_NULL) {
        return;
    }

    fprintf(file, "@zr_aot_function_thunks = private constant [%u x ptr] [", (unsigned)functionTable->count);
    for (functionIndex = 0; functionIndex < functionTable->count; functionIndex++) {
        if (functionIndex > 0) {
            fprintf(file, ", ");
        }
        {
            TZrChar functionSymbol[64];
            backend_aot_llvm_format_function_symbol(functionSymbol,
                                                    sizeof(functionSymbol),
                                                    functionIndex,
                                                    stripGeneratedSymbols);
            fprintf(file, "ptr @%s", functionSymbol);
        }
    }
    fprintf(file, "]\n");
    fprintf(file, "\n");
}

static void backend_aot_llvm_write_entry_thunk(FILE *file,
                                               const SZrAotFunctionTable *functionTable,
                                               TZrBool stripGeneratedSymbols) {
    if (file == ZR_NULL || functionTable == ZR_NULL) {
        return;
    }

    fprintf(file, "define i64 @zr_aot_entry(ptr %%state) {\n");
    fprintf(file, "entry:\n");
    if (functionTable->count > 0) {
        TZrChar functionSymbol[64];
        backend_aot_llvm_format_function_symbol(functionSymbol,
                                                sizeof(functionSymbol),
                                                ZR_AOT_FUNCTION_TREE_ROOT_INDEX,
                                                stripGeneratedSymbols);
        fprintf(file, "  %%ret = call i64 @%s(ptr %%state)\n", functionSymbol);
    } else {
        fprintf(file,
                "  %%ret = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %%state, i32 0, i32 0, i32 0)\n");
    }
    fprintf(file, "  ret i64 %%ret\n");
    fprintf(file, "}\n");
    fprintf(file, "\n");
}

static void backend_aot_llvm_write_reflection_invoker(FILE *file) {
    fprintf(file,
            "define void @zr_aot_reflection_invoke_unsupported(ptr %%state, ptr %%target, "
            "ptr %%method, ptr %%self, ptr %%args, ptr %%out_return) {\n");
    fprintf(file, "entry:\n");
    fprintf(file,
            "  %%ignored = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction("
            "ptr %%state, i32 0, i32 0, i32 0)\n");
    fprintf(file, "  ret void\n");
    fprintf(file, "}\n");
    fprintf(file, "@zr_aot_reflection_invokers = private constant [1 x ptr] "
                  "[ptr @zr_aot_reflection_invoke_unsupported]\n");
    fprintf(file, "\n");
}

void backend_aot_llvm_write_module_exports(FILE *file,
                                           const TZrChar *moduleName,
                                           TZrUInt32 inputKind,
                                           const TZrChar *inputHash,
                                           const SZrAotFunctionTable *functionTable,
                                           const SZrAotWriterOptions *options,
                                           TZrBool stripGeneratedSymbols) {
    TZrUInt32 nativeImportContractCount;
    TZrUInt32 functionIndexSpace;

    if (file == ZR_NULL || moduleName == ZR_NULL || inputHash == ZR_NULL || functionTable == ZR_NULL) {
        return;
    }

    backend_aot_llvm_write_entry_thunk(file, functionTable, stripGeneratedSymbols);
    backend_aot_llvm_write_reflection_invoker(file);
    functionIndexSpace = backend_aot_function_table_index_space(functionTable);
    nativeImportContractCount = backend_aot_llvm_write_native_import_tables(
            file, functionTable, functionIndexSpace);
    fprintf(file, "@zr_aot_code_registration = private constant %%SZrAotCodeRegistration {\n");
    fprintf(file, "  i32 %u,\n", (unsigned)functionTable->count);
    fprintf(file, "  ptr @zr_aot_function_thunks,\n");
    fprintf(file, "  ptr null, i32 0,\n");
    fprintf(file, "  ptr null, i32 0,\n");
    fprintf(file, "  ptr null, i32 0,\n");
    fprintf(file, "  ptr null, i32 0,\n");
    fprintf(file, "  ptr @zr_aot_reflection_invokers, i32 1,\n");
    fprintf(file, "  ptr null, i32 0,\n");
    fprintf(file, "  ptr null, i32 0,\n");
    fprintf(file, "  ptr null, i32 0,\n");
    fprintf(
            file,
            "  %s, i32 %u,\n",
            nativeImportContractCount > 0u
                    ? "ptr @zr_aot_native_import_contracts"
                    : "ptr null",
            (unsigned)nativeImportContractCount);
    fprintf(file, "  ptr @zr_aot_native_import_ranges, i32 %u\n", (unsigned)functionIndexSpace);
    fprintf(file, "}\n");
    fprintf(file, "@zr_aot_module = private constant %%ZrAotCompiledModule {\n");
    fprintf(file, "  i32 %u,\n", (unsigned)ZR_VM_AOT_ABI_VERSION);
    fprintf(file, "  i32 %u,\n", (unsigned)ZR_AOT_BACKEND_KIND_LLVM);
    fprintf(file, "  ptr @zr_aot_module_name,\n");
    fprintf(file, "  i32 %u,\n", (unsigned)inputKind);
    fprintf(file, "  ptr @zr_aot_input_hash,\n");
    fprintf(file, "  ptr @zr_aot_runtime_contracts,\n");
    fprintf(file,
            "  %s,\n",
            (options != ZR_NULL && options->embeddedModuleBlob != ZR_NULL && options->embeddedModuleBlobLength > 0)
                    ? "ptr @zr_aot_embedded_module_blob"
                    : "ptr null");
    fprintf(file,
            "  i64 %llu,\n",
            (unsigned long long)((options != ZR_NULL) ? options->embeddedModuleBlobLength : 0));
    fprintf(file, "  ptr @zr_aot_function_thunks,\n");
    fprintf(file, "  i32 %u,\n", (unsigned)functionTable->count);
    fprintf(file, "  ptr @zr_aot_entry,\n");
    fprintf(file, "  ptr null,\n");
    fprintf(file, "  i32 0,\n");
    fprintf(file, "  ptr null,\n");
    fprintf(file, "  i32 0,\n");
    fprintf(file, "  ptr null,\n");
    fprintf(file, "  i32 0,\n");
    fprintf(file, "  ptr null,\n");
    fprintf(file, "  i32 0,\n");
    fprintf(file, "  ptr null,\n");
    fprintf(file, "  i32 0,\n");
    fprintf(file, "  ptr null,\n");
    fprintf(file, "  i32 0,\n");
    fprintf(file, "  ptr null,\n");
    fprintf(file, "  i32 0,\n");
    fprintf(
            file,
            "  %s,\n",
            nativeImportContractCount > 0u
                    ? "ptr @zr_aot_native_import_contracts"
                    : "ptr null");
    fprintf(file, "  i32 %u,\n", (unsigned)nativeImportContractCount);
    fprintf(file, "  ptr @zr_aot_native_import_ranges,\n");
    fprintf(file, "  i32 %u,\n", (unsigned)functionIndexSpace);
    fprintf(file, "  ptr @zr_aot_code_registration\n");
    fprintf(file, "}\n");
    fprintf(file, "\n");
    fprintf(file, "; export-symbol: ZrVm_GetAotCompiledModule\n");
    fprintf(file, "; descriptor.moduleName = %s\n", moduleName);
    fprintf(file, "; descriptor.inputKind = %u\n", (unsigned)inputKind);
    fprintf(file, "; descriptor.inputHash = %s\n", inputHash);
    fprintf(file, "; descriptor.backendKind = llvm\n");
    backend_aot_write_llvm_runtime_helper_decls(file);
    fprintf(file, "define ptr @ZrVm_GetAotCompiledModule() {\n");
    fprintf(file, "entry_export:\n");
    fprintf(file, "  ret ptr @zr_aot_module\n");
    fprintf(file, "}\n");
}
