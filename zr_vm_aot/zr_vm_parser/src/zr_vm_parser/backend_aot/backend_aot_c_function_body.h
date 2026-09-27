#ifndef ZR_VM_PARSER_BACKEND_AOT_C_FUNCTION_BODY_H
#define ZR_VM_PARSER_BACKEND_AOT_C_FUNCTION_BODY_H

#include <stdio.h>

#include "backend_aot_internal.h"

/** @brief 按 ExecIR/字节码分派写出一个函数体，负责帧准备、逐指令降低和统一退出清理。
 * @pre entry 指向已建表函数；module 若存在须与 entry 的 flat index 对应。
 */
void backend_aot_write_c_function_body(FILE *file,
                                       SZrState *state,
                                       const SZrAotFunctionTable *functionTable,
                                       const SZrAotExecIrModule *module,
                                       const SZrAotFunctionEntry *entry,
                                       const SZrAotWriterOptions *options);

#endif
