#ifndef ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_FIELDS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_FIELDS_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

/** @brief 记录 FIELD_ADDR 的源槽与字段布局以供生成源码审查。 */
void backend_aot_write_c_value_semir_field_addr(FILE *file,
                                                SZrState *state,
                                                const SZrAotExecIrFunction *functionIr,
                                                const SZrAotExecIrFrameLayout *frameLayout,
                                                const SZrAotExecIrInstruction *instruction);
/** @brief 记录 LOAD_VALUE 的字段与目标槽布局。 */
void backend_aot_write_c_value_semir_load(FILE *file,
                                          SZrState *state,
                                          const SZrAotExecIrFunction *functionIr,
                                          const SZrAotExecIrFrameLayout *frameLayout,
                                          const SZrAotExecIrInstruction *instruction);
/** @brief 记录 STORE_VALUE 的字段与源槽布局。 */
void backend_aot_write_c_value_semir_store(FILE *file,
                                           SZrState *state,
                                           const SZrAotExecIrFunction *functionIr,
                                           const SZrAotExecIrFrameLayout *frameLayout,
                                           const SZrAotExecIrInstruction *instruction);
/** @brief 在字段布局和目标槽兼容时生成原始值或内联结构体字段读取。 */
TZrBool backend_aot_try_write_c_value_semir_field_load_exec(FILE *file,
                                                           SZrState *state,
                                                           const SZrAotExecIrFunction *functionIr,
                                                           const SZrAotExecIrFrameLayout *frameLayout,
                                                           const SZrAotExecIrInstruction *instruction);
/** @brief 在字段布局和源槽兼容时生成字段写入，保留非 POD 的布局复制语义。 */
TZrBool backend_aot_try_write_c_value_semir_field_store_exec(FILE *file,
                                                            SZrState *state,
                                                            const SZrAotExecIrFunction *functionIr,
                                                            const SZrAotExecIrFrameLayout *frameLayout,
                                                            const SZrAotExecIrInstruction *instruction);

#endif
