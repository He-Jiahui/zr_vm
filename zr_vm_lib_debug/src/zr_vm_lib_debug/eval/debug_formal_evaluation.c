#include "eval/debug_evaluation_effect_internal.h"

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/type_inference.h"

#include <string.h>

TZrBool zr_debug_formal_has_paused_array_index_facts(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const SZrSemanticContext *semanticContext,
        const SZrAstNode *expression,
        const SZrSemanticExpressionFact *expressionFact);

static TZrBool zr_debug_formal_is_reference_free_value_expression(const SZrAstNode *expression) {
    if (expression == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (expression->type) {
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_NULL_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_STRING_LITERAL:
            return ZR_TRUE;
        case ZR_AST_ARRAY_LITERAL:
            if (expression->data.arrayLiteral.elements != ZR_NULL) {
                for (TZrSize index = 0u; index < expression->data.arrayLiteral.elements->count; ++index) {
                    if (!zr_debug_formal_is_reference_free_value_expression(
                                expression->data.arrayLiteral.elements->nodes[index])) return ZR_FALSE;
                }
            }
            return ZR_TRUE;
        case ZR_AST_OBJECT_LITERAL:
            if (expression->data.objectLiteral.properties != ZR_NULL) {
                for (TZrSize index = 0u; index < expression->data.objectLiteral.properties->count; ++index) {
                    const SZrAstNode *property = expression->data.objectLiteral.properties->nodes[index];
                    if (property == ZR_NULL || property->type != ZR_AST_KEY_VALUE_PAIR ||
                        (property->data.keyValuePair.keyIsComputed &&
                         !zr_debug_formal_is_reference_free_value_expression(property->data.keyValuePair.key)) ||
                        !zr_debug_formal_is_reference_free_value_expression(property->data.keyValuePair.value))
                        return ZR_FALSE;
                }
            }
            return ZR_TRUE;
        case ZR_AST_UNARY_EXPRESSION:
            return zr_debug_formal_is_reference_free_value_expression(
                    expression->data.unaryExpression.argument);
        case ZR_AST_BINARY_EXPRESSION:
            return (TZrBool)(
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.binaryExpression.left) &&
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.binaryExpression.right));
        case ZR_AST_LOGICAL_EXPRESSION:
            return (TZrBool)(
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.logicalExpression.left) &&
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.logicalExpression.right));
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return (TZrBool)(
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.conditionalExpression.test) &&
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.conditionalExpression.consequent) &&
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.conditionalExpression.alternate));
        case ZR_AST_PRIMARY_EXPRESSION:
            return (TZrBool)(
                    (expression->data.primaryExpression.members == ZR_NULL ||
                     expression->data.primaryExpression.members->count == 0u) &&
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.primaryExpression.property));
        default:
            return ZR_FALSE;
    }
}

static TZrBool zr_debug_formal_allows_legacy_live_scope_compatibility(
        const SZrSemanticContext *semanticContext,
        const SZrAstNode *expression) {
    TZrBool hasCompatibleReference = ZR_FALSE;
    TZrSize index;

    if (semanticContext == ZR_NULL || !semanticContext->referenceFacts.isValid) {
        return ZR_FALSE;
    }
    if (semanticContext->referenceFacts.length == 0u) {
        return expression != ZR_NULL ? ZR_TRUE : ZR_FALSE;
    }
    /* 仅允许可映射到活动作用域的读取或成员引用落回兼容路径；其它未解析引用必须拒绝。 */
    for (index = 0u; index < semanticContext->referenceFacts.length; ++index) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&semanticContext->referenceFacts,
                        index);

        if (fact == ZR_NULL) {
            continue;
        }
        if (fact->isResolved) {
            if (fact->kind == ZR_SEMANTIC_REFERENCE_READ &&
                fact->symbolId != ZR_SEMANTIC_ID_INVALID) {
                hasCompatibleReference = ZR_TRUE;
            } else if (fact->kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS) {
                hasCompatibleReference = ZR_TRUE;
            }
            continue;
        }
        if (fact->kind != ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS) {
            return ZR_FALSE;
        }
        hasCompatibleReference = ZR_TRUE;
    }
    return hasCompatibleReference;
}

static const TZrChar *zr_debug_formal_diagnostic_text(const SZrString *text) {
    return text != ZR_NULL ? ZrCore_String_GetNativeString((SZrString *)text) : "";
}

