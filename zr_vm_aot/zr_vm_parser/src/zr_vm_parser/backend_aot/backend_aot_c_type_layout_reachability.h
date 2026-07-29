#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPE_LAYOUT_REACHABILITY_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPE_LAYOUT_REACHABILITY_H

#include <stdio.h>

#include "backend_aot_function_table.h"

TZrBool backend_aot_c_type_layout_reachability_write_manifest(
        FILE *file,
        SZrState *state,
        const SZrAotFunctionTable *table,
        const TZrUInt32 *typeLayoutRoots,
        TZrUInt32 typeLayoutRootCount,
        TZrUInt32 expectedRetainedCount);

#endif
