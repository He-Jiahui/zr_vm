#ifndef ZR_VM_PARSER_BACKEND_AOT_FUNCTION_TABLE_H
#define ZR_VM_PARSER_BACKEND_AOT_FUNCTION_TABLE_H

#include "zr_vm_parser/writer.h"

struct SZrAotReachabilityMark;

typedef struct SZrAotFunctionEntry {
    const SZrFunction *function;
    TZrUInt32 flatIndex;
} SZrAotFunctionEntry;

typedef struct SZrAotFunctionTable {
    SZrAotFunctionEntry *entries;
    TZrUInt32 count;
    TZrUInt32 capacity;
    TZrUInt32 indexSpace;
} SZrAotFunctionTable;

TZrBool backend_aot_build_function_table(SZrState *state,
                                         const SZrFunction *function,
                                         SZrAotFunctionTable *outTable);
void backend_aot_release_function_table(SZrState *state, SZrAotFunctionTable *table);
TZrBool backend_aot_filter_function_table_by_reachability(SZrAotFunctionTable *table,
                                                           const struct SZrAotReachabilityMark *marks,
                                                           TZrUInt32 markCount);
TZrUInt32 backend_aot_function_table_index_space(const SZrAotFunctionTable *table);
TZrBool backend_aot_resolve_callable_constant_function_index(const SZrAotFunctionTable *table,
                                                             SZrState *state,
                                                             const SZrFunction *function,
                                                             TZrInt32 constantIndex,
                                                             TZrUInt32 *outFunctionIndex);
TZrUInt32 backend_aot_find_function_table_index(const SZrAotFunctionTable *table, const SZrFunction *function);

#endif