static void zr_debug_formal_capture_parser_diagnostic(
        TZrPtr userData,
        const SZrStructuredDiagnostic *diagnostic,
        EZrToken token) {
    SZrDebugFormalEvaluationContext *context = (SZrDebugFormalEvaluationContext *)userData;

    (void)token;
    if (context == ZR_NULL || context->parserState.state == ZR_NULL || diagnostic == ZR_NULL) {
        return;
    }
    if (context->hasParserDiagnostic) {
        ZrParser_StructuredDiagnostic_Free(context->parserState.state, &context->parserDiagnostic);
        context->hasParserDiagnostic = ZR_FALSE;
    }
    if (ZrParser_StructuredDiagnostic_Copy(
                context->parserState.state,
                &context->parserDiagnostic,
                diagnostic)) {
        context->hasParserDiagnostic = ZR_TRUE;
    }
}

static void zr_debug_formal_copy_diagnostic_failure(
        const SZrStructuredDiagnostic *diagnostic,
        EZrDebugEvaluateFailureKind kind,
        TZrUInt64 stateId,
        ZrDebugEvaluateFailure *outFailure) {
    if (outFailure == ZR_NULL || diagnostic == ZR_NULL) {
        return;
    }
    zr_debug_evaluate_failure_set(outFailure,
                                  kind,
                                  stateId,
                                  zr_debug_formal_diagnostic_text(diagnostic->code),
                                  zr_debug_formal_diagnostic_text(diagnostic->message));
    outFailure->descriptor_id = diagnostic->descriptorId;
    outFailure->range_start_offset = diagnostic->location.start.offset;
    outFailure->range_end_offset = diagnostic->location.end.offset;
    zr_debug_copy_text(outFailure->cause,
                       sizeof(outFailure->cause),
                       zr_debug_formal_diagnostic_text(diagnostic->cause));
    zr_debug_copy_text(outFailure->suggestion,
                       sizeof(outFailure->suggestion),
                       zr_debug_formal_diagnostic_text(diagnostic->suggestion));
}

void zr_debug_formal_free_prepared_expression(SZrDebugFormalEvaluationContext *context) {
    if (context == ZR_NULL) {
        return;
    }
    /* 推断结果和编译器仍依赖 VM 状态，AST 和诊断仍依赖解析器状态。 */
    if (context->inferredTypeInitialized) {
        ZrParser_InferredType_Free(context->compilerState.state, &context->inferredType);
    }
    if (context->compilerStateInitialized) {
        ZrParser_CompilerState_Free(&context->compilerState);
    }
    if (context->expression != ZR_NULL) {
        ZrParser_Ast_Free(context->parserState.state, context->expression);
    }
    if (context->hasParserDiagnostic) {
        ZrParser_StructuredDiagnostic_Free(context->parserState.state, &context->parserDiagnostic);
    }
    if (context->parserStateInitialized) {
        ZrParser_State_Free(&context->parserState);
    }
    memset(context, 0, sizeof(*context));
}

