typedef struct SZrDebugCanonicalBindingCapture {
    ZrDebugAgent *agent;
    TZrBool sawPausedBinding;
    TZrBool sawReferenceFact;
    TZrBool sawEvaluationPolicy;
    TZrBool sawFormalEvaluation;
    TZrBool policyHasCanonicalFacts;
    TZrBool clearPlaceBeforePolicy;
    TZrBool rejectTypeDriftDuringFormalRead;
    TZrBool typeDriftRejected;
    const TZrChar *expectedBindingName;
    const TZrChar *formalExpression;
    TZrUInt32 expectedSymbolId;
    TZrUInt32 expectedTypeId;
    TZrUInt32 expectedPlaceId;
    TZrUInt32 expectedStartLine;
    TZrUInt32 expectedStartColumn;
    TZrUInt32 actualSymbolId;
    TZrUInt32 actualTypeId;
    TZrUInt32 actualStartLine;
    TZrUInt32 actualStartColumn;
    EZrSemanticReferenceOriginKind actualOriginKind;
    EZrSemanticRuntimeRootKind actualRuntimeRootKind;
    TZrUInt64 actualOriginToken;
    TZrChar formalEvaluationType[ZR_DEBUG_NAME_CAPACITY];
    TZrChar formalEvaluationValue[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar formalEvaluationError[ZR_DEBUG_TEXT_CAPACITY];
} SZrDebugCanonicalBindingCapture;

static SZrDebugCanonicalBindingCapture g_debugCanonicalBindingCapture;

typedef struct SZrDebugClosureBindingCapture {
    ZrDebugAgent *agent;
    TZrBool sawClosureActivation;
    TZrBool sawClosureReferenceFact;
    TZrBool hasCanonicalIdentity;
    TZrBool sawEvaluationPolicy;
    TZrBool policyHasCanonicalFacts;
    TZrUInt32 captureIndex;
    TZrUInt32 symbolId;
    TZrUInt32 typeId;
    TZrUInt64 token;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];
} SZrDebugClosureBindingCapture;

static SZrDebugClosureBindingCapture g_debugClosureBindingCapture;

typedef struct SZrDebugRuntimeRootBindingCapture {
    ZrDebugAgent *agent;
    TZrBool sawPausedFrame;
    TZrBool sawReferenceFact;
    TZrBool hasCanonicalIdentity;
    TZrBool hasNoSourceIdentity;
    TZrBool definitionRangeDriftRejected;
    TZrBool tokenMatchesCoreBinding;
    TZrBool tokenDriftRejected;
    TZrBool sawFormalEvaluation;
    EZrSemanticReferenceOriginKind originKind;
    EZrSemanticRuntimeRootKind runtimeRootKind;
    TZrUInt64 originToken;
    TZrChar formalEvaluationType[ZR_DEBUG_NAME_CAPACITY];
    TZrChar formalEvaluationValue[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar formalEvaluationError[ZR_DEBUG_TEXT_CAPACITY];
} SZrDebugRuntimeRootBindingCapture;

static SZrDebugRuntimeRootBindingCapture g_debugRuntimeRootBindingCapture;

static const TZrChar *debug_canonical_binding_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return "";
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
                   ? ZrCore_String_GetNativeStringShort(value)
                   : ZrCore_String_GetNativeString(value);
}

static const SZrFunctionTypedLocalBinding *debug_canonical_binding_find_typed_local(
        const SZrFunction *function,
        TZrUInt32 stackSlot) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0u; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        if (binding->stackSlot == stackSlot) {
            return binding;
        }
    }

    return ZR_NULL;
}

