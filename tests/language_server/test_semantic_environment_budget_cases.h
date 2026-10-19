#ifndef ZR_TEST_SEMANTIC_ENVIRONMENT_BUDGET_CASES_H
#define ZR_TEST_SEMANTIC_ENVIRONMENT_BUDGET_CASES_H

#include "zr_vm_parser/comptime_contract.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/syntax_context.h"
#include "zr_vm_parser/type_system.h"

typedef struct SEnvironmentBudgetFixture {
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table;
    SZrSyntaxContext marked;
    SZrSyntaxContext mixed;
    SZrTypeEnvironment *env;
    SZrAstNode *ast;
    SZrInferredType intType;
    TZrBool hasType;
    SZrArray parameters[2];
    SZrArray results;
} SEnvironmentBudgetFixture;

typedef struct SEnvironmentBudgetSnapshot {
    TZrSize variables, functions, external, named, names;
} SEnvironmentBudgetSnapshot;

static SEnvironmentBudgetFixture g_environment_fixture;

static void environment_budget_fixture_free(void) {
    if (g_state != ZR_NULL) {
        ZrCore_Array_Free(g_state, &g_environment_fixture.results);
        ZrParser_TypeEnvironment_Free(g_state, g_environment_fixture.env);
        /* Parameter buffers borrow the primitive input; env owns its copies. */
        for (TZrSize index = 0U; index < 2U; index++) {
            ZrCore_Array_Free(g_state, &g_environment_fixture.parameters[index]);
        }
        if (g_environment_fixture.hasType) {
            ZrParser_InferredType_Free(g_state, &g_environment_fixture.intType);
        }
        if (g_environment_fixture.ast != ZR_NULL) {
            ZrParser_Ast_Free(g_state, g_environment_fixture.ast);
        }
        ZrParser_SyntaxContextTable_Release(g_environment_fixture.table);
    }
    /* The caller-owned budget remains alive until every table owner is gone. */
    memset(&g_environment_fixture, 0, sizeof(g_environment_fixture));
}

static TZrBool environment_budget_fixture_init(const char *source, TZrSize statements) {
    SZrParserComptimeBudgetLimits limits = {0};
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;
    SZrString *sourceName;

    memset(&g_environment_fixture, 0, sizeof(g_environment_fixture));
    ZrCore_Array_Construct(&g_environment_fixture.results);
    for (TZrSize index = 0U; index < 2U; index++) {
        ZrCore_Array_Construct(&g_environment_fixture.parameters[index]);
    }
    limits.fuel = UINT64_MAX;
    limits.callDepth = UINT64_MAX;
    limits.heapBytes = UINT64_MAX;
    limits.aggregateCount = UINT64_MAX;
    limits.generatedDeclarationCount = UINT64_MAX;
    limits.diagnosticCount = UINT64_MAX;
    limits.tokenCount = UINT64_MAX;
    limits.nodeCount = UINT64_MAX;
    limits.matchingCount = UINT64_MAX;
    limits.expansionCount = UINT64_MAX;
    ZrParser_ComptimeBudget_Init(&g_environment_fixture.budget, &limits);
    g_environment_fixture.table = ZrParser_SyntaxContextTable_Create(
            g_state->global, &g_environment_fixture.budget);
    g_environment_fixture.env = ZrParser_TypeEnvironment_New(g_state);
    if (g_environment_fixture.table == ZR_NULL || g_environment_fixture.env == ZR_NULL ||
        g_environment_fixture.env->semanticContext != ZR_NULL ||
        g_environment_fixture.env->parent != ZR_NULL ||
        !g_environment_fixture.env->variableTypes.isValid ||
        !g_environment_fixture.env->functionReturnTypes.isValid ||
        !g_environment_fixture.env->externalCallableValues.isValid ||
        !g_environment_fixture.env->namedTypeBindings.isValid ||
        !g_environment_fixture.env->typeNames.isValid) {
        return ZR_FALSE;
    }
    expansion.stableIdentity = UINT64_C(0x4D31454E56425544);
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    if (!ZrParser_SyntaxContextTable_RegisterExpansion(
                g_environment_fixture.table, &expansion, &expansionId)) {
        return ZR_FALSE;
    }
    g_environment_fixture.marked = ZrParser_SyntaxContext_DefSite(
            g_environment_fixture.table, expansionId);
    g_environment_fixture.mixed = ZrParser_SyntaxContext_MixedSite(
            g_environment_fixture.table, expansionId);
    if (!ZrParser_SyntaxContext_IsValid(g_environment_fixture.marked) ||
        !ZrParser_SyntaxContext_IsValid(g_environment_fixture.mixed)) {
        return ZR_FALSE;
    }
    ZrParser_InferredType_Init(g_state, &g_environment_fixture.intType, ZR_VALUE_TYPE_INT64);
    g_environment_fixture.hasType = ZR_TRUE;
    sourceName = ZrCore_String_CreateFromNative(g_state, "semantic_environment_budget.zr");
    if (sourceName == ZR_NULL) {
        return ZR_FALSE;
    }
    g_environment_fixture.ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    return g_environment_fixture.ast != ZR_NULL &&
           g_environment_fixture.ast->type == ZR_AST_SCRIPT &&
           g_environment_fixture.ast->data.script.statements != ZR_NULL &&
           g_environment_fixture.ast->data.script.statements->count == statements &&
           g_environment_fixture.env->semanticContext == ZR_NULL &&
           g_environment_fixture.env->parent == ZR_NULL;
}

