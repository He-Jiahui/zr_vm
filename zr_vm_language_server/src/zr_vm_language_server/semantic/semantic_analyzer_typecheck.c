//
// Created by Auto on 2025/01/XX.
//

#include "semantic/semantic_analyzer_internal.h"
#include "semantic/semantic_analyzer_expected_type.h"
#include "semantic/semantic_analyzer_ownership_diagnostics.h"
#include "semantic/semantic_analyzer_union_patterns.h"

SZrTypePrototypeInfo *find_compiler_type_prototype_inference(SZrCompilerState *cs, SZrString *typeName);
void free_resolved_call_signature(SZrState *state, SZrResolvedCallSignature *signature);
TZrBool bind_foreach_element_type_from_inferred_iterable(SZrCompilerState *cs,
                                                         const SZrInferredType *iterableType,
                                                         SZrInferredType *outType);
static TZrBool semantic_type_from_ast(SZrState *state,
                                      SZrSemanticAnalyzer *analyzer,
                                      const SZrType *typeNode,
                                      SZrInferredType *result);
static TZrBool semantic_infer_node_type(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrAstNode *node,
                                        SZrInferredType *result);

static void semantic_typecheck_using_statement(SZrState *state,
                                               SZrSemanticAnalyzer *analyzer,
                                               SZrAstNode *node);
static void semantic_typecheck_switch_expression(SZrState *state,
                                                 SZrSemanticAnalyzer *analyzer,
                                                 SZrAstNode *node);

static const TZrChar *semantic_identifier_node_text(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_IDENTIFIER_LITERAL || node->data.identifier.name == ZR_NULL) {
        return ZR_NULL;
    }

    return semantic_string_native(node->data.identifier.name);
}

static const TZrChar *semantic_member_property_text(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_MEMBER_EXPRESSION ||
        node->data.memberExpression.computed) {
        return ZR_NULL;
    }

    return semantic_identifier_node_text(node->data.memberExpression.property);
}

static void semantic_add_cannot_infer_exact_type_diagnostic(SZrState *state,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrFileRange location) {
    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                    analyzer,
                                                    ZR_DIAGNOSTIC_ERROR,
                                                    location,
                                                    "cannot infer exact type",
                                                    "cannot_infer_exact_type");
}

static void semantic_add_initializer_requires_annotation_diagnostic(SZrState *state,
                                                                    SZrSemanticAnalyzer *analyzer,
                                                                    SZrFileRange location) {
    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                    analyzer,
                                                    ZR_DIAGNOSTIC_ERROR,
                                                    location,
                                                    "initializer requires annotation",
                                                    "initializer_requires_annotation");
}

static TZrBool semantic_text_equals(const TZrChar *value, const TZrChar *expected) {
    return value != ZR_NULL && expected != ZR_NULL && strcmp(value, expected) == 0;
}

static void semantic_record_reachability_fact_at_range(SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNode *node,
                                                       SZrFileRange range,
                                                       EZrSemanticReachabilityCause cause,
                                                       SZrAstNode *causeNode) {
    SZrSemanticReachabilityFact fact;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || node == ZR_NULL) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = range;
    fact.state = ZR_SEMANTIC_REACHABILITY_UNREACHABLE;
    fact.cause = cause;
    fact.causeNode = causeNode;
    ZrParser_SemanticFacts_AppendReachability(analyzer->semanticContext, &fact);
}

static SZrFileRange semantic_reachability_fact_range_for_node(const SZrAstNode *node) {
    SZrFileRange range;

    if (node == ZR_NULL) {
        return ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 0, 0),
                                         ZrParser_FilePosition_Create(0, 0, 0),
                                         ZR_NULL);
    }

    range = node->location;
    if (node->type == ZR_AST_VARIABLE_DECLARATION) {
        const SZrVariableDeclaration *declaration = &node->data.variableDeclaration;

        if (declaration->pattern != ZR_NULL) {
            range = ZrParser_FileRange_Merge(range, declaration->pattern->location);
        }
        if (declaration->typeInfo != ZR_NULL && declaration->typeInfo->name != ZR_NULL) {
            range = ZrParser_FileRange_Merge(range, declaration->typeInfo->name->location);
        }
        if (declaration->value != ZR_NULL) {
            range = ZrParser_FileRange_Merge(range, declaration->value->location);
        }
    }

    return range;
}

static void semantic_record_reachability_fact(SZrSemanticAnalyzer *analyzer,
                                              SZrAstNode *node,
                                              EZrSemanticReachabilityCause cause,
                                              SZrAstNode *causeNode) {
    if (node == ZR_NULL) {
        return;
    }

    semantic_record_reachability_fact_at_range(analyzer,
                                               node,
                                               semantic_reachability_fact_range_for_node(node),
                                               cause,
                                               causeNode);
}

static void semantic_record_logical_fact(SZrSemanticAnalyzer *analyzer,
                                         SZrAstNode *node,
                                         EZrSemanticLogicalFactKind kind,
                                         TZrBool hasKnownValue,
                                         TZrBool knownValue,
                                         SZrAstNode *relatedNode) {
    SZrSemanticLogicalFact fact;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || node == ZR_NULL) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = kind;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.hasKnownValue = hasKnownValue;
    fact.knownValue = knownValue;
    fact.relatedNode = relatedNode;
    ZrParser_SemanticFacts_AppendLogical(analyzer->semanticContext, &fact);
}

static TZrBool semantic_has_constant_branch_reachability_fact(SZrSemanticAnalyzer *analyzer,
                                                              SZrAstNode *node) {
    TZrSize index;

    if (analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        !analyzer->semanticContext->reachabilityFacts.isValid) {
        return ZR_FALSE;
    }

    for (index = 0; index < analyzer->semanticContext->reachabilityFacts.length; index++) {
        const SZrSemanticReachabilityFact *fact =
                (const SZrSemanticReachabilityFact *)ZrCore_Array_Get(
                        &analyzer->semanticContext->reachabilityFacts,
                        index);
        if (fact != ZR_NULL &&
            fact->node == node &&
            fact->cause == ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void semantic_record_constant_if_condition_facts(SZrState *state,
                                                        SZrSemanticAnalyzer *analyzer,
                                                        SZrAstNode *node) {
    TZrBool conditionValue = ZR_FALSE;
    SZrAstNode *conditionEvidence = ZR_NULL;
    SZrAstNode *condition;
    SZrAstNode *unreachableBranch;

    if (state == ZR_NULL ||
        analyzer == ZR_NULL ||
        node == ZR_NULL ||
        node->type != ZR_AST_IF_EXPRESSION) {
        return;
    }

    condition = node->data.ifExpression.condition;
    if (!ZrLanguageServer_SemanticAnalyzer_TryEvaluateConstantBooleanCondition(
                analyzer,
                condition,
                &conditionValue,
                &conditionEvidence)) {
        return;
    }

    unreachableBranch = conditionValue
                        ? node->data.ifExpression.elseExpr
                        : node->data.ifExpression.thenExpr;
    if (semantic_has_constant_branch_reachability_fact(analyzer, unreachableBranch)) {
        return;
    }

    semantic_record_logical_fact(analyzer,
                                 condition,
                                 conditionValue
                                ? ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE
                                : ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE,
                                 ZR_TRUE,
                                 conditionValue,
                                 conditionEvidence != ZR_NULL ? conditionEvidence : unreachableBranch);
    if (unreachableBranch != ZR_NULL) {
        semantic_record_reachability_fact(analyzer,
                                          unreachableBranch,
                                          ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH,
                                          condition);
    }
}

typedef struct SZrSemanticTypecheckContextSnapshot {
    SZrTypePrototypeInfo *typePrototype;
    SZrAstNode *typeNode;
    SZrString *typeName;
    SZrAstNode *functionNode;
} SZrSemanticTypecheckContextSnapshot;

static SZrTypeEnvironment *semantic_typecheck_push_runtime_type_binding_scope(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer) {
    SZrTypeEnvironment *savedEnv;
    SZrTypeEnvironment *newEnv;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL) {
        return ZR_NULL;
    }

    savedEnv = analyzer->compilerState->typeEnv;
    newEnv = ZrParser_TypeEnvironment_New(state);
    if (newEnv == ZR_NULL) {
        return savedEnv;
    }

    newEnv->parent = savedEnv;
    newEnv->semanticContext =
        savedEnv != ZR_NULL ? savedEnv->semanticContext : analyzer->compilerState->semanticContext;
    analyzer->compilerState->typeEnv = newEnv;
    return savedEnv;
}

static void semantic_typecheck_pop_runtime_type_binding_scope(SZrState *state,
                                                              SZrSemanticAnalyzer *analyzer,
                                                              SZrTypeEnvironment *savedEnv) {
    SZrTypeEnvironment *currentEnv;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL) {
        return;
    }

    currentEnv = analyzer->compilerState->typeEnv;
    if (currentEnv == savedEnv) {
        return;
    }

    analyzer->compilerState->typeEnv = savedEnv;
    if (currentEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(state, currentEnv);
    }
}

static void semantic_typecheck_register_inferred_binding(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrString *name,
        const SZrInferredType *bindingType,
        SZrAstNode *declarationNode) {
    SZrSymbol *symbol;

    if (state == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->compilerState == ZR_NULL ||
        analyzer->compilerState->typeEnv == ZR_NULL ||
        name == ZR_NULL || bindingType == ZR_NULL) {
        return;
    }

    symbol = declarationNode != ZR_NULL
                 ? ZrLanguageServer_SymbolTable_LookupAtPosition(
                       analyzer->symbolTable,
                       name,
                       declarationNode->location)
                 : ZR_NULL;
    if (symbol != ZR_NULL &&
        symbol->semanticId != ZR_SEMANTIC_ID_INVALID &&
        symbol->semanticTypeId != ZR_SEMANTIC_ID_INVALID &&
        ZrParser_TypeEnvironment_RegisterCanonicalVariable(
            state,
            analyzer->compilerState->typeEnv,
            name,
            bindingType,
            symbol->semanticId,
            symbol->semanticTypeId,
            symbol->selectionRange)) {
        return;
    }

    ZrParser_TypeEnvironment_RegisterVariable(
        state,
        analyzer->compilerState->typeEnv,
        name,
        bindingType);
}

static void semantic_typecheck_register_variable_binding(SZrState *state,
                                                         SZrSemanticAnalyzer *analyzer,
                                                         SZrString *name,
                                                         const SZrType *typeNode,
                                                         SZrAstNode *valueNode,
                                                         SZrAstNode *declarationNode) {
    SZrInferredType bindingType;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        analyzer->compilerState->typeEnv == ZR_NULL || name == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(state, &bindingType, ZR_VALUE_TYPE_OBJECT);
    if (typeNode != ZR_NULL) {
        if (!semantic_type_from_ast(state, analyzer, typeNode, &bindingType)) {
            semantic_add_cannot_infer_exact_type_diagnostic(
                    state,
                    analyzer,
                    typeNode->name != ZR_NULL ? typeNode->name->location : ZrParser_FileRange_Create(
                                                                          ZrParser_FilePosition_Create(0, 0, 0),
                                                                          ZrParser_FilePosition_Create(0, 0, 0),
                                                                          ZR_NULL));
            ZrParser_InferredType_Free(state, &bindingType);
            return;
        }
        if (valueNode != ZR_NULL) {
            SZrInferredType initializerType;
            ZrParser_InferredType_Init(state, &initializerType, ZR_VALUE_TYPE_OBJECT);
            if (semantic_infer_node_type(state, analyzer, valueNode, &initializerType)) {
                (void)ZrParser_TypeInference_TryApplyInitializerNumericRange(state,
                                                                             &bindingType,
                                                                             &initializerType);
            }
            ZrParser_InferredType_Free(state, &initializerType);
        }
    } else if (valueNode != ZR_NULL) {
        if (!semantic_infer_node_type(state, analyzer, valueNode, &bindingType)) {
            if (analyzer->compilerState != ZR_NULL && analyzer->compilerState->hasError) {
                ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(state, analyzer, valueNode->location);
            }
            semantic_add_initializer_requires_annotation_diagnostic(state, analyzer, valueNode->location);
            ZrParser_InferredType_Free(state, &bindingType);
            return;
        }
    }

    semantic_typecheck_register_inferred_binding(
        state,
        analyzer,
        name,
        &bindingType,
        declarationNode);
    ZrParser_InferredType_Free(state, &bindingType);
}

static void semantic_typecheck_register_parameter_bindings(SZrState *state,
                                                           SZrSemanticAnalyzer *analyzer,
                                                           SZrAstNodeArray *params) {
    if (state == ZR_NULL || analyzer == ZR_NULL || params == ZR_NULL || params->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        SZrParameter *param;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        param = &paramNode->data.parameter;
        semantic_typecheck_register_variable_binding(state,
                                                     analyzer,
                                                     param->name != ZR_NULL ? param->name->name : ZR_NULL,
                                                     param->typeInfo,
                                                     ZR_NULL,
                                                     paramNode);
    }
}

static void semantic_typecheck_register_foreach_binding(SZrState *state,
                                                        SZrSemanticAnalyzer *analyzer,
                                                        SZrForeachLoop *foreachLoop,
                                                        SZrFileRange diagnosticLocation) {
    SZrString *name;
    SZrInferredType bindingType;
    TZrBool hasBindingType = ZR_FALSE;

    if (state == ZR_NULL || analyzer == ZR_NULL || foreachLoop == ZR_NULL ||
        analyzer->compilerState == ZR_NULL || analyzer->compilerState->typeEnv == ZR_NULL) {
        return;
    }

    name = ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(state, foreachLoop->pattern);
    if (name == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(state, &bindingType, ZR_VALUE_TYPE_OBJECT);
    if (foreachLoop->typeInfo != ZR_NULL) {
        hasBindingType = semantic_type_from_ast(state, analyzer, foreachLoop->typeInfo, &bindingType);
    } else if (foreachLoop->expr != ZR_NULL) {
        SZrInferredType iterableType;

        ZrParser_InferredType_Init(state, &iterableType, ZR_VALUE_TYPE_OBJECT);
        if (semantic_infer_node_type(state, analyzer, foreachLoop->expr, &iterableType)) {
            hasBindingType =
                bind_foreach_element_type_from_inferred_iterable(analyzer->compilerState, &iterableType, &bindingType);
        }
        ZrParser_InferredType_Free(state, &iterableType);
    }

    if (!hasBindingType) {
        semantic_add_cannot_infer_exact_type_diagnostic(state, analyzer, diagnosticLocation);
        ZrParser_InferredType_Free(state, &bindingType);
        return;
    }

    semantic_typecheck_register_inferred_binding(
        state,
        analyzer,
        name,
        &bindingType,
        foreachLoop->pattern);
    ZrParser_InferredType_Free(state, &bindingType);
}

static SZrString *semantic_typecheck_extract_owner_type_name(SZrAstNode *ownerTypeNode) {
    if (ownerTypeNode == ZR_NULL) {
        return ZR_NULL;
    }

    switch (ownerTypeNode->type) {
        case ZR_AST_CLASS_DECLARATION:
            return ownerTypeNode->data.classDeclaration.name != ZR_NULL
                       ? ownerTypeNode->data.classDeclaration.name->name
                       : ZR_NULL;

        case ZR_AST_STRUCT_DECLARATION:
            return ownerTypeNode->data.structDeclaration.name != ZR_NULL
                       ? ownerTypeNode->data.structDeclaration.name->name
                       : ZR_NULL;

        case ZR_AST_INTERFACE_DECLARATION:
            return ownerTypeNode->data.interfaceDeclaration.name != ZR_NULL
                       ? ownerTypeNode->data.interfaceDeclaration.name->name
                       : ZR_NULL;

        default:
            return ZR_NULL;
    }
}

static void semantic_typecheck_push_compiler_context(SZrSemanticAnalyzer *analyzer,
                                                     SZrAstNode *ownerTypeNode,
                                                     SZrAstNode *functionNode,
                                                     SZrSemanticTypecheckContextSnapshot *snapshot) {
    SZrCompilerState *compilerState;
    SZrString *typeName;

    if (snapshot != ZR_NULL) {
        memset(snapshot, 0, sizeof(*snapshot));
    }

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    snapshot->typePrototype = compilerState->currentTypePrototypeInfo;
    snapshot->typeNode = compilerState->currentTypeNode;
    snapshot->typeName = compilerState->currentTypeName;
    snapshot->functionNode = compilerState->currentFunctionNode;

    typeName = semantic_typecheck_extract_owner_type_name(ownerTypeNode);
    if (typeName != ZR_NULL) {
        compilerState->currentTypeNode = ownerTypeNode;
        compilerState->currentTypeName = typeName;
        compilerState->currentTypePrototypeInfo =
            find_compiler_type_prototype_inference(compilerState, typeName);
    }

    if (functionNode != ZR_NULL) {
        compilerState->currentFunctionNode = functionNode;
    }
}

static void semantic_typecheck_pop_compiler_context(
        SZrSemanticAnalyzer *analyzer,
        const SZrSemanticTypecheckContextSnapshot *snapshot) {
    SZrCompilerState *compilerState;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    compilerState->currentTypePrototypeInfo = snapshot->typePrototype;
    compilerState->currentTypeNode = snapshot->typeNode;
    compilerState->currentTypeName = snapshot->typeName;
    compilerState->currentFunctionNode = snapshot->functionNode;
}

static void semantic_typecheck_callable_body(SZrState *state,
                                             SZrSemanticAnalyzer *analyzer,
                                             SZrAstNode *functionNode,
                                             SZrAstNodeArray *params,
                                             SZrAstNodeArray *preludeExpressions,
                                             SZrAstNode *body) {
    SZrSemanticTypecheckContextSnapshot contextSnapshot;
    SZrTypeEnvironment *savedTypeEnv;

    semantic_typecheck_push_compiler_context(analyzer, ZR_NULL, functionNode, &contextSnapshot);
    savedTypeEnv = semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
    semantic_typecheck_register_parameter_bindings(state, analyzer, params);

    if (params != ZR_NULL && params->nodes != ZR_NULL) {
        for (TZrSize i = 0; i < params->count; i++) {
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, params->nodes[i]);
        }
    }
    if (preludeExpressions != ZR_NULL && preludeExpressions->nodes != ZR_NULL) {
        for (TZrSize i = 0; i < preludeExpressions->count; i++) {
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state,
                                                                  analyzer,
                                                                  preludeExpressions->nodes[i]);
        }
    }
    ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, body);

    semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
    semantic_typecheck_pop_compiler_context(analyzer, &contextSnapshot);
}

