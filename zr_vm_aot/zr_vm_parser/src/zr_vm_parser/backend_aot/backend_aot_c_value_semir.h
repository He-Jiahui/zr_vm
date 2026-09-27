#ifndef ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_H
#define ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

/** @brief 为整个函数输出值类型 SemIR 布局与操作注解，供生成源码及侧车检查。 */
void backend_aot_write_c_value_semir_for_function(FILE *file,
                                                  SZrState *state,
                                                  const SZrAotExecIrModule *module,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  const SZrAotExecIrFrameLayout *frameLayout);
/** @brief 直接返回内联值时保留返回源槽，供帧清理跳过析构。 */
TZrBool backend_aot_c_value_semir_needs_skip_drop_slot(
        const SZrAotExecIrModule *module,
        const SZrAotExecIrFunction *functionIr);
/** @brief 尝试把对应执行指令的值 SemIR 写成可执行 C，未覆盖时交还调用方。 */
TZrBool backend_aot_try_write_c_value_semir_for_exec_instruction(FILE *file,
                                                                 SZrState *state,
                                                                 const SZrAotExecIrModule *module,
                                                                 const SZrAotExecIrFunction *functionIr,
                                                                 TZrUInt32 execInstructionIndex,
                                                                 TZrUInt32 calleeFunctionIndex,
                                                                 TZrBool requireFullAot,
                                                                 TZrBool allowTypedReturn);

#endif