static SZrAstNode *environment_budget_node(TZrSize index, EZrAstNodeType kind) {
    SZrAstNodeArray *statements = g_environment_fixture.ast->data.script.statements;
    SZrAstNode *node = index < statements->count ? statements->nodes[index] : ZR_NULL;
    BUDGET_PREREQUISITE(node != ZR_NULL && node->type == kind, "environment declaration AST kind");
    return node;
}

static SZrIdentifier *environment_budget_identifier(SZrAstNode *node) {
    SZrIdentifier *identifier = ZR_NULL;
    if (node->type == ZR_AST_FUNCTION_DECLARATION) {
        identifier = node->data.functionDeclaration.name;
    } else if (node->type == ZR_AST_VARIABLE_DECLARATION &&
               node->data.variableDeclaration.pattern != ZR_NULL &&
               node->data.variableDeclaration.pattern->type == ZR_AST_IDENTIFIER_LITERAL) {
        identifier = &node->data.variableDeclaration.pattern->data.identifier;
    }
    BUDGET_PREREQUISITE(identifier != ZR_NULL && identifier->name != ZR_NULL,
            "environment declaration identifier");
    return identifier;
}

static SZrArray *environment_budget_parameters(TZrSize index, TZrSize count) {
    SZrArray *parameters = &g_environment_fixture.parameters[index];
    ZrCore_Array_Init(g_state, parameters, sizeof(SZrInferredType), count != 0U ? count : 1U);
    for (TZrSize parameter = 0U; parameter < count; parameter++) {
        ZrCore_Array_Push(g_state, parameters, &g_environment_fixture.intType);
    }
    BUDGET_PREREQUISITE(parameters->isValid && parameters->head != ZR_NULL &&
            parameters->length == count, "environment input parameter array");
    return parameters;
}

static SEnvironmentBudgetSnapshot environment_budget_snapshot(void) {
    SZrTypeEnvironment *env = g_environment_fixture.env;
    SEnvironmentBudgetSnapshot snapshot;
    snapshot.variables = env->variableTypes.length;
    snapshot.functions = env->functionReturnTypes.length;
    snapshot.external = env->externalCallableValues.length;
    snapshot.named = env->namedTypeBindings.length;
    snapshot.names = env->typeNames.length;
    return snapshot;
}

static TZrBool environment_budget_unchanged(SEnvironmentBudgetSnapshot before) {
    SEnvironmentBudgetSnapshot after = environment_budget_snapshot();
    return g_environment_fixture.env->semanticContext == ZR_NULL &&
           g_environment_fixture.env->parent == ZR_NULL &&
           before.variables == after.variables && before.functions == after.functions &&
           before.external == after.external && before.named == after.named &&
           before.names == after.names;
}