static TZrBool semantic_extract_ffi_decorator(SZrAstNode *decoratorNode,
                                              const TZrChar **outLeafName,
                                              TZrBool *outHasCall,
                                              SZrFunctionCall **outCall) {
    SZrAstNode *expr;
    SZrPrimaryExpression *primary;
    SZrAstNode *ffiMember;
    SZrAstNode *leafMember;

    if (outLeafName != ZR_NULL) {
        *outLeafName = ZR_NULL;
    }
    if (outHasCall != ZR_NULL) {
        *outHasCall = ZR_FALSE;
    }
    if (outCall != ZR_NULL) {
        *outCall = ZR_NULL;
    }

    if (decoratorNode == ZR_NULL || decoratorNode->type != ZR_AST_DECORATOR_EXPRESSION) {
        return ZR_FALSE;
    }

    expr = decoratorNode->data.decoratorExpression.expr;
    if (expr == ZR_NULL || expr->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    primary = &expr->data.primaryExpression;
    if (!semantic_text_equals(semantic_identifier_node_text(primary->property), "zr") ||
        primary->members == ZR_NULL || primary->members->count < 2 || primary->members->count > 3) {
        return ZR_FALSE;
    }

    ffiMember = primary->members->nodes[0];
    leafMember = primary->members->nodes[1];
    if (!semantic_text_equals(semantic_member_property_text(ffiMember), "ffi")) {
        return ZR_FALSE;
    }

    if (outLeafName != ZR_NULL) {
        *outLeafName = semantic_member_property_text(leafMember);
    }
    if (outLeafName != ZR_NULL && *outLeafName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (primary->members->count == 3) {
        SZrAstNode *callNode = primary->members->nodes[2];
        if (callNode == ZR_NULL || callNode->type != ZR_AST_FUNCTION_CALL) {
            return ZR_FALSE;
        }
        if (outHasCall != ZR_NULL) {
            *outHasCall = ZR_TRUE;
        }
        if (outCall != ZR_NULL) {
            *outCall = &callNode->data.functionCall;
        }
    }

    return ZR_TRUE;
}

static TZrBool semantic_call_has_single_string_arg(SZrFunctionCall *call, const TZrChar **outValue) {
    SZrAstNode *arg;

    if (outValue != ZR_NULL) {
        *outValue = ZR_NULL;
    }
    if (call == ZR_NULL || call->args == ZR_NULL || call->args->count != 1) {
        return ZR_FALSE;
    }

    arg = call->args->nodes[0];
    if (arg == ZR_NULL || arg->type != ZR_AST_STRING_LITERAL || arg->data.stringLiteral.value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outValue != ZR_NULL) {
        *outValue = semantic_string_native(arg->data.stringLiteral.value);
    }
    return ZR_TRUE;
}

static TZrBool semantic_call_has_single_integer_arg(SZrFunctionCall *call) {
    return call != ZR_NULL && call->args != ZR_NULL && call->args->count == 1 &&
           call->args->nodes[0] != ZR_NULL &&
           call->args->nodes[0]->type == ZR_AST_INTEGER_LITERAL;
}

static TZrBool semantic_text_in_set(const TZrChar *value, const TZrChar *const *allowedValues, TZrSize count) {
    TZrSize index;

    if (value == ZR_NULL || allowedValues == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < count; index++) {
        if (semantic_text_equals(value, allowedValues[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void semantic_add_invalid_decorator(SZrState *state,
                                           SZrSemanticAnalyzer *analyzer,
                                           SZrAstNode *decoratorNode,
                                           const TZrChar *message) {
    if (state == ZR_NULL || analyzer == ZR_NULL || decoratorNode == ZR_NULL) {
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                    analyzer,
                                                    ZR_DIAGNOSTIC_ERROR,
                                                    decoratorNode->location,
                                                    message,
                                                    "invalid_decorator");
}

static void semantic_validate_extern_callable_decorators(SZrState *state,
                                                         SZrSemanticAnalyzer *analyzer,
                                                         SZrAstNodeArray *decorators,
                                                         const TZrChar *targetName) {
    static const TZrChar *const allowedCallconvs[] = {"cdecl", "stdcall", "system"};
    static const TZrChar *const allowedCharsets[] = {"utf8", "utf16", "ansi"};
    TZrSize index;

    if (decorators == ZR_NULL) {
        return;
    }

    for (index = 0; index < decorators->count; index++) {
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        const TZrChar *stringArg = ZR_NULL;
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (!semantic_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) || leafName == ZR_NULL) {
            continue;
        }

        if (semantic_text_equals(leafName, "entry")) {
            if (!hasCall || !semantic_call_has_single_string_arg(call, ZR_NULL)) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "zr.ffi.entry requires a single string argument");
            }
        } else if (semantic_text_equals(leafName, "callconv")) {
            if (!hasCall || !semantic_call_has_single_string_arg(call, &stringArg) ||
                !semantic_text_in_set(stringArg, allowedCallconvs, 3)) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "zr.ffi.callconv requires one of: cdecl, stdcall, system");
            }
        } else if (semantic_text_equals(leafName, "charset")) {
            if (!hasCall || !semantic_call_has_single_string_arg(call, &stringArg) ||
                !semantic_text_in_set(stringArg, allowedCharsets, 3)) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "zr.ffi.charset requires one of: utf8, utf16, ansi");
            }
        } else {
            TZrChar buffer[ZR_LSP_TYPE_BUFFER_LENGTH];
            snprintf(buffer, sizeof(buffer), "zr.ffi.%s is not valid on %s", leafName, targetName);
            semantic_add_invalid_decorator(state, analyzer, decoratorNode, buffer);
        }
    }
}

static void semantic_validate_extern_struct_decorators(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNodeArray *decorators) {
    TZrSize index;

    if (decorators == ZR_NULL) {
        return;
    }

    for (index = 0; index < decorators->count; index++) {
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (!semantic_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) || leafName == ZR_NULL) {
            continue;
        }

        if ((semantic_text_equals(leafName, "pack") || semantic_text_equals(leafName, "align")) &&
            hasCall && semantic_call_has_single_integer_arg(call)) {
            continue;
        }

        if (semantic_text_equals(leafName, "pack") || semantic_text_equals(leafName, "align")) {
            semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                           "zr.ffi.pack/align require a single integer argument");
        } else {
            TZrChar buffer[ZR_LSP_TYPE_BUFFER_LENGTH];
            snprintf(buffer, sizeof(buffer), "zr.ffi.%s is not valid on extern struct declarations", leafName);
            semantic_add_invalid_decorator(state, analyzer, decoratorNode, buffer);
        }
    }
}