TZrBool zr_debug_formal_prepare_expression_with_failure(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const TZrChar *expression,
        SZrDebugFormalEvaluationContext *outContext,
        ZrDebugEvaluateFailure *outFailure,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize) {
    SZrString *sourceName;

    if (outContext == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outContext, 0, sizeof(*outContext));
    if (agent == ZR_NULL || agent->state == ZR_NULL || expression == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "invalid debug formal evaluation request");
        zr_debug_evaluate_failure_set(outFailure,
                                      ZR_DEBUG_EVALUATE_FAILURE_REQUEST,
                                      agent != ZR_NULL ? agent->stopStateId : 0u,
                                      "debug_evaluation_invalid_request",
                                      "invalid debug formal evaluation request");
        return ZR_FALSE;
    }

    sourceName = ZrCore_String_CreateFromNative(agent->state, "<debug:formal-evaluation>");
    ZrParser_State_Init(&outContext->parserState,
                        agent->state,
                        expression,
                        strlen(expression),
                        sourceName);
    outContext->parserStateInitialized = ZR_TRUE;
    outContext->parserState.suppressErrorOutput = ZR_TRUE;
    ZrParser_StructuredDiagnostic_Init(&outContext->parserDiagnostic);
    outContext->parserState.structuredErrorCallback = zr_debug_formal_capture_parser_diagnostic;
    outContext->parserState.errorUserData = outContext;
    /* 表达式使用完整语言解析器，结构化诊断留给协议失败对象。 */
    outContext->expression = ZrParser_ParseExpressionWithState(&outContext->parserState);
    if (outContext->parserState.hasError || outContext->expression == ZR_NULL) {
        zr_debug_copy_text(errorBuffer,
                           errorBufferSize,
                           outContext->parserState.errorMessage != ZR_NULL
                                   ? outContext->parserState.errorMessage
                                   : "failed to parse debug evaluation expression");
        if (outContext->hasParserDiagnostic) {
            zr_debug_formal_copy_diagnostic_failure(&outContext->parserDiagnostic,
                                                    ZR_DEBUG_EVALUATE_FAILURE_PARSER,
                                                    agent->stopStateId,
                                                    outFailure);
        } else {
            zr_debug_evaluate_failure_set(outFailure,
                                          ZR_DEBUG_EVALUATE_FAILURE_PARSER,
                                          agent->stopStateId,
                                          "debug_evaluation_parser_failure",
                                          "failed to parse debug evaluation expression");
        }
        zr_debug_formal_free_prepared_expression(outContext);
        return ZR_FALSE;
    }

    ZrParser_CompilerState_Init(&outContext->compilerState, agent->state);
    outContext->compilerStateInitialized = ZR_TRUE;
    outContext->compilerState.currentAst = outContext->expression;
    outContext->compilerState.scriptAst = outContext->expression;
    outContext->compilerState.suppressErrorOutput = ZR_TRUE;
    /* Literal expressions do not depend on paused-frame metadata. Continue
     * inference for those; allocation still requires the caller's capability. */
    if (!zr_debug_semantic_register_bindings(agent, frameId, &outContext->compilerState) &&
        !zr_debug_formal_is_reference_free_value_expression(outContext->expression)) {
        return ZR_TRUE;
    }

    ZrParser_InferredType_Init(agent->state, &outContext->inferredType, ZR_VALUE_TYPE_OBJECT);
    outContext->inferredTypeInitialized = ZR_TRUE;
    if (!ZrParser_ExpressionType_Infer(
                &outContext->compilerState, outContext->expression, &outContext->inferredType) ||
        outContext->compilerState.hasError) {
        return ZR_TRUE;
    }

    outContext->expressionFact = ZrParser_SemanticFacts_FindExpressionByNode(
            outContext->compilerState.semanticContext,
            outContext->expression);
    outContext->hasCanonicalFacts = zr_debug_evaluation_effect_has_canonical_facts(
            outContext->compilerState.semanticContext,
            outContext->expression,
            outContext->expressionFact);
    if (!outContext->hasCanonicalFacts) {
        outContext->hasCanonicalFacts = zr_debug_formal_has_paused_array_index_facts(
                agent,
                frameId,
                outContext->compilerState.semanticContext,
                outContext->expression,
                outContext->expressionFact);
    }
    return ZR_TRUE;
}

TZrBool zr_debug_formal_prepare_expression(ZrDebugAgent *agent,
                                           TZrUInt32 frameId,
                                           const TZrChar *expression,
                                           SZrDebugFormalEvaluationContext *outContext,
                                           TZrChar *errorBuffer,
                                           TZrSize errorBufferSize) {
    return zr_debug_formal_prepare_expression_with_failure(agent,
                                                           frameId,
                                                           expression,
                                                           outContext,
                                                           ZR_NULL,
                                                           errorBuffer,
                                                           errorBufferSize);
}