static void debug_canonical_binding_hook(SZrState *state, SZrDebugInfo *debugInfo) {
    SZrDebugEvaluationContext context;
    SZrDebugFrameBinding frameBinding;
    const SZrFunctionTypedLocalBinding *typedBinding = ZR_NULL;
    SZrParserState parserState;
    SZrCompilerState compilerState;
    SZrInferredType inferredType;
    SZrString *sourceName;
    SZrAstNode *expression = ZR_NULL;
    SZrAstNode *referenceNode;
    const SZrSemanticReferenceFact *reference;
    TZrUInt32 index;

    ZR_UNUSED_PARAMETER(debugInfo);
    memset(&frameBinding, 0, sizeof(frameBinding));
    if (state == ZR_NULL || g_debugCanonicalBindingCapture.agent == ZR_NULL ||
        g_debugCanonicalBindingCapture.sawReferenceFact) {
        return;
    }

    memset(&context, 0, sizeof(context));
    if (ZrCore_Debug_GetEvaluationContext(state, 0u, &context) != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return;
    }

    if (g_debugCanonicalBindingCapture.expectedBindingName != ZR_NULL &&
        strcmp(g_debugCanonicalBindingCapture.expectedBindingName, "this") == 0) {
        SZrTypeValue receiverValue;

        ZrCore_Value_ResetAsNull(&receiverValue);
        if (ZrCore_Debug_EvaluationContext_GetReceiver(state,
                                                       &context,
                                                       &frameBinding,
                                                       &receiverValue) ==
            ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
            typedBinding = debug_canonical_binding_find_typed_local(
                    context.activation.function, frameBinding.stackSlot);
        }
    } else {
        for (index = 0u; index < context.activeBindingCount; index++) {
            memset(&frameBinding, 0, sizeof(frameBinding));
            if (ZrCore_Debug_EvaluationContext_GetBinding(state, &context, index, &frameBinding) !=
                ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
                return;
            }
            typedBinding = debug_canonical_binding_find_typed_local(
                    context.activation.function, frameBinding.stackSlot);
            if (typedBinding != ZR_NULL &&
                strcmp(debug_canonical_binding_string_text(typedBinding->name),
                       g_debugCanonicalBindingCapture.expectedBindingName != ZR_NULL
                               ? g_debugCanonicalBindingCapture.expectedBindingName
                               : "paused") == 0) {
                break;
            }
            typedBinding = ZR_NULL;
        }
    }

    if (typedBinding != ZR_NULL &&
        strcmp(debug_canonical_binding_string_text(typedBinding->name),
               g_debugCanonicalBindingCapture.expectedBindingName != ZR_NULL
                       ? g_debugCanonicalBindingCapture.expectedBindingName
                       : "paused") != 0) {
        typedBinding = ZR_NULL;
    }

    if (typedBinding == ZR_NULL) {
        return;
    }

    g_debugCanonicalBindingCapture.sawPausedBinding = ZR_TRUE;
    g_debugCanonicalBindingCapture.expectedSymbolId = frameBinding.symbolId;
    g_debugCanonicalBindingCapture.expectedTypeId = frameBinding.typeId;
    g_debugCanonicalBindingCapture.expectedPlaceId = frameBinding.placeId;
    g_debugCanonicalBindingCapture.expectedStartLine = frameBinding.declarationStartLine;
    g_debugCanonicalBindingCapture.expectedStartColumn = frameBinding.declarationStartColumn;
    sourceName = ZrCore_String_CreateFromNative(state, "<debug:e2b1-binding>");
    if (sourceName == ZR_NULL) {
        return;
    }
    ZrParser_State_Init(
            &parserState,
            state,
            g_debugCanonicalBindingCapture.formalExpression != ZR_NULL
                    ? g_debugCanonicalBindingCapture.formalExpression
                    : "paused",
            strlen(g_debugCanonicalBindingCapture.formalExpression != ZR_NULL
                           ? g_debugCanonicalBindingCapture.formalExpression
                           : "paused"),
            sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    expression = ZrParser_ParseExpressionWithState(&parserState);
    if (parserState.hasError || expression == ZR_NULL) {
        ZrParser_State_Free(&parserState);
        return;
    }
    referenceNode = expression;
    if (expression->type == ZR_AST_PRIMARY_EXPRESSION) {
        referenceNode = expression->data.primaryExpression.property;
    }

    memset(&compilerState, 0, sizeof(compilerState));
    ZrParser_CompilerState_Init(&compilerState, state);
    compilerState.currentAst = expression;
    compilerState.scriptAst = expression;
    compilerState.suppressErrorOutput = ZR_TRUE;
    zr_debug_semantic_register_bindings(g_debugCanonicalBindingCapture.agent, 1u, &compilerState);
    ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_ExpressionType_Infer(&compilerState, expression, &inferredType)) {
        reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                compilerState.semanticContext,
                referenceNode,
                ZR_SEMANTIC_REFERENCE_READ);
        if (reference != ZR_NULL) {
            g_debugCanonicalBindingCapture.sawReferenceFact = ZR_TRUE;
            g_debugCanonicalBindingCapture.actualSymbolId = reference->symbolId;
            g_debugCanonicalBindingCapture.actualTypeId = reference->typeId;
            g_debugCanonicalBindingCapture.actualStartLine = (TZrUInt32)reference->declarationRange.start.line;
            g_debugCanonicalBindingCapture.actualStartColumn = (TZrUInt32)reference->declarationRange.start.column;
            g_debugCanonicalBindingCapture.actualOriginKind = reference->originKind;
            g_debugCanonicalBindingCapture.actualRuntimeRootKind = reference->runtimeRootKind;
            g_debugCanonicalBindingCapture.actualOriginToken = reference->originToken;
        }
    }
    if (g_debugCanonicalBindingCapture.rejectTypeDriftDuringFormalRead) {
        SZrDebugFormalEvaluationContext formalContext;
        SZrTypeValue formalValue;
        TZrBool formalSupported = ZR_TRUE;
        TZrUInt32 originalTypeId = ((SZrFunctionTypedLocalBinding *)typedBinding)->typeId;
        TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

        memset(&formalContext, 0, sizeof(formalContext));
        ZrCore_Value_ResetAsNull(&formalValue);
        error[0] = '\0';
        if (zr_debug_formal_prepare_expression(
                    g_debugCanonicalBindingCapture.agent,
                    1u,
                    g_debugCanonicalBindingCapture.formalExpression != ZR_NULL
                            ? g_debugCanonicalBindingCapture.formalExpression
                            : "paused",
                    &formalContext,
                    error,
                    sizeof(error))) {
            ((SZrFunctionTypedLocalBinding *)typedBinding)->typeId =
                    originalTypeId == ZR_SEMANTIC_ID_INVALID ? 1u : originalTypeId + 1u;
            if (zr_debug_formal_evaluate_node(
                        g_debugCanonicalBindingCapture.agent,
                        1u,
                        formalContext.compilerState.semanticContext,
                        formalContext.expression,
                        &formalValue,
                        &formalSupported,
                        error,
                        sizeof(error))) {
                g_debugCanonicalBindingCapture.typeDriftRejected = !formalSupported;
            }
            ((SZrFunctionTypedLocalBinding *)typedBinding)->typeId = originalTypeId;
            zr_debug_formal_free_prepared_expression(&formalContext);
        }
    }
    if (g_debugCanonicalBindingCapture.clearPlaceBeforePolicy) {
        ((SZrFunctionTypedLocalBinding *)typedBinding)->placeId = 0u;
    }
    {
        ZrDebugEvaluationEffectPolicy policy;
        TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

        memset(&policy, 0, sizeof(policy));
        error[0] = '\0';
        if (ZrDebug_ClassifyEvaluationEffect(g_debugCanonicalBindingCapture.agent,
                                             1u,
                                             g_debugCanonicalBindingCapture.formalExpression != ZR_NULL
                                                     ? g_debugCanonicalBindingCapture.formalExpression
                                                     : "paused",
                                             &policy,
                                             error,
                                             sizeof(error))) {
            g_debugCanonicalBindingCapture.sawEvaluationPolicy = ZR_TRUE;
            g_debugCanonicalBindingCapture.policyHasCanonicalFacts = policy.hasCanonicalFacts;
        }
    }
    {
        ZrDebugEvaluateResult result;
        TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

        memset(&result, 0, sizeof(result));
        error[0] = '\0';
        if (ZrDebug_EvaluateWithCapabilities(
                    g_debugCanonicalBindingCapture.agent,
                    1u,
                    g_debugCanonicalBindingCapture.formalExpression != ZR_NULL
                            ? g_debugCanonicalBindingCapture.formalExpression
                            : "paused << 1",
                    ZR_DEBUG_EVALUATION_EFFECT_NONE,
                    &result,
                    error,
                    sizeof(error))) {
            g_debugCanonicalBindingCapture.sawFormalEvaluation = ZR_TRUE;
            strncpy(g_debugCanonicalBindingCapture.formalEvaluationType,
                    result.type_name,
                    sizeof(g_debugCanonicalBindingCapture.formalEvaluationType) - 1u);
            g_debugCanonicalBindingCapture.formalEvaluationType[
                    sizeof(g_debugCanonicalBindingCapture.formalEvaluationType) - 1u] = '\0';
            strncpy(g_debugCanonicalBindingCapture.formalEvaluationValue,
                    result.value_text,
                    sizeof(g_debugCanonicalBindingCapture.formalEvaluationValue) - 1u);
            g_debugCanonicalBindingCapture.formalEvaluationValue[
                    sizeof(g_debugCanonicalBindingCapture.formalEvaluationValue) - 1u] = '\0';
        } else {
            strncpy(g_debugCanonicalBindingCapture.formalEvaluationError,
                    error,
                    sizeof(g_debugCanonicalBindingCapture.formalEvaluationError) - 1u);
            g_debugCanonicalBindingCapture.formalEvaluationError[
                    sizeof(g_debugCanonicalBindingCapture.formalEvaluationError) - 1u] = '\0';
        }
    }
    ZrParser_InferredType_Free(state, &inferredType);
    ZrParser_CompilerState_Free(&compilerState);
    ZrParser_Ast_Free(state, expression);
    ZrParser_State_Free(&parserState);
}

