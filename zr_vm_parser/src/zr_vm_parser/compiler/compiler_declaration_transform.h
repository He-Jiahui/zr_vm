#ifndef ZR_VM_PARSER_COMPILER_DECLARATION_TRANSFORM_H
#define ZR_VM_PARSER_COMPILER_DECLARATION_TRANSFORM_H

#include "compiler_internal.h"

/**
 * @brief 为带 declarationTransform 属性的 comptime 函数验证同步、单目标视图到 Patch 的签名契约。
 * @pre 调用方已完成属性角色识别，cs 处于 comptime 上下文，binding 中包含实际导入的声明 provider。
 * @return 签名合法返回 true；不合法返回 false 并在有效编译状态下报告错误。
 */
TZrBool ZrParser_DeclarationTransform_ValidateSignature(
        SZrCompilerState *cs,
        SZrAstNode *functionNode);

#endif // ZR_VM_PARSER_COMPILER_DECLARATION_TRANSFORM_H