static void environment_budget_zero_heap(SZrSyntaxContext context, EZrSyntaxNamespace space) {
    SZrSyntaxContext normalized;
    TZrUInt64 before;
    normalized = ZrParser_SyntaxContext_Normalize(context, space);
    BUDGET_PREREQUISITE(ZrParser_SyntaxContext_IsValid(normalized),
            "environment normalization warmup");
    before = g_environment_fixture.budget.usage.heapBytes;
    normalized = ZrParser_SyntaxContext_Normalize(context, space);
    BUDGET_PREREQUISITE(ZrParser_SyntaxContext_IsValid(normalized) &&
            g_environment_fixture.budget.usage.heapBytes > before,
            "environment normalization has positive scratch heap delta");
    g_environment_fixture.budget.limits.heapBytes = g_environment_fixture.budget.usage.heapBytes;
}

static TZrBool environment_budget_range_equal(SZrFileRange left, SZrFileRange right) {
    return left.source == right.source &&
           left.start.line == right.start.line && left.start.column == right.start.column &&
           left.start.offset == right.start.offset && left.end.line == right.end.line &&
           left.end.column == right.end.column && left.end.offset == right.end.offset;
}

static TZrBool environment_budget_variable_equal(
        const SZrTypeBinding *binding, const SZrTypeBinding *before) {
    return binding != ZR_NULL && binding->name == before->name &&
           ZrParser_InferredType_Equal(&binding->type, &before->type) &&
           environment_budget_range_equal(binding->declarationRange, before->declarationRange) &&
           binding->hasDeclarationRange == before->hasDeclarationRange &&
           binding->typeId == before->typeId && binding->symbolId == before->symbolId &&
           binding->placeId == before->placeId && binding->originKind == before->originKind &&
           binding->runtimeRootKind == before->runtimeRootKind &&
           binding->originToken == before->originToken && binding->originIndex == before->originIndex &&
           binding->syntaxContext.table == before->syntaxContext.table &&
           binding->syntaxContext.markId == before->syntaxContext.markId;
}

static SZrFunctionTypeInfo *environment_budget_function(TZrSize index) {
    SZrFunctionTypeInfo **entry = ZrCore_Array_Get(
            &g_environment_fixture.env->functionReturnTypes, index);
    return entry != ZR_NULL ? *entry : ZR_NULL;
}

