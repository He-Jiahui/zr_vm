#ifndef ZR_VM_DEBUG_EVALUATION_EFFECT_INTERNAL_H
#define ZR_VM_DEBUG_EVALUATION_EFFECT_INTERNAL_H

#include "debug_internal.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_inference.h"

typedef struct SZrDebugFormalEvaluationContext {
    SZrParserState parserState;
    SZrCompilerState compilerState;
    SZrInferredType inferredType;
    SZrStructuredDiagnostic parserDiagnostic;
    SZrAstNode *expression;
    const SZrSemanticExpressionFact *expressionFact;
    TZrBool parserStateInitialized;
    TZrBool compilerStateInitialized;
    TZrBool inferredTypeInitialized;
    TZrBool hasParserDiagnostic;
    TZrBool hasCanonicalFacts;
} SZrDebugFormalEvaluationContext;

void zr_debug_evaluation_effect_classify_structure(const SZrAstNode *node, TZrUInt32 *effectFlags);
void zr_debug_evaluation_effect_classify_resolved_properties(
        const SZrSemanticContext *context,
        const SZrAstNode *node,
        TZrUInt32 *effectFlags);
TZrBool zr_debug_evaluation_effect_has_canonical_facts(const SZrSemanticContext *context,
                                                       const SZrAstNode *expression,
                                                       const SZrSemanticExpressionFact *expressionFact);
ZR_DEBUG_API TZrBool zr_debug_formal_prepare_expression(ZrDebugAgent *agent,
                                                        TZrUInt32 frameId,
                                                        const TZrChar *expression,
                                                        SZrDebugFormalEvaluationContext *outContext,
                                                        TZrChar *errorBuffer,
                                                        TZrSize errorBufferSize);
TZrBool zr_debug_formal_prepare_expression_with_failure(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const TZrChar *expression,
        SZrDebugFormalEvaluationContext *outContext,
        ZrDebugEvaluateFailure *outFailure,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);
ZR_DEBUG_API void zr_debug_formal_free_prepared_expression(SZrDebugFormalEvaluationContext *context);
TZrBool zr_debug_formal_has_paused_array_index_facts(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const SZrSemanticContext *semanticContext,
        const SZrAstNode *expression,
        const SZrSemanticExpressionFact *expressionFact);
ZR_DEBUG_API TZrBool zr_debug_formal_evaluate_node(ZrDebugAgent *agent,
                                                   TZrUInt32 frameId,
                                                   const SZrSemanticContext *semanticContext,
                                                   const SZrAstNode *node,
                                                   SZrTypeValue *outValue,
                                                   TZrBool *outSupported,
                                                   TZrChar *errorBuffer,
                                                   TZrSize errorBufferSize);

#endif // ZR_VM_DEBUG_EVALUATION_EFFECT_INTERNAL_H
