#include "compile_expression_internal.h"

#include <string.h>

#include "zr_vm_parser/semantic_facts.h"

static EZrOwnershipBuiltinKind ownership_intrinsic_builtin_kind(
        EZrOwnershipIntrinsicOperation operation) {
    switch (operation) {
        case ZR_OWNERSHIP_INTRINSIC_SHARE:
            return ZR_OWNERSHIP_BUILTIN_KIND_SHARED;
        case ZR_OWNERSHIP_INTRINSIC_DEGRADE:
            return ZR_OWNERSHIP_BUILTIN_KIND_WEAK;
        case ZR_OWNERSHIP_INTRINSIC_WAKE:
            return ZR_OWNERSHIP_BUILTIN_KIND_UPGRADE;
        case ZR_OWNERSHIP_INTRINSIC_INTO_GC:
            return ZR_OWNERSHIP_BUILTIN_KIND_INTO_GC;
        case ZR_OWNERSHIP_INTRINSIC_DROP:
            return ZR_OWNERSHIP_BUILTIN_KIND_RELEASE;
        default:
            return ZR_OWNERSHIP_BUILTIN_KIND_NONE;
    }
}

void compile_ownership_intrinsic_expression(SZrCompilerState *cs, SZrAstNode *node) {
    const SZrOwnershipIntrinsicFact *fact;
    SZrConstructExpression constructExpression;
    SZrInferredType inferredType;
    EZrOwnershipBuiltinKind builtinKind;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError ||
        node->type != ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION) {
        return;
    }

    fact = ZrParser_SemanticFacts_FindOwnershipIntrinsicByNode(
            cs->semanticContext, node);
    if (fact == ZR_NULL) {
        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_ExpressionType_Infer(cs, node, &inferredType)) {
            ZrParser_InferredType_Free(cs->state, &inferredType);
            return;
        }
        ZrParser_InferredType_Free(cs->state, &inferredType);
        fact = ZrParser_SemanticFacts_FindOwnershipIntrinsicByNode(
                cs->semanticContext, node);
    }
    if (fact == ZR_NULL || fact->argument == ZR_NULL) {
        ZrParser_Compiler_Error(
                cs, "Ownership intrinsic is missing its canonical semantic fact", node->location);
        return;
    }

    builtinKind = ownership_intrinsic_builtin_kind(fact->operation);
    if (builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_NONE) {
        ZrParser_Compiler_Error(cs, "Unsupported ownership intrinsic", fact->range);
        return;
    }

    memset(&constructExpression, 0, sizeof(constructExpression));
    constructExpression.target = fact->argument;
    constructExpression.builtinKind = builtinKind;
    constructExpression.isNew = ZR_FALSE;
    compile_ownership_builtin_expression(
            cs, &constructExpression, ZR_PARSER_SLOT_NONE, fact->range);
}