static void debug_closure_binding_hook(SZrState *state, SZrDebugInfo *debugInfo) {
    SZrDebugEvaluationContext context;
    SZrDebugClosureCaptureBinding capture;
    ZrDebugEvaluationEffectPolicy policy;
    SZrParserState parserState;
    SZrCompilerState compilerState;
    SZrInferredType inferredType;
    SZrString *sourceName;
    SZrAstNode *expression = ZR_NULL;
    const SZrSemanticReferenceFact *reference;

    ZR_UNUSED_PARAMETER(debugInfo);
    if (state == ZR_NULL || g_debugClosureBindingCapture.agent == ZR_NULL ||
        g_debugClosureBindingCapture.sawClosureActivation) {
        return;
    }

    memset(&context, 0, sizeof(context));
    if (ZrCore_Debug_GetEvaluationContext(state, 0u, &context) != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK ||
        context.activation.function == ZR_NULL || context.activation.function->closureValueLength == 0u) {
        return;
    }

    memset(&capture, 0, sizeof(capture));
    if (ZrCore_Debug_EvaluationContext_GetClosureCapture(
                state, &context, 0u, &capture) != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return;
    }

    g_debugClosureBindingCapture.sawClosureActivation = ZR_TRUE;
    g_debugClosureBindingCapture.captureIndex = capture.captureIndex;
    g_debugClosureBindingCapture.symbolId = capture.symbolId;
    g_debugClosureBindingCapture.typeId = capture.typeId;
    g_debugClosureBindingCapture.token = capture.token;

    sourceName = ZrCore_String_CreateFromNative(state, "<debug:e2b6c-closure-capture>");
    if (sourceName != ZR_NULL) {
        ZrParser_State_Init(&parserState, state, "seed", strlen("seed"), sourceName);
        parserState.suppressErrorOutput = ZR_TRUE;
        expression = ZrParser_ParseExpressionWithState(&parserState);
        if (!parserState.hasError && expression != ZR_NULL) {
            memset(&compilerState, 0, sizeof(compilerState));
            ZrParser_CompilerState_Init(&compilerState, state);
            compilerState.currentAst = expression;
            compilerState.scriptAst = expression;
            compilerState.suppressErrorOutput = ZR_TRUE;
            ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
            if (zr_debug_semantic_register_bindings(
                        g_debugClosureBindingCapture.agent, 1u, &compilerState) &&
                ZrParser_ExpressionType_Infer(&compilerState, expression, &inferredType)) {
                reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                        compilerState.semanticContext, expression, ZR_SEMANTIC_REFERENCE_READ);
                if (reference != ZR_NULL) {
                    g_debugClosureBindingCapture.sawClosureReferenceFact = ZR_TRUE;
                    g_debugClosureBindingCapture.hasCanonicalIdentity = (TZrBool)(
                            reference->isResolved &&
                            reference->originKind == ZR_SEMANTIC_REFERENCE_ORIGIN_CLOSURE_CAPTURE &&
                            reference->runtimeRootKind == ZR_SEMANTIC_RUNTIME_ROOT_NONE &&
                            reference->originIndex == capture.captureIndex &&
                            reference->originToken == capture.token &&
                            reference->symbolId == capture.symbolId &&
                            reference->typeId == capture.typeId &&
                            reference->placeId == ZR_SEMANTIC_ID_INVALID &&
                            reference->declarationRange.source == context.activation.function->sourceCodeList &&
                            reference->declarationRange.start.line == (TZrInt32)capture.declarationStartLine &&
                            reference->declarationRange.start.column == (TZrInt32)capture.declarationStartColumn &&
                            reference->declarationRange.end.line == (TZrInt32)capture.declarationEndLine &&
                            reference->declarationRange.end.column == (TZrInt32)capture.declarationEndColumn);
                }
            }
            ZrParser_InferredType_Free(state, &inferredType);
            ZrParser_CompilerState_Free(&compilerState);
        }
        if (expression != ZR_NULL) {
            ZrParser_Ast_Free(state, expression);
        }
        ZrParser_State_Free(&parserState);
    }

    memset(&policy, 0, sizeof(policy));
    g_debugClosureBindingCapture.error[0] = '\0';
    if (ZrDebug_ClassifyEvaluationEffect(g_debugClosureBindingCapture.agent,
                                         1u,
                                         "seed",
                                         &policy,
                                         g_debugClosureBindingCapture.error,
                                         sizeof(g_debugClosureBindingCapture.error))) {
        g_debugClosureBindingCapture.sawEvaluationPolicy = ZR_TRUE;
        g_debugClosureBindingCapture.policyHasCanonicalFacts = policy.hasCanonicalFacts;
    }
}

