#ifndef ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_LOCALS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_LOCALS_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

/** @brief 根据类型绑定、SemIR 和执行指令为候选槽声明 C 标量局部变量。 */
void backend_aot_write_c_scalar_locals(FILE *file, const SZrAotExecIrFunction *functionIr);
/** @brief 检查槽是否可作为布尔标量局部变量。 */
TZrBool backend_aot_c_scalar_locals_has_bool_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot);
/** @brief 检查槽是否可作为浮点标量局部变量。 */
TZrBool backend_aot_c_scalar_locals_has_f64_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot);
/** @brief 检查槽是否可作为有符号整数标量局部变量。 */
TZrBool backend_aot_c_scalar_locals_has_i64_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot);
/** @brief 检查槽是否可作为无符号整数标量局部变量。 */
TZrBool backend_aot_c_scalar_locals_has_u64_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot);
/** @brief 识别执行指令对指定槽的原始值写入，供复制来源证明使用。 */
TZrBool backend_aot_c_scalar_locals_instruction_writes_primitive(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 slot);
/** @brief 在控制流前驱合流后证明布尔局部值已在当前指令前写入。 */
TZrBool backend_aot_c_scalar_locals_bool_written_before(const SZrAotExecIrFunction *functionIr,
                                                        TZrUInt32 slot,
                                                        TZrUInt32 execInstructionIndex);
/** @brief 对需要布尔值而非仅布尔类型的路径验证到达写入。 */
TZrBool backend_aot_c_scalar_locals_bool_value_written_before(const SZrAotExecIrFunction *functionIr,
                                                              TZrUInt32 slot,
                                                              TZrUInt32 execInstructionIndex);
/** @brief 证明浮点局部值在当前指令前已由所有可达路径定义。 */
TZrBool backend_aot_c_scalar_locals_f64_written_before(const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 slot,
                                                       TZrUInt32 execInstructionIndex);
/** @brief 证明有符号整数局部值在当前指令前已由所有可达路径定义。 */
TZrBool backend_aot_c_scalar_locals_i64_written_before(const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 slot,
                                                       TZrUInt32 execInstructionIndex);
/** @brief 证明无符号整数局部值在当前指令前已由所有可达路径定义。 */
TZrBool backend_aot_c_scalar_locals_u64_written_before(const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 slot,
                                                       TZrUInt32 execInstructionIndex);
/** @brief 验证返回槽的有符号局部值可直接用于返回。 */
TZrBool backend_aot_c_scalar_locals_can_direct_return_i64_local(const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 slot,
                                                                TZrUInt32 execInstructionIndex);
/** @brief 验证返回槽的布尔局部值可直接用于返回。 */
TZrBool backend_aot_c_scalar_locals_can_direct_return_bool_local(const SZrAotExecIrFunction *functionIr,
                                                                 TZrUInt32 slot,
                                                                 TZrUInt32 execInstructionIndex);
/** @brief 验证返回槽的无符号局部值可直接用于返回。 */
TZrBool backend_aot_c_scalar_locals_can_direct_return_u64_local(const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 slot,
                                                                TZrUInt32 execInstructionIndex);
/** @brief 验证返回槽的浮点局部值可直接用于返回。 */
TZrBool backend_aot_c_scalar_locals_can_direct_return_f64_local(const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 slot,
                                                                TZrUInt32 execInstructionIndex);
/** @brief 为布尔返回值推断可用的局部来源。 */
TZrBool backend_aot_c_scalar_locals_can_infer_return_bool_local(const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 slot,
                                                                TZrUInt32 execInstructionIndex);
/** @brief 为无符号整数返回值推断可用的局部来源。 */
TZrBool backend_aot_c_scalar_locals_can_infer_return_u64_local(const SZrAotExecIrFunction *functionIr,
                                                               TZrUInt32 slot,
                                                               TZrUInt32 execInstructionIndex);
/** @brief 为浮点返回值推断可用的局部来源。 */
TZrBool backend_aot_c_scalar_locals_can_infer_return_f64_local(const SZrAotExecIrFunction *functionIr,
                                                               TZrUInt32 slot,
                                                               TZrUInt32 execInstructionIndex);
/** @brief 仅在复位槽后无值槽消费者时省去单槽复位的值槽写入。 */
TZrBool backend_aot_c_scalar_locals_reset_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                              TZrUInt32 slot,
                                                              TZrUInt32 execInstructionIndex);
/** @brief 对双槽复位分别检查后续消费者与写入覆盖。 */
TZrBool backend_aot_c_scalar_locals_reset2_can_skip_value_slots(const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 firstSlot,
                                                                TZrUInt32 secondSlot,
                                                                TZrUInt32 execInstructionIndex);
/** @brief 判定布尔常量结果能否只保留 C 局部变量。 */
TZrBool backend_aot_c_scalar_locals_bool_constant_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                      TZrUInt32 slot,
                                                                      TZrUInt32 execInstructionIndex);
/** @brief 判定有符号整数常量结果能否只保留 C 局部变量。 */
TZrBool backend_aot_c_scalar_locals_i64_constant_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                     TZrUInt32 slot,
                                                                     TZrUInt32 execInstructionIndex);
/** @brief 判定无符号整数常量结果能否只保留 C 局部变量。 */
TZrBool backend_aot_c_scalar_locals_u64_constant_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                     TZrUInt32 slot,
                                                                     TZrUInt32 execInstructionIndex);
/** @brief 判定浮点常量结果能否只保留 C 局部变量。 */
TZrBool backend_aot_c_scalar_locals_f64_constant_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                     TZrUInt32 slot,
                                                                     TZrUInt32 execInstructionIndex);
/** @brief 当所有后续有符号消费者均读取局部值时省去结果值槽写入。 */
TZrBool backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                   TZrUInt32 slot,
                                                                   TZrUInt32 execInstructionIndex);
/** @brief 当所有后续浮点消费者均读取局部值时省去结果值槽写入。 */
TZrBool backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                   TZrUInt32 slot,
                                                                   TZrUInt32 execInstructionIndex);
/** @brief 当所有后续布尔消费者均读取局部值时省去结果值槽写入。 */
TZrBool backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                    TZrUInt32 slot,
                                                                    TZrUInt32 execInstructionIndex);
/** @brief 当所有后续无符号消费者均读取局部值时省去结果值槽写入。 */
TZrBool backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                   TZrUInt32 slot,
                                                                   TZrUInt32 execInstructionIndex);

#endif
