#ifndef ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_FIELD_SCALAR_LOCALS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_FIELD_SCALAR_LOCALS_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"
#include "zr_vm_core/function.h"

/** @brief 在源标量局部值已定义时直接写入内联字段字节，避免读取未物化的值槽。 */
TZrBool backend_aot_try_write_c_value_field_scalar_local_store_exec(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        const SZrAotExecIrFrameSlotLayout *destinationLayout,
        const SZrAotExecIrInstruction *instruction,
        const SZrFunctionFrameFieldLayout *fieldLayout,
        const char *fieldTypeName);

#endif
