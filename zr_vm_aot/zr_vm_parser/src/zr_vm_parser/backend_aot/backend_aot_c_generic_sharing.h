#ifndef ZR_VM_PARSER_BACKEND_AOT_C_GENERIC_SHARING_H
#define ZR_VM_PARSER_BACKEND_AOT_C_GENERIC_SHARING_H

#include <stdio.h>

#include "backend_aot_function_table.h"

void backend_aot_write_c_generic_dictionary_macros(FILE *file);

TZrBool backend_aot_c_generic_sharing_validate_function_tree(
        const SZrFunction *function);

TZrBool backend_aot_c_generic_sharing_count_dictionaries(
        const SZrAotFunctionTable *table,
        TZrUInt32 *outCount);

TZrBool backend_aot_c_generic_sharing_write_reachability_manifest(
        FILE *file,
        const SZrAotFunctionTable *table);

void backend_aot_write_c_generic_sharing_entries(FILE *file,
                                                 const SZrAotFunctionTable *table,
                                                 TZrBool stripGeneratedSymbols);

TZrUInt32 backend_aot_c_generic_sharing_dictionary_id_for_function(const SZrAotFunctionTable *table,
                                                                   const SZrFunction *function);

#endif
