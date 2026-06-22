#ifndef ZR_VM_PARSER_BACKEND_AOT_C_FRAME_DESCRIPTOR_H
#define ZR_VM_PARSER_BACKEND_AOT_C_FRAME_DESCRIPTOR_H

#include "backend_aot_exec_ir.h"

TZrBool backend_aot_c_function_body_needs_frame_descriptor(const SZrAotExecIrModule *module,
                                                           const SZrAotExecIrFunction *functionIr,
                                                           const SZrFunction *function,
                                                           TZrBool publishExports,
                                                           TZrBool needsFrameCleanup);

#endif
