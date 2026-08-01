#ifndef ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_RETURN_LAYOUT_H
#define ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_RETURN_LAYOUT_H

#include "backend_aot_exec_ir.h"

TZrBool backend_aot_exec_ir_project_direct_inline_return_layout(
        SZrState *state,
        const SZrFunction *function,
        SZrAotExecIrFunction *outFunction);

#endif
