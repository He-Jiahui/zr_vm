#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPE_LAYOUT_REACHABILITY_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPE_LAYOUT_REACHABILITY_H

#include <stdio.h>

#include "backend_aot_function_table.h"

/** @brief 校验并输出布局可达性清单；保留计数必须与 emitter 的统计一致。 */
TZrBool backend_aot_c_type_layout_reachability_write_manifest(
        FILE *file,
        SZrState *state,
        const SZrAotFunctionTable *table,
        const TZrUInt32 *typeLayoutRoots,
        TZrUInt32 typeLayoutRootCount,
        TZrUInt32 expectedRetainedCount);

#endif
