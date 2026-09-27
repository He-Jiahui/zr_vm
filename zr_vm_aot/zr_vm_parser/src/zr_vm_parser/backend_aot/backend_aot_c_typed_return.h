#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_RETURN_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_RETURN_H

#include <stdio.h>

#include "backend_aot_internal.h"

/** @brief 在返回槽的标量类型可证明时直接生成返回代码。
 * @note 返回假表示调用方仍需走通用值槽路径；写出前按需发布导出。
 */
TZrBool backend_aot_try_write_c_typed_return(FILE *file,
                                             const SZrAotExecIrFunction *functionIr,
                                             TZrUInt32 sourceSlot,
                                             TZrUInt32 execInstructionIndex,
                                             TZrBool publishExports);

#endif
