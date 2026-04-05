#include "backend_aot_llvm_emitter.h"

#include "backend_aot_llvm_function_body.h"
#include "backend_aot_llvm_module_artifacts.h"

ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotLlvmFileWithOptions(SZrState *state,
                                                                  SZrFunction *function,
                                                                  const TZrChar *filename,
                                                                  const SZrAotWriterOptions *options) {
    SZrAotExecIrModule module = {0};
    SZrAotFunctionTable functionTable = {0};
    FILE *file;
    const TZrChar *moduleName;
    const TZrChar *sourceHash;
    const TZrChar *zroHash;
    const TZrChar *inputHash;
    TZrUInt32 inputKind;
    TZrBool requireExecutableLowering;

    if (state == ZR_NULL || function == ZR_NULL || filename == ZR_NULL) {
        return ZR_FALSE;
    }

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

    if (requireExecutableLowering || backend_aot_report_first_unsupported_instruction("aot_llvm", moduleName, &functionTable)) {
        for (TZrUInt32 functionIndex = 0; functionIndex < functionTable.count; functionIndex++) {
            const SZrAotFunctionEntry *entry = &functionTable.entries[functionIndex];
            if (entry->function != ZR_NULL && !backend_aot_function_is_executable_subset(entry->function)) {
                backend_aot_report_first_unsupported_instruction("aot_llvm", moduleName, &functionTable);
                fclose(file);
                remove(filename);
                backend_aot_release_function_table(state, &functionTable);
                backend_aot_exec_ir_release_module(state, &module);
                return ZR_FALSE;
            }
        }
    }

    fprintf(file, "; ZR AOT LLVM Backend\n");
    fprintf(file, "; SemIR overlay + generated exec thunks.\n");
    backend_aot_llvm_write_module_prelude(file, &module, moduleName, inputHash, options);

    for (TZrUInt32 functionIndex = 0; functionIndex < functionTable.count; functionIndex++) {
        backend_aot_write_llvm_function_body(file, state, &functionTable, &functionTable.entries[functionIndex]);
    }

    backend_aot_llvm_write_function_thunk_table(file, &functionTable);
    backend_aot_llvm_write_module_exports(file, moduleName, inputKind, inputHash, &functionTable, options);

    fclose(file);
    backend_aot_release_function_table(state, &functionTable);
    backend_aot_exec_ir_release_module(state, &module);
    return ZR_TRUE;
}

ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotLlvmFile(SZrState *state,
                                                       SZrFunction *function,
                                                       const TZrChar *filename) {
    return ZrParser_Writer_WriteAotLlvmFileWithOptions(state, function, filename, ZR_NULL);
}
