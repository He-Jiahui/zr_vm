#ifndef ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_CALLS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_CALLS_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

/** @brief 将类型化调用的目标槽布局写入生成源码注解。 */
void backend_aot_write_c_value_semir_call_typed(FILE *file,
                                                const SZrAotExecIrFrameLayout *frameLayout,
                                                const SZrAotExecIrInstruction *instruction);
/** @brief 将类型化返回的源槽布局写入生成源码注解。 */
void backend_aot_write_c_value_semir_return_typed(FILE *file,
                                                  const SZrAotExecIrFrameLayout *frameLayout,
                                                  const SZrAotExecIrInstruction *instruction);
/** @brief 仅对布局、传参和被调者均匹配的内联结构体生成直接调用或元数据失配回退。 */
TZrBool backend_aot_try_write_c_value_semir_call_typed_exec(FILE *file,
                                                           const SZrAotExecIrFrameLayout *frameLayout,
                                                           const SZrAotExecIrInstruction *instruction,
                                                           const SZrAotExecIrFunction *calleeFunctionIr,
                                                           TZrUInt32 callerFunctionIndex,
                                                           TZrUInt32 execInstructionIndex,
                                                           TZrUInt32 calleeFunctionIndex,
                                                           TZrBool requireFullAot);
/** @brief 为可直接返回的内联结构体输出返回桥，并转移源槽清理责任。 */
TZrBool backend_aot_try_write_c_value_semir_return_typed_exec(FILE *file,
                                                             const SZrAotExecIrFunction *functionIr,
                                                             const SZrAotExecIrInstruction *instruction,
                                                             TZrBool allowTypedReturn);

#endif