static void semantic_validate_extern_struct_field_decorators(SZrState *state,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             SZrAstNodeArray *decorators) {
    TZrSize index;

    if (decorators == ZR_NULL) {
        return;
    }

    for (index = 0; index < decorators->count; index++) {
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (!semantic_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) || leafName == ZR_NULL) {
            continue;
        }

        if (semantic_text_equals(leafName, "offset") && hasCall && semantic_call_has_single_integer_arg(call)) {
            continue;
        }

        if (semantic_text_equals(leafName, "offset")) {
            semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                           "zr.ffi.offset requires a single integer argument");
        } else {
            TZrChar buffer[ZR_LSP_TYPE_BUFFER_LENGTH];
            snprintf(buffer, sizeof(buffer), "zr.ffi.%s is not valid on extern struct fields", leafName);
            semantic_add_invalid_decorator(state, analyzer, decoratorNode, buffer);
        }
    }
}

static void semantic_validate_extern_enum_decorators(SZrState *state,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     SZrAstNodeArray *decorators) {
    TZrSize index;

    if (decorators == ZR_NULL) {
        return;
    }

    for (index = 0; index < decorators->count; index++) {
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (!semantic_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) || leafName == ZR_NULL) {
            continue;
        }

        if (semantic_text_equals(leafName, "underlying") && hasCall &&
            semantic_call_has_single_string_arg(call, ZR_NULL)) {
            continue;
        }

        if (semantic_text_equals(leafName, "underlying")) {
            semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                           "zr.ffi.underlying requires a single string argument");
        } else {
            TZrChar buffer[ZR_LSP_TYPE_BUFFER_LENGTH];
            snprintf(buffer, sizeof(buffer), "zr.ffi.%s is not valid on extern enum declarations", leafName);
            semantic_add_invalid_decorator(state, analyzer, decoratorNode, buffer);
        }
    }
}

static void semantic_validate_extern_enum_member_decorators(SZrState *state,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrAstNodeArray *decorators) {
    TZrSize index;

    if (decorators == ZR_NULL) {
        return;
    }

    for (index = 0; index < decorators->count; index++) {
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (!semantic_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) || leafName == ZR_NULL) {
            continue;
        }

        if (semantic_text_equals(leafName, "value") && hasCall && semantic_call_has_single_integer_arg(call)) {
            continue;
        }

        if (semantic_text_equals(leafName, "value")) {
            semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                           "zr.ffi.value requires a single integer argument");
        } else {
            TZrChar buffer[ZR_LSP_TYPE_BUFFER_LENGTH];
            snprintf(buffer, sizeof(buffer), "zr.ffi.%s is not valid on extern enum members", leafName);
            semantic_add_invalid_decorator(state, analyzer, decoratorNode, buffer);
        }
    }
}

static TZrBool semantic_ffi_integer_type_name_supported(const TZrChar *typeName) {
    static const TZrChar *const kSupportedIntegerTypeNames[] = {
            "i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64",
    };

    if (typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    return semantic_text_in_set(typeName, kSupportedIntegerTypeNames, ZR_ARRAY_COUNT(kSupportedIntegerTypeNames));
}

static TZrBool semantic_view_type_is_source_extern_struct(SZrSemanticAnalyzer *analyzer, const TZrChar *typeName) {
    SZrScript *script;

    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL || typeName == ZR_NULL ||
        analyzer->ast->type != ZR_AST_SCRIPT) {
        return ZR_FALSE;
    }

    script = &analyzer->ast->data.script;
    if (script->statements == ZR_NULL || script->statements->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize statementIndex = 0; statementIndex < script->statements->count; statementIndex++) {
        SZrAstNode *statement = script->statements->nodes[statementIndex];

        if (statement == ZR_NULL || statement->type != ZR_AST_EXTERN_BLOCK ||
            statement->data.externBlock.declarations == ZR_NULL ||
            statement->data.externBlock.declarations->nodes == ZR_NULL) {
            continue;
        }

        for (TZrSize declarationIndex = 0;
             declarationIndex < statement->data.externBlock.declarations->count;
             declarationIndex++) {
            SZrAstNode *declaration = statement->data.externBlock.declarations->nodes[declarationIndex];

            if (declaration == ZR_NULL || declaration->type != ZR_AST_STRUCT_DECLARATION ||
                declaration->data.structDeclaration.name == ZR_NULL ||
                declaration->data.structDeclaration.name->name == ZR_NULL) {
                continue;
            }

            if (semantic_text_equals(semantic_string_native(declaration->data.structDeclaration.name->name), typeName)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static void semantic_validate_class_wrapper_decorators(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNodeArray *decorators) {
    static const TZrChar *const allowedLowerings[] = {"value", "pointer", "handle_id"};
    static const TZrChar *const allowedOwnerModes[] = {"borrowed", "owned"};
    SZrAstNode *loweringDecoratorNode = ZR_NULL;
    SZrAstNode *viewTypeDecoratorNode = ZR_NULL;
    SZrAstNode *underlyingDecoratorNode = ZR_NULL;
    const TZrChar *viewTypeName = ZR_NULL;
    const TZrChar *underlyingTypeName = ZR_NULL;
    TZrBool loweringIsHandleId = ZR_FALSE;
    TZrBool loweringWasValid = ZR_FALSE;
    TZrBool viewTypeWasValid = ZR_FALSE;
    TZrBool underlyingWasValid = ZR_FALSE;
    TZrSize index;

    if (decorators == ZR_NULL) {
        return;
    }

    for (index = 0; index < decorators->count; index++) {
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        const TZrChar *stringArg = ZR_NULL;
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (!semantic_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) || leafName == ZR_NULL) {
            continue;
        }

        if (semantic_text_equals(leafName, "lowering")) {
            loweringDecoratorNode = decoratorNode;
            if (!hasCall || !semantic_call_has_single_string_arg(call, &stringArg) ||
                !semantic_text_in_set(stringArg, allowedLowerings, ZR_ARRAY_COUNT(allowedLowerings))) {
                semantic_add_invalid_decorator(state,
                                               analyzer,
                                               decoratorNode,
                                               "zr.ffi.lowering requires one of: value, pointer, handle_id");
            } else {
                loweringWasValid = ZR_TRUE;
                loweringIsHandleId = semantic_text_equals(stringArg, "handle_id");
            }
        } else if (semantic_text_equals(leafName, "viewType")) {
            viewTypeDecoratorNode = decoratorNode;
            if (!hasCall || !semantic_call_has_single_string_arg(call, &stringArg)) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "zr.ffi.viewType requires a single string argument");
            } else {
                viewTypeWasValid = ZR_TRUE;
                viewTypeName = stringArg;
            }
        } else if (semantic_text_equals(leafName, "underlying")) {
            underlyingDecoratorNode = decoratorNode;
            if (!hasCall || !semantic_call_has_single_string_arg(call, &stringArg)) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "zr.ffi.underlying requires a single string argument");
            } else {
                underlyingWasValid = ZR_TRUE;
                underlyingTypeName = stringArg;
            }
        } else if (semantic_text_equals(leafName, "ownerMode")) {
            if (!hasCall || !semantic_call_has_single_string_arg(call, &stringArg) ||
                !semantic_text_in_set(stringArg, allowedOwnerModes, ZR_ARRAY_COUNT(allowedOwnerModes))) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "zr.ffi.ownerMode requires one of: borrowed, owned");
            }
        } else if (semantic_text_equals(leafName, "releaseHook")) {
            if (!hasCall || !semantic_call_has_single_string_arg(call, ZR_NULL)) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "zr.ffi.releaseHook requires a single string argument");
            }
        } else {
            TZrChar buffer[ZR_LSP_TYPE_BUFFER_LENGTH];
            snprintf(buffer, sizeof(buffer), "zr.ffi.%s is not valid on class declarations", leafName);
            semantic_add_invalid_decorator(state, analyzer, decoratorNode, buffer);
        }
    }

    if (underlyingWasValid && (!loweringWasValid || !loweringIsHandleId) && underlyingDecoratorNode != ZR_NULL) {
        semantic_add_invalid_decorator(state,
                                       analyzer,
                                       underlyingDecoratorNode,
                                       "zr.ffi.underlying on class wrappers requires zr.ffi.lowering(\"handle_id\")");
    }

    if (loweringWasValid && loweringIsHandleId && !underlyingWasValid && loweringDecoratorNode != ZR_NULL) {
        semantic_add_invalid_decorator(state,
                                       analyzer,
                                       loweringDecoratorNode,
                                       "zr.ffi.lowering(\"handle_id\") requires zr.ffi.underlying(...)");
    }

    if (loweringWasValid && loweringIsHandleId && underlyingWasValid &&
        !semantic_ffi_integer_type_name_supported(underlyingTypeName) && underlyingDecoratorNode != ZR_NULL) {
        semantic_add_invalid_decorator(state,
                                       analyzer,
                                       underlyingDecoratorNode,
                                       "zr.ffi.underlying on class wrappers requires a supported integer type name: i8, u8, i16, u16, i32, u32, i64, u64");
    }

    if (viewTypeWasValid && !semantic_view_type_is_source_extern_struct(analyzer, viewTypeName) &&
        viewTypeDecoratorNode != ZR_NULL) {
        semantic_add_invalid_decorator(state,
                                       analyzer,
                                       viewTypeDecoratorNode,
                                       "zr.ffi.viewType on class wrappers requires a source extern struct name");
    }
}

static void semantic_validate_extern_parameter_decorators(SZrState *state,
                                                          SZrSemanticAnalyzer *analyzer,
                                                          SZrAstNode *parameterNode) {
    SZrAstNodeArray *decorators;
    TZrSize index;
    TZrSize directionCount = 0;

    if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
        return;
    }

    decorators = parameterNode->data.parameter.decorators;
    if (decorators == ZR_NULL) {
        return;
    }

    for (index = 0; index < decorators->count; index++) {
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (!semantic_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) || leafName == ZR_NULL) {
            continue;
        }

        if (semantic_text_equals(leafName, "in") ||
            semantic_text_equals(leafName, "out") ||
            semantic_text_equals(leafName, "inout")) {
            if (hasCall || call != ZR_NULL) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "FFI direction decorators do not take arguments");
            } else {
                directionCount++;
            }
        } else {
            TZrChar buffer[ZR_LSP_TYPE_BUFFER_LENGTH];
            snprintf(buffer, sizeof(buffer), "zr.ffi.%s is not valid on extern parameters", leafName);
            semantic_add_invalid_decorator(state, analyzer, decoratorNode, buffer);
        }
    }

    if (directionCount > 1) {
        ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                        analyzer,
                                                        ZR_DIAGNOSTIC_ERROR,
                                                        parameterNode->location,
                                                        "Extern parameters may specify only one of zr.ffi.in/out/inout",
                                                        "invalid_decorator");
    }
}

static void semantic_add_type_mismatch_diagnostic(SZrState *state,
                                                  SZrSemanticAnalyzer *analyzer,
                                                  SZrFileRange location,
                                                  const TZrChar *message) {
    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                    analyzer,
                                                    ZR_DIAGNOSTIC_ERROR,
                                                    location,
                                                    message != ZR_NULL ? message : "Type mismatch",
                                                    "type_mismatch");
}

typedef enum EZrSemanticOwnershipDiagnosticKind {
    ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_NONE = 0,
    ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_MISMATCH,
    ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_WEAK_REQUIRES_WAKE,
    ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_OWNER_TO_PLAIN,
    ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_BORROW_ESCAPE,
    ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_LOAN_ESCAPE
} EZrSemanticOwnershipDiagnosticKind;

typedef struct SZrSemanticOwnershipDiagnosticMatch {
    EZrSemanticOwnershipDiagnosticKind kind;
    SZrAstNode *node;
    SZrAstNode *relatedNode;
    SZrAstNode *enclosingCallable;
    SZrFileRange location;
    EZrOwnershipQualifier qualifier;
    TZrChar expectedText[ZR_LSP_TYPE_BUFFER_LENGTH];
    TZrChar actualText[ZR_LSP_TYPE_BUFFER_LENGTH];
} SZrSemanticOwnershipDiagnosticMatch;

static void semantic_ownership_diagnostic_match_init(SZrSemanticOwnershipDiagnosticMatch *match) {
    if (match == ZR_NULL) {
        return;
    }

    memset(match, 0, sizeof(*match));
    match->kind = ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_NONE;
    match->qualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
}

static TZrBool semantic_ownership_is_owned_qualifier(EZrOwnershipQualifier qualifier) {
    return qualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
           qualifier == ZR_OWNERSHIP_QUALIFIER_SHARED;
}

static TZrBool semantic_ownership_type_surface_matches(const SZrInferredType *expectedType,
                                                       const SZrInferredType *actualType) {
    if (expectedType == ZR_NULL || actualType == ZR_NULL ||
        expectedType->baseType != actualType->baseType) {
        return ZR_FALSE;
    }

    if (expectedType->typeName == actualType->typeName) {
        return ZR_TRUE;
    }
    if (expectedType->typeName != ZR_NULL && actualType->typeName != ZR_NULL) {
        return ZrCore_String_Equal(expectedType->typeName, actualType->typeName);
    }

    return ZR_FALSE;
}