TZrBool zr_debug_formal_evaluate_expression(ZrDebugAgent *agent,
                                            TZrUInt32 frameId,
                                            const TZrChar *expression,
                                            TZrUInt32 allowedEffectFlags,
                                            SZrTypeValue *outValue,
                                            TZrUInt32 *outCanonicalTypeId,
                                            ZrDebugEvaluateFailure *outFailure,
                                            TZrChar *errorBuffer,
                                            TZrSize errorBufferSize,
                                            TZrBool *outHandled,
                                            TZrBool *outParserFailure) {
    SZrDebugFormalEvaluationContext context;
    TZrBool supported = ZR_FALSE;
    TZrBool ok = ZR_TRUE;
    TZrUInt32 effectFlags = ZR_DEBUG_EVALUATION_EFFECT_NONE;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticTypeQuery typeQuery;

    if (outHandled != ZR_NULL) {
        *outHandled = ZR_FALSE;
    }
    if (outParserFailure != ZR_NULL) {
        *outParserFailure = ZR_FALSE;
    }
    if (outCanonicalTypeId != ZR_NULL) {
        *outCanonicalTypeId = ZR_DEBUG_CANONICAL_TYPE_ID_INVALID;
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || expression == ZR_NULL ||
        outValue == ZR_NULL || outHandled == ZR_NULL) {
        return ZR_TRUE;
    }

    memset(&context, 0, sizeof(context));
    if (!zr_debug_formal_prepare_expression_with_failure(agent,
                                                         frameId,
                                                         expression,
                                                         &context,
                                                         outFailure,
                                                         errorBuffer,
                                                         errorBufferSize)) {
        /* The caller may select the legacy syntax-compatibility route explicitly. */
        if (outParserFailure != ZR_NULL) {
            *outParserFailure = ZR_TRUE;
        }
        return ZR_FALSE;
    }

    if (context.compilerState.hasError) {
        if (context.compilerState.hasStructuredError) {
            zr_debug_formal_copy_diagnostic_failure(&context.compilerState.structuredError,
                                                    ZR_DEBUG_EVALUATE_FAILURE_SEMANTIC,
                                                    agent->stopStateId,
                                                    outFailure);
        } else {
            zr_debug_evaluate_failure_set(outFailure,
                                          ZR_DEBUG_EVALUATE_FAILURE_SEMANTIC,
                                          agent->stopStateId,
                                          "debug_evaluation_semantic_failure",
                                          "canonical semantic analysis rejected debug evaluation");
        }
        ok = ZR_FALSE;
        goto cleanup;
    }

    /* 先拒绝结构性写入，再要求引用有足够语义事实，最后比对调用方授予的副作用能力。 */
    zr_debug_evaluation_effect_classify_structure(context.expression, &effectFlags);
    if (context.hasCanonicalFacts) {
        zr_debug_evaluation_effect_classify_resolved_properties(
                context.compilerState.semanticContext,
                context.expression,
                &effectFlags);
    }
    if ((effectFlags & ZR_DEBUG_EVALUATION_EFFECT_MUTATION) != 0u) {
        zr_debug_copy_text(
                errorBuffer,
                errorBufferSize,
                "Assignment is not allowed in safe debug evaluate. Cause: debug evaluate and conditional "
                "breakpoints are read-only and must not mutate program state. Suggestion: inspect an "
                "expression without '=' or change the program state from source code instead.");
        zr_debug_evaluate_failure_set(outFailure,
                                      ZR_DEBUG_EVALUATE_FAILURE_CAPABILITY,
                                      agent->stopStateId,
                                      "debug_evaluation_capability_denied",
                                      "debug evaluation requires a read-only expression");
        ok = ZR_FALSE;
        goto cleanup;
    }
    if (!context.hasCanonicalFacts &&
        !zr_debug_formal_is_reference_free_value_expression(context.expression)) {
        if (agent->entryFunction != ZR_NULL &&
            effectFlags == ZR_DEBUG_EVALUATION_EFFECT_NONE &&
            zr_debug_formal_allows_legacy_live_scope_compatibility(
                    context.compilerState.semanticContext,
                    context.expression)) {
            /* The caller owns compatibility for exact live-scope metadata gaps. */
            zr_debug_copy_text(errorBuffer,
                               errorBufferSize,
                               "canonical semantic facts are unavailable for debug evaluation");
            goto cleanup;
        }
        zr_debug_copy_text(errorBuffer,
                           errorBufferSize,
                           "canonical semantic facts are unavailable for debug evaluation");
        zr_debug_evaluate_failure_set(outFailure,
                                      ZR_DEBUG_EVALUATE_FAILURE_CANONICAL_FACTS,
                                      agent->stopStateId,
                                      "debug_evaluation_canonical_facts_unavailable",
                                      "canonical semantic facts are unavailable for debug evaluation");
        ok = ZR_FALSE;
        goto cleanup;
    }
    if (outCanonicalTypeId != ZR_NULL) {
        ZrParser_SemanticQueryScope_Node(&scope, context.expression);
        memset(&typeQuery, 0, sizeof(typeQuery));
        if (ZrParser_SemanticQuery_CanonicalTypeAt(context.compilerState.semanticContext,
                                                    context.expression->location,
                                                    &scope,
                                                    &typeQuery)) {
            *outCanonicalTypeId = typeQuery.typeId;
        }
    }
    if ((effectFlags & ~allowedEffectFlags) != 0u) {
        zr_debug_copy_text(errorBuffer,
                           errorBufferSize,
                           "debug evaluation requires an explicit capability for canonical effects");
        zr_debug_evaluate_failure_set(outFailure,
                                      ZR_DEBUG_EVALUATE_FAILURE_CAPABILITY,
                                      agent->stopStateId,
                                      "debug_evaluation_capability_denied",
                                      "debug evaluation requires an explicit capability for canonical effects");
        ok = ZR_FALSE;
        goto cleanup;
    }

    ok = zr_debug_formal_evaluate_node(agent,
                                       frameId,
                                       context.compilerState.semanticContext,
                                       context.expression,
                                       outValue,
                                       &supported,
                                       errorBuffer,
                                       errorBufferSize);
    *outHandled = supported;
    if (!ok && outFailure != ZR_NULL && outFailure->kind == ZR_DEBUG_EVALUATE_FAILURE_NONE) {
        zr_debug_evaluate_failure_set(outFailure,
                                      ZR_DEBUG_EVALUATE_FAILURE_FORMAL_EXECUTION,
                                      agent->stopStateId,
                                      "debug_evaluation_formal_execution_failed",
                                      "formal debug evaluation failed");
    }

cleanup:
    zr_debug_formal_free_prepared_expression(&context);
    return ok;
}

