#ifndef ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_SEMIR_H
#define ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_SEMIR_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

/** @brief 将执行指令对应的标量 SemIR 分派到转换、位操作、算术或比较生成器。 */
TZrBool backend_aot_try_write_c_scalar_semir_for_exec_instruction(FILE *file,
                                                                  const SZrAotExecIrModule *module,
                                                                  const SZrAotExecIrFunction *functionIr,
                                                                  const TZrInstruction *execInstruction,
                                                                  TZrUInt32 execInstructionIndex);
/** @brief 仅在生成代码不访问 frame 时允许调用方省去帧设置。 */
TZrBool backend_aot_c_scalar_semir_can_write_frame_free_for_exec_instruction(
        const SZrAotExecIrModule *module,
        const SZrAotExecIrFunction *functionIr,
        const TZrInstruction *execInstruction,
        TZrUInt32 execInstructionIndex);

#endif