static TZrBool environment_budget_function_valid(
        SZrFunctionTypeInfo *function, SZrAstNode *node, SZrSyntaxContext context,
        TZrSize parameters, TZrBool valueBinding) {
    if (function == ZR_NULL || function->declarationNode != node ||
        function->isCallableValueBinding != valueBinding || function->isExternalCallable ||
        function->symbolId != ZR_SEMANTIC_ID_INVALID || function->typeId != ZR_SEMANTIC_ID_INVALID ||
        function->syntaxContext.table != context.table || function->syntaxContext.markId != context.markId ||
        !ZrParser_InferredType_Equal(&function->returnType, &g_environment_fixture.intType) ||
        function->paramTypes.length != parameters || function->genericParameters.length != 0U ||
        function->parameterPassingModes.length != 0U) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < parameters; index++) {
        const SZrInferredType *parameter = ZrCore_Array_Get(&function->paramTypes, index);
        if (parameter == ZR_NULL ||
            !ZrParser_InferredType_Equal(parameter, &g_environment_fixture.intType)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool environment_budget_function_unchanged(
        TZrSize index, SZrFunctionTypeInfo *original, const SZrFunctionTypeInfo *before) {
    SZrFunctionTypeInfo *function = environment_budget_function(index);
    return function == original && function != ZR_NULL &&
           memcmp(function, before, sizeof(*before)) == 0 &&
           environment_budget_function_valid(function, before->declarationNode,
                   before->syntaxContext, before->paramTypes.length, before->isCallableValueBinding);
}

static void test_marked_variable_reregistration_without_snapshot_preserves_binding(void) {
    SZrAstNode *node;
    SZrIdentifier *identifier;
    SZrTypeBinding beforeBinding;
    const SZrTypeBinding *binding;
    SEnvironmentBudgetSnapshot before;
    TZrBool result;
    BUDGET_PREREQUISITE(environment_budget_fixture_init("var environmentValue=1;", 1U),
            "independent variable environment without a semantic snapshot");
    node = environment_budget_node(0U, ZR_AST_VARIABLE_DECLARATION);
    identifier = environment_budget_identifier(node);
    identifier->syntaxContext = g_environment_fixture.marked;
    BUDGET_PREREQUISITE(ZrParser_TypeEnvironment_RegisterVariableExWithContext(
            g_state, g_environment_fixture.env, identifier->name, &g_environment_fixture.intType,
            node, node->location, g_environment_fixture.marked), "wide marked variable registration");
    BUDGET_PREREQUISITE(ZrParser_TypeEnvironment_RegisterVariableExWithContext(
            g_state, g_environment_fixture.env, identifier->name, &g_environment_fixture.intType,
            node, node->location, g_environment_fixture.marked) &&
            g_environment_fixture.env->variableTypes.length == 1U, "wide marked variable re-registration");
    binding = ZrCore_Array_Get(&g_environment_fixture.env->variableTypes, 0U);
    BUDGET_PREREQUISITE(binding != ZR_NULL && binding->name == identifier->name &&
            binding->symbolId == ZR_SEMANTIC_ID_INVALID && binding->typeId == ZR_SEMANTIC_ID_INVALID &&
            binding->syntaxContext.table == g_environment_fixture.marked.table &&
            binding->syntaxContext.markId == g_environment_fixture.marked.markId &&
            ZrParser_InferredType_Equal(&binding->type, &g_environment_fixture.intType),
            "unique marked variable binding before the constrained call");
    memcpy(&beforeBinding, binding, sizeof(beforeBinding));
    environment_budget_zero_heap(g_environment_fixture.marked, ZR_SYNTAX_NAMESPACE_LOCAL);
    before = environment_budget_snapshot();
    result = ZrParser_TypeEnvironment_RegisterVariableExWithContext(
            g_state, g_environment_fixture.env, identifier->name, &g_environment_fixture.intType,
            node, node->location, g_environment_fixture.marked);
    binding = ZrCore_Array_Get(&g_environment_fixture.env->variableTypes, 0U);
    if (!environment_budget_unchanged(before) || !environment_budget_variable_equal(binding, &beforeBinding) ||
        (!result && memcmp(binding, &beforeBinding, sizeof(beforeBinding)) != 0)) {
        TEST_FAIL_MESSAGE("Marked variable re-registration without a semantic snapshot must not append a duplicate binding");
    }
}

static void test_marked_callable_duplicate_without_snapshot_is_rejected(void) {
    SZrAstNode *node;
    SZrIdentifier *identifier;
    SZrArray *parameters;
    SZrFunctionTypeInfo *original;
    SZrFunctionTypeInfo beforeFunction;
    SEnvironmentBudgetSnapshot before;
    TZrBool result;
    BUDGET_PREREQUISITE(environment_budget_fixture_init(
            "fn environmentCallable(value:int):int { return value; }", 1U),
            "independent callable-value environment without a semantic snapshot");
    node = environment_budget_node(0U, ZR_AST_FUNCTION_DECLARATION);
    identifier = environment_budget_identifier(node);
    identifier->syntaxContext = g_environment_fixture.marked;
    parameters = environment_budget_parameters(0U, 1U);
    BUDGET_PREREQUISITE(ZrParser_TypeEnvironment_RegisterCallableValueFunctionWithContext(
            g_state, g_environment_fixture.env, identifier->name, &g_environment_fixture.intType,
            parameters, ZR_NULL, ZR_NULL, node, g_environment_fixture.marked),
            "wide marked callable-value registration");
    BUDGET_PREREQUISITE(!ZrParser_TypeEnvironment_RegisterCallableValueFunctionWithContext(
            g_state, g_environment_fixture.env, identifier->name, &g_environment_fixture.intType,
            parameters, ZR_NULL, ZR_NULL, node, g_environment_fixture.marked) &&
            g_environment_fixture.env->functionReturnTypes.length == 1U,
            "wide marked same-AST callable-value rejection");
    original = environment_budget_function(0U);
    BUDGET_PREREQUISITE(environment_budget_function_valid(
            original, node, g_environment_fixture.marked, 1U, ZR_TRUE),
            "complete marked callable-value binding before the constrained call");
    memcpy(&beforeFunction, original, sizeof(beforeFunction));
    environment_budget_zero_heap(g_environment_fixture.marked, ZR_SYNTAX_NAMESPACE_LOCAL);
    before = environment_budget_snapshot();
    result = ZrParser_TypeEnvironment_RegisterCallableValueFunctionWithContext(
            g_state, g_environment_fixture.env, identifier->name, &g_environment_fixture.intType,
            parameters, ZR_NULL, ZR_NULL, node, g_environment_fixture.marked);
    if (result || !environment_budget_unchanged(before) ||
        !environment_budget_function_unchanged(0U, original, &beforeFunction)) {
        TEST_FAIL_MESSAGE("Marked callable re-registration without a semantic snapshot must reject the same AST");
    }
}

static void test_equivalent_item_callable_duplicate_without_snapshot_is_rejected(void) {
    SZrAstNode *node;
    SZrIdentifier *identifier;
    SZrArray *parameters;
    SZrFunctionTypeInfo *original;
    SZrFunctionTypeInfo beforeFunction;
    SEnvironmentBudgetSnapshot before;
    SZrSyntaxContext root = ZrParser_SyntaxContext_Root();
    SZrSyntaxContext normalized;
    TZrBool result;
    BUDGET_PREREQUISITE(environment_budget_fixture_init(
            "fn environmentItem(value:int):int { return value; }", 1U),
            "independent ordinary callable environment without a semantic snapshot");
    node = environment_budget_node(0U, ZR_AST_FUNCTION_DECLARATION);
    identifier = environment_budget_identifier(node);
    identifier->syntaxContext = root;
    parameters = environment_budget_parameters(0U, 1U);
    BUDGET_PREREQUISITE(ZrParser_TypeEnvironment_RegisterFunctionEx(
            g_state, g_environment_fixture.env, identifier->name, &g_environment_fixture.intType,
            parameters, ZR_NULL, ZR_NULL, node), "wide Root ordinary callable registration");
    identifier->syntaxContext = g_environment_fixture.mixed;
    normalized = ZrParser_SyntaxContext_Normalize(g_environment_fixture.mixed, ZR_SYNTAX_NAMESPACE_ITEM);
    BUDGET_PREREQUISITE(!ZrParser_SyntaxContext_Equal(root, g_environment_fixture.mixed) &&
            ZrParser_SyntaxContext_IsValid(normalized) && ZrParser_SyntaxContext_Equal(root, normalized),
            "different raw Root/MixedSite contexts have equal ITEM identity");
    BUDGET_PREREQUISITE(!ZrParser_TypeEnvironment_RegisterFunctionEx(
            g_state, g_environment_fixture.env, identifier->name, &g_environment_fixture.intType,
            parameters, ZR_NULL, ZR_NULL, node) &&
            g_environment_fixture.env->functionReturnTypes.length == 1U,
            "wide ITEM-equivalent same-AST ordinary callable rejection");
    original = environment_budget_function(0U);
    BUDGET_PREREQUISITE(environment_budget_function_valid(original, node, root, 1U, ZR_FALSE),
            "complete Root ordinary callable binding before the constrained call");
    memcpy(&beforeFunction, original, sizeof(beforeFunction));
    environment_budget_zero_heap(g_environment_fixture.mixed, ZR_SYNTAX_NAMESPACE_ITEM);
    before = environment_budget_snapshot();
    result = ZrParser_TypeEnvironment_RegisterFunctionEx(
            g_state, g_environment_fixture.env, identifier->name, &g_environment_fixture.intType,
            parameters, ZR_NULL, ZR_NULL, node);
    if (result || !environment_budget_unchanged(before) ||
        !environment_budget_function_unchanged(0U, original, &beforeFunction)) {
        TEST_FAIL_MESSAGE("Root and MixedSite ITEM-equivalent declarations must reject the same AST without mutation");
    }
}

static void test_callable_lookup_without_snapshot_never_returns_partial_overloads(void) {
    SZrAstNode *nodes[2];
    SZrIdentifier *identifiers[2];
    SZrFunctionTypeInfo *original[2];
    SZrFunctionTypeInfo beforeFunctions[2];
    SEnvironmentBudgetSnapshot before;
    SZrSyntaxContext root = ZrParser_SyntaxContext_Root();
    SZrSyntaxContext normalized;
    TZrBool result;
    BUDGET_PREREQUISITE(environment_budget_fixture_init(
            "fn environmentOverload():int { return 1; } "
            "fn environmentOverload(value:int):int { return value; }", 2U),
            "independent two-overload environment without a semantic snapshot");
    for (TZrSize index = 0U; index < 2U; index++) {
        SZrArray *parameters;
        nodes[index] = environment_budget_node(index, ZR_AST_FUNCTION_DECLARATION);
        identifiers[index] = environment_budget_identifier(nodes[index]);
        identifiers[index]->syntaxContext = index == 0U ? root : g_environment_fixture.mixed;
        BUDGET_PREREQUISITE(nodes[index]->data.functionDeclaration.params != ZR_NULL &&
                nodes[index]->data.functionDeclaration.params->count == index,
                "real zero/one-parameter overload AST signatures");
        parameters = environment_budget_parameters(index, index);
        BUDGET_PREREQUISITE(ZrParser_TypeEnvironment_RegisterFunctionEx(
                g_state, g_environment_fixture.env, identifiers[index]->name,
                &g_environment_fixture.intType, parameters, ZR_NULL, ZR_NULL, nodes[index]),
                "wide distinct ordinary overload registration");
        original[index] = environment_budget_function(index);
        BUDGET_PREREQUISITE(environment_budget_function_valid(original[index], nodes[index],
                identifiers[index]->syntaxContext, index, ZR_FALSE),
                "complete ordered Root/MixedSite ordinary overload records");
        memcpy(&beforeFunctions[index], original[index], sizeof(beforeFunctions[index]));
    }
    normalized = ZrParser_SyntaxContext_Normalize(g_environment_fixture.mixed, ZR_SYNTAX_NAMESPACE_ITEM);
    BUDGET_PREREQUISITE(nodes[0] != nodes[1] &&
            ZrCore_String_Equal(identifiers[0]->name, identifiers[1]->name) &&
            !ZrParser_SyntaxContext_Equal(root, g_environment_fixture.mixed) &&
            ZrParser_SyntaxContext_IsValid(normalized) && ZrParser_SyntaxContext_Equal(root, normalized) &&
            g_environment_fixture.env->functionReturnTypes.length == 2U,
            "different raw ITEM-equivalent Root-first/MixedSite-second overloads");
    BUDGET_PREREQUISITE(ZrParser_TypeEnvironment_LookupFunctionsWithContext(
            g_state, g_environment_fixture.env, identifiers[0]->name, root,
            &g_environment_fixture.results) && g_environment_fixture.results.length == 2U,
            "wide lookup returns the complete two-overload set");
    for (TZrSize index = 0U; index < 2U; index++) {
        SZrFunctionTypeInfo **entry = ZrCore_Array_Get(&g_environment_fixture.results, index);
        BUDGET_PREREQUISITE(entry != ZR_NULL && *entry == original[index],
                "wide overload result order matches Root then MixedSite");
    }
    ZrCore_Array_Free(g_state, &g_environment_fixture.results);
    ZrCore_Array_Construct(&g_environment_fixture.results);
    environment_budget_zero_heap(g_environment_fixture.mixed, ZR_SYNTAX_NAMESPACE_ITEM);
    before = environment_budget_snapshot();
    result = ZrParser_TypeEnvironment_LookupFunctionsWithContext(
            g_state, g_environment_fixture.env, identifiers[0]->name, root,
            &g_environment_fixture.results);
    if (result || !environment_budget_unchanged(before) ||
        !environment_budget_function_unchanged(0U, original[0], &beforeFunctions[0]) ||
        !environment_budget_function_unchanged(1U, original[1], &beforeFunctions[1]) ||
        g_environment_fixture.results.head != ZR_NULL || g_environment_fixture.results.length != 0U ||
        g_environment_fixture.results.capacity != 0U || g_environment_fixture.results.elementSize != 0U ||
        g_environment_fixture.results.isValid) {
        TEST_FAIL_MESSAGE("Callable lookup normalization failure must not return a partial overload set");
    }
}

#endif
