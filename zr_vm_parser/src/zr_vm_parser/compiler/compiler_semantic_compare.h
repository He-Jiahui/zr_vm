#ifndef ZR_VM_PARSER_COMPILER_SEMANTIC_COMPARE_H
#define ZR_VM_PARSER_COMPILER_SEMANTIC_COMPARE_H

#include "compiler_internal.h"

typedef enum EZrCompilerSemanticCompareResult {
    ZR_COMPILER_SEMANTIC_COMPARE_NOT_APPLICABLE = 0,
    ZR_COMPILER_SEMANTIC_COMPARE_LOWERED,
    ZR_COMPILER_SEMANTIC_COMPARE_FAILED
} EZrCompilerSemanticCompareResult;

TZrBool compiler_semantic_compare_source_supported(const SZrAstNode *node);
EZrCompilerSemanticCompareResult compiler_semantic_compare_lower(
        SZrCompilerState *cs, const SZrAstNode *node,
        EZrInstructionCode opcode, TZrUInt32 leftSlot, TZrUInt32 rightSlot,
        TZrUInt32 resultSlot, const SZrInferredType *resultType);
TZrBool compiler_semantic_compare_validate(const SZrCompilerState *cs);
TZrBool compiler_semantic_compare_condition_valid(
        const SZrCompilerState *cs, const SZrAstNode *node,
        const SZrSemanticIrValue *condition);

#endif