/**
 * @fn TZrBool zr_debug_formal_is_reference_free_value_expression(const SZrAstNode *expression)
 * ## Summary
 * 在准备与执行门禁之间识别不需要暂停帧引用的值表达式形状，使字面量及其递归组合可在绑定注册不完整时继续推断。它只检查 AST 结构，不执行表达式，也不证明正式执行器覆盖该节点。
 *
 * ## Parameters
 * @param[in] expression 借用待检查 AST；可为 NULL，节点及子列表须在递归期间保持有效。
 *
 * ## Returns
 * @return NULL 或出现标识符、成员访问及未列出的节点时返回 FALSE；全部节点属于受限值形状时返回 TRUE。空数组及无属性对象可为 TRUE。
 *
 * ## Constraints and lifecycle
 * 只读 AST，不分配、不持有节点。数组、对象以及计算属性和值会递归检查；primary 必须没有成员。TRUE 不代表无分配、已有规范事实或可执行；相应判断仍由准备、能力门禁和正式执行器完成。
 *
 * ## References
 * - zr_debug_formal_prepare_expression_with_failure — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:262（实际调用或注册）；绑定失败时按值表达式例外继续推断。
 * - zr_debug_formal_evaluate_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:395（实际调用或注册）；规范事实不足时区分无引用值和暂停帧引用。
 */

/**
 * @fn TZrBool zr_debug_formal_allows_legacy_live_scope_compatibility( const SZrSemanticContext *semanticContext, const SZrAstNode *expression)
 * ## Summary
 * 在正式语义事实不完整时识别可交回活动作用域兼容求值的窄缺口，为上层决定保留旧兼容路径提供依据；本函数本身既不启动兼容求值，也不授予副作用能力。
 *
 * ## Parameters
 * @param[in] semanticContext 借用已建立且 referenceFacts 有效的语义上下文；NULL 或无有效事实集合时不能允许兼容。
 * @param[in] expression 借用对应 AST；事实集合为空时仅用它是否非 NULL 判断。
 *
 * ## Returns
 * @return 有效且为空的事实集合配合非 NULL 表达式返回 TRUE；有事实时，规范读取、成员事实及未解析成员缺口可建立兼容候选，其他未解析引用返回 FALSE。没有兼容候选则返回 FALSE。
 *
 * ## Constraints and lifecycle
 * 已解析且 symbolId 有效的 READ 以及 MEMBER 事实可形成候选；未解析 MEMBER 也可形成候选，不能写成只允许已解析引用。仅本返回值不足以兼容：调用者还要求 entryFunction 非空且结构 effectFlags 为 NONE，并由外层显式允许 legacy compatibility。上下文只借用，不能在事实销毁后调用。
 *
 * ## References
 * - zr_debug_formal_evaluate_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:398（实际调用或注册）；结合 entryFunction 与 effectNONE 产生未处理结果。
 * - zr_debug_evaluate_expression_with_capabilities — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:932（配套实现定义）；最终决定是否启动旧语法兼容求值。
 */