static EZrSemanticOwnershipDiagnosticKind semantic_classify_ownership_mismatch(
        const SZrInferredType *expectedType,
        const SZrInferredType *actualType) {
    if (!semantic_ownership_type_surface_matches(expectedType, actualType)) {
        return ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_NONE;
    }

    if (expectedType->ownershipQualifier == actualType->ownershipQualifier) {
        return ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_NONE;
    }
    if ((expectedType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED ||
         expectedType->referenceAccess == ZR_REFERENCE_ACCESS_READONLY) &&
        actualType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_WEAK) {
        return ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_WEAK_REQUIRES_WAKE;
    }
    if (expectedType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_NONE &&
        semantic_ownership_is_owned_qualifier(actualType->ownershipQualifier)) {
        return ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_OWNER_TO_PLAIN;
    }
    if (expectedType->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE ||
        actualType->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE) {
        return ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_MISMATCH;
    }

    return ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_NONE;
}

static void semantic_format_type_name(SZrState *state,
                                      const SZrInferredType *type,
                                      TZrChar *buffer,
                                      TZrSize bufferSize) {
    TZrChar localBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    const TZrChar *typeText;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    typeText = ZrParser_TypeNameString_Get(state, type, localBuffer, sizeof(localBuffer));
    snprintf(buffer, bufferSize, "%s", typeText != ZR_NULL ? typeText : "unknown");
}

static TZrBool semantic_prepare_ownership_mismatch_diagnostic(
        SZrState *state,
        SZrAstNode *node,
        SZrFileRange location,
        const SZrInferredType *expectedType,
        const SZrInferredType *actualType,
        SZrSemanticOwnershipDiagnosticMatch *outMatch) {
    EZrSemanticOwnershipDiagnosticKind kind;

    if (outMatch == ZR_NULL) {
        return ZR_FALSE;
    }

    kind = semantic_classify_ownership_mismatch(expectedType, actualType);
    if (kind == ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_NONE) {
        return ZR_FALSE;
    }

    semantic_ownership_diagnostic_match_init(outMatch);
    outMatch->kind = kind;
    outMatch->node = node;
    outMatch->location = location;
    outMatch->qualifier = actualType != ZR_NULL
                          ? actualType->ownershipQualifier
                          : ZR_OWNERSHIP_QUALIFIER_NONE;
    semantic_format_type_name(state, expectedType, outMatch->expectedText, sizeof(outMatch->expectedText));
    semantic_format_type_name(state, actualType, outMatch->actualText, sizeof(outMatch->actualText));
    return ZR_TRUE;
}

static EZrOwnershipBuiltinKind semantic_ownership_builtin_kind_for_node(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_CONSTRUCT_EXPRESSION) {
        return ZR_OWNERSHIP_BUILTIN_KIND_NONE;
    }

    return node->data.constructExpression.builtinKind;
}

static SZrFileRange semantic_line_range_from_range(SZrFileRange range);

static TZrBool semantic_prepare_return_ownership_escape_diagnostic(
        SZrAstNode *node,
        SZrAstNode *returnNode,
        SZrAstNode *enclosingCallable,
        TZrBool isReferenceReturn,
        EZrReferenceAccess expectedReferenceAccess,
        const SZrInferredType *expectedType,
        const SZrInferredType *actualType,
        SZrSemanticOwnershipDiagnosticMatch *outMatch) {
    EZrOwnershipBuiltinKind builtinKind;

    if (node == ZR_NULL || expectedType == ZR_NULL || actualType == ZR_NULL || outMatch == ZR_NULL) {
        return ZR_FALSE;
    }

    builtinKind = semantic_ownership_builtin_kind_for_node(node);
    if (((isReferenceReturn && expectedReferenceAccess == ZR_REFERENCE_ACCESS_READONLY) ||
         expectedType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED) &&
        ((isReferenceReturn) ||
         (actualType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED &&
          builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_BORROW))) {
        semantic_ownership_diagnostic_match_init(outMatch);
        outMatch->kind = ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_BORROW_ESCAPE;
        outMatch->node = isReferenceReturn && returnNode != ZR_NULL ? returnNode : node;
        outMatch->relatedNode = node->type == ZR_AST_CONSTRUCT_EXPRESSION
                                ? node->data.constructExpression.target
                                : node;
        outMatch->enclosingCallable = enclosingCallable;
        outMatch->location = semantic_line_range_from_range(node->location);
        outMatch->qualifier = ZR_OWNERSHIP_QUALIFIER_BORROWED;
        return ZR_TRUE;
    }
    if (((isReferenceReturn && expectedReferenceAccess == ZR_REFERENCE_ACCESS_WRITABLE) ||
         expectedType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_LOANED) &&
        ((isReferenceReturn) ||
         (actualType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_LOANED &&
          builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_LOAN))) {
        semantic_ownership_diagnostic_match_init(outMatch);
        outMatch->kind = ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_LOAN_ESCAPE;
        outMatch->node = isReferenceReturn && returnNode != ZR_NULL ? returnNode : node;
        outMatch->relatedNode = node->type == ZR_AST_CONSTRUCT_EXPRESSION
                                ? node->data.constructExpression.target
                                : node;
        outMatch->enclosingCallable = enclosingCallable;
        outMatch->location = semantic_line_range_from_range(node->location);
        outMatch->qualifier = ZR_OWNERSHIP_QUALIFIER_LOANED;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static SZrDiagnostic *semantic_add_structured_diagnostic(SZrState *state,
                                                         SZrSemanticAnalyzer *analyzer,
                                                         const SZrStructuredDiagnostic *structured) {
    SZrDiagnostic *diagnostic;

    if (state == ZR_NULL || analyzer == ZR_NULL || structured == ZR_NULL) {
        return ZR_NULL;
    }

    diagnostic = ZrLanguageServer_Diagnostic_FromStructured(state, structured);
    if (diagnostic == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Array_Push(state, &analyzer->diagnostics, &diagnostic);
    return diagnostic;
}

static SZrFileRange semantic_line_range_from_range(SZrFileRange range) {
    range.start.offset = 0;
    range.end.offset = 0;
    range.start.column = 1;
    range.end.column = 32767;
    return range;
}

static void semantic_record_ownership_fact(SZrSemanticAnalyzer *analyzer,
                                           SZrAstNode *node,
                                           SZrAstNode *relatedNode,
                                           SZrFileRange range,
                                           EZrOwnershipQualifier qualifier,
                                           const SZrDiagnostic *diagnostic) {
    SZrSemanticOwnershipFact fact;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || node == ZR_NULL) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location.start.line > 0 ? node->location : range;
    if (node->type == ZR_AST_CONSTRUCT_EXPRESSION &&
        node->data.constructExpression.builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_NONE &&
        fact.range.start.offset >= 2) {
        fact.range.start.offset -= 2;
        fact.range.start.column = fact.range.start.column > 2 ? fact.range.start.column - 2 : 1;
    }
    fact.kind = ZR_SEMANTIC_OWNERSHIP_FACT_ERROR;
    fact.qualifier = qualifier;
    fact.symbolId = ZR_SEMANTIC_ID_INVALID;
    fact.lifetimeRegionId = ZR_SEMANTIC_ID_INVALID;
    fact.ownerLifetimeRegionId = ZR_SEMANTIC_ID_INVALID;
    fact.relatedNode = relatedNode != ZR_NULL ? relatedNode : node;
    fact.isViolation = ZR_TRUE;
    fact.diagnosticMessage = diagnostic != ZR_NULL ? diagnostic->message : ZR_NULL;
    ZrParser_SemanticFacts_AppendOwnership(analyzer->semanticContext, &fact);
}

static void semantic_clear_compiler_error(SZrSemanticAnalyzer *analyzer) {
    if (analyzer != ZR_NULL && analyzer->compilerState != ZR_NULL) {
        analyzer->compilerState->hasError = ZR_FALSE;
        ZrParser_Compiler_ClearStructuredError(analyzer->compilerState);
    }
}

static TZrBool semantic_publish_current_compiler_diagnostic(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange callRange) {
    TZrBool published;

    if (state == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->compilerState == ZR_NULL) {
        return ZR_FALSE;
    }

    published =
            ZrLanguageServer_SemanticAnalyzer_PublishCurrentCompilerQueryDiagnostic(
                    state,
                    analyzer,
                    callRange);
    semantic_clear_compiler_error(analyzer);
    return published;
}

static TZrBool semantic_emit_ownership_diagnostic(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        const SZrSemanticOwnershipDiagnosticMatch *match) {
    SZrStructuredDiagnostic structured;
    SZrDiagnostic *diagnostic = ZR_NULL;
    TZrBool built = ZR_FALSE;

    if (state == ZR_NULL || analyzer == ZR_NULL || match == ZR_NULL ||
        match->kind == ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_NONE) {
        return ZR_FALSE;
    }

    ZrParser_StructuredDiagnostic_Init(&structured);
    switch (match->kind) {
        case ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_WEAK_REQUIRES_WAKE:
            built = ZrParser_DiagnosticBuilder_BuildWeakWake(state, &structured, match->location);
            break;
        case ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_OWNER_TO_PLAIN:
            built = ZrParser_DiagnosticBuilder_BuildOwnerToPlainEscape(state, &structured, match->location);
            break;
        case ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_BORROW_ESCAPE:
            built = ZrParser_DiagnosticBuilder_BuildBorrowEscape(state, &structured, match->location);
            break;
        case ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_LOAN_ESCAPE:
            built = ZrParser_DiagnosticBuilder_BuildLoanEscape(state, &structured, match->location);
            break;
        case ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_MISMATCH:
            built = ZrParser_DiagnosticBuilder_BuildOwnershipMismatch(
                    state,
                    &structured,
                    match->location,
                    match->expectedText,
                    match->actualText);
            break;
        case ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_NONE:
        default:
            break;
    }

    if (built &&
        (match->kind == ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_BORROW_ESCAPE ||
         match->kind == ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_LOAN_ESCAPE)) {
        built = ZrLanguageServer_SemanticOwnership_AddEscapeRelatedInformation(
                state,
                analyzer,
                &structured,
                match->node,
                match->relatedNode,
                match->enclosingCallable,
                match->qualifier);
    }

    if (built) {
        diagnostic = semantic_add_structured_diagnostic(state, analyzer, &structured);
    }
    if (diagnostic != ZR_NULL) {
        semantic_record_ownership_fact(analyzer,
                                       match->node,
                                       match->relatedNode,
                                       match->location,
                                       match->qualifier,
                                       diagnostic);
        semantic_clear_compiler_error(analyzer);
    }
    ZrParser_StructuredDiagnostic_Free(state, &structured);
    return diagnostic != ZR_NULL;
}

static TZrBool semantic_emit_ownership_compatibility_diagnostic(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *node,
        SZrFileRange location,
        const SZrInferredType *expectedType,
        const SZrInferredType *actualType) {
    SZrSemanticOwnershipDiagnosticMatch match;

    semantic_ownership_diagnostic_match_init(&match);
    if (!semantic_prepare_ownership_mismatch_diagnostic(state,
                                                       node,
                                                       location,
                                                       expectedType,
                                                       actualType,
                                                       &match)) {
        return ZR_FALSE;
    }

    return semantic_emit_ownership_diagnostic(state, analyzer, &match);
}


static TZrBool semantic_type_from_ast(SZrState *state,
                                      SZrSemanticAnalyzer *analyzer,
                                      const SZrType *typeNode,
                                      SZrInferredType *result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeNode != ZR_NULL &&
        ZrLanguageServer_SemanticAnalyzer_BuildDeclaredTypeInferredType(analyzer,
                                                                        ZR_NULL,
                                                                        analyzer->compilerState != ZR_NULL
                                                                            ? analyzer->compilerState->currentFunctionNode
                                                                            : ZR_NULL,
                                                                        typeNode,
                                                                        result)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool semantic_infer_node_type(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrAstNode *node,
                                        SZrInferredType *result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_SemanticAnalyzer_InferExactExpressionType(state, analyzer, node, result)) {
        return ZR_TRUE;
    }

    if (analyzer->compilerState != ZR_NULL && analyzer->compilerState->hasError) {
        ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(state, analyzer, node->location);
    }
    return ZR_FALSE;
}

static void semantic_typecheck_using_statement(SZrState *state,
                                               SZrSemanticAnalyzer *analyzer,
                                               SZrAstNode *node) {
    SZrUsingStatement *usingStmt;
    SZrSemanticUnionPatternResolution resolution;
    TZrBool hasUnionPattern;

    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_USING_STATEMENT) {
        return;
    }

    usingStmt = &node->data.usingStatement;
    ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, usingStmt->resource);

    ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionInit(state, &resolution);
    hasUnionPattern = ZrLanguageServer_SemanticAnalyzer_ResolveUsingUnionPattern(state,
                                                                                 analyzer,
                                                                                 usingStmt,
                                                                                 &resolution);
    if (hasUnionPattern && usingStmt->body != ZR_NULL) {
        SZrTypeEnvironment *savedTypeEnv =
            semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
        ZrLanguageServer_SemanticAnalyzer_RegisterUnionPatternBindings(state,
                                                                       analyzer,
                                                                       &resolution,
                                                                       ZR_FALSE,
                                                                       ZR_TRUE);
        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, usingStmt->body);
        semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
    } else {
        if (hasUnionPattern) {
            ZrLanguageServer_SemanticAnalyzer_RegisterUnionPatternBindings(state,
                                                                           analyzer,
                                                                           &resolution,
                                                                           ZR_FALSE,
                                                                           ZR_TRUE);
        }
        if (!hasUnionPattern && usingStmt->body != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, usingStmt->body);
        }
    }

    if (usingStmt->elseBody != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, usingStmt->elseBody);
    }
    ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionFree(state, &resolution);
}

