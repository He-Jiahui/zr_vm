#ifndef ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_CONVERSION_H
#define ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_CONVERSION_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

/** @brief 为受支持的数值转换生成 C；返回 false 时由后续执行路径处理。 */
TZrBool backend_aot_try_write_c_scalar_conversion(FILE *file,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  const TZrInstruction *execInstruction,
                                                  TZrUInt32 execInstructionIndex);

#endif
