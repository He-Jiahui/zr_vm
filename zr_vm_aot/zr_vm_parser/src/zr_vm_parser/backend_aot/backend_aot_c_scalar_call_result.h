#ifndef ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_CALL_RESULT_H
#define ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_CALL_RESULT_H

#include "backend_aot_exec_ir.h"

/** @brief 合并被调函数返回声明与调用点 SemIR 类型，阻止非原始值进入标量局部变量。 */
TZrBool backend_aot_c_scalar_call_result_has_nonprimitive_type(
        const SZrFunction *function,
        const SZrFunction *calleeFunction,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 destinationSlot);

#endif
