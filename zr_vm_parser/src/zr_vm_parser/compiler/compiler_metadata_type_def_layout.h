#ifndef ZR_VM_PARSER_COMPILER_METADATA_TYPE_DEF_LAYOUT_H
#define ZR_VM_PARSER_COMPILER_METADATA_TYPE_DEF_LAYOUT_H

#include "compiler_internal.h"

/**
 * @brief 计算 union TypeDef 的规范物理布局身份，供 metadata 与模块 ABI 摘要使用。
 * @pre unionDeclaration 必须为 union AST；两个输出指针必须有效。
 * @return 成功时输出布局 schema 版本和哈希；无法构造或验证布局时返回 false。
 * @note 只计算 schema version/hash，不发布运行时布局；TypeDef 发射器把身份写入元数据记录。
 */
TZrBool compiler_metadata_type_def_compute_union_layout_identity(SZrCompilerState *cs,
                                                                 const SZrAstNode *unionDeclaration,
                                                                 TZrUInt32 *outLayoutVersion,
                                                                 TZrUInt64 *outLayoutHash);

#endif