/**
 * @fn const TZrChar *zr_debug_formal_diagnostic_text(const SZrString *text)
 * ## Summary
 * 为调试失败转换提供可读取的诊断文本入口，将缺失的字符串对象映射为空文本，避免调用处逐个处理可选 cause 或 suggestion。
 *
 * ## Parameters
 * @param[in] text 借用诊断持有的字符串对象；NULL 表示该文本缺失。
 *
 * ## Returns
 * @return text 为 NULL 时返回静态空文本，否则返回 ZrCore_String_GetNativeString 的原生文本指针；不会新建或转移字符串。
 *
 * ## Constraints and lifecycle
 * 返回值仅借用，须在原字符串及其 VM 存储有效期间使用。此包装只处理对象指针为 NULL 的情况，不额外验证非空对象内部载荷；上层马上把文本复制到有限失败字段。
 *
 * ## References
 * - zr_debug_formal_copy_diagnostic_failure — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:159（实际调用或注册）；复制 code、message、cause 与 suggestion。
 */

/**
 * @fn void zr_debug_formal_capture_parser_diagnostic( TZrPtr userData, const SZrStructuredDiagnostic *diagnostic, EZrToken token)
 * ## Summary
 * 作为 ParserState 的结构化错误回调，把最新诊断复制进单次正式求值上下文，使解析器报告不必在离开解析阶段后继续借用原诊断容器。
 *
 * ## Parameters
 * @param[in,out] userData 注册时传入的 SZrDebugFormalEvaluationContext；可为 NULL。上下文和 parserState.state 必须仍有效。
 * @param[in] diagnostic 回调期间借用的诊断；NULL 时不动作。
 * @param[in] token 解析器派发的 token；本回调不读取该参数。
 *
 * ## Completion and failure
 * 完成与失败通过上述输出或资源状态体现；本函数没有返回值。
 *
 * ## Constraints and lifecycle
 * 先释放上下文标记持有的旧副本并清除标志，再调用 StructuredDiagnostic_Copy；仅复制成功才设置 hasParserDiagnostic。普通复制失败不会保留上一条诊断，随后由 prepare 的普通错误文本兜底。复制容器由上下文 Free 释放；回调不保存 source 诊断指针。分配相关异常仍可向上传播，不承诺非局部退出后的完整回滚。
 *
 * ## References
 * - zr_debug_formal_prepare_expression_with_failure — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:228（实际调用或注册）；安装 callback 与 errorUserData。
 * - ZrParser_StructuredDiagnostic_Copy — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:140（配套消费位置）；建立独立副本并在自身普通失败路径清理。
 * - zr_debug_formal_free_prepared_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:172（配套实现定义）；释放已标记持有的副本。
 */

/**
 * @fn void zr_debug_formal_copy_diagnostic_failure( const SZrStructuredDiagnostic *diagnostic, EZrDebugEvaluateFailureKind kind, TZrUInt64 stateId, ZrDebugEvaluateFailure *outFailure)
 * ## Summary
 * 把已捕获的解析或语义诊断转换为调试协议可携带的失败快照，保留类别、停点代号、描述符、源码范围以及可选原因与建议。
 *
 * ## Parameters
 * @param[in] diagnostic 借用诊断；NULL 时不动作，其字符串在复制结束前须有效。
 * @param[in] kind 调用方选择的调试失败类别，区分解析与语义失败。
 * @param[in] stateId 该次失败所属的停点代号，按值复制。
 * @param[out] outFailure 调用方失败结构；可为 NULL，非 NULL 时写入有限文本与范围字段。
 *
 * ## Completion and failure
 * 完成与失败通过上述输出或资源状态体现；本函数没有返回值。
 *
 * ## Constraints and lifecycle
 * 输出采用失败结构自身的定长文本空间，过长文本按配套 copy_text 截断；不会转移诊断字符串或引用其数组。范围、描述符与文本只反映传入诊断，不把失败转换当作再次执行或重新定位。调用方可在转换后销毁原诊断上下文。
 *
 * ## References
 * - zr_debug_formal_prepare_expression_with_failure — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:239（实际调用或注册）；转交解析失败。
 * - zr_debug_formal_evaluate_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:356（实际调用或注册）；转交编译器结构化语义失败。
 * - zr_debug_evaluate_failure_set — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:156（配套消费位置）；初始化类别与主要文本。
 */

