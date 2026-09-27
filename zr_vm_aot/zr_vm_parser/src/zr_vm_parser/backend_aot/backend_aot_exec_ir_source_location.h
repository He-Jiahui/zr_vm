#ifndef ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_SOURCE_LOCATION_H
#define ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_SOURCE_LOCATION_H

#include "zr_vm_core/function.h"

/** @brief 校验执行指令到源码位置的有序映射，供 ExecIR 投影使用。 */
TZrBool backend_aot_exec_ir_validate_source_locations(const SZrFunction *function);
/** @brief 取执行指令起始行；先查逐指令行，再查位置区间与函数起始行。 */
TZrUInt32 backend_aot_exec_ir_debug_line_for_instruction(const SZrFunction *function,
                                                         TZrUInt32 execInstructionIndex);
/** @brief 取结束行，并保证非零起始行不会晚于结束行。 */
TZrUInt32 backend_aot_exec_ir_debug_line_end_for_instruction(const SZrFunction *function,
                                                             TZrUInt32 execInstructionIndex,
                                                             TZrUInt32 debugLine);
/** @brief 从不晚于该指令的最近位置记录取起始列；无记录返回零。 */
TZrUInt32 backend_aot_exec_ir_debug_column_for_instruction(const SZrFunction *function,
                                                           TZrUInt32 execInstructionIndex);
/** @brief 取结束列；缺失时使用传入的起始列并修正倒置范围。 */
TZrUInt32 backend_aot_exec_ir_debug_column_end_for_instruction(const SZrFunction *function,
                                                               TZrUInt32 execInstructionIndex,
                                                               TZrUInt32 debugColumn);

#endif
