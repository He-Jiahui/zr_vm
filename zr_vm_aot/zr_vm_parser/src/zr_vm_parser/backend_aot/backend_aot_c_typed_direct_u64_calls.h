#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_DIRECT_U64_CALLS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_DIRECT_U64_CALLS_H

#include "backend_aot_internal.h"

/** @brief 核对零参数 u64 thunk 与目标标量槽。 */
TZrBool backend_aot_can_write_c_static_direct_u64_no_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 calleeFunctionIndex);
/** @brief 核对一参数 u64 thunk 与已写入的参数槽。 */
TZrBool backend_aot_can_write_c_static_direct_u64_one_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outArgumentSlot);
/** @brief 核对二参数 u64 thunk；成功时返回两参数槽及 state ABI 选择。 */
TZrBool backend_aot_can_write_c_static_direct_u64_two_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outFirstArgumentSlot,
        TZrUInt32 *outSecondArgumentSlot,
        TZrBool *outPassStateToThunk);
/** @brief 核对两个 u64 参数、bool 返回值的静态直接调用。 */
TZrBool backend_aot_can_write_c_static_direct_u64_bool_two_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outFirstArgumentSlot,
        TZrUInt32 *outSecondArgumentSlot);
/** @brief 核对三参数 u64 thunk；成功时返回参数槽及 state ABI 选择。 */
TZrBool backend_aot_can_write_c_static_direct_u64_three_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outFirstArgumentSlot,
        TZrUInt32 *outSecondArgumentSlot,
        TZrUInt32 *outThirdArgumentSlot,
        TZrBool *outPassStateToThunk);

#endif
