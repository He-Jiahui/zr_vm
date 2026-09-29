#ifndef ZR_VM_PARSER_COMPILE_TIME_DECORATOR_IDENTITY_H
#define ZR_VM_PARSER_COMPILE_TIME_DECORATOR_IDENTITY_H

#include "compiler_internal.h"

/**
 * @brief 为 function/member/parameter decorator snapshot 补上原声明对应的 semantic symbol id。
 * @pre cs、cs->state 及 semantic context 有效，snapshot/declaration/name 均有效。
 * @return 返回 false 时调用方不得继续执行 decorator；底层对象 setter 无状态返回，存储条目分配失败可能无法反馈。
 */
TZrBool ZrParser_CompileTime_EnsureDecoratorSnapshotSymbol(
        SZrCompilerState *cs,
        SZrObject *snapshot,
        SZrAstNode *declarationNode,
        SZrString *name,
        EZrSemanticSymbolKind kind,
        TZrTypeId typeId,
        TZrSymbolId *outSymbolId);

/**
 * @brief 验证 function/member/parameter leaf transform 返回的 typed Patch 身份与受限形状。
 * @pre cs 和 cs->state 有效；targetSnapshot 与 patchValue 是有效对象值。
 * @note 无内部 role 字段表示普通对象：函数返回 true 但 outIsTypedPatch=false，调用方必须另行拒绝。
 * @return typed Patch 不匹配或包含禁止集合时记录诊断并返回 false。
 */
TZrBool ZrParser_CompileTime_ValidateLeafDeclarationPatch(
        SZrCompilerState *cs,
        const SZrTypeValue *targetSnapshot,
        const SZrTypeValue *patchValue,
        SZrFileRange location,
        TZrBool *outIsTypedPatch);

#endif
