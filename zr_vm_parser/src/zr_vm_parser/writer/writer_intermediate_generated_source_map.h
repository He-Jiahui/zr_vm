#ifndef ZR_VM_PARSER_WRITER_INTERMEDIATE_GENERATED_SOURCE_MAP_H
#define ZR_VM_PARSER_WRITER_INTERMEDIATE_GENERATED_SOURCE_MAP_H

#include <stdio.h>

#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"

/**
 * @brief 在创建 .zri 前验证整个函数树的紧凑 prototype 记录边界。
 * @note 调用方仍须保证其他 SZrFunction 数组与长度匹配；此检查只覆盖
 *       prototypeData 的布局以及子函数列表的可遍历性。
 */
TZrBool writer_intermediate_validate_function_prototype_data(
        const SZrFunction *function);

/**
 * @brief 将编译期生成字段的来源映射附加到可读的函数投影。
 * @pre function 的 prototype 布局已经通过上述验证，state 与其常量池仍有效。
 * @note 普通成员和缺少完整来源元数据的成员不产生映射段。
 */
void writer_intermediate_write_generated_source_maps(
        FILE *file,
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 indentLevel);

#endif // ZR_VM_PARSER_WRITER_INTERMEDIATE_GENERATED_SOURCE_MAP_H