/**
 * @fn void zr_debug_formal_free_prepared_expression(SZrDebugFormalEvaluationContext *context)
 * ## Summary
 * 按取得资源的状态标志释放一次正式表达式准备结果，供普通解析失败、Effect 分类结束和正式执行收尾共用。
 *
 * ## Parameters
 * @param[in,out] context 可为 NULL；非 NULL 时必须是清零或由 prepare 建立且初始化标志一致的上下文，不得传入任意未初始化内存。
 *
 * ## Completion and failure
 * 完成与失败通过上述输出或资源状态体现；本函数没有返回值。
 *
 * ## Constraints and lifecycle
 * 依次释放 inferredType、compilerState、expression AST、已复制诊断和 parserState，最后把整个结构清零；顺序保留各资源释放所需的 VM 与解析器状态。已清零上下文可再次释放。expressionFact 和其他语义指针均随配套状态销毁而失效。此函数不为别处的非局部退出自动注册 finally，也不释放调用方 agent 或 VM。
 *
 * ## References
 * - zr_debug_formal_prepare_expression_with_failure — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:250（实际调用或注册）；普通解析失败时清理部分初始化结果。
 * - ZrDebug_ClassifyEvaluationEffect — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:546（实际调用或注册）；分类结束后清理。
 * - zr_debug_formal_evaluate_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:459（实际调用或注册）；普通执行收尾统一清理。
 */

/**
 * @fn TZrBool zr_debug_formal_prepare_expression_with_failure( ZrDebugAgent *agent, TZrUInt32 frameId, const TZrChar *expression, SZrDebugFormalEvaluationContext *outContext, ZrDebugEvaluateFailure *outFailure, TZrChar *errorBuffer, TZrSize errorBufferSize)
 * ## Summary
 * 把 NUL 终止表达式解析成 AST，并在当前调试帧的语义绑定下尝试类型推断，交付供副作用分类与正式执行共同消费的部分或完整上下文。
 *
 * ## Parameters
 * @param[in] agent 借用调试代理及有效 VM；NULL 或 state 为 NULL 时普通失败。
 * @param[in] frameId 调试帧标识，由绑定注册与暂停数组事实查询消费；本函数不单独强制公开 paused API 的全部门禁。
 * @param[in] expression 借用 NUL 终止表达式；NULL 时失败，解析期间须保持有效。
 * @param[out] outContext 非 NULL 的独立输出槽；入口清零但不释放旧资源，不能复用仍持有资源的上下文。TRUE 后由调用者 Free。
 * @param[out] outFailure 可选失败详情；只在相应失败分支改写，成功不保证清除既有内容。
 * @param[out] errorBuffer 可选可写诊断缓冲区，与 errorBufferSize 配套；函数不承诺成功时主动清空旧文本。
 * @param[in] errorBufferSize errorBuffer 的可写字节容量，0 表示不输出文本。
 *
 * ## Returns
 * @return 参数或普通解析失败返回 FALSE；成功解析后即使绑定注册或类型推断未完成，也可能返回 TRUE。TRUE 不保证 hasCanonicalFacts，调用方必须检查 compilerState、初始化标志及事实状态并最终 Free。
 *
 * ## Constraints and lifecycle
 * sourceName 是 '<debug:formal-evaluation>' 内存源码标签，不是磁盘输出。解析器安装结构化诊断回调；编译器借用该 AST。绑定注册失败仅在表达式不是 reference-free 值形状时提前返回 TRUE；无引用值仍继续推断，数组等分配仍须另行授权。推断失败或编译器已有错误也保留上下文交上层分类。按标志普通释放；VM 分配可能非局部退出，不承诺所有异常路径都经过本地清理。
 *
 * ## References
 * - zr_debug_formal_prepare_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:298（实际调用或注册）；省略详细失败对象的包装。
 * - zr_debug_formal_evaluate_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:340（实际调用或注册）；读取编译器错误与规范事实再执行。
 * - zr_debug_semantic_register_bindings — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:261（配套消费位置）；发布暂停帧规范身份。
 * - zr_debug_formal_free_prepared_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:250（配套消费位置）；释放所有成功交付或部分初始化资源。
 */

