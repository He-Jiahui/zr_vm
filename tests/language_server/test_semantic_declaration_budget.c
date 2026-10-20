#include "unity.h"
#include "harness/runtime_support.h"
#include "semantic/analyzer/semantic_analyzer_internal.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/comptime_contract.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/syntax_context.h"

#include <stdint.h>
#include <string.h>

#define BUDGET_PREREQUISITE(condition, message) \
    TEST_ASSERT_TRUE_MESSAGE((condition), "Fixture prerequisite: " message)

typedef struct SDeclarationBudgetFixture {
    SZrParserComptimeBudget budget;
    SZrSyntaxContextTable *table;
    SZrSyntaxContext marked;
    SZrSyntaxContext mixed;
    SZrSemanticAnalyzer *analyzer;
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrIdentifier *identifier;
    SZrSymbol *display;
    SZrInferredType projection;
    TZrBool hasProjection;
    TZrSymbolId symbolId;
    TZrTypeId typeId;
    TZrOverloadSetId overloadId;
    SZrSyntaxContext publishedContext;
    SZrCanonicalParameterContract parameter;
    TZrTypeId returnTypeId;
    EZrCanonicalReceiverEffect receiverEffect;
    TZrUInt32 effectFlags;
} SDeclarationBudgetFixture;

typedef struct SDeclarationBudgetSnapshot {
    TZrSize symbols, types, canonical, definitions, overloads, members;
    TZrSize variables, named, names, references, syntaxTables;
    TZrTypeId nextType;
    TZrSymbolId nextSymbol;
    TZrOverloadSetId nextOverload;
    TZrLifetimeRegionId nextLifetime;
    TZrSemanticScopeId nextScope;
} SDeclarationBudgetSnapshot;

static SZrState *g_state;
static SDeclarationBudgetFixture g_fixture;

static void environment_budget_fixture_free(void);

static void fixture_free(void) {
    if (g_state != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_Free(g_state, g_fixture.analyzer);
        if (g_fixture.ast != ZR_NULL) {
            ZrParser_Ast_Free(g_state, g_fixture.ast);
        }
        if (g_fixture.hasProjection) {
            ZrParser_InferredType_Free(g_state, &g_fixture.projection);
        }
        /* The public caller-owned budget outlives every retained table owner. */
        ZrParser_SyntaxContextTable_Release(g_fixture.table);
    }
    memset(&g_fixture, 0, sizeof(g_fixture));
}

void setUp(void) {
    memset(&g_fixture, 0, sizeof(g_fixture));
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    BUDGET_PREREQUISITE(g_state != ZR_NULL, "runtime state creation");
}

