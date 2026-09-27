#ifndef ZR_VM_PARSER_BACKEND_AOT_C_REFERENCE_LOCALS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_REFERENCE_LOCALS_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"
#include "backend_aot_function_table.h"

/** @brief 判断 TO_STRING/TO_OBJECT 是否需要按地址注册生成 C 的引用局部变量。 */
TZrBool backend_aot_c_reference_locals_has_locals(const SZrAotExecIrFunction *functionIr);
/** @brief 为每个函数生成去重后的引用局部变量结构。 */
void backend_aot_write_c_reference_local_structs(FILE *file, const SZrAotFunctionTable *table);
/** @brief 为对应结构生成 LOCAL_ADDRESS GC 根描述符。 */
void backend_aot_write_c_reference_local_root_maps(FILE *file, const SZrAotFunctionTable *table);
/** @brief 在函数体内实例化零初始化的引用局部变量结构。 */
void backend_aot_write_c_reference_locals(FILE *file, const SZrAotExecIrFunction *functionIr);
/** @brief 声明根帧与是否已压入的状态，用于失败路径清理。 */
void backend_aot_write_c_reference_local_root_frame_declaration(FILE *file);
/** @brief 在可能触发 GC 的指令之前压入引用局部变量根帧。 */
void backend_aot_write_c_reference_local_root_frame_push(FILE *file, const SZrAotExecIrFunction *functionIr);
/** @brief 在统一退出路径中仅弹出已成功压入的根帧。 */
void backend_aot_write_c_reference_local_root_frame_cleanup(FILE *file);

#endif
