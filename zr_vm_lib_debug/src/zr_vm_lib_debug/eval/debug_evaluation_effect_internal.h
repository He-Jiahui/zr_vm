#ifndef ZR_VM_DEBUG_EVALUATION_EFFECT_INTERNAL_H
#define ZR_VM_DEBUG_EVALUATION_EFFECT_INTERNAL_H

#include "debug_internal.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_inference.h"

/** 单次正式表达式分析的资源集合；AST、诊断和语义事实均以其中的解析器/编译器状态为生存期。
 * 初始化标志允许解析或推断在任意阶段失败后按依赖顺序释放。
 */
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
/** @brief 在暂停帧绑定下解析表达式并准备语义事实，供副作用分类器读取。
 * @note 成功时调用方必须调用 zr_debug_formal_free_prepared_expression；成功不保证有规范语义事实。
 */
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
/** @brief 释放准备阶段持有的推断结果、编译器、AST、诊断和解析器状态。 */
ZR_DEBUG_API void zr_debug_formal_free_prepared_expression(SZrDebugFormalEvaluationContext *context);
TZrBool zr_debug_formal_has_paused_array_index_facts(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const SZrSemanticContext *semanticContext,
        const SZrAstNode *expression,
        const SZrSemanticExpressionFact *expressionFact);
/** @brief 在规范语义上下文中读取暂停帧并执行上层已许可的表达式节点。
 * @return false 表示执行错误；true 且 *outSupported 为 false 表示节点未由正式执行器处理。
 */
ZR_DEBUG_API TZrBool zr_debug_formal_evaluate_node(ZrDebugAgent *agent,
                                                   TZrUInt32 frameId,
                                                   const SZrSemanticContext *semanticContext,
                                                   const SZrAstNode *node,
                                                   SZrTypeValue *outValue,
                                                   TZrBool *outSupported,
                                                   TZrChar *errorBuffer,
                                                   TZrSize errorBufferSize);

#endif // ZR_VM_DEBUG_EVALUATION_EFFECT_INTERNAL_H
