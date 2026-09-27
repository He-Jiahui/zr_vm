#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_DIRECT_BOOL_CALLS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_DIRECT_BOOL_CALLS_H

#include "backend_aot_internal.h"

/** @brief 检查零参数 bool 目标与结果标量槽是否允许直接调用。 */
TZrBool backend_aot_can_write_c_static_direct_bool_no_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 calleeFunctionIndex);
/** @brief 检查一参数 bool 调用，并在成功时返回已初始化的参数槽。 */
TZrBool backend_aot_can_write_c_static_direct_bool_one_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outArgumentSlot);
/** @brief 检查双 bool 参数调用和两参数槽的写入先序。 */
TZrBool backend_aot_can_write_c_static_direct_bool_two_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outFirstArgumentSlot,
        TZrUInt32 *outSecondArgumentSlot);
/** @brief 检查三 bool 参数调用和三参数槽的写入先序。 */
TZrBool backend_aot_can_write_c_static_direct_bool_three_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outFirstArgumentSlot,
        TZrUInt32 *outSecondArgumentSlot,
        TZrUInt32 *outThirdArgumentSlot);
/** @brief 检查两个 i64 参数、bool 返回值的静态直接调用。 */
TZrBool backend_aot_can_write_c_static_direct_i64_bool_two_arg_call(
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrUInt32 *outFirstArgumentSlot,
        TZrUInt32 *outSecondArgumentSlot);

#endif
