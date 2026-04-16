#include "backend_aot_c_function_body.h"
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
    for (contractBit = 1; contractBit <= ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE; contractBit <<= 1) {
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

static void backend_aot_write_c_guard_macro(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "#define ZR_AOT_C_FAIL() return ZrLibrary_AotRuntime_FailGeneratedFunction(state, &frame)\n"
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
    fprintf(file, "#include \"zr_vm_core/call_info.h\"\n");
    fprintf(file, "#include \"zr_vm_core/closure.h\"\n");
    fprintf(file, "#include \"zr_vm_core/debug.h\"\n");
    fprintf(file, "#include \"zr_vm_core/ownership.h\"\n");
    fprintf(file, "#include \"zr_vm_library/aot_runtime.h\"\n");
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
    backend_aot_write_c_function_forward_decls(file, &functionTable);
    fprintf(file, "\n");
    backend_aot_write_c_function_table(file, &functionTable);
    fprintf(file, "\n");
    for (TZrUInt32 functionIndex = 0; functionIndex < functionTable.count; functionIndex++) {
        backend_aot_write_c_function_body(file, state, &functionTable, &functionTable.entries[functionIndex]);
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
