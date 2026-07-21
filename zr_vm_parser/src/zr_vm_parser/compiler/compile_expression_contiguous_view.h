#ifndef ZR_VM_PARSER_COMPILE_EXPRESSION_CONTIGUOUS_VIEW_H
#define ZR_VM_PARSER_COMPILE_EXPRESSION_CONTIGUOUS_VIEW_H

#include "compile_expression_internal.h"

typedef enum EZrCompilerContiguousViewLoweringResult {
    ZR_COMPILER_CONTIGUOUS_VIEW_NOT_APPLICABLE = 0,
    ZR_COMPILER_CONTIGUOUS_VIEW_LOWERED,
    ZR_COMPILER_CONTIGUOUS_VIEW_ERROR
} EZrCompilerContiguousViewLoweringResult;

TZrBool compiler_contiguous_view_member_is_structural(
        const SZrTypeMemberInfo *memberInfo);

TZrBool compiler_contiguous_view_type_is_readonly(
        SZrCompilerState *cs,
        SZrString *typeName);

EZrCompilerContiguousViewLoweringResult
compiler_contiguous_view_lower_member_call(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo,
        SZrString *receiverTypeName,
        TZrUInt32 receiverSlot,
        TZrUInt32 argumentStartSlot,
        TZrUInt32 argumentCount,
        const SZrAstNodeArray *argumentNodes,
        TZrUInt32 resultSlot,
        const SZrInferredType *resultType,
        SZrFileRange location);

EZrCompilerContiguousViewLoweringResult
compiler_contiguous_view_lower_index_get(
        SZrCompilerState *cs,
        SZrString *receiverTypeName,
        TZrUInt32 receiverSlot,
        TZrUInt32 indexSlot,
        const SZrAstNode *indexExpression,
        TZrUInt32 resultSlot,
        SZrFileRange location);

EZrCompilerContiguousViewLoweringResult
compiler_contiguous_view_lower_index_set(
        SZrCompilerState *cs,
        SZrString *receiverTypeName,
        TZrUInt32 receiverSlot,
        TZrUInt32 indexSlot,
        const SZrAstNode *indexExpression,
        TZrUInt32 valueSlot,
        SZrFileRange location);

#endif
