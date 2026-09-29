#ifndef ZR_VM_PARSER_COMPILER_METADATA_REF_H
#define ZR_VM_PARSER_COMPILER_METADATA_REF_H

#include "compiler_internal.h"

/**
 * @brief 从函数 metadata 聚合模块级导入引用快照，供 artifact 写出及运行时查询。
 * @pre cs、其全局状态及 function 必须有效；关联实体必须有对应 SIGNATURE 记录。
 * @return 无记录时成功生成空表；输入无效、分配失败或关联签名缺失时返回 false。
 */
TZrBool compiler_build_module_metadata_ref_table(SZrCompilerState *cs, SZrFunction *function);

#endif // ZR_VM_PARSER_COMPILER_METADATA_REF_H