static void semantic_typecheck_switch_expression(SZrState *state,
                                                 SZrSemanticAnalyzer *analyzer,
                                                 SZrAstNode *node) {
    SZrSwitchExpression *switchExpr;
    SZrInferredType subjectType;
    TZrBool subjectTypeInitialized = ZR_FALSE;
    TZrBool hasSubjectType = ZR_FALSE;
    TZrSize index;

    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_SWITCH_EXPRESSION) {
        return;
    }

    switchExpr = &node->data.switchExpression;
    switchExpr->isUnionExhaustive = ZR_FALSE;
    ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, switchExpr->expr);
    if (switchExpr->expr != ZR_NULL) {
        ZrParser_InferredType_Init(state, &subjectType, ZR_VALUE_TYPE_OBJECT);
        subjectTypeInitialized = ZR_TRUE;
        hasSubjectType = semantic_infer_node_type(state, analyzer, switchExpr->expr, &subjectType);
    }

    if (switchExpr->cases != ZR_NULL && switchExpr->cases->nodes != ZR_NULL) {
        for (index = 0; index < switchExpr->cases->count; index++) {
            SZrAstNode *caseNode = switchExpr->cases->nodes[index];
            SZrSwitchCase *switchCase;
            SZrSemanticUnionPatternResolution resolution;
            TZrBool hasUnionPattern;

            if (caseNode == ZR_NULL || caseNode->type != ZR_AST_SWITCH_CASE) {
                continue;
            }

            switchCase = &caseNode->data.switchCase;
            ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionInit(state, &resolution);
            hasUnionPattern = hasSubjectType &&
                              ZrLanguageServer_SemanticAnalyzer_ResolveSwitchUnionPattern(state,
                                                                                          analyzer,
                                                                                          switchCase->value,
                                                                                          &subjectType,
                                                                                          &resolution);
            if (hasUnionPattern && switchCase->block != ZR_NULL) {
                SZrTypeEnvironment *savedTypeEnv =
                    semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
                ZrLanguageServer_SemanticAnalyzer_RegisterUnionPatternBindings(state,
                                                                               analyzer,
                                                                               &resolution,
                                                                               ZR_FALSE,
                                                                               ZR_TRUE);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, switchCase->block);
                semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            } else {
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, switchCase->value);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, switchCase->block);
            }
            ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionFree(state, &resolution);
        }
    }

    if (hasSubjectType) {
        ZrLanguageServer_SemanticAnalyzer_AnalyzeSwitchUnionExhaustiveness(state,
                                                                           analyzer,
                                                                           node,
                                                                           &subjectType);
    }

    if (switchExpr->defaultCase != ZR_NULL &&
        switchExpr->defaultCase->type == ZR_AST_SWITCH_DEFAULT) {
        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(
                state,
                analyzer,
                switchExpr->defaultCase->data.switchDefault.block);
    }

    if (subjectTypeInitialized) {
        ZrParser_InferredType_Free(state, &subjectType);
    }
}

static EZrReferenceAccess semantic_parameter_reference_access(const SZrParameter *parameter) {
    if (parameter == ZR_NULL) {
        return ZR_REFERENCE_ACCESS_NONE;
    }

    switch (parameter->sourcePassingForm) {
        case ZR_PARAMETER_SOURCE_REF:
        case ZR_PARAMETER_SOURCE_SCOPED_REF:
        case ZR_PARAMETER_SOURCE_OUT:
            return ZR_REFERENCE_ACCESS_WRITABLE;
        case ZR_PARAMETER_SOURCE_IN:
        case ZR_PARAMETER_SOURCE_REF_READONLY:
        case ZR_PARAMETER_SOURCE_SCOPED_REF_READONLY:
            return ZR_REFERENCE_ACCESS_READONLY;
        default:
            return parameter->typeInfo != ZR_NULL
                    ? parameter->typeInfo->referenceAccess
                    : ZR_REFERENCE_ACCESS_NONE;
    }
}

static TZrBool semantic_call_matches_parameters(SZrState *state,
                                                SZrSemanticAnalyzer *analyzer,
                                                SZrAstNodeArray *params,
                                                SZrFunctionCall *call,
                                                SZrSemanticOwnershipDiagnosticMatch *outOwnershipDiagnostic) {
    if (state == ZR_NULL || analyzer == ZR_NULL || call == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outOwnershipDiagnostic != ZR_NULL) {
        semantic_ownership_diagnostic_match_init(outOwnershipDiagnostic);
    }

    if (params == ZR_NULL) {
        return call->args == ZR_NULL || call->args->count == 0;
    }

    if ((call->args != ZR_NULL ? call->args->count : 0) != params->count) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        SZrAstNode *argNode = call->args != ZR_NULL ? call->args->nodes[index] : ZR_NULL;
        SZrInferredType expectedType;
        SZrInferredType actualType;
        TZrBool compatible;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER || argNode == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrParser_InferredType_Init(state, &expectedType, ZR_VALUE_TYPE_OBJECT);
        ZrParser_InferredType_Init(state, &actualType, ZR_VALUE_TYPE_OBJECT);
        if (!semantic_type_from_ast(state, analyzer, paramNode->data.parameter.typeInfo, &expectedType)) {
            ZrParser_InferredType_Free(state, &actualType);
            ZrParser_InferredType_Free(state, &expectedType);
            semantic_add_cannot_infer_exact_type_diagnostic(
                    state,
                    analyzer,
                    paramNode->data.parameter.typeInfo != ZR_NULL && paramNode->data.parameter.typeInfo->name != ZR_NULL
                        ? paramNode->data.parameter.typeInfo->name->location
                        : paramNode->location);
            return ZR_FALSE;
        }
        expectedType.referenceAccess = semantic_parameter_reference_access(&paramNode->data.parameter);
        if (!semantic_infer_node_type(state, analyzer, argNode, &actualType)) {
            ZrParser_InferredType_Free(state, &actualType);
            ZrParser_InferredType_Free(state, &expectedType);
            return ZR_FALSE;
        }
        if (outOwnershipDiagnostic != ZR_NULL &&
            semantic_prepare_ownership_mismatch_diagnostic(state,
                                                           argNode,
                                                           argNode->location,
                                                           &expectedType,
                                                           &actualType,
                                                           outOwnershipDiagnostic)) {
            ZrParser_InferredType_Free(state, &actualType);
            ZrParser_InferredType_Free(state, &expectedType);
            return ZR_FALSE;
        }
        compatible = analyzer->compilerState != ZR_NULL &&
                     ZrParser_AssignmentCompatibility_Check(analyzer->compilerState,
                                                            &expectedType,
                                                            &actualType,
                                                            argNode->location);
        if (!compatible) {
            if (outOwnershipDiagnostic != ZR_NULL) {
                semantic_prepare_ownership_mismatch_diagnostic(state,
                                                               argNode,
                                                               argNode->location,
                                                               &expectedType,
                                                               &actualType,
                                                               outOwnershipDiagnostic);
            }
            ZrParser_InferredType_Free(state, &actualType);
            ZrParser_InferredType_Free(state, &expectedType);
            return ZR_FALSE;
        }
        ZrParser_InferredType_Free(state, &actualType);
        ZrParser_InferredType_Free(state, &expectedType);
    }

    return ZR_TRUE;
}

static TZrBool semantic_find_resolved_function_ownership_mismatch(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrFunctionTypeInfo *functionType,
        const SZrArray *parameterTypes,
        SZrFunctionCall *call,
        SZrSemanticOwnershipDiagnosticMatch *outOwnershipDiagnostic);

static TZrBool semantic_find_function_type_ownership_mismatch(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrTypeEnvironment *typeEnv,
        SZrString *name,
        SZrFunctionCall *call,
        SZrSemanticOwnershipDiagnosticMatch *outOwnershipDiagnostic) {
    SZrArray candidates;
    TZrBool found = ZR_FALSE;

    if (outOwnershipDiagnostic != ZR_NULL) {
        semantic_ownership_diagnostic_match_init(outOwnershipDiagnostic);
    }
    if (state == ZR_NULL || analyzer == ZR_NULL || typeEnv == ZR_NULL ||
        name == ZR_NULL || call == ZR_NULL || outOwnershipDiagnostic == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&candidates);
    if (!ZrParser_TypeEnvironment_LookupFunctions(state, typeEnv, name, &candidates)) {
        return ZR_FALSE;
    }

    for (TZrSize candidateIndex = 0; candidateIndex < candidates.length && !found; candidateIndex++) {
        SZrFunctionTypeInfo **functionTypePtr =
                (SZrFunctionTypeInfo **)ZrCore_Array_Get(&candidates, candidateIndex);
        SZrFunctionTypeInfo *functionType =
                functionTypePtr != ZR_NULL ? *functionTypePtr : ZR_NULL;
        TZrSize argCount = call->args != ZR_NULL ? call->args->count : 0;

        if (functionType == ZR_NULL || functionType->paramTypes.length != argCount) {
            continue;
        }

        found = semantic_find_resolved_function_ownership_mismatch(
                state,
                analyzer,
                functionType,
                &functionType->paramTypes,
                call,
                outOwnershipDiagnostic);
    }

    if (candidates.isValid) {
        ZrCore_Array_Free(state, &candidates);
    }
    return found;
}

static TZrBool semantic_find_resolved_function_ownership_mismatch(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrFunctionTypeInfo *functionType,
        const SZrArray *parameterTypes,
        SZrFunctionCall *call,
        SZrSemanticOwnershipDiagnosticMatch *outOwnershipDiagnostic) {
    TZrSize argumentCount;
    SZrAstNodeArray *declaredParameters = ZR_NULL;

    if (outOwnershipDiagnostic != ZR_NULL) {
        semantic_ownership_diagnostic_match_init(outOwnershipDiagnostic);
    }
    if (state == ZR_NULL || analyzer == ZR_NULL || parameterTypes == ZR_NULL ||
        call == ZR_NULL || outOwnershipDiagnostic == ZR_NULL) {
        return ZR_FALSE;
    }

    argumentCount = call->args != ZR_NULL ? call->args->count : 0;
    if (functionType != ZR_NULL && functionType->declarationNode != ZR_NULL) {
        if (functionType->declarationNode->type == ZR_AST_FUNCTION_DECLARATION) {
            declaredParameters = functionType->declarationNode->data.functionDeclaration.params;
        } else if (functionType->declarationNode->type == ZR_AST_EXTERN_FUNCTION_DECLARATION) {
            declaredParameters = functionType->declarationNode->data.externFunctionDeclaration.params;
        }
    }
    if (declaredParameters != ZR_NULL && declaredParameters->count == argumentCount) {
        for (TZrSize index = 0; index < argumentCount; index++) {
            SZrAstNode *parameterNode = declaredParameters->nodes[index];
            SZrAstNode *argumentNode = call->args->nodes[index];
            SZrInferredType expectedType;
            SZrInferredType actualType;
            TZrBool expectedTypeResolved;
            TZrBool actualTypeResolved;
            TZrBool hasMismatch;

            if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER ||
                argumentNode == ZR_NULL) {
                continue;
            }

            ZrParser_InferredType_Init(state, &expectedType, ZR_VALUE_TYPE_OBJECT);
            ZrParser_InferredType_Init(state, &actualType, ZR_VALUE_TYPE_OBJECT);
            expectedTypeResolved = semantic_type_from_ast(state,
                                                          analyzer,
                                                          parameterNode->data.parameter.typeInfo,
                                                          &expectedType);
            if (expectedTypeResolved) {
                expectedType.referenceAccess = semantic_parameter_reference_access(
                        &parameterNode->data.parameter);
            }
            actualTypeResolved = semantic_infer_node_type(state, analyzer, argumentNode, &actualType);
            hasMismatch = expectedTypeResolved && actualTypeResolved &&
                          semantic_prepare_ownership_mismatch_diagnostic(state,
                                                                         argumentNode,
                                                                         argumentNode->location,
                                                                         &expectedType,
                                                                         &actualType,
                                                                         outOwnershipDiagnostic);
            if (hasMismatch) {
                ZrParser_InferredType_Free(state, &actualType);
                ZrParser_InferredType_Free(state, &expectedType);
                return ZR_TRUE;
            }
            ZrParser_InferredType_Free(state, &actualType);
            ZrParser_InferredType_Free(state, &expectedType);
        }
        return ZR_FALSE;
    }

    if (argumentCount != parameterTypes->length) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < argumentCount; index++) {
        SZrAstNode *argumentNode = call->args->nodes[index];
        SZrInferredType *expectedType =
                (SZrInferredType *)ZrCore_Array_Get((SZrArray *)parameterTypes, index);
        SZrInferredType actualType;

        if (argumentNode == ZR_NULL || expectedType == ZR_NULL) {
            continue;
        }

        ZrParser_InferredType_Init(state, &actualType, ZR_VALUE_TYPE_OBJECT);
        if (semantic_infer_node_type(state, analyzer, argumentNode, &actualType) &&
            semantic_prepare_ownership_mismatch_diagnostic(state,
                                                           argumentNode,
                                                           argumentNode->location,
                                                           expectedType,
                                                           &actualType,
                                                           outOwnershipDiagnostic)) {
            ZrParser_InferredType_Free(state, &actualType);
            return ZR_TRUE;
        }
        ZrParser_InferredType_Free(state, &actualType);
    }

    return ZR_FALSE;
}

