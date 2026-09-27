#ifndef ZR_VM_PARSER_BACKEND_AOT_C_DEBUG_SIDECAR_MANIFEST_H
#define ZR_VM_PARSER_BACKEND_AOT_C_DEBUG_SIDECAR_MANIFEST_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"
#include "backend_aot_function_table.h"

/** @brief 校验保留函数的源位置 sidecar 并统计节点，防止裁剪摘要计数溢出。 */
TZrBool backend_aot_c_debug_sidecar_count_locations(
        const SZrAotExecIrModule *module,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 *outCount);

/** @brief 按 flat index 顺序写出保留 sidecar 节点的可追踪清单。 */
TZrBool backend_aot_c_debug_sidecar_write_reachability_manifest(
        FILE *file,
        const SZrAotExecIrModule *module,
        const SZrAotFunctionTable *functionTable);

#endif