/**
 * @fn TZrBool zr_debug_formal_prepare_expression(ZrDebugAgent *agent, TZrUInt32 frameId, const TZrChar *expression, SZrDebugFormalEvaluationContext *outContext, TZrChar *errorBuffer, TZrSize errorBufferSize)
 * ## Summary
 * 为 Effect 分类及内部调试测试提供不要求详细 Failure 对象的准备入口，复用正式表达式解析、绑定及推断流程。
 *
 * ## Parameters
 * @param[in] agent 借用代理和有效 VM，约束同 prepare_expression_with_failure。
 * @param[in] frameId 交给规范绑定与暂停帧事实查询的帧标识。
 * @param[in] expression 解析期间借用的 NUL 终止表达式。
 * @param[out] outContext 独立输出槽，不能仍持有旧准备资源；TRUE 后必须调用配套 Free。
 * @param[out] errorBuffer 与容量配套的可选错误文本输出，成功不保证清空原内容。
 * @param[in] errorBufferSize 错误输出的可写字节容量。
 *
 * ## Returns
 * @return 原样返回 prepare_expression_with_failure 的结果；TRUE 可以只交付部分上下文，不能当作 hasCanonicalFacts 或可安全执行。
 *
 * ## Constraints and lifecycle
 * 本包装把 outFailure 传 NULL，不改变普通失败、分配异常或资源归属。调用者只在上下文资源有效期间读取 expression、expressionFact 与 semanticContext，普通结束时必须释放。内部测试也直接使用本入口，并非只有 Effect 分类器消费。
 *
 * ## References
 * - ZrDebug_ClassifyEvaluationEffect — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:526（实际调用或注册）；消费准备结果后分类并释放。
 * - zr_debug_formal_prepare_expression_with_failure — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:298（配套消费位置）；实现全部准备状态转换。
 * - zr_debug_formal_free_prepared_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:172（配套实现定义）；配套清理。
 */

/**
 * @fn TZrBool zr_debug_formal_evaluate_expression(ZrDebugAgent *agent, TZrUInt32 frameId, const TZrChar *expression, TZrUInt32 allowedEffectFlags, SZrTypeValue *outValue, TZrUInt32 *outCanonicalTypeId, ZrDebugEvaluateFailure *outFailure, TZrChar *errorBuffer, TZrSize errorBufferSize, TZrBool *outHandled, TZrBool *outParserFailure)
 * ## Summary
 * 在兼容求值器之前尝试正式语义路径：准备上下文，拒绝结构性写入，核对规范事实与许可的副作用，再执行受支持 AST 子集，明确向外层交付失败、未处理或已处理三种结果。
 *
 * ## Parameters
 * @param[in] agent 借用代理及 VM；调用方负责同步暂停帧读取，本函数也由断点 hook 内部链消费。
 * @param[in] frameId 正式绑定、规范身份读取和执行使用的同一帧标识。
 * @param[in] expression 借用 NUL 终止表达式；准备及执行结束前须有效。
 * @param[in] allowedEffectFlags 调用方已授予的效果位集合；仍不能允许结构 MUTATION，未知或未实现执行节点也不会因此变为支持。
 * @param[out] outValue 已处理成功时交付值；为 NULL 则不处理。对象是 VM 管理的借用/值记录，不由本函数发布变量句柄。
 * @param[out] outCanonicalTypeId 可选类型 ID 输出，入口置 INVALID；可能在后续能力失败前已写入，须结合最终状态消费。
 * @param[in,out] outFailure 可选失败结构；调用方应先清空，某些分支仅在 kind 为 NONE 时写通用失败。
 * @param[out] errorBuffer 与容量配套的可选文本输出，不保证所有未处理路径都留下错误。
 * @param[in] errorBufferSize 错误输出的可写字节容量。
 * @param[out] outHandled 必须非 NULL；入口置 FALSE；节点普通返回后复制其支持位，硬执行错误时也可为 TRUE，须结合本函数返回值判断成功。
 * @param[out] outParserFailure 可选输出，入口置 FALSE；准备返回 FALSE 时置 TRUE，供外层兼容策略判别。
 *
 * ## Returns
 * @return FALSE 表示准备、语义、能力或执行硬错误；TRUE 且 outHandled 为 FALSE 表示正式路径未处理，外层仍须拒绝或显式选择兼容；TRUE 且 outHandled 为 TRUE 才是正式结果。参数不足也可 TRUE 未处理。
 *
 * ## Constraints and lifecycle
 * 先检查整棵 AST 的结构效果，MUTATION 一律拒绝；非 reference-free 引用通常必须有规范事实。无规范事实的窄活动作用域缺口可 TRUE 未处理，但不在此处启动 legacy。效果分类覆盖未实际执行的分支，不能用短路绕过授权。普通出口统一释放上下文；返回值不授予跨 resume 的对象寿命、线程安全或异常回滚保证。
 *
 * ## References
 * - zr_debug_evaluate_expression_with_capabilities — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:969（实际调用或注册）；根据 outHandled 与 outParserFailure 决定后续路径。
 * - zr_debug_formal_evaluate_node — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:441（配套消费位置）；在授权后执行节点。
 * - zr_debug_formal_free_prepared_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:459（配套消费位置）；普通收尾释放上下文。
 */
