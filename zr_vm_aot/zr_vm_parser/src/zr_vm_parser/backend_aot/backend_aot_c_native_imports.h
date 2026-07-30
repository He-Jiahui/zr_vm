#ifndef ZR_VM_PARSER_BACKEND_AOT_C_NATIVE_IMPORTS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_NATIVE_IMPORTS_H

#include "backend_aot_function_table.h"

TZrBool backend_aot_c_native_import_validate_function_tree(
        const SZrFunction *function);

TZrBool backend_aot_c_native_import_count(
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 *outCount);

TZrBool backend_aot_c_native_import_write_reachability_manifest(
        FILE *file,
        const SZrAotFunctionTable *functionTable);

void backend_aot_c_write_native_import_table(
        FILE *file,
        const SZrAotFunctionTable *functionTable);

void backend_aot_c_write_native_import_range_table(
        FILE *file,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 functionIndexSpace);

#endif
