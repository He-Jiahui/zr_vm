#ifndef ZR_VM_PARSER_BACKEND_AOT_C_GENERIC_MONOMORPHIZATION_H
#define ZR_VM_PARSER_BACKEND_AOT_C_GENERIC_MONOMORPHIZATION_H

#include <stdio.h>

#include "backend_aot_function_table.h"

/** @brief 为已闭合且未找到原生结构布局的泛型值槽生成回退 C 布局。 */
void backend_aot_write_c_generic_monomorphization_layouts(FILE *file,
                                                          SZrState *state,
                                                          const SZrAotFunctionTable *table);

/** @brief 为函数表中独立的闭合泛型值槽输出单态化调用条目。 */
void backend_aot_write_c_generic_monomorphization_entries(FILE *file,
                                                          const SZrAotFunctionTable *table,
                                                          TZrBool stripGeneratedSymbols);

#endif
