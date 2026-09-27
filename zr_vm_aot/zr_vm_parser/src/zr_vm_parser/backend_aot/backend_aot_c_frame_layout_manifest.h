#ifndef ZR_VM_PARSER_BACKEND_AOT_C_FRAME_LAYOUT_MANIFEST_H
#define ZR_VM_PARSER_BACKEND_AOT_C_FRAME_LAYOUT_MANIFEST_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"
#include "backend_aot_function_table.h"

/** @brief 校验保留函数的帧布局对应关系并统计槽数；失败时 outCount 为零。 */
TZrBool backend_aot_c_frame_layout_count_slots(
        const SZrAotExecIrModule *module,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 *outCount);

/** @brief 按 flat index 写出裁剪后仍存活的帧槽清单。 */
TZrBool backend_aot_c_frame_layout_write_reachability_manifest(
        FILE *file,
        const SZrAotExecIrModule *module,
        const SZrAotFunctionTable *functionTable);

#endif
