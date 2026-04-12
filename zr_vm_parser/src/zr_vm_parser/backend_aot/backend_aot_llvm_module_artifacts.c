#include "backend_aot_llvm_module_artifacts.h"

#include "zr_vm_common/zr_aot_abi.h"

void backend_aot_llvm_write_function_thunk_table(FILE *file, const SZrAotFunctionTable *functionTable) {
    TZrUInt32 functionIndex;

    if (file == ZR_NULL || functionTable == ZR_NULL) {
        return;
    }

    fprintf(file, "@zr_aot_function_thunks = private constant [%u x ptr] [", (unsigned)functionTable->count);
    for (functionIndex = 0; functionIndex < functionTable->count; functionIndex++) {
        if (functionIndex > 0) {
            fprintf(file, ", ");
        }
        fprintf(file, "ptr @zr_aot_fn_%u", (unsigned)functionIndex);
    }
    fprintf(file, "]\n");
    fprintf(file, "\n");
}

static void backend_aot_llvm_write_entry_thunk(FILE *file, const SZrAotFunctionTable *functionTable) {
    if (file == ZR_NULL || functionTable == ZR_NULL) {
        return;
    }

    fprintf(file, "define i64 @zr_aot_entry(ptr %%state) {\n");
    fprintf(file, "entry:\n");
    if (functionTable->count > 0) {
        fprintf(file, "  %%ret = call i64 @zr_aot_fn_%u(ptr %%state)\n", (unsigned)ZR_AOT_FUNCTION_TREE_ROOT_INDEX);
    } else {
        fprintf(file,
                "  %%ret = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %%state, i32 0, i32 0, i32 0)\n");
    }
    fprintf(file, "  ret i64 %%ret\n");
    fprintf(file, "}\n");
    fprintf(file, "\n");
}

void backend_aot_llvm_write_module_exports(FILE *file,
                                           const TZrChar *moduleName,
                                           TZrUInt32 inputKind,
                                           const TZrChar *inputHash,
                                           const SZrAotFunctionTable *functionTable,
                                           const SZrAotWriterOptions *options) {
    if (file == ZR_NULL || moduleName == ZR_NULL || inputHash == ZR_NULL || functionTable == ZR_NULL) {
        return;
    }

    backend_aot_llvm_write_entry_thunk(file, functionTable);
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
    fprintf(file, "  ptr @zr_aot_entry\n");
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