void tearDown(void) {
    environment_budget_fixture_free();
    fixture_free();
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static TZrBool fixture_init(const char *source, TZrBool marked) {
    SZrParserComptimeBudgetLimits limits = {0};
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;
    SZrString *sourceName;
    SZrAstNodeArray *statements;
    SZrCompilerState *compiler;

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
    ZrParser_ComptimeBudget_Init(&g_fixture.budget, &limits);
    g_fixture.table = ZrParser_SyntaxContextTable_Create(g_state->global, &g_fixture.budget);
    g_fixture.analyzer = ZrLanguageServer_SemanticAnalyzer_New(g_state);
    if (g_fixture.table == ZR_NULL || g_fixture.analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    expansion.stableIdentity = UINT64_C(0x4D31425544474554);
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    if (!ZrParser_SyntaxContextTable_RegisterExpansion(
                g_fixture.table, &expansion, &expansionId)) {
        return ZR_FALSE;
    }
    g_fixture.marked = ZrParser_SyntaxContext_DefSite(g_fixture.table, expansionId);
    g_fixture.mixed = ZrParser_SyntaxContext_MixedSite(g_fixture.table, expansionId);
    if (!ZrParser_SyntaxContext_IsValid(g_fixture.marked) ||
        !ZrParser_SyntaxContext_IsValid(g_fixture.mixed)) {
        return ZR_FALSE;
    }
    sourceName = ZrCore_String_CreateFromNative(g_state, "semantic_declaration_budget.zr");
    if (sourceName == ZR_NULL) {
        return ZR_FALSE;
    }
    g_fixture.ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    if (g_fixture.ast == ZR_NULL || g_fixture.ast->type != ZR_AST_SCRIPT ||
        g_fixture.ast->data.script.statements == ZR_NULL) {
        return ZR_FALSE;
    }
    statements = g_fixture.ast->data.script.statements;
    if (statements->count != 1U || statements->nodes[0] == ZR_NULL) {
        return ZR_FALSE;
    }
    g_fixture.declaration = statements->nodes[0];
    if (g_fixture.declaration->type == ZR_AST_EXTERN_BLOCK) {
        SZrAstNodeArray *declarations = g_fixture.declaration->data.externBlock.declarations;
        if (declarations == ZR_NULL || declarations->count != 1U) {
            return ZR_FALSE;
        }
        g_fixture.declaration = declarations->nodes[0];
    }
    if (g_fixture.declaration == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (g_fixture.declaration->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            g_fixture.identifier = g_fixture.declaration->data.functionDeclaration.name;
            break;
        case ZR_AST_INTERFACE_DECLARATION:
            g_fixture.identifier = g_fixture.declaration->data.interfaceDeclaration.name;
            break;
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            g_fixture.identifier = g_fixture.declaration->data.externDelegateDeclaration.name;
            break;
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrAstNode *pattern = g_fixture.declaration->data.variableDeclaration.pattern;
            if (pattern != ZR_NULL && pattern->type == ZR_AST_IDENTIFIER_LITERAL) {
                g_fixture.identifier = &pattern->data.identifier;
            }
            break;
        }
        default:
            return ZR_FALSE;
    }
    if (g_fixture.identifier == ZR_NULL || g_fixture.identifier->name == ZR_NULL) {
        return ZR_FALSE;
    }
    g_fixture.identifier->syntaxContext = marked ? g_fixture.marked : ZrParser_SyntaxContext_Root();
    if (!ZrLanguageServer_SemanticAnalyzer_PrepareState(g_state, g_fixture.analyzer, g_fixture.ast)) {
        return ZR_FALSE;
    }
    compiler = g_fixture.analyzer->compilerState;
    return compiler != ZR_NULL && compiler->typeEnv != ZR_NULL &&
           g_fixture.analyzer->semanticContext != ZR_NULL &&
           compiler->semanticContext == g_fixture.analyzer->semanticContext &&
           compiler->typeEnv->semanticContext == g_fixture.analyzer->semanticContext;
}

static SDeclarationBudgetSnapshot snapshot(void) {
    SDeclarationBudgetSnapshot result = {0};
    SZrSemanticContext *semantic = g_fixture.analyzer->semanticContext;
    SZrTypeEnvironment *env = g_fixture.analyzer->compilerState->typeEnv;
    result.symbols = semantic->symbols.length;
    result.types = semantic->types.length;
    result.canonical = semantic->canonicalTypes.length;
    result.definitions = semantic->canonicalTypeDefinitions.length;
    result.overloads = semantic->overloadSets.length;
    for (TZrSize index = 0U; index < semantic->overloadSets.length; index++) {
        const SZrSemanticOverloadSetRecord *overload = ZrCore_Array_Get(&semantic->overloadSets, index);
        if (overload != ZR_NULL) {
            result.members += overload->members.length;
        }
    }
    result.variables = env->variableTypes.length;
    result.named = env->namedTypeBindings.length;
    result.names = env->typeNames.length;
    result.references = semantic->referenceFacts.length;
    result.syntaxTables = semantic->syntaxContextTables.length;
    result.nextType = semantic->nextTypeId;
    result.nextSymbol = semantic->nextSymbolId;
    result.nextOverload = semantic->nextOverloadSetId;
    result.nextLifetime = semantic->nextLifetimeRegionId;
    result.nextScope = semantic->nextScopeId;
    return result;
}

static TZrBool unchanged(SDeclarationBudgetSnapshot before, SDeclarationBudgetSnapshot after) {
    return before.symbols == after.symbols && before.types == after.types &&
           before.canonical == after.canonical && before.definitions == after.definitions &&
           before.overloads == after.overloads && before.members == after.members &&
           before.variables == after.variables && before.named == after.named &&
           before.names == after.names && before.references == after.references &&
           before.syntaxTables == after.syntaxTables && before.nextType == after.nextType &&
           before.nextSymbol == after.nextSymbol && before.nextOverload == after.nextOverload &&
           before.nextLifetime == after.nextLifetime && before.nextScope == after.nextScope;
}

static const SZrSemanticSymbolRecord *raw_record(EZrSemanticSymbolKind kind, SZrSyntaxContext context) {
    SZrSemanticContext *semantic = g_fixture.analyzer->semanticContext;
    for (TZrSize index = 0U; index < semantic->symbols.length; index++) {
        const SZrSemanticSymbolRecord *record = ZrCore_Array_Get(&semantic->symbols, index);
        if (record != ZR_NULL && record->astNode == g_fixture.declaration && record->kind == kind &&
            record->name != ZR_NULL && ZrCore_String_Equal(record->name, g_fixture.identifier->name) &&
            ZrParser_SyntaxContext_Equal(record->syntaxContext, context)) {
            return record;
        }
    }
    return ZR_NULL;
}

static const SZrNamedTypeBinding *raw_type_binding(SZrSyntaxContext context) {
    SZrTypeEnvironment *env = g_fixture.analyzer->compilerState->typeEnv;
    for (TZrSize index = 0U; index < env->namedTypeBindings.length; index++) {
        const SZrNamedTypeBinding *binding = ZrCore_Array_Get(&env->namedTypeBindings, index);
        if (binding != ZR_NULL && binding->name != ZR_NULL &&
            ZrCore_String_Equal(binding->name, g_fixture.identifier->name) &&
            ZrParser_SyntaxContext_Equal(binding->syntaxContext, context)) {
            return binding;
        }
    }
    return ZR_NULL;
}

static const SZrSemanticTypeRecord *type_record(TZrTypeId id) {
    SZrArray *types = &g_fixture.analyzer->semanticContext->types;
    for (TZrSize index = 0U; index < types->length; index++) {
        const SZrSemanticTypeRecord *record = ZrCore_Array_Get(types, index);
        if (record != ZR_NULL && record->id == id) {
            return record;
        }
    }
    return ZR_NULL;
}

static TZrBool overload_contains(TZrOverloadSetId overloadId, TZrSymbolId symbolId) {
    SZrArray *overloads = &g_fixture.analyzer->semanticContext->overloadSets;
    for (TZrSize index = 0U; index < overloads->length; index++) {
        const SZrSemanticOverloadSetRecord *overload = ZrCore_Array_Get(overloads, index);
        if (overload == ZR_NULL || overload->id != overloadId) {
            continue;
        }
        for (TZrSize memberIndex = 0U; memberIndex < overload->members.length; memberIndex++) {
            const TZrSymbolId *member = ZrCore_Array_Get((SZrArray *)&overload->members, memberIndex);
            if (member != ZR_NULL && *member == symbolId) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

static TZrBool add_display(EZrSymbolType kind, const SZrInferredType *type) {
    return ZrLanguageServer_SymbolTable_AddSymbolEx(
                   g_state, g_fixture.analyzer->symbolTable, kind, g_fixture.identifier->name,
                   g_fixture.declaration->location, (SZrInferredType *)type, ZR_ACCESS_PUBLIC,
                   g_fixture.declaration, &g_fixture.display) &&
           g_fixture.display != ZR_NULL &&
           g_fixture.display->semanticId == ZR_SEMANTIC_ID_INVALID &&
           g_fixture.display->semanticTypeId == ZR_SEMANTIC_ID_INVALID &&
           g_fixture.display->overloadSetId == ZR_SEMANTIC_ID_INVALID;
}

static TZrBool prepare_function(TZrBool marked) {
    SZrArray parameters;
    TZrBool registered;
    const SZrSemanticSymbolRecord *record;
    const SZrCanonicalTypeNode *canonical;
    const SZrCanonicalTypeNode *returnType;
    const SZrCanonicalParameterContract *parameter;
    if (!fixture_init("fn budgetEcho(value:int):int { return value; }", marked)) {
        return ZR_FALSE;
    }
    ZrParser_InferredType_Init(g_state, &g_fixture.projection, ZR_VALUE_TYPE_INT64);
    g_fixture.hasProjection = ZR_TRUE;
    ZrCore_Array_Init(g_state, &parameters, sizeof(SZrInferredType), 1U);
    ZrCore_Array_Push(g_state, &parameters, &g_fixture.projection);
    registered = ZrParser_TypeEnvironment_RegisterFunctionEx(
            g_state, g_fixture.analyzer->compilerState->typeEnv, g_fixture.identifier->name,
            &g_fixture.projection, &parameters, ZR_NULL, ZR_NULL, g_fixture.declaration);
    ZrCore_Array_Free(g_state, &parameters);
    g_fixture.publishedContext = g_fixture.identifier->syntaxContext;
    record = raw_record(ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, g_fixture.publishedContext);
    canonical = record != ZR_NULL ? ZrParser_CanonicalType_Find(
            g_fixture.analyzer->semanticContext, record->typeId) : ZR_NULL;
    if (!registered || record == ZR_NULL || canonical == ZR_NULL ||
        canonical->kind != ZR_CANONICAL_TYPE_FUNCTION ||
        canonical->data.function.parameterContracts.length != 1U ||
        record->overloadSetId == ZR_SEMANTIC_ID_INVALID ||
        !overload_contains(record->overloadSetId, record->id)) {
        return ZR_FALSE;
    }
    parameter = ZrCore_Array_Get((SZrArray *)&canonical->data.function.parameterContracts, 0U);
    returnType = ZrParser_CanonicalType_Find(
            g_fixture.analyzer->semanticContext, canonical->data.function.returnTypeId);
    if (parameter == ZR_NULL || parameter->typeId != canonical->data.function.returnTypeId ||
        returnType == ZR_NULL || returnType->kind != ZR_CANONICAL_TYPE_PRIMITIVE ||
        returnType->data.primitive.valueType != ZR_VALUE_TYPE_INT64) {
        return ZR_FALSE;
    }
    g_fixture.symbolId = record->id;
    g_fixture.typeId = record->typeId;
    g_fixture.overloadId = record->overloadSetId;
    g_fixture.parameter = *parameter;
    g_fixture.returnTypeId = canonical->data.function.returnTypeId;
    g_fixture.receiverEffect = canonical->data.function.receiverEffect;
    g_fixture.effectFlags = canonical->data.function.effectFlags;
    return add_display(ZR_SYMBOL_FUNCTION, &g_fixture.projection);
}

static TZrBool function_unchanged(void) {
    const SZrSemanticSymbolRecord *record = ZrParser_Semantic_FindSymbolById(
            g_fixture.analyzer->semanticContext, g_fixture.symbolId);
    const SZrCanonicalTypeNode *canonical = ZrParser_CanonicalType_Find(
            g_fixture.analyzer->semanticContext, g_fixture.typeId);
    const SZrCanonicalParameterContract *parameter;
    if (record == ZR_NULL || record->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
        record->name == ZR_NULL || !ZrCore_String_Equal(record->name, g_fixture.identifier->name) ||
        record->astNode != g_fixture.declaration || record->typeId != g_fixture.typeId ||
        record->overloadSetId != g_fixture.overloadId ||
        !ZrParser_SyntaxContext_Equal(record->syntaxContext, g_fixture.publishedContext) ||
        !overload_contains(g_fixture.overloadId, g_fixture.symbolId) ||
        canonical == ZR_NULL || canonical->kind != ZR_CANONICAL_TYPE_FUNCTION ||
        canonical->data.function.parameterContracts.length != 1U ||
        canonical->data.function.returnTypeId != g_fixture.returnTypeId ||
        canonical->data.function.receiverEffect != g_fixture.receiverEffect ||
        canonical->data.function.effectFlags != g_fixture.effectFlags) {
        return ZR_FALSE;
    }
    parameter = ZrCore_Array_Get((SZrArray *)&canonical->data.function.parameterContracts, 0U);
    return parameter != ZR_NULL && parameter->typeId == g_fixture.parameter.typeId &&
           parameter->passingForm == g_fixture.parameter.passingForm &&
           parameter->escapeUpperBound == g_fixture.parameter.escapeUpperBound &&
           parameter->entryInitialization == g_fixture.parameter.entryInitialization &&
           parameter->exitInitialization == g_fixture.parameter.exitInitialization &&
           parameter->acceptsTemporary == g_fixture.parameter.acceptsTemporary &&
           parameter->callSiteMarker == g_fixture.parameter.callSiteMarker;
}

static TZrBool exact_display(void) {
    return g_fixture.display->semanticId == g_fixture.symbolId &&
           g_fixture.display->semanticTypeId == g_fixture.typeId &&
           g_fixture.display->overloadSetId == g_fixture.overloadId;
}

static TZrBool empty_display(void) {
    return g_fixture.display->semanticId == ZR_SEMANTIC_ID_INVALID &&
           g_fixture.display->semanticTypeId == ZR_SEMANTIC_ID_INVALID &&
           g_fixture.display->overloadSetId == ZR_SEMANTIC_ID_INVALID;
}

static TZrUInt64 normalization_cost(SZrSyntaxContext context) {
    SZrSyntaxContext normalized;
    TZrUInt64 before;
    normalized = ZrParser_SyntaxContext_Normalize(context, ZR_SYNTAX_NAMESPACE_ITEM);
    BUDGET_PREREQUISITE(ZrParser_SyntaxContext_IsValid(normalized), "normalization warmup");
    before = g_fixture.budget.usage.heapBytes;
    normalized = ZrParser_SyntaxContext_Normalize(context, ZR_SYNTAX_NAMESPACE_ITEM);
    BUDGET_PREREQUISITE(ZrParser_SyntaxContext_IsValid(normalized), "normalization measurement");
    BUDGET_PREREQUISITE(g_fixture.budget.usage.heapBytes > before, "positive scratch heap delta");
    return g_fixture.budget.usage.heapBytes - before;
}

static void limit_normalizations(TZrUInt64 cost, TZrUInt64 count) {
    BUDGET_PREREQUISITE(cost != 0U && count != 0U &&
            cost <= (UINT64_MAX - g_fixture.budget.usage.heapBytes) / count,
            "normalization allowance does not overflow");
    g_fixture.budget.limits.heapBytes = g_fixture.budget.usage.heapBytes + cost * count;
}

static TZrBool register_function_display(void) {
    return ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(
            g_fixture.analyzer, g_fixture.display, ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            &g_fixture.projection, ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
}

static void test_marked_callable_budget_never_publishes_replacement(void) {
    SDeclarationBudgetSnapshot before;
    TZrBool result;
    TZrUInt64 cost;
    BUDGET_PREREQUISITE(prepare_function(ZR_TRUE), "marked full callable and overload member");
    cost = normalization_cost(g_fixture.marked);
    limit_normalizations(cost, 1U);
    before = snapshot();
    result = register_function_display();
    if (!unchanged(before, snapshot()) || !function_unchanged() ||
        !(result ? exact_display() : empty_display())) {
        TEST_FAIL_MESSAGE("Marked callable budget exhaustion must not publish a replacement symbol or member");
    }
}

static TZrBool prepare_nominal(TZrBool delegate) {
    const char *source = delegate
            ? "native extern(\"fixture\") { delegate BudgetDelegate(value:i32):void; }"
            : "interface BudgetInterface {}";
    if (!fixture_init(source, ZR_TRUE) ||
        raw_record(ZR_SEMANTIC_SYMBOL_KIND_TYPE, g_fixture.marked) != ZR_NULL ||
        raw_type_binding(g_fixture.marked) != ZR_NULL ||
        raw_record(ZR_SEMANTIC_SYMBOL_KIND_TYPE, ZrParser_SyntaxContext_Root()) != ZR_NULL ||
        raw_type_binding(ZrParser_SyntaxContext_Root()) != ZR_NULL) {
        return ZR_FALSE;
    }
    ZrParser_InferredType_InitFull(g_state, &g_fixture.projection,
            ZR_VALUE_TYPE_OBJECT, ZR_FALSE, g_fixture.identifier->name);
    g_fixture.hasProjection = ZR_TRUE;
    return add_display(delegate ? ZR_SYMBOL_FUNCTION : ZR_SYMBOL_INTERFACE,
                       delegate ? &g_fixture.projection : ZR_NULL);
}

static TZrBool register_nominal_display(TZrBool delegate) {
    return ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(
            g_fixture.analyzer, g_fixture.display, ZR_SEMANTIC_SYMBOL_KIND_TYPE,
            delegate ? &g_fixture.projection : ZR_NULL, ZR_SEMANTIC_TYPE_KIND_REFERENCE);
}

static TZrBool exact_nominal(void) {
    const SZrSemanticSymbolRecord *record = raw_record(ZR_SEMANTIC_SYMBOL_KIND_TYPE, g_fixture.marked);
    const SZrNamedTypeBinding *binding = raw_type_binding(g_fixture.marked);
    const SZrSemanticTypeRecord *type = type_record(g_fixture.display->semanticTypeId);
    const SZrCanonicalTypeNode *canonical = ZrParser_CanonicalType_Find(
            g_fixture.analyzer->semanticContext, g_fixture.display->semanticTypeId);
    return record != ZR_NULL && record->syntaxContext.table == g_fixture.marked.table &&
           record->syntaxContext.markId == g_fixture.marked.markId &&
           record->id == g_fixture.display->semanticId && record->typeId == g_fixture.display->semanticTypeId &&
           g_fixture.display->overloadSetId == ZR_SEMANTIC_ID_INVALID &&
           type != ZR_NULL && type->kind == ZR_SEMANTIC_TYPE_KIND_REFERENCE &&
           type->inferredType.baseType == ZR_VALUE_TYPE_OBJECT &&
           type->inferredType.typeName != ZR_NULL &&
           ZrCore_String_Equal(type->inferredType.typeName, g_fixture.identifier->name) &&
           canonical != ZR_NULL && canonical->kind == ZR_CANONICAL_TYPE_NOMINAL &&
           binding != ZR_NULL && binding->declarationNode == g_fixture.declaration &&
           binding->syntaxContext.table == g_fixture.marked.table &&
           binding->syntaxContext.markId == g_fixture.marked.markId &&
           binding->symbolId == record->id && binding->typeId == record->typeId;
}

static void nominal_budget_case(TZrBool decoy) {
    const char *reason = decoy
            ? "A preceding Root decoy must not make fresh nominal publication return false afterward"
            : "Two normalization allowances must not publish a fresh nominal and then return false";
    TZrBool allPassed = ZR_TRUE;
    for (TZrSize delegate = 0U; delegate < 2U; delegate++) {
        SDeclarationBudgetSnapshot before, after;
        TZrUInt64 cost;
        TZrSymbolId decoyId = ZR_SEMANTIC_ID_INVALID;
        TZrSymbolId symbolId;
        TZrTypeId typeId;
        TZrBool result;
        TZrBool passed;
        if (delegate != 0U) {
            fixture_free();
        }
        BUDGET_PREREQUISITE(prepare_nominal(delegate != 0U), "fresh marked interface/delegate presentation");
        if (decoy) {
            decoyId = ZrParser_Semantic_RegisterSymbolWithContext(
                    g_fixture.analyzer->semanticContext, g_fixture.identifier->name,
                    ZR_SEMANTIC_SYMBOL_KIND_TYPE, ZR_SEMANTIC_ID_INVALID, ZR_SEMANTIC_ID_INVALID,
                    g_fixture.declaration, g_fixture.declaration->location, ZrParser_SyntaxContext_Root());
            BUDGET_PREREQUISITE(decoyId != ZR_SEMANTIC_ID_INVALID &&
                    raw_record(ZR_SEMANTIC_SYMBOL_KIND_TYPE, ZrParser_SyntaxContext_Root()) != ZR_NULL,
                    "same-AST/name/kind Root decoy precedes fresh nominal");
        }
        cost = normalization_cost(g_fixture.marked);
        limit_normalizations(cost, decoy ? 3U : 2U);
        before = snapshot();
        result = register_nominal_display(delegate != 0U);
        after = snapshot();
        passed = result && exact_nominal() && after.symbols == before.symbols + 1U &&
                 after.named == before.named + 1U && after.names == before.names &&
                 raw_type_binding(ZrParser_SyntaxContext_Root()) == ZR_NULL;
        if (decoy) {
            const SZrSemanticSymbolRecord *record = ZrParser_Semantic_FindSymbolById(
                    g_fixture.analyzer->semanticContext, decoyId);
            passed = passed && record != ZR_NULL && record->astNode == g_fixture.declaration &&
                    record->typeId == ZR_SEMANTIC_ID_INVALID &&
                    ZrParser_SyntaxContext_Equal(record->syntaxContext, ZrParser_SyntaxContext_Root()) &&
                    g_fixture.display->semanticId != decoyId;
        } else {
            passed = passed && raw_record(
                    ZR_SEMANTIC_SYMBOL_KIND_TYPE, ZrParser_SyntaxContext_Root()) == ZR_NULL;
        }
        if (!passed) {
            allPassed = ZR_FALSE;
            continue;
        }
        symbolId = g_fixture.display->semanticId;
        typeId = g_fixture.display->semanticTypeId;
        g_fixture.budget.limits.heapBytes = UINT64_MAX;
        result = register_nominal_display(delegate != 0U);
        if (!result || g_fixture.display->semanticId != symbolId ||
            g_fixture.display->semanticTypeId != typeId ||
            !exact_nominal() || !unchanged(after, snapshot())) {
            allPassed = ZR_FALSE;
        }
    }
    if (!allPassed) {
        TEST_FAIL_MESSAGE(reason);
    }
}

static void test_fresh_nominal_two_normalizations_publish_successfully(void) {
    nominal_budget_case(ZR_FALSE);
}

static void test_wide_marked_callable_reuses_full_identity(void) {
    SDeclarationBudgetSnapshot before;
    TZrBool result;
    BUDGET_PREREQUISITE(prepare_function(ZR_TRUE), "wide marked full callable");
    (void)normalization_cost(g_fixture.marked);
    before = snapshot();
    result = register_function_display();
    if (!result || !exact_display() || !function_unchanged() || !unchanged(before, snapshot())) {
        TEST_FAIL_MESSAGE("Wide marked callable reuse must preserve its complete signature and member identity");
    }
}

static void test_root_callable_reuses_without_scratch_heap(void) {
    SDeclarationBudgetSnapshot before;
    TZrUInt64 heapBefore;
    TZrBool result;
    BUDGET_PREREQUISITE(prepare_function(ZR_FALSE), "Root full callable");
    heapBefore = g_fixture.budget.usage.heapBytes;
    g_fixture.budget.limits.heapBytes = heapBefore;
    before = snapshot();
    result = register_function_display();
    if (!result || !exact_display() || !function_unchanged() ||
        !unchanged(before, snapshot()) || g_fixture.budget.usage.heapBytes != heapBefore) {
        TEST_FAIL_MESSAGE("Root reuse must preserve its complete identity without scratch heap");
    }
}

static void test_equivalent_raw_context_lookup_failure_is_closed(void) {
    SDeclarationBudgetSnapshot before;
    SZrSyntaxContext normalized;
    TZrUInt64 cost;
    TZrBool result;
    BUDGET_PREREQUISITE(prepare_function(ZR_FALSE), "Root canonical callable for equivalent-context lookup");
    g_fixture.identifier->syntaxContext = g_fixture.mixed;
    normalized = ZrParser_SyntaxContext_Normalize(g_fixture.mixed, ZR_SYNTAX_NAMESPACE_ITEM);
    BUDGET_PREREQUISITE(!ZrParser_SyntaxContext_Equal(g_fixture.mixed, g_fixture.publishedContext) &&
            ZrParser_SyntaxContext_Equal(normalized, g_fixture.publishedContext),
            "different raw contexts with equal ITEM identity");
    cost = normalization_cost(g_fixture.mixed);
    limit_normalizations(cost, 1U);
    before = snapshot();
    result = register_function_display();
    if (result || !empty_display() || !function_unchanged() || !unchanged(before, snapshot())) {
        TEST_FAIL_MESSAGE("Equivalent-context normalization failure must not be treated as a miss");
    }
}

static void test_fresh_nominal_raw_match_precedes_root_decoy(void) {
    nominal_budget_case(ZR_TRUE);
}

static void test_variable_lookup_failure_precedes_type_publication(void) {
    SDeclarationBudgetSnapshot before;
    SZrSemanticContext *semantic;
    TZrSymbolId decoyId;
    const SZrSemanticSymbolRecord *decoy;
    SZrSemanticSymbolRecord decoyBefore;
    TZrBool unpublished = ZR_TRUE;
    TZrBool result;
    BUDGET_PREREQUISITE(fixture_init("var budgetValue:int=1;", ZR_TRUE), "parsed marked variable");
    semantic = g_fixture.analyzer->semanticContext;
    for (TZrSize index = 0U; index < semantic->types.length; index++) {
        const SZrSemanticTypeRecord *type = ZrCore_Array_Get(&semantic->types, index);
        if (type != ZR_NULL) {
            const SZrCanonicalTypeNode *canonical = ZrParser_CanonicalType_Find(semantic, type->id);
            if (type->baseType == ZR_VALUE_TYPE_UINT16 ||
                (canonical != ZR_NULL && canonical->kind == ZR_CANONICAL_TYPE_PRIMITIVE &&
                 canonical->data.primitive.valueType == ZR_VALUE_TYPE_UINT16)) {
                unpublished = ZR_FALSE;
            }
        }
    }
    BUDGET_PREREQUISITE(unpublished, "concrete uint16 projection is not already published");
    ZrParser_InferredType_Init(g_state, &g_fixture.projection, ZR_VALUE_TYPE_UINT16);
    g_fixture.hasProjection = ZR_TRUE;
    decoyId = ZrParser_Semantic_RegisterSymbolWithContext(
            semantic, g_fixture.identifier->name, ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
            ZR_SEMANTIC_ID_INVALID, ZR_SEMANTIC_ID_INVALID, g_fixture.declaration,
            g_fixture.declaration->location, ZrParser_SyntaxContext_Root());
    decoy = ZrParser_Semantic_FindSymbolById(semantic, decoyId);
    BUDGET_PREREQUISITE(decoyId != ZR_SEMANTIC_ID_INVALID && decoy != ZR_NULL,
            "same-AST/name VARIABLE Root decoy");
    memcpy(&decoyBefore, decoy, sizeof(decoyBefore));
    g_fixture.budget.limits.heapBytes = g_fixture.budget.usage.heapBytes;
    before = snapshot();
    result = ZrParser_TypeEnvironment_RegisterVariableExWithContext(
            g_state, g_fixture.analyzer->compilerState->typeEnv, g_fixture.identifier->name,
            &g_fixture.projection, g_fixture.declaration, g_fixture.declaration->location, g_fixture.marked);
    decoy = ZrParser_Semantic_FindSymbolById(semantic, decoyId);
    if (result || !unchanged(before, snapshot()) || decoy == ZR_NULL ||
        memcmp(&decoyBefore, decoy, sizeof(decoyBefore)) != 0 ||
        decoy->kind != ZR_SEMANTIC_SYMBOL_KIND_VARIABLE || decoy->astNode != g_fixture.declaration ||
        decoy->name == ZR_NULL || !ZrCore_String_Equal(decoy->name, g_fixture.identifier->name) ||
        decoy->typeId != ZR_SEMANTIC_ID_INVALID || decoy->overloadSetId != ZR_SEMANTIC_ID_INVALID ||
        !ZrParser_SyntaxContext_Equal(decoy->syntaxContext, ZrParser_SyntaxContext_Root())) {
        TEST_FAIL_MESSAGE("Variable declaration lookup failure must stop before inferred-type publication");
    }
}

#include "test_semantic_environment_budget_cases.h"

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_marked_callable_budget_never_publishes_replacement);
    RUN_TEST(test_fresh_nominal_two_normalizations_publish_successfully);
    RUN_TEST(test_wide_marked_callable_reuses_full_identity);
    RUN_TEST(test_root_callable_reuses_without_scratch_heap);
    RUN_TEST(test_equivalent_raw_context_lookup_failure_is_closed);
    RUN_TEST(test_fresh_nominal_raw_match_precedes_root_decoy);
    RUN_TEST(test_variable_lookup_failure_precedes_type_publication);
    RUN_TEST(test_marked_variable_reregistration_without_snapshot_preserves_binding);
    RUN_TEST(test_marked_callable_duplicate_without_snapshot_is_rejected);
    RUN_TEST(test_equivalent_item_callable_duplicate_without_snapshot_is_rejected);
    RUN_TEST(test_callable_lookup_without_snapshot_never_returns_partial_overloads);
    return UNITY_END();
}
