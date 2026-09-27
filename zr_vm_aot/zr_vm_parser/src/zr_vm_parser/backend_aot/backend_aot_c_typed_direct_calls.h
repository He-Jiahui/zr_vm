#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_DIRECT_CALLS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_DIRECT_CALLS_H

#include <stdio.h>

#include "backend_aot_internal.h"

/** @brief 为已证明类型和参数就绪的静态目标选择并生成 typed 直接调用。
 * @note requireFullAot 时只操作标量局部变量；否则保留运行时守卫及值槽同步。
 */
TZrBool backend_aot_try_write_c_static_direct_typed_function_call(
        FILE *file,
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrBool requireFullAot);
/** @brief 为无参数调用选择 typed thunk；返回假时由调用方生成通用调用。 */
TZrBool backend_aot_try_write_c_static_direct_typed_no_arg_function_call(
        FILE *file,
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrBool requireFullAot);

#endif
