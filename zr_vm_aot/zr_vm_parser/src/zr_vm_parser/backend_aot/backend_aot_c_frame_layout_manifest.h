#ifndef ZR_VM_PARSER_BACKEND_AOT_C_FRAME_LAYOUT_MANIFEST_H
#define ZR_VM_PARSER_BACKEND_AOT_C_FRAME_LAYOUT_MANIFEST_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"
#include "backend_aot_function_table.h"

TZrBool backend_aot_c_frame_layout_count_slots(
        const SZrAotExecIrModule *module,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 *outCount);

TZrBool backend_aot_c_frame_layout_write_reachability_manifest(
        FILE *file,
        const SZrAotExecIrModule *module,
        const SZrAotFunctionTable *functionTable);

#endif
