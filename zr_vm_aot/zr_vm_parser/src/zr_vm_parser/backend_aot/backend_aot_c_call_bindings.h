#ifndef ZR_VM_PARSER_BACKEND_AOT_C_CALL_BINDINGS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_CALL_BINDINGS_H

#include "backend_aot_function_table.h"
#include "zr_vm_core/artifact_schema.h"

ZR_PARSER_API TZrBool backend_aot_c_project_call_binding(
        SZrState *state,
        const SZrAotFunctionTable *table,
        const SZrAotFunctionEntry *entry,
        TZrUInt32 cacheIndex,
        SZrArtifactCallBindingRow *outRow,
        TZrUInt32 *outTargetFunctionIndex,
        SZrArtifactDiagnostic *diagnostic);

TZrBool backend_aot_c_validate_call_bindings(
        SZrState *state, const SZrAotFunctionTable *table, TZrUInt32 *outCount);

TZrBool backend_aot_c_write_call_bindings(
        FILE *file, SZrState *state, const SZrAotFunctionTable *table, TZrUInt32 count);

void backend_aot_c_write_call_binding_registration(FILE *file, TZrUInt32 count);

#endif