static void debug_runtime_root_binding_hook(SZrState *state, SZrDebugInfo *debugInfo) {
    SZrDebugEvaluationContext evaluationContext;
    SZrDebugRuntimeRootBinding runtimeRoot;
    SZrParserState parserState;
    SZrCompilerState compilerState;
    SZrInferredType inferredType;
    SZrString *sourceName;
    SZrAstNode *expression = ZR_NULL;
    SZrAstNode *referenceNode;
    const SZrSemanticReferenceFact *reference;

    ZR_UNUSED_PARAMETER(debugInfo);
    if (state == ZR_NULL || g_debugRuntimeRootBindingCapture.agent == ZR_NULL ||
        g_debugRuntimeRootBindingCapture.sawPausedFrame) {
        return;
    }

    memset(&evaluationContext, 0, sizeof(evaluationContext));
    memset(&runtimeRoot, 0, sizeof(runtimeRoot));
    if (ZrCore_Debug_GetEvaluationContext(state, 0u, &evaluationContext) !=
            ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK ||
        ZrCore_Debug_EvaluationContext_GetRuntimeRoot(
                state,
                &evaluationContext,
                ZR_DEBUG_RUNTIME_ROOT_ZR,
                &runtimeRoot) != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return;
    }
    g_debugRuntimeRootBindingCapture.sawPausedFrame = ZR_TRUE;

    sourceName = ZrCore_String_CreateFromNative(state, "<debug:e2b-runtime-root>");
    if (sourceName == ZR_NULL) {
        return;
    }
    ZrParser_State_Init(&parserState, state, "zr[1]", strlen("zr[1]"), sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    expression = ZrParser_ParseExpressionWithState(&parserState);
    if (parserState.hasError || expression == ZR_NULL ||
        expression->type != ZR_AST_PRIMARY_EXPRESSION) {
        ZrParser_State_Free(&parserState);
        return;
    }
    referenceNode = expression->data.primaryExpression.property;

    memset(&compilerState, 0, sizeof(compilerState));
    ZrParser_CompilerState_Init(&compilerState, state);
    compilerState.currentAst = expression;
    compilerState.scriptAst = expression;
    compilerState.suppressErrorOutput = ZR_TRUE;
    (void)zr_debug_semantic_register_bindings(
            g_debugRuntimeRootBindingCapture.agent, 1u, &compilerState);
    ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_ExpressionType_Infer(&compilerState, expression, &inferredType)) {
        reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                compilerState.semanticContext,
                referenceNode,
                ZR_SEMANTIC_REFERENCE_READ);
        if (reference != ZR_NULL) {
            g_debugRuntimeRootBindingCapture.sawReferenceFact = ZR_TRUE;
            g_debugRuntimeRootBindingCapture.hasCanonicalIdentity =
                    reference->isResolved &&
                    reference->symbolId != ZR_SEMANTIC_ID_INVALID &&
                    reference->typeId != ZR_SEMANTIC_ID_INVALID;
            g_debugRuntimeRootBindingCapture.hasNoSourceIdentity =
                    reference->placeId == ZR_SEMANTIC_ID_INVALID &&
                    reference->declarationRange.source == ZR_NULL &&
                    !reference->hasDefinitionRange;
            g_debugRuntimeRootBindingCapture.originKind = reference->originKind;
            g_debugRuntimeRootBindingCapture.runtimeRootKind = reference->runtimeRootKind;
            g_debugRuntimeRootBindingCapture.originToken = reference->originToken;
            g_debugRuntimeRootBindingCapture.tokenMatchesCoreBinding =
                    reference->originToken == runtimeRoot.token;
        }
    }
    ZrParser_InferredType_Free(state, &inferredType);
    ZrParser_CompilerState_Free(&compilerState);
    ZrParser_Ast_Free(state, expression);
    ZrParser_State_Free(&parserState);

    {
        SZrDebugFormalEvaluationContext formalContext;
        SZrTypeValue formalValue;
        const SZrSemanticReferenceFact *formalReference;
        SZrAstNode *formalReferenceNode;
        TZrBool formalSupported = ZR_TRUE;
        TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

        memset(&formalContext, 0, sizeof(formalContext));
        ZrCore_Value_ResetAsNull(&formalValue);
        error[0] = '\0';
        if (zr_debug_formal_prepare_expression(
                    g_debugRuntimeRootBindingCapture.agent,
                    1u,
                    "zr[1]",
                    &formalContext,
                    error,
                    sizeof(error)) &&
            formalContext.expression != ZR_NULL &&
            formalContext.expression->type == ZR_AST_PRIMARY_EXPRESSION) {
            formalReferenceNode = formalContext.expression->data.primaryExpression.property;
            formalReference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                    formalContext.compilerState.semanticContext,
                    formalReferenceNode,
                    ZR_SEMANTIC_REFERENCE_READ);
            if (formalReference != ZR_NULL && formalReference->originToken != 0u) {
                SZrSemanticReferenceFact *mutableReference =
                        (SZrSemanticReferenceFact *)formalReference;
                const TZrBool originalHasDefinitionRange =
                        mutableReference->hasDefinitionRange;
                const TZrUInt64 originalToken = mutableReference->originToken;

                mutableReference->hasDefinitionRange = ZR_TRUE;
                if (zr_debug_formal_evaluate_node(
                            g_debugRuntimeRootBindingCapture.agent,
                            1u,
                            formalContext.compilerState.semanticContext,
                            formalContext.expression,
                            &formalValue,
                            &formalSupported,
                            error,
                            sizeof(error))) {
                    g_debugRuntimeRootBindingCapture.definitionRangeDriftRejected =
                            !formalSupported;
                }
                mutableReference->hasDefinitionRange = originalHasDefinitionRange;
                formalSupported = ZR_TRUE;
                ZrCore_Value_ResetAsNull(&formalValue);
                mutableReference->originToken++;
                if (zr_debug_formal_evaluate_node(
                            g_debugRuntimeRootBindingCapture.agent,
                            1u,
                            formalContext.compilerState.semanticContext,
                            formalContext.expression,
                            &formalValue,
                            &formalSupported,
                            error,
                            sizeof(error))) {
                    g_debugRuntimeRootBindingCapture.tokenDriftRejected = !formalSupported;
                }
                mutableReference->originToken = originalToken;
            }
            zr_debug_formal_free_prepared_expression(&formalContext);
        }
    }

    {
        ZrDebugEvaluateResult result;

        memset(&result, 0, sizeof(result));
        g_debugRuntimeRootBindingCapture.formalEvaluationError[0] = '\0';
        if (ZrDebug_EvaluateWithCapabilities(
                    g_debugRuntimeRootBindingCapture.agent,
                    1u,
                    "zr[1]",
                    ZR_DEBUG_EVALUATION_EFFECT_NONE,
                    &result,
                    g_debugRuntimeRootBindingCapture.formalEvaluationError,
                    sizeof(g_debugRuntimeRootBindingCapture.formalEvaluationError))) {
            g_debugRuntimeRootBindingCapture.sawFormalEvaluation = ZR_TRUE;
            strncpy(g_debugRuntimeRootBindingCapture.formalEvaluationType,
                    result.type_name,
                    sizeof(g_debugRuntimeRootBindingCapture.formalEvaluationType) - 1u);
            g_debugRuntimeRootBindingCapture.formalEvaluationType[
                    sizeof(g_debugRuntimeRootBindingCapture.formalEvaluationType) - 1u] = '\0';
            strncpy(g_debugRuntimeRootBindingCapture.formalEvaluationValue,
                    result.value_text,
                    sizeof(g_debugRuntimeRootBindingCapture.formalEvaluationValue) - 1u);
            g_debugRuntimeRootBindingCapture.formalEvaluationValue[
                    sizeof(g_debugRuntimeRootBindingCapture.formalEvaluationValue) - 1u] = '\0';
        }
    }
}

