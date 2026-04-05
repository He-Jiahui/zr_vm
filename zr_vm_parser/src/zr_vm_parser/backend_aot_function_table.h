#ifndef ZR_VM_PARSER_BACKEND_AOT_FUNCTION_TABLE_H
#define ZR_VM_PARSER_BACKEND_AOT_FUNCTION_TABLE_H

#include "zr_vm_parser/writer.h"

typedef struct SZrAotFunctionEntry {
    const SZrFunction *function;
    TZrUInt32 flatIndex;
} SZrAotFunctionEntry;

typedef struct SZrAotFunctionTable {
    SZrAotFunctionEntry *entries;
    TZrUInt32 count;
    TZrUInt32 capacity;
} SZrAotFunctionTable;

TZrBool backend_aot_build_function_table(SZrState *state,
                                         const SZrFunction *function,
                                         SZrAotFunctionTable *outTable);
void backend_aot_release_function_table(SZrState *state, SZrAotFunctionTable *table);
TZrBool backend_aot_resolve_callable_constant_function_index(const SZrAotFunctionTable *table,
                                                             SZrState *state,
                                                             const SZrFunction *function,
                                                             TZrInt32 constantIndex,
                                                             TZrUInt32 *outFunctionIndex);

#endif