static TZrBool semantic_resolve_named_function_call_in_env(SZrState *state,
                                                           SZrSemanticAnalyzer *analyzer,
                                                           SZrCompilerState *compilerState,
                                                           SZrTypeEnvironment *typeEnv,
                                                           SZrString *name,
                                                           SZrFunctionCall *call,
                                                           SZrFileRange location,
                                                           TZrBool *outHasCandidate,
                                                           SZrSemanticOwnershipDiagnosticMatch *outOwnershipDiagnostic) {
    SZrFunctionTypeInfo *resolvedFunction = ZR_NULL;
    SZrResolvedCallSignature resolvedSignature;
    TZrBool compatible;

    if (outHasCandidate != ZR_NULL) {
        *outHasCandidate = ZR_FALSE;
    }
    if (outOwnershipDiagnostic != ZR_NULL) {
        semantic_ownership_diagnostic_match_init(outOwnershipDiagnostic);
    }

    if (state == ZR_NULL || analyzer == ZR_NULL || compilerState == ZR_NULL || typeEnv == ZR_NULL ||
        name == ZR_NULL || call == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrParser_TypeEnvironment_LookupFunction(typeEnv, name, &resolvedFunction)) {
        return ZR_FALSE;
    }

    if (outHasCandidate != ZR_NULL) {
        *outHasCandidate = ZR_TRUE;
    }

    memset(&resolvedSignature, 0, sizeof(resolvedSignature));
    ZrParser_InferredType_Init(state, &resolvedSignature.returnType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Construct(&resolvedSignature.parameterTypes);
    ZrCore_Array_Construct(&resolvedSignature.parameterPassingModes);

    if (!ZrParser_FunctionCallOverload_Resolve(compilerState,
                                               typeEnv,
                                               name,
                                               call,
                                               location,
                                               &resolvedFunction,
                                               &resolvedSignature)) {
        free_resolved_call_signature(state, &resolvedSignature);
        return ZR_FALSE;
    }

    if (outOwnershipDiagnostic != ZR_NULL) {
        (void)semantic_find_resolved_function_ownership_mismatch(
                state,
                analyzer,
                resolvedFunction,
                &resolvedSignature.parameterTypes,
                call,
                outOwnershipDiagnostic);
    }

    compatible = ZrParser_FunctionCallCompatibility_Check(compilerState,
                                                          typeEnv,
                                                          name,
                                                          call,
                                                          resolvedFunction,
                                                          &resolvedSignature,
                                                          location);
    if (compatible) {
        compilerState->hasError = ZR_FALSE;
        ZrParser_Compiler_ClearStructuredError(compilerState);
    }
    free_resolved_call_signature(state, &resolvedSignature);
    return compatible;
}

static void semantic_check_named_function_call(SZrState *state,
                                               SZrSemanticAnalyzer *analyzer,
                                               SZrString *name,
                                               SZrFunctionCall *call,
                                               SZrFileRange location) {
    SZrCompilerState *compilerState;
    TZrBool hasRuntimeFunction = ZR_FALSE;
    TZrBool hasCompileTimeFunction = ZR_FALSE;
    SZrSemanticOwnershipDiagnosticMatch ownershipDiagnostic;

    if (state == ZR_NULL || analyzer == ZR_NULL || name == ZR_NULL || call == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    if (compilerState == ZR_NULL) {
        return;
    }
    semantic_ownership_diagnostic_match_init(&ownershipDiagnostic);

    if (semantic_resolve_named_function_call_in_env(state,
                                                    analyzer,
                                                    compilerState,
                                                    compilerState->typeEnv,
                                                    name,
                                                    call,
                                                    location,
                                                    &hasRuntimeFunction,
                                                    &ownershipDiagnostic)) {
        if (semantic_emit_ownership_diagnostic(state, analyzer, &ownershipDiagnostic)) {
            return;
        }
        return;
    }
    if (hasRuntimeFunction) {
        if (semantic_find_function_type_ownership_mismatch(state,
                                                           analyzer,
                                                           compilerState->typeEnv,
                                                           name,
                                                           call,
                                                           &ownershipDiagnostic) &&
            semantic_emit_ownership_diagnostic(state, analyzer, &ownershipDiagnostic)) {
            return;
        }
        if (semantic_publish_current_compiler_diagnostic(
                    state, analyzer, location)) {
            return;
        }
        semantic_add_type_mismatch_diagnostic(state, analyzer, location, "Type mismatch in function call");
        return;
    }

    if (semantic_resolve_named_function_call_in_env(state,
                                                    analyzer,
                                                    compilerState,
                                                    compilerState->compileTimeTypeEnv,
                                                    name,
                                                    call,
                                                    location,
                                                    &hasCompileTimeFunction,
                                                    &ownershipDiagnostic)) {
        if (semantic_emit_ownership_diagnostic(state, analyzer, &ownershipDiagnostic)) {
            return;
        }
        return;
    }
    if (hasCompileTimeFunction) {
        if (semantic_find_function_type_ownership_mismatch(state,
                                                           analyzer,
                                                           compilerState->compileTimeTypeEnv,
                                                           name,
                                                           call,
                                                           &ownershipDiagnostic) &&
            semantic_emit_ownership_diagnostic(state, analyzer, &ownershipDiagnostic)) {
            return;
        }
        if (semantic_publish_current_compiler_diagnostic(
                    state, analyzer, location)) {
            return;
        }
        semantic_add_type_mismatch_diagnostic(state, analyzer, location, "Type mismatch in function call");
    }
}

static void semantic_check_method_call(SZrState *state,
                                       SZrSemanticAnalyzer *analyzer,
                                       SZrAstNode *receiverNode,
                                       SZrAstNode *memberNode,
                                       SZrFunctionCall *call,
                                       SZrFileRange location) {
    SZrInferredType receiverType;
    SZrSymbol *classSymbol;
    TZrBool sawCandidate = ZR_FALSE;
    TZrBool matchedCandidate = ZR_FALSE;
    const TZrChar *memberName;
    SZrSemanticOwnershipDiagnosticMatch ownershipDiagnostic;

    if (state == ZR_NULL || analyzer == ZR_NULL || receiverNode == ZR_NULL ||
        memberNode == ZR_NULL || call == ZR_NULL) {
        return;
    }

    memberName = semantic_member_property_text(memberNode);
    if (memberName == ZR_NULL) {
        return;
    }
    semantic_ownership_diagnostic_match_init(&ownershipDiagnostic);

    ZrParser_InferredType_Init(state, &receiverType, ZR_VALUE_TYPE_OBJECT);
    if (!semantic_infer_node_type(state, analyzer, receiverNode, &receiverType)) {
        ZrParser_InferredType_Free(state, &receiverType);
        return;
    }
    if (receiverType.typeName == ZR_NULL) {
        ZrParser_InferredType_Free(state, &receiverType);
        return;
    }

    classSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, receiverType.typeName, ZR_NULL);
    if (classSymbol != ZR_NULL && classSymbol->astNode != ZR_NULL &&
        classSymbol->astNode->type == ZR_AST_CLASS_DECLARATION &&
        classSymbol->astNode->data.classDeclaration.members != ZR_NULL) {
        for (TZrSize index = 0; index < classSymbol->astNode->data.classDeclaration.members->count; index++) {
            SZrAstNode *candidateNode = classSymbol->astNode->data.classDeclaration.members->nodes[index];
            if (candidateNode == ZR_NULL || candidateNode->type != ZR_AST_CLASS_METHOD ||
                candidateNode->data.classMethod.name == ZR_NULL ||
                !semantic_text_equals(semantic_string_native(candidateNode->data.classMethod.name->name), memberName)) {
                continue;
            }

            sawCandidate = ZR_TRUE;
            SZrSemanticOwnershipDiagnosticMatch candidateOwnershipDiagnostic;
            semantic_ownership_diagnostic_match_init(&candidateOwnershipDiagnostic);
            if (semantic_call_matches_parameters(state,
                                                  analyzer,
                                                  candidateNode->data.classMethod.params,
                                                  call,
                                                  &candidateOwnershipDiagnostic)) {
                matchedCandidate = ZR_TRUE;
                break;
            }
            if (ownershipDiagnostic.kind == ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_NONE &&
                candidateOwnershipDiagnostic.kind != ZR_SEMANTIC_OWNERSHIP_DIAGNOSTIC_NONE) {
                ownershipDiagnostic = candidateOwnershipDiagnostic;
            }
        }
    }

    ZrParser_InferredType_Free(state, &receiverType);
    if (sawCandidate && !matchedCandidate) {
        if (semantic_emit_ownership_diagnostic(state, analyzer, &ownershipDiagnostic)) {
            return;
        }
        semantic_add_type_mismatch_diagnostic(state, analyzer, location, "Type mismatch in method call");
    }
}

static SZrSymbol *semantic_find_enclosing_callable(SZrSymbolTable *table, SZrFileRange position) {
    SZrSymbol *bestSymbol = ZR_NULL;

    if (table == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize scopeIndex = 0; scopeIndex < table->allScopes.length; scopeIndex++) {
        SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrCore_Array_Get(&table->allScopes, scopeIndex);
        if (scopePtr == ZR_NULL || *scopePtr == ZR_NULL) {
            continue;
        }

        for (TZrSize symbolIndex = 0; symbolIndex < (*scopePtr)->symbols.length; symbolIndex++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&(*scopePtr)->symbols, symbolIndex);
            if (symbolPtr == ZR_NULL || *symbolPtr == ZR_NULL || (*symbolPtr)->astNode == ZR_NULL) {
                continue;
            }

            if ((*symbolPtr)->type != ZR_SYMBOL_FUNCTION && (*symbolPtr)->type != ZR_SYMBOL_METHOD) {
                continue;
            }

            if ((*symbolPtr)->location.start.offset <= position.start.offset &&
                position.end.offset <= (*symbolPtr)->location.end.offset &&
                (bestSymbol == ZR_NULL ||
                 (*symbolPtr)->location.start.offset >= bestSymbol->location.start.offset)) {
                bestSymbol = *symbolPtr;
            }
        }
    }

    return bestSymbol;
}

static const SZrType *semantic_callable_return_type(SZrSymbol *symbol) {
    if (symbol == ZR_NULL || symbol->astNode == ZR_NULL) {
        return ZR_NULL;
    }

    if (symbol->astNode->type == ZR_AST_FUNCTION_DECLARATION) {
        return symbol->astNode->data.functionDeclaration.returnType;
    }
    if (symbol->astNode->type == ZR_AST_CLASS_METHOD) {
        return symbol->astNode->data.classMethod.returnType;
    }

    return ZR_NULL;
}

void ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }
    
    // 根据节点类型进行类型检查
    switch (node->type) {
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            if (binExpr->left != ZR_NULL && binExpr->right != ZR_NULL) {
                ZR_UNUSED_PARAMETER(binExpr);
            }
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            if (assignExpr->left != ZR_NULL && assignExpr->right != ZR_NULL) {
                SZrInferredType leftType, rightType;
                SZrFileRange expectedLocation;
                TZrBool hasLeftType;
                TZrBool hasRightType;
                ZrParser_InferredType_Init(state, &leftType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Init(state, &rightType, ZR_VALUE_TYPE_OBJECT);
                hasLeftType = ZrParser_ExpressionType_Infer(analyzer->compilerState, assignExpr->left, &leftType);
                hasRightType = hasLeftType
                               ? ZrParser_ExpressionType_Infer(analyzer->compilerState, assignExpr->right, &rightType)
                               : ZR_FALSE;
                if (hasLeftType && hasRightType) {
                    expectedLocation =
                            ZrLanguageServer_SemanticAnalyzer_AssignmentExpectedTypeLocation(
                                    analyzer,
                                    assignExpr->left);
                    // 检查赋值类型兼容性
                    if (!ZrParser_AssignmentCompatibility_CheckDetailed(
                                analyzer->compilerState,
                                &leftType,
                                &rightType,
                                assignExpr->right->location,
                                &expectedLocation)) {
                        if (!semantic_emit_ownership_compatibility_diagnostic(state,
                                                                              analyzer,
                                                                              assignExpr->right,
                                                                              assignExpr->right->location,
                                                                              &leftType,
                                                                              &rightType)) {
                            (void)semantic_publish_current_compiler_diagnostic(
                                    state,
                                    analyzer,
                                    assignExpr->right->location);
                        }
                    }
                }
                if (hasRightType) {
                    ZrParser_InferredType_Free(state, &rightType);
                }
                if (hasLeftType) {
                    ZrParser_InferredType_Free(state, &leftType);
                }
                
                (void)ZrLanguageServer_SemanticAnalyzer_ProjectConstAssignment(
                        state, analyzer, node);
            }
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            ZR_UNUSED_PARAMETER(&node->data.functionCall);
            // 检查函数调用的参数类型
            // TODO: 注意：这里需要查找函数定义并检查参数类型，简化实现暂时跳过
            // 完整实现需要使用 ZrParser_FunctionCallCompatibility_Check
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            if (varDecl->typeInfo != ZR_NULL && varDecl->value != ZR_NULL) {
                SZrInferredType expectedType;
                SZrInferredType valueType;
                SZrFileRange expectedLocation;
                TZrBool hasValueType;
                TZrBool compatible = ZR_FALSE;

                ZrParser_InferredType_Init(state, &expectedType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Init(state, &valueType, ZR_VALUE_TYPE_OBJECT);
                if (!semantic_type_from_ast(state, analyzer, varDecl->typeInfo, &expectedType)) {
                    semantic_add_cannot_infer_exact_type_diagnostic(
                            state,
                            analyzer,
                            varDecl->typeInfo->name != ZR_NULL ? varDecl->typeInfo->name->location : node->location);
                    ZrParser_InferredType_Free(state, &valueType);
                    ZrParser_InferredType_Free(state, &expectedType);
                    break;
                }
                hasValueType = semantic_infer_node_type(state, analyzer, varDecl->value, &valueType);
                if (!hasValueType) {
                    semantic_add_cannot_infer_exact_type_diagnostic(state, analyzer, varDecl->value->location);
                } else {
                    expectedLocation =
                            varDecl->typeInfo->name != ZR_NULL
                                    ? varDecl->typeInfo->name->location
                                    : node->location;
                    compatible = analyzer->compilerState != ZR_NULL &&
                                 ZrParser_AssignmentCompatibility_CheckDetailed(
                                         analyzer->compilerState,
                                         &expectedType,
                                         &valueType,
                                         varDecl->value->location,
                                         &expectedLocation);
                }
                if (hasValueType && !compatible) {
                    if (!semantic_emit_ownership_compatibility_diagnostic(state,
                                                                          analyzer,
                                                                          varDecl->value,
                                                                          varDecl->value->location,
                                                                          &expectedType,
                                                                          &valueType)) {
                        (void)semantic_publish_current_compiler_diagnostic(
                                state,
                                analyzer,
                                varDecl->value->location);
                    }
                }
                ZrParser_InferredType_Free(state, &valueType);
                ZrParser_InferredType_Free(state, &expectedType);
            }
            break;
        }
        
        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *returnStmt = &node->data.returnStatement;
            if (returnStmt->expr != ZR_NULL) {
                SZrSymbol *enclosingCallable =
                        semantic_find_enclosing_callable(analyzer->symbolTable, node->location);
                const SZrType *returnTypeNode = semantic_callable_return_type(enclosingCallable);
                if (returnTypeNode != ZR_NULL) {
                    SZrInferredType expectedType;
                    SZrInferredType actualType;
                    SZrFileRange expectedLocation;
                    TZrBool compatible = ZR_FALSE;
                    TZrBool hasActualType;
                    TZrBool emittedOwnershipDiagnostic = ZR_FALSE;

                    ZrParser_InferredType_Init(state, &expectedType, ZR_VALUE_TYPE_OBJECT);
                    ZrParser_InferredType_Init(state, &actualType, ZR_VALUE_TYPE_OBJECT);
                    if (!semantic_type_from_ast(state, analyzer, returnTypeNode, &expectedType)) {
                        semantic_add_cannot_infer_exact_type_diagnostic(
                                state,
                                analyzer,
                                returnTypeNode->name != ZR_NULL ? returnTypeNode->name->location : node->location);
                        ZrParser_InferredType_Free(state, &actualType);
                        ZrParser_InferredType_Free(state, &expectedType);
                        break;
                    }
                    if (returnStmt->isReferenceReturn) {
                        SZrSemanticOwnershipDiagnosticMatch escapeDiagnostic;
                        semantic_ownership_diagnostic_match_init(&escapeDiagnostic);
                        if (semantic_prepare_return_ownership_escape_diagnostic(returnStmt->expr,
                                                                                node,
                                                                                enclosingCallable != ZR_NULL
                                                                                    ? enclosingCallable->astNode
                                                                                    : ZR_NULL,
                                                                                returnStmt->isReferenceReturn,
                                                                                returnTypeNode->referenceAccess,
                                                                                &expectedType,
                                                                                &actualType,
                                                                                &escapeDiagnostic)) {
                            emittedOwnershipDiagnostic =
                                    semantic_emit_ownership_diagnostic(state, analyzer, &escapeDiagnostic);
                        }
                    }
                    hasActualType = semantic_infer_node_type(state, analyzer, returnStmt->expr, &actualType);
                    expectedLocation =
                            returnTypeNode->name != ZR_NULL
                                    ? returnTypeNode->name->location
                                    : node->location;
                    compatible = hasActualType &&
                                 analyzer->compilerState != ZR_NULL &&
                                 ZrParser_AssignmentCompatibility_CheckDetailed(
                                         analyzer->compilerState,
                                         &expectedType,
                                         &actualType,
                                         returnStmt->expr->location,
                                         &expectedLocation);
                    if (emittedOwnershipDiagnostic) {
                        compatible = ZR_FALSE;
                    }
                    if (hasActualType && !emittedOwnershipDiagnostic) {
                        SZrSemanticOwnershipDiagnosticMatch escapeDiagnostic;
                        semantic_ownership_diagnostic_match_init(&escapeDiagnostic);
                        if (semantic_prepare_return_ownership_escape_diagnostic(returnStmt->expr,
                                                                                node,
                                                                                enclosingCallable != ZR_NULL
                                                                                    ? enclosingCallable->astNode
                                                                                    : ZR_NULL,
                                                                                ZR_FALSE,
                                                                                returnTypeNode->referenceAccess,
                                                                                &expectedType,
                                                                                &actualType,
                                                                                &escapeDiagnostic)) {
                            emittedOwnershipDiagnostic =
                                    semantic_emit_ownership_diagnostic(state, analyzer, &escapeDiagnostic);
                            if (emittedOwnershipDiagnostic) {
                                compatible = ZR_FALSE;
                            }
                        }
                    }
                    if (!hasActualType) {
                        semantic_add_cannot_infer_exact_type_diagnostic(state,
                                                                        analyzer,
                                                                        returnStmt->expr->location);
                    }
                    if (hasActualType && !compatible && !emittedOwnershipDiagnostic) {
                        emittedOwnershipDiagnostic =
                                semantic_emit_ownership_compatibility_diagnostic(state,
                                                                                 analyzer,
                                                                                 returnStmt->expr,
                                                                                 node->location,
                                                                                 &expectedType,
                                                                                 &actualType);
                    }
                    if (hasActualType && !compatible && !emittedOwnershipDiagnostic) {
                        (void)semantic_publish_current_compiler_diagnostic(
                                state,
                                analyzer,
                                returnStmt->expr->location);
                    }
                    ZrParser_InferredType_Free(state, &actualType);
                    ZrParser_InferredType_Free(state, &expectedType);
                } else {
                    SZrInferredType actualType;
                    TZrBool hasActualType;

                    ZrParser_InferredType_Init(state, &actualType, ZR_VALUE_TYPE_OBJECT);
                    hasActualType = semantic_infer_node_type(state, analyzer, returnStmt->expr, &actualType);
                    ZrParser_InferredType_Free(state, &actualType);
                    if (!hasActualType) {
                        semantic_add_cannot_infer_exact_type_diagnostic(state,
                                                                        analyzer,
                                                                        returnStmt->expr->location);
                    }
                }
            }
            break;
        }

        case ZR_AST_EXPRESSION_STATEMENT: {
            SZrAstNode *expr = node->data.expressionStatement.expr;
            SZrInferredType exprType;

            if (expr != ZR_NULL && expr->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
                ZrParser_InferredType_Init(state, &exprType, ZR_VALUE_TYPE_OBJECT);
                (void)semantic_infer_node_type(state, analyzer, expr, &exprType);
                ZrParser_InferredType_Free(state, &exprType);
            }
            break;
        }

        case ZR_AST_LOGICAL_EXPRESSION: {
            TZrBool leftValue = ZR_FALSE;
            SZrAstNode *leftEvidence = ZR_NULL;
            SZrAstNode *left = node->data.logicalExpression.left;
            SZrAstNode *right = node->data.logicalExpression.right;
            if (right != ZR_NULL &&
                ZrLanguageServer_SemanticAnalyzer_TryEvaluateConstantBooleanCondition(
                        analyzer,
                        left,
                        &leftValue,
                        &leftEvidence) &&
                ((semantic_text_equals(node->data.logicalExpression.op, "||") && leftValue) ||
                 (semantic_text_equals(node->data.logicalExpression.op, "&&") && !leftValue))) {
                semantic_record_logical_fact(analyzer,
                                             node,
                                             ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT,
                                             ZR_TRUE,
                                             leftValue,
                                             right);
                semantic_record_reachability_fact_at_range(analyzer,
                                                           right,
                                                           right->location,
                                                           ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT,
                                                           leftEvidence != ZR_NULL ? leftEvidence : left);
            }
            break;
        }

        case ZR_AST_IF_EXPRESSION: {
            semantic_record_constant_if_condition_facts(state, analyzer, node);
            break;
        }

        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primaryExpr = &node->data.primaryExpression;
            if (primaryExpr->property != ZR_NULL &&
                primaryExpr->property->type == ZR_AST_IDENTIFIER_LITERAL &&
                primaryExpr->members != ZR_NULL &&
                primaryExpr->members->count > 0 &&
                primaryExpr->members->nodes[0] != ZR_NULL &&
                primaryExpr->members->nodes[0]->type == ZR_AST_FUNCTION_CALL) {
                semantic_check_named_function_call(state,
                                                  analyzer,
                                                  primaryExpr->property->data.identifier.name,
                                                  &primaryExpr->members->nodes[0]->data.functionCall,
                                                  node->location);
            } else if (primaryExpr->property != ZR_NULL &&
                       primaryExpr->members != ZR_NULL &&
                       primaryExpr->members->count > 1 &&
                       primaryExpr->members->nodes[0] != ZR_NULL &&
                       primaryExpr->members->nodes[0]->type == ZR_AST_MEMBER_EXPRESSION &&
                       primaryExpr->members->nodes[1] != ZR_NULL &&
                       primaryExpr->members->nodes[1]->type == ZR_AST_FUNCTION_CALL) {
                semantic_check_method_call(state,
                                           analyzer,
                                           primaryExpr->property,
                                           primaryExpr->members->nodes[0],
                                           &primaryExpr->members->nodes[1]->data.functionCall,
                                           node->location);
            }
            break;
        }

        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            semantic_validate_extern_callable_decorators(state,
                                                         analyzer,
                                                         node->data.externFunctionDeclaration.decorators,
                                                         "extern functions");
            break;

        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            semantic_validate_extern_callable_decorators(state,
                                                         analyzer,
                                                         node->data.externDelegateDeclaration.decorators,
                                                         "extern delegates");
            break;

        case ZR_AST_CLASS_DECLARATION:
            semantic_validate_class_wrapper_decorators(state,
                                                       analyzer,
                                                       node->data.classDeclaration.decorators);
            break;

        case ZR_AST_STRUCT_DECLARATION:
            semantic_validate_extern_struct_decorators(state,
                                                       analyzer,
                                                       node->data.structDeclaration.decorators);
            break;

        case ZR_AST_STRUCT_FIELD:
            semantic_validate_extern_struct_field_decorators(state,
                                                             analyzer,
                                                             node->data.structField.decorators);
            break;

        case ZR_AST_ENUM_DECLARATION:
            semantic_validate_extern_enum_decorators(state,
                                                     analyzer,
                                                     node->data.enumDeclaration.decorators);
            break;

        case ZR_AST_ENUM_MEMBER:
            semantic_validate_extern_enum_member_decorators(state,
                                                            analyzer,
                                                            node->data.enumMember.decorators);
            break;

        case ZR_AST_PARAMETER:
            semantic_validate_extern_parameter_decorators(state, analyzer, node);
            break;

        case ZR_AST_INTERFACE_DECLARATION:
            ZrLanguageServer_SemanticAnalyzer_ValidateInterfaceVarianceRules(state, analyzer, node);
            break;
        
        default:
            break;
    }
    
    // 递归检查子节点
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL && script->statements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    if (script->statements->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, script->statements->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_EXTERN_BLOCK: {
            SZrExternBlock *externBlock = &node->data.externBlock;
            if (externBlock->declarations != ZR_NULL && externBlock->declarations->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < externBlock->declarations->count; i++) {
                    if (externBlock->declarations->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state,
                                                                              analyzer,
                                                                              externBlock->declarations->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            SZrTypeEnvironment *savedTypeEnv =
                semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
            TZrBool terminated = ZR_FALSE;
            SZrAstNode *terminatingNode = ZR_NULL;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    if (block->body->nodes[i] != ZR_NULL) {
                        if (terminated) {
                            semantic_record_reachability_fact(
                                    analyzer,
                                    block->body->nodes[i],
                                    ZrLanguageServer_SemanticAnalyzer_ReachabilityCauseForExitNode(terminatingNode),
                                    terminatingNode);
                        }
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, block->body->nodes[i]);
                        if (!terminated &&
                            ZrLanguageServer_SemanticAnalyzer_StatementDefinitelyExits(
                                    analyzer,
                                    block->body->nodes[i])) {
                            terminated = ZR_TRUE;
                            terminatingNode = block->body->nodes[i];
                        }
                    }
                }
            }
            semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            break;
        }
        
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, binExpr->left);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, binExpr->right);
            break;
        }
        
        case ZR_AST_UNARY_EXPRESSION: {
            SZrUnaryExpression *unaryExpr = &node->data.unaryExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, unaryExpr->argument);
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, assignExpr->left);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, assignExpr->right);
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            SZrFunctionCall *funcCall = &node->data.functionCall;
            if (funcCall->args != ZR_NULL && funcCall->args->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcCall->args->count; i++) {
                    if (funcCall->args->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, funcCall->args->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primaryExpr = &node->data.primaryExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, primaryExpr->property);
            if (primaryExpr->members != ZR_NULL && primaryExpr->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < primaryExpr->members->count; i++) {
                    if (primaryExpr->members->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, primaryExpr->members->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, varDecl->pattern);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, varDecl->value);
            semantic_typecheck_register_variable_binding(
                    state,
                    analyzer,
                    ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(state, varDecl->pattern),
                    varDecl->typeInfo,
                    varDecl->value,
                    node);
            break;
        }

        case ZR_AST_EXPRESSION_STATEMENT: {
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state,
                                                                  analyzer,
                                                                  node->data.expressionStatement.expr);
            break;
        }

        case ZR_AST_RETURN_STATEMENT: {
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state,
                                                                  analyzer,
                                                                  node->data.returnStatement.expr);
            break;
        }

        case ZR_AST_THROW_STATEMENT: {
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state,
                                                                  analyzer,
                                                                  node->data.throwStatement.expr);
            break;
        }

        case ZR_AST_USING_STATEMENT: {
            semantic_typecheck_using_statement(state, analyzer, node);
            break;
        }

        case ZR_AST_TEMPLATE_STRING_LITERAL: {
            SZrTemplateStringLiteral *templateLiteral = &node->data.templateStringLiteral;
            if (templateLiteral->segments != ZR_NULL && templateLiteral->segments->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < templateLiteral->segments->count; i++) {
                    if (templateLiteral->segments->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, templateLiteral->segments->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_INTERPOLATED_SEGMENT: {
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, node->data.interpolatedSegment.expression);
            break;
        }
        
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            SZrSemanticTypecheckContextSnapshot contextSnapshot;
            SZrTypeEnvironment *savedTypeEnv;

            semantic_typecheck_push_compiler_context(analyzer, ZR_NULL, node, &contextSnapshot);
            savedTypeEnv = semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
            semantic_typecheck_register_parameter_bindings(state, analyzer, funcDecl->params);
            if (funcDecl->params != ZR_NULL && funcDecl->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                    if (funcDecl->params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, funcDecl->params->nodes[i]);
                    }
                }
            }
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, funcDecl->body);
            semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            semantic_typecheck_pop_compiler_context(analyzer, &contextSnapshot);
            break;
        }

        case ZR_AST_EXTERN_FUNCTION_DECLARATION: {
            SZrExternFunctionDeclaration *funcDecl = &node->data.externFunctionDeclaration;
            SZrSemanticTypecheckContextSnapshot contextSnapshot;
            SZrTypeEnvironment *savedTypeEnv;

            semantic_typecheck_push_compiler_context(analyzer, ZR_NULL, node, &contextSnapshot);
            savedTypeEnv = semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
            semantic_typecheck_register_parameter_bindings(state, analyzer, funcDecl->params);
            if (funcDecl->params != ZR_NULL && funcDecl->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                    if (funcDecl->params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, funcDecl->params->nodes[i]);
                    }
                }
            }
            semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            semantic_typecheck_pop_compiler_context(analyzer, &contextSnapshot);
            break;
        }

        case ZR_AST_LOGICAL_EXPRESSION: {
            SZrLogicalExpression *logicalExpr = &node->data.logicalExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, logicalExpr->left);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, logicalExpr->right);
            break;
        }

        case ZR_AST_EXTERN_DELEGATE_DECLARATION: {
            SZrExternDelegateDeclaration *delegateDecl = &node->data.externDelegateDeclaration;
            if (delegateDecl->params != ZR_NULL && delegateDecl->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < delegateDecl->params->count; i++) {
                    if (delegateDecl->params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, delegateDecl->params->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *structDecl = &node->data.structDeclaration;
            SZrSemanticTypecheckContextSnapshot contextSnapshot;

            semantic_typecheck_push_compiler_context(analyzer, node, ZR_NULL, &contextSnapshot);
            if (structDecl->members != ZR_NULL && structDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < structDecl->members->count; i++) {
                    if (structDecl->members->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, structDecl->members->nodes[i]);
                    }
                }
            }
            semantic_typecheck_pop_compiler_context(analyzer, &contextSnapshot);
            break;
        }

        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *classDecl = &node->data.classDeclaration;
            SZrSemanticTypecheckContextSnapshot contextSnapshot;

            semantic_typecheck_push_compiler_context(analyzer, node, ZR_NULL, &contextSnapshot);
            if (classDecl->members != ZR_NULL && classDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < classDecl->members->count; i++) {
                    if (classDecl->members->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state,
                                                                              analyzer,
                                                                              classDecl->members->nodes[i]);
                    }
                }
            }
            semantic_typecheck_pop_compiler_context(analyzer, &contextSnapshot);
            break;
        }

        case ZR_AST_STRUCT_METHOD:
            semantic_typecheck_callable_body(state,
                                             analyzer,
                                             node,
                                             node->data.structMethod.params,
                                             ZR_NULL,
                                             node->data.structMethod.body);
            break;

        case ZR_AST_STRUCT_META_FUNCTION:
            semantic_typecheck_callable_body(state,
                                             analyzer,
                                             node,
                                             node->data.structMetaFunction.params,
                                             ZR_NULL,
                                             node->data.structMetaFunction.body);
            break;

        case ZR_AST_CLASS_METHOD:
            semantic_typecheck_callable_body(state,
                                             analyzer,
                                             node,
                                             node->data.classMethod.params,
                                             ZR_NULL,
                                             node->data.classMethod.body);
            break;

        case ZR_AST_CLASS_META_FUNCTION:
            semantic_typecheck_callable_body(state,
                                             analyzer,
                                             node,
                                             node->data.classMetaFunction.params,
                                             node->data.classMetaFunction.superArgs,
                                             node->data.classMetaFunction.body);
            break;

        case ZR_AST_ENUM_DECLARATION: {
            SZrEnumDeclaration *enumDecl = &node->data.enumDeclaration;
            if (enumDecl->members != ZR_NULL && enumDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < enumDecl->members->count; i++) {
                    if (enumDecl->members->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, enumDecl->members->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpr = &node->data.ifExpression;
            SZrTypeInferenceBranchScope branchScope;
            SZrInferredType conditionType;

            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, ifExpr->condition);
            ZrParser_InferredType_Init(state, &conditionType, ZR_VALUE_TYPE_OBJECT);
            (void)semantic_infer_node_type(state, analyzer, ifExpr->condition, &conditionType);
            ZrParser_InferredType_Free(state, &conditionType);
            semantic_record_constant_if_condition_facts(state, analyzer, node);

            (void)ZrParser_TypeInference_PushTrueBranchNumericRangeScope(analyzer->compilerState,
                                                                          ifExpr->condition,
                                                                          &branchScope);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, ifExpr->thenExpr);
            ZrParser_TypeInference_PopBranchScope(analyzer->compilerState, &branchScope);

            (void)ZrParser_TypeInference_PushFalseBranchNumericRangeScope(analyzer->compilerState,
                                                                          ifExpr->condition,
                                                                          &branchScope);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, ifExpr->elseExpr);
            ZrParser_TypeInference_PopBranchScope(analyzer->compilerState, &branchScope);
            (void)ZrParser_TypeInference_TryJoinIfElseNumericAssignments(analyzer->compilerState, node);
            break;
        }

        case ZR_AST_SWITCH_EXPRESSION: {
            semantic_typecheck_switch_expression(state, analyzer, node);
            break;
        }
        
        case ZR_AST_WHILE_LOOP: {
            SZrWhileLoop *whileLoop = &node->data.whileLoop;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, whileLoop->cond);
            ZrLanguageServer_SemanticAnalyzer_RecordConstantLoopConditionFacts(state,
                                                                               analyzer,
                                                                               whileLoop->cond,
                                                                               whileLoop->block);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, whileLoop->block);
            (void)ZrParser_TypeInference_TryJoinWhileNumericAssignments(analyzer->compilerState, node);
            break;
        }
        
        case ZR_AST_FOR_LOOP: {
            SZrForLoop *forLoop = &node->data.forLoop;
            if (forLoop->init != ZR_NULL &&
                forLoop->init->type == ZR_AST_VARIABLE_DECLARATION) {
                SZrTypeEnvironment *savedTypeEnv =
                    semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);

                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->init);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->cond);
                ZrLanguageServer_SemanticAnalyzer_RecordConstantLoopConditionFacts(state,
                                                                                   analyzer,
                                                                                   forLoop->cond,
                                                                                   forLoop->block);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->step);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->block);
                semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            } else {
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->init);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->cond);
                ZrLanguageServer_SemanticAnalyzer_RecordConstantLoopConditionFacts(state,
                                                                                   analyzer,
                                                                                   forLoop->cond,
                                                                                   forLoop->block);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->step);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->block);
            }
            (void)ZrParser_TypeInference_TryJoinForNumericAssignments(analyzer->compilerState, node);
            break;
        }

        case ZR_AST_FOREACH_LOOP: {
            SZrForeachLoop *foreachLoop = &node->data.foreachLoop;
            SZrTypeEnvironment *savedTypeEnv =
                semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);

            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, foreachLoop->expr);
            semantic_typecheck_register_foreach_binding(state,
                                                        analyzer,
                                                        foreachLoop,
                                                        foreachLoop->pattern != ZR_NULL
                                                            ? foreachLoop->pattern->location
                                                            : node->location);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, foreachLoop->block);
            semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            (void)ZrParser_TypeInference_TryJoinForeachNumericAssignments(analyzer->compilerState, node);
            break;
        }
        
        default:
            break;
    }
}

// 创建语义分析器
