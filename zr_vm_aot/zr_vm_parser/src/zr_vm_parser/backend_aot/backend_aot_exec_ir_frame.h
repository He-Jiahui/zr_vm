#ifndef ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_FRAME_H
#define ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_FRAME_H

#include "backend_aot_exec_ir.h"

TZrBool backend_aot_exec_ir_build_frame_layout(
        SZrState *state,
        const SZrFunction *function,
        SZrAotExecIrFrameLayout *outFrameLayout);
void backend_aot_exec_ir_release_frame_layout(
        SZrState *state,
        SZrAotExecIrFrameLayout *frameLayout);

#endif
