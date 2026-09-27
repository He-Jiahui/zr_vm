#ifndef ZR_VM_PARSER_BACKEND_AOT_C_FRAME_DESCRIPTOR_H
#define ZR_VM_PARSER_BACKEND_AOT_C_FRAME_DESCRIPTOR_H

#include "backend_aot_exec_ir.h"

/** @brief 保守证明整段生成函数是否仍访问 frame；无法证明时必须保留描述符。 */
TZrBool backend_aot_c_function_body_needs_frame_descriptor(const SZrAotExecIrModule *module,
                                                           const SZrAotExecIrFunction *functionIr,
                                                           const SZrFunction *function,
                                                           TZrBool publishExports,
                                                           TZrBool needsFrameCleanup);

#endif