static void test_debug_semantic_binding_preserves_paused_frame_canonical_identity(void) {
    const char *source =
            "fn target(paused: int): int {\n"
            "    return paused;\n"
            "}\n"
            "return target(4);";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_e2b1_paused_binding.zr", source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&agent, 0, sizeof(agent));
    memset(&g_debugCanonicalBindingCapture, 0, sizeof(g_debugCanonicalBindingCapture));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    g_debugCanonicalBindingCapture.agent = &agent;

    ZrCore_Debug_SetHook(state, debug_canonical_binding_hook, ZR_DEBUG_HOOK_MASK_LINE, 0u);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(4, result);
    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);

    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawPausedBinding);
    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawReferenceFact);
    TEST_ASSERT_EQUAL_UINT32(
            g_debugCanonicalBindingCapture.expectedSymbolId,
            g_debugCanonicalBindingCapture.actualSymbolId);
    TEST_ASSERT_EQUAL_UINT32(
            g_debugCanonicalBindingCapture.expectedTypeId,
            g_debugCanonicalBindingCapture.actualTypeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, g_debugCanonicalBindingCapture.expectedPlaceId);
    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawEvaluationPolicy);
    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.policyHasCanonicalFacts);
    TEST_ASSERT_TRUE_MESSAGE(g_debugCanonicalBindingCapture.sawFormalEvaluation,
                             g_debugCanonicalBindingCapture.formalEvaluationError);
    TEST_ASSERT_EQUAL_STRING("int", g_debugCanonicalBindingCapture.formalEvaluationType);
    TEST_ASSERT_EQUAL_STRING("8", g_debugCanonicalBindingCapture.formalEvaluationValue);
    TEST_ASSERT_EQUAL_UINT32(
            g_debugCanonicalBindingCapture.expectedStartLine,
            g_debugCanonicalBindingCapture.actualStartLine);
    TEST_ASSERT_EQUAL_UINT32(
            g_debugCanonicalBindingCapture.expectedStartColumn,
            g_debugCanonicalBindingCapture.actualStartColumn);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_semantic_binding_rejects_missing_paused_place(void) {
    const char *source =
            "fn target(paused: int[]): int {\n"
            "    return paused[0];\n"
            "}\n"
            "return target([4, 9]);";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_e2b2_missing_place.zr", source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&agent, 0, sizeof(agent));
    memset(&g_debugCanonicalBindingCapture, 0, sizeof(g_debugCanonicalBindingCapture));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    g_debugCanonicalBindingCapture.agent = &agent;
    g_debugCanonicalBindingCapture.clearPlaceBeforePolicy = ZR_TRUE;
    g_debugCanonicalBindingCapture.formalExpression = "paused[1]";

    ZrCore_Debug_SetHook(state, debug_canonical_binding_hook, ZR_DEBUG_HOOK_MASK_LINE, 0u);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(4, result);
    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);

    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawPausedBinding);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, g_debugCanonicalBindingCapture.expectedPlaceId);
    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawEvaluationPolicy);
    TEST_ASSERT_FALSE(g_debugCanonicalBindingCapture.policyHasCanonicalFacts);
    TEST_ASSERT_FALSE(g_debugCanonicalBindingCapture.sawFormalEvaluation);
    assert_text_contains(g_debugCanonicalBindingCapture.formalEvaluationError,
                         "canonical semantic facts");

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_source_binding_shadows_runtime_root_spelling(void) {
    const char *source =
            "fn target(zr: int): int {\n"
            "    return zr;\n"
            "}\n"
            "return target(4);";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *arrayText;
    SZrObject *arrayObject = ZR_NULL;
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_e2b_runtime_root_shadow.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    arrayText = ZrCore_String_CreateFromNative(state, "abcd");
    TEST_ASSERT_NOT_NULL(arrayText);
    TEST_ASSERT_TRUE(ZrCore_String_ToByteArray(state, arrayText, &arrayObject));
    TEST_ASSERT_NOT_NULL(arrayObject);
    ZrCore_Value_InitAsRawObject(
            state, &state->global->zrObject, ZR_CAST_RAW_OBJECT_AS_SUPER(arrayObject));
    state->global->zrObject.type = ZR_VALUE_TYPE_ARRAY;

    memset(&agent, 0, sizeof(agent));
    memset(&g_debugCanonicalBindingCapture, 0, sizeof(g_debugCanonicalBindingCapture));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    g_debugCanonicalBindingCapture.agent = &agent;
    g_debugCanonicalBindingCapture.expectedBindingName = "zr";
    g_debugCanonicalBindingCapture.formalExpression = "zr";

    ZrCore_Debug_SetHook(state, debug_canonical_binding_hook, ZR_DEBUG_HOOK_MASK_LINE, 0u);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(4, result);
    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);

    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawReferenceFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION,
                          g_debugCanonicalBindingCapture.actualOriginKind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RUNTIME_ROOT_NONE,
                          g_debugCanonicalBindingCapture.actualRuntimeRootKind);
    TEST_ASSERT_EQUAL_UINT64(0u, g_debugCanonicalBindingCapture.actualOriginToken);
    TEST_ASSERT_TRUE_MESSAGE(g_debugCanonicalBindingCapture.sawFormalEvaluation,
                             g_debugCanonicalBindingCapture.formalEvaluationError);
    TEST_ASSERT_EQUAL_STRING("int", g_debugCanonicalBindingCapture.formalEvaluationType);
    TEST_ASSERT_EQUAL_STRING("4", g_debugCanonicalBindingCapture.formalEvaluationValue);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_formal_evaluation_rejects_paused_binding_type_drift(void) {
    const char *source =
            "fn target(paused: int): int {\n"
            "    return paused;\n"
            "}\n"
            "return target(4);";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_e2b1_type_drift.zr", source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&agent, 0, sizeof(agent));
    memset(&g_debugCanonicalBindingCapture, 0, sizeof(g_debugCanonicalBindingCapture));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    g_debugCanonicalBindingCapture.agent = &agent;
    g_debugCanonicalBindingCapture.formalExpression = "paused";
    g_debugCanonicalBindingCapture.rejectTypeDriftDuringFormalRead = ZR_TRUE;

    ZrCore_Debug_SetHook(state, debug_canonical_binding_hook, ZR_DEBUG_HOOK_MASK_LINE, 0u);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(4, result);
    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);

    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawPausedBinding);
    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawReferenceFact);
    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.typeDriftRejected);
    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawFormalEvaluation);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_semantic_binding_registers_canonical_receiver(void) {
    const char *source =
            "class Meter {\n"
            "    pri var _hp: int = 0;\n"
            "    pub @constructor() {\n"
            "        this._hp = 4;\n"
            "    }\n"
            "    pub fn target(): int {\n"
            "        return this._hp;\n"
            "    }\n"
            "}\n"
            "var meter = new Meter();\n"
            "return meter.target();";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_e2_receiver_binding.zr", source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&agent, 0, sizeof(agent));
    memset(&g_debugCanonicalBindingCapture, 0, sizeof(g_debugCanonicalBindingCapture));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    g_debugCanonicalBindingCapture.agent = &agent;
    g_debugCanonicalBindingCapture.expectedBindingName = "this";
    g_debugCanonicalBindingCapture.formalExpression = "this";

    ZrCore_Debug_SetHook(state, debug_canonical_binding_hook, ZR_DEBUG_HOOK_MASK_LINE, 0u);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(4, result);
    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);

    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawPausedBinding);
    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawReferenceFact);
    TEST_ASSERT_EQUAL_UINT32(g_debugCanonicalBindingCapture.expectedSymbolId,
                             g_debugCanonicalBindingCapture.actualSymbolId);
    TEST_ASSERT_EQUAL_UINT32(g_debugCanonicalBindingCapture.expectedTypeId,
                             g_debugCanonicalBindingCapture.actualTypeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, g_debugCanonicalBindingCapture.expectedPlaceId);
    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawEvaluationPolicy);
    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.policyHasCanonicalFacts);
    TEST_ASSERT_TRUE_MESSAGE(g_debugCanonicalBindingCapture.sawFormalEvaluation,
                             g_debugCanonicalBindingCapture.formalEvaluationError);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_formal_evaluation_reads_indexed_paused_frame_binding(void) {
    const char *source =
            "fn target(paused: int[]): int {\n"
            "    return paused[0];\n"
            "}\n"
            "return target([4, 9]);";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_e2b2_indexed_binding.zr", source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&agent, 0, sizeof(agent));
    memset(&g_debugCanonicalBindingCapture, 0, sizeof(g_debugCanonicalBindingCapture));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    g_debugCanonicalBindingCapture.agent = &agent;
    g_debugCanonicalBindingCapture.formalExpression = "paused[1]";

    ZrCore_Debug_SetHook(state, debug_canonical_binding_hook, ZR_DEBUG_HOOK_MASK_LINE, 0u);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(4, result);
    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);

    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawPausedBinding);
    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawReferenceFact);
    TEST_ASSERT_EQUAL_UINT32(g_debugCanonicalBindingCapture.expectedSymbolId,
                             g_debugCanonicalBindingCapture.actualSymbolId);
    TEST_ASSERT_EQUAL_UINT32(g_debugCanonicalBindingCapture.expectedTypeId,
                             g_debugCanonicalBindingCapture.actualTypeId);
    TEST_ASSERT_TRUE_MESSAGE(g_debugCanonicalBindingCapture.sawFormalEvaluation,
                             g_debugCanonicalBindingCapture.formalEvaluationError);
    TEST_ASSERT_EQUAL_STRING("int", g_debugCanonicalBindingCapture.formalEvaluationType);
    TEST_ASSERT_EQUAL_STRING("9", g_debugCanonicalBindingCapture.formalEvaluationValue);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_formal_evaluation_resolves_generation_checked_runtime_root(void) {
    const char *source = "return 1;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *arrayText;
    SZrObject *arrayObject = ZR_NULL;
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_e2b_runtime_root.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    arrayText = ZrCore_String_CreateFromNative(state, "abcd");
    TEST_ASSERT_NOT_NULL(arrayText);
    TEST_ASSERT_TRUE(ZrCore_String_ToByteArray(state, arrayText, &arrayObject));
    TEST_ASSERT_NOT_NULL(arrayObject);
    ZrCore_Value_InitAsRawObject(
            state, &state->global->zrObject, ZR_CAST_RAW_OBJECT_AS_SUPER(arrayObject));
    state->global->zrObject.type = ZR_VALUE_TYPE_ARRAY;

    memset(&agent, 0, sizeof(agent));
    memset(&g_debugRuntimeRootBindingCapture, 0, sizeof(g_debugRuntimeRootBindingCapture));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    g_debugRuntimeRootBindingCapture.agent = &agent;

    ZrCore_Debug_SetHook(state, debug_runtime_root_binding_hook, ZR_DEBUG_HOOK_MASK_LINE, 0u);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);

    TEST_ASSERT_TRUE(g_debugRuntimeRootBindingCapture.sawPausedFrame);
    TEST_ASSERT_TRUE(g_debugRuntimeRootBindingCapture.sawReferenceFact);
    TEST_ASSERT_TRUE(g_debugRuntimeRootBindingCapture.hasCanonicalIdentity);
    TEST_ASSERT_TRUE(g_debugRuntimeRootBindingCapture.hasNoSourceIdentity);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_ORIGIN_RUNTIME_ROOT,
                          g_debugRuntimeRootBindingCapture.originKind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RUNTIME_ROOT_ZR,
                          g_debugRuntimeRootBindingCapture.runtimeRootKind);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, g_debugRuntimeRootBindingCapture.originToken);
    TEST_ASSERT_TRUE(g_debugRuntimeRootBindingCapture.tokenMatchesCoreBinding);
    TEST_ASSERT_TRUE(g_debugRuntimeRootBindingCapture.definitionRangeDriftRejected);
    TEST_ASSERT_TRUE(g_debugRuntimeRootBindingCapture.tokenDriftRejected);
    TEST_ASSERT_TRUE_MESSAGE(g_debugRuntimeRootBindingCapture.sawFormalEvaluation,
                             g_debugRuntimeRootBindingCapture.formalEvaluationError);
    TEST_ASSERT_EQUAL_STRING("uint", g_debugRuntimeRootBindingCapture.formalEvaluationType);
    TEST_ASSERT_EQUAL_STRING("98", g_debugRuntimeRootBindingCapture.formalEvaluationValue);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_semantic_binding_publishes_canonical_closure_capture(void) {
    const char *source =
            "fn makeRunner() {\n"
            "    var seed = 4;\n"
            "    return fn() => {\n"
            "        return seed + 1;\n"
            "    };\n"
            "}\n"
            "var runner = makeRunner();\n"
            "return runner();";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_e2_closure_binding.zr", source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&agent, 0, sizeof(agent));
    memset(&g_debugClosureBindingCapture, 0, sizeof(g_debugClosureBindingCapture));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    g_debugClosureBindingCapture.agent = &agent;

    ZrCore_Debug_SetHook(state, debug_closure_binding_hook, ZR_DEBUG_HOOK_MASK_LINE, 0u);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);
    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);

    TEST_ASSERT_TRUE(g_debugClosureBindingCapture.sawClosureActivation);
    TEST_ASSERT_TRUE(g_debugClosureBindingCapture.sawClosureReferenceFact);
    TEST_ASSERT_TRUE(g_debugClosureBindingCapture.hasCanonicalIdentity);
    TEST_ASSERT_EQUAL_UINT32(0u, g_debugClosureBindingCapture.captureIndex);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, g_debugClosureBindingCapture.symbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, g_debugClosureBindingCapture.typeId);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, g_debugClosureBindingCapture.token);
    TEST_ASSERT_TRUE(g_debugClosureBindingCapture.sawEvaluationPolicy);
    TEST_ASSERT_TRUE(g_debugClosureBindingCapture.policyHasCanonicalFacts);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_semantic_binding_rejects_entry_binding_without_identity(void) {
    const char *source =
            "var provisional: int = 4;\n"
            "return provisional;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    ZrDebugEvaluationEffectPolicy policy;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_e2_entry_binding_identity.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function->typedLocalBindingLength > 0u);

    function->typedLocalBindings[0].symbolId = ZR_SEMANTIC_ID_INVALID;
    function->typedLocalBindings[0].typeId = ZR_SEMANTIC_ID_INVALID;
    memset(&agent, 0, sizeof(agent));
    memset(&policy, 0, sizeof(policy));
    error[0] = '\0';
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_ClassifyEvaluationEffect(
            &agent, 1u, "provisional", &policy, error, sizeof(error)));
    TEST_ASSERT_FALSE(policy.hasCanonicalFacts);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}
