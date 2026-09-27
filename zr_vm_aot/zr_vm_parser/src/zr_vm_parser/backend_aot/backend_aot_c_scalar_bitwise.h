#ifndef ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_BITWISE_H
#define ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_BITWISE_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

/** @brief 尝试为已知整数类型生成位操作，并保留值槽回退路径。 */
TZrBool backend_aot_try_write_c_scalar_bitwise(FILE *file,
                                               const SZrAotExecIrFunction *functionIr,
                                               const SZrAotExecIrInstruction *semIrInstruction,
                                               const TZrInstruction *execInstruction,
                                               EZrStaticCType staticCType);

#endif
