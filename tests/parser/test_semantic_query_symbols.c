#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"
#include "../../zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.h"

#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

static SZrState *g_state;

static const ZrLibGenericParameterDescriptor kSymbolGenericEchoParameters[] = {
        {"T", "The generic value passed through the native receiver method.", ZR_NULL, 0U},
};

static const ZrLibParameterDescriptor kSymbolGenericEchoValueParameters[] = {
        {"value", "T", "The value to return.", ZR_LIB_PARAMETER_PASSING_MODE_VALUE},
};

static const ZrLibMethodDescriptor kSymbolGenericEchoMethods[] = {
        {
                .name = "echo",
                .minArgumentCount = 1U,
                .maxArgumentCount = 1U,
                .callback = ZR_NULL,
                .returnTypeName = "T",
                .documentation = "Return a value through a native method generic.",
                .isStatic = ZR_FALSE,
                .parameters = kSymbolGenericEchoValueParameters,
                .parameterCount = ZR_ARRAY_COUNT(kSymbolGenericEchoValueParameters),
                .genericParameters = kSymbolGenericEchoParameters,
                .genericParameterCount = ZR_ARRAY_COUNT(kSymbolGenericEchoParameters),
        },
};

static const ZrLibTypeDescriptor kSymbolGenericEchoTypes[] = {
        {
                .name = "NativeEchoDevice",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                .methods = kSymbolGenericEchoMethods,
                .methodCount = ZR_ARRAY_COUNT(kSymbolGenericEchoMethods),
                .documentation = "A native receiver with one generic method.",
                .allowValueConstruction = ZR_TRUE,
                .allowBoxedConstruction = ZR_TRUE,
                .constructorSignature = "NativeEchoDevice()",
        },
};

static const ZrLibModuleDescriptor kSymbolGenericEchoModule = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "semantic.generic_identity",
        .types = kSymbolGenericEchoTypes,
        .typeCount = ZR_ARRAY_COUNT(kSymbolGenericEchoTypes),
        .documentation = "Native generic identity semantic-query fixture.",
        .moduleVersion = "1.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
};

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrFileRange symbol_range(TZrSize startOffset, TZrSize endOffset) {
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source = ZrCore_String_CreateFromNative(g_state, "semantic_query_symbols.zr");
    return range;
}

static void symbol_init_node(SZrAstNode *node, TZrSize startOffset, TZrSize endOffset) {
    memset(node, 0, sizeof(*node));
    node->type = ZR_AST_IDENTIFIER_LITERAL;
    node->location = symbol_range(startOffset, endOffset);
}

static SZrFileRange symbol_source_position(const TZrChar *source,
                                           SZrString *sourceName,
                                           const TZrChar *needle,
                                           TZrSize occurrence) {
    const TZrChar *cursor = source;
    const TZrChar *match;
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    do {
        cursor = strstr(cursor, needle);
        TEST_ASSERT_NOT_NULL(cursor);
        match = cursor;
        cursor++;
    } while (occurrence-- != 0U);
    range.source = sourceName;
    range.start.offset = (TZrSize)(match - source);
    range.end = range.start;
    return range;
}

static void symbol_release_compiler_function(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }
    if (cs->topLevelFunction != ZR_NULL && cs->topLevelFunction != cs->currentFunction) {
        ZrCore_Function_Free(g_state, cs->topLevelFunction);
        cs->topLevelFunction = ZR_NULL;
    }
    if (cs->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
    }
}

static void symbol_append_reference(SZrSemanticContext *context,
                                    SZrAstNode *node,
                                    EZrSemanticReferenceKind kind,
                                    TZrSymbolId symbolId,
                                    TZrTypeId typeId,
                                    SZrFileRange declarationRange,
                                    SZrFileRange definitionRange,
                                    TZrBool hasDefinitionRange,
                                    TZrBool isResolved,
                                    SZrString *name,
                                    SZrString *signatureDisplay) {
    SZrSemanticReferenceFact fact;

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.declarationRange = declarationRange;
    fact.definitionRange = definitionRange;
    fact.hasDefinitionRange = hasDefinitionRange;
    fact.kind = kind;
    fact.symbolId = symbolId;
    fact.typeId = typeId;
    fact.isResolved = isResolved;
    fact.name = name;
    fact.signatureDisplay = signatureDisplay;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));
}

static void test_symbol_at_projects_resolved_reference_identity(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode declarationNode;
    SZrAstNode readNode;
    SZrParserSemanticSymbolQuery first;
    SZrParserSemanticSymbolQuery second;
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "counter");
    SZrString *signature = ZrCore_String_CreateFromNative(g_state, "counter: int");

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_NOT_NULL(signature);
    symbol_init_node(&declarationNode, 0U, 6U);
    symbol_init_node(&readNode, 24U, 30U);
    symbol_append_reference(context,
                            &declarationNode,
                            ZR_SEMANTIC_REFERENCE_DECLARATION,
                            701U,
                            91U,
                            declarationNode.location,
                            declarationNode.location,
                            ZR_TRUE,
                            ZR_TRUE,
                            name,
                            signature);
    symbol_append_reference(context,
                            &readNode,
                            ZR_SEMANTIC_REFERENCE_READ,
                            701U,
                            91U,
                            declarationNode.location,
                            declarationNode.location,
                            ZR_TRUE,
                            ZR_TRUE,
                            name,
                            signature);

    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_SymbolAt(
            context, symbol_range(26U, 26U), ZR_NULL, &first));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_SymbolAt(
            context, symbol_range(26U, 26U), ZR_NULL, &second));
    TEST_ASSERT_EQUAL_UINT32(701U, first.symbolId);
    TEST_ASSERT_EQUAL_UINT32(91U, first.typeId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, first.ownerSymbolId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_READ, first.role);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)first.declarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)first.definitionRange.start.offset);
    TEST_ASSERT_NOT_NULL(first.displayName);
    TEST_ASSERT_NOT_NULL(first.signatureDisplay);
    TEST_ASSERT_EQUAL_STRING("counter", ZrCore_String_GetNativeString(first.displayName));
    TEST_ASSERT_EQUAL_STRING("counter: int", ZrCore_String_GetNativeString(first.signatureDisplay));
    TEST_ASSERT_EQUAL_PTR(first.displayName, second.displayName);
    TEST_ASSERT_EQUAL_PTR(first.signatureDisplay, second.signatureDisplay);

    ZrParser_SemanticContext_Free(context);
}

static void test_symbol_at_fails_closed_for_unresolved_reference(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode node;
    SZrParserSemanticSymbolQuery query;
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "missing");

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(name);
    symbol_init_node(&node, 8U, 14U);
    symbol_append_reference(context,
                            &node,
                            ZR_SEMANTIC_REFERENCE_READ,
                            ZR_SEMANTIC_ID_INVALID,
                            ZR_SEMANTIC_ID_INVALID,
                            symbol_range(8U, 14U),
                            symbol_range(8U, 14U),
                            ZR_FALSE,
                            ZR_FALSE,
                            name,
                            ZR_NULL);

    memset(&query, 0x7f, sizeof(query));
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_SymbolAt(
            context, symbol_range(10U, 10U), ZR_NULL, &query));
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.symbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.typeId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.ownerSymbolId);
    TEST_ASSERT_NULL(query.displayName);
    TEST_ASSERT_NULL(query.signatureDisplay);

    ZrParser_SemanticContext_Free(context);
}

static TZrSemanticScopeId symbol_publish_scope(SZrSemanticContext *context,
                                               TZrSemanticScopeId parentScopeId,
                                               TZrSize startOffset,
                                               TZrSize endOffset,
                                               TZrSymbolId ownerSymbolId,
                                               TZrBool isStaticContext) {
    SZrSemanticScopeFact fact;

    memset(&fact, 0, sizeof(fact));
    fact.parentScopeId = parentScopeId;
    fact.range = symbol_range(startOffset, endOffset);
    fact.ownerSymbolId = ownerSymbolId;
    fact.isStaticContext = isStaticContext;
    return ZrParser_Semantic_PublishScopeFact(context, &fact);
}

static void symbol_register(SZrSemanticContext *context,
                            TZrSymbolId symbolId,
                            TZrNativeString name,
                            EZrSemanticSymbolKind kind,
                            TZrTypeId typeId,
                            TZrOverloadSetId overloadSetId,
                            TZrSize declarationStart,
                            TZrSize declarationEnd) {
    SZrString *symbolName = ZrCore_String_CreateFromNative(g_state, name);

    TEST_ASSERT_NOT_NULL(symbolName);
    TEST_ASSERT_EQUAL_UINT32(
            symbolId,
            ZrParser_Semantic_RegisterSymbolWithId(context,
                                                    symbolId,
                                                    symbolName,
                                                    kind,
                                                    typeId,
                                                    overloadSetId,
                                                    ZR_NULL,
                                                    symbol_range(
                                                            declarationStart,
                                                            declarationEnd)));
}

static void symbol_publish_visible(SZrSemanticContext *context,
                                   TZrSemanticScopeId scopeId,
                                   TZrSymbolId symbolId,
                                   TZrSymbolId ownerSymbolId,
                                   TZrUInt32 declarationOrder,
                                   TZrSize declarationStart,
                                   TZrSize declarationEnd,
                                   TZrBool isHoisted,
                                   TZrBool isAccessible,
                                   TZrBool isReceiverMember,
                                   TZrBool isStatic,
                                   TZrBool isImport,
                                   TZrBool isAlias,
                                   TZrBool isGenericParameter,
                                   SZrString *signatureDisplay) {
    SZrSemanticVisibleSymbolFact fact;

    memset(&fact, 0, sizeof(fact));
    fact.scopeId = scopeId;
    fact.symbolId = symbolId;
    fact.ownerSymbolId = ownerSymbolId;
    fact.access = isAccessible ? ZR_ACCESS_PUBLIC : ZR_ACCESS_PRIVATE;
    fact.declarationOrder = declarationOrder;
    fact.declarationRange = symbol_range(declarationStart, declarationEnd);
    fact.definitionRange = fact.declarationRange;
    fact.hasDefinitionRange = ZR_TRUE;
    fact.isHoisted = isHoisted;
    fact.isAccessible = isAccessible;
    fact.isReceiverMember = isReceiverMember;
    fact.isStatic = isStatic;
    fact.isImport = isImport;
    fact.isAlias = isAlias;
    fact.isGenericParameter = isGenericParameter;
    fact.signatureDisplay = signatureDisplay;
    TEST_ASSERT_TRUE(ZrParser_Semantic_PublishVisibleSymbolFact(context, &fact));
}

static const SZrParserSemanticSymbolQuery *symbol_visible_at(
        const SZrArray *symbols,
        TZrSize index) {
    return (const SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(
            (SZrArray *)symbols, index);
}

static TZrSize symbol_count_visible_name(const SZrArray *symbols, const TZrChar *name) {
    TZrSize count = 0U;

    if (symbols == ZR_NULL || name == ZR_NULL) {
        return 0U;
    }
    for (TZrSize index = 0U; index < symbols->length; index++) {
        const SZrParserSemanticSymbolQuery *symbol = symbol_visible_at(symbols, index);

        if (symbol != ZR_NULL && symbol->displayName != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(symbol->displayName), name) == 0) {
            count++;
        }
    }
    return count;
}

static const SZrParserSemanticSymbolQuery *symbol_find_visible_name(
        const SZrArray *symbols,
        const TZrChar *name) {
    if (symbols == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < symbols->length; index++) {
        const SZrParserSemanticSymbolQuery *symbol = symbol_visible_at(symbols, index);

        if (symbol != ZR_NULL && symbol->displayName != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(symbol->displayName), name) == 0) {
            return symbol;
        }
    }
    return ZR_NULL;
}

static const SZrSemanticSymbolRecord *symbol_find_registered_node(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    if (context == ZR_NULL || node == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);

        if (symbol != ZR_NULL && symbol->astNode == node) {
            return symbol;
        }
    }
    return ZR_NULL;
}

static const SZrSemanticVisibleSymbolFact *symbol_find_visible_fact(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    if (context == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < context->visibleSymbolFacts.length; index++) {
        const SZrSemanticVisibleSymbolFact *fact =
                (const SZrSemanticVisibleSymbolFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->visibleSymbolFacts, index);

        if (fact != ZR_NULL && fact->symbolId == symbolId) {
            return fact;
        }
    }
    return ZR_NULL;
}

static void test_visible_symbols_uses_scope_facts_for_shadowing_and_options(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    TZrSemanticScopeId moduleScope;
    TZrSemanticScopeId functionScope;
    TZrSemanticScopeId blockScope;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    SZrString *sumSignature = ZrCore_String_CreateFromNative(g_state, "fn sum(): int");

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(sumSignature);
    moduleScope = symbol_publish_scope(context,
                                       ZR_SEMANTIC_ID_INVALID,
                                       0U,
                                       240U,
                                       ZR_SEMANTIC_ID_INVALID,
                                       ZR_FALSE);
    functionScope = symbol_publish_scope(context, moduleScope, 20U, 180U, 900U, ZR_FALSE);
    blockScope = symbol_publish_scope(context, functionScope, 60U, 140U, 900U, ZR_FALSE);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, moduleScope);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, functionScope);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, blockScope);

    symbol_register(context,
                    100U,
                    "value",
                    ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                    10U,
                    ZR_SEMANTIC_ID_INVALID,
                    5U,
                    10U);
    symbol_register(context,
                    200U,
                    "value",
                    ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                    11U,
                    ZR_SEMANTIC_ID_INVALID,
                    30U,
                    35U);
    symbol_register(context,
                    210U,
                    "value",
                    ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                    19U,
                    ZR_SEMANTIC_ID_INVALID,
                    70U,
                    75U);
    symbol_register(context,
                    250U,
                    "T",
                    ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                    12U,
                    ZR_SEMANTIC_ID_INVALID,
                    22U,
                    23U);
    symbol_register(context,
                    300U,
                    "sum",
                    ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                    13U,
                    700U,
                    36U,
                    39U);
    symbol_register(context,
                    301U,
                    "sum",
                    ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                    14U,
                    700U,
                    40U,
                    43U);
    symbol_register(context,
                    400U,
                    "count",
                    ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                    15U,
                    ZR_SEMANTIC_ID_INVALID,
                    44U,
                    49U);
    symbol_register(context,
                    500U,
                    "Widget",
                    ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                    16U,
                    ZR_SEMANTIC_ID_INVALID,
                    12U,
                    18U);
    symbol_register(context,
                    600U,
                    "hidden",
                    ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                    17U,
                    ZR_SEMANTIC_ID_INVALID,
                    14U,
                    19U);
    symbol_register(context,
                    700U,
                    "later",
                    ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                    18U,
                    ZR_SEMANTIC_ID_INVALID,
                    120U,
                    125U);

    symbol_publish_visible(context,
                           moduleScope,
                           100U,
                           ZR_SEMANTIC_ID_INVALID,
                           1U,
                           5U,
                           10U,
                           ZR_TRUE,
                           ZR_TRUE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_NULL);
    symbol_publish_visible(context,
                           functionScope,
                           200U,
                           ZR_SEMANTIC_ID_INVALID,
                           1U,
                           30U,
                           35U,
                           ZR_FALSE,
                           ZR_TRUE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_NULL);
    symbol_publish_visible(context,
                           functionScope,
                           250U,
                           900U,
                           1U,
                           22U,
                           23U,
                           ZR_TRUE,
                           ZR_TRUE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_TRUE,
                           ZR_NULL);
    symbol_publish_visible(context,
                           functionScope,
                           300U,
                           900U,
                           2U,
                           36U,
                           39U,
                           ZR_TRUE,
                           ZR_TRUE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           sumSignature);
    symbol_publish_visible(context,
                           functionScope,
                           301U,
                           900U,
                           2U,
                           40U,
                           43U,
                           ZR_TRUE,
                           ZR_TRUE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           sumSignature);
    symbol_publish_visible(context,
                           functionScope,
                           400U,
                           900U,
                           3U,
                           44U,
                           49U,
                           ZR_TRUE,
                           ZR_TRUE,
                           ZR_TRUE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_NULL);
    symbol_publish_visible(context,
                           moduleScope,
                           500U,
                           ZR_SEMANTIC_ID_INVALID,
                           2U,
                           12U,
                           18U,
                           ZR_TRUE,
                           ZR_TRUE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_TRUE,
                           ZR_TRUE,
                           ZR_FALSE,
                           ZR_NULL);
    symbol_publish_visible(context,
                           moduleScope,
                           600U,
                           901U,
                           3U,
                           14U,
                           19U,
                           ZR_TRUE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_NULL);
    symbol_publish_visible(context,
                           blockScope,
                           700U,
                           ZR_SEMANTIC_ID_INVALID,
                           2U,
                           120U,
                           125U,
                           ZR_FALSE,
                           ZR_TRUE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_NULL);
    symbol_publish_visible(context,
                           blockScope,
                           210U,
                           ZR_SEMANTIC_ID_INVALID,
                           1U,
                           70U,
                           75U,
                           ZR_FALSE,
                           ZR_TRUE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_NULL);

    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            context, symbol_range(90U, 90U), ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(4U, (TZrUInt32)symbols.length);
    TEST_ASSERT_EQUAL_UINT32(210U, symbol_visible_at(&symbols, 0U)->symbolId);
    TEST_ASSERT_EQUAL_UINT32(250U, symbol_visible_at(&symbols, 1U)->symbolId);
    TEST_ASSERT_EQUAL_UINT32(300U, symbol_visible_at(&symbols, 2U)->symbolId);
    TEST_ASSERT_EQUAL_UINT32(301U, symbol_visible_at(&symbols, 3U)->symbolId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                          symbol_visible_at(&symbols, 0U)->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                          symbol_visible_at(&symbols, 1U)->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                          symbol_visible_at(&symbols, 2U)->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                          symbol_visible_at(&symbols, 3U)->kind);
    TEST_ASSERT_EQUAL_STRING(
            "sum", ZrCore_String_GetNativeString(symbol_visible_at(&symbols, 2U)->displayName));
    TEST_ASSERT_EQUAL_STRING("fn sum(): int",
                             ZrCore_String_GetNativeString(
                                     symbol_visible_at(&symbols, 2U)->signatureDisplay));

    options.includeReceiverMembers = ZR_TRUE;
    options.includeImports = ZR_TRUE;
    options.includeInaccessible = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            context, symbol_range(90U, 90U), ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(7U, (TZrUInt32)symbols.length);
    TEST_ASSERT_EQUAL_UINT32(400U, symbol_visible_at(&symbols, 4U)->symbolId);
    TEST_ASSERT_EQUAL_UINT32(500U, symbol_visible_at(&symbols, 5U)->symbolId);
    TEST_ASSERT_EQUAL_UINT32(600U, symbol_visible_at(&symbols, 6U)->symbolId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                          symbol_visible_at(&symbols, 4U)->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                          symbol_visible_at(&symbols, 5U)->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                          symbol_visible_at(&symbols, 6U)->kind);

    ZrCore_Array_Free(g_state, &symbols);
    ZrParser_SemanticContext_Free(context);
}

static void test_visible_symbols_excludes_instance_members_from_static_scope(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    TZrSemanticScopeId moduleScope;
    TZrSemanticScopeId staticScope;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;

    TEST_ASSERT_NOT_NULL(context);
    moduleScope = symbol_publish_scope(context,
                                       ZR_SEMANTIC_ID_INVALID,
                                       0U,
                                       100U,
                                       ZR_SEMANTIC_ID_INVALID,
                                       ZR_FALSE);
    staticScope = symbol_publish_scope(context, moduleScope, 20U, 80U, 901U, ZR_TRUE);
    symbol_register(context,
                    801U,
                    "factory",
                    ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                    20U,
                    ZR_SEMANTIC_ID_INVALID,
                    25U,
                    32U);
    symbol_register(context,
                    802U,
                    "instanceOnly",
                    ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                    21U,
                    ZR_SEMANTIC_ID_INVALID,
                    33U,
                    45U);
    symbol_publish_visible(context,
                           staticScope,
                           801U,
                           901U,
                           1U,
                           25U,
                           32U,
                           ZR_TRUE,
                           ZR_TRUE,
                           ZR_TRUE,
                           ZR_TRUE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_NULL);
    symbol_publish_visible(context,
                           staticScope,
                           802U,
                           901U,
                           2U,
                           33U,
                           45U,
                           ZR_TRUE,
                           ZR_TRUE,
                           ZR_TRUE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_FALSE,
                           ZR_NULL);

    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    options.includeReceiverMembers = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            context, symbol_range(50U, 50U), ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbols.length);
    TEST_ASSERT_EQUAL_UINT32(801U, symbol_visible_at(&symbols, 0U)->symbolId);

    ZrCore_Array_Free(g_state, &symbols);
    ZrParser_SemanticContext_Free(context);
}

static void test_visible_symbols_project_compiled_source_scope_facts(void) {
    const TZrChar *source =
            "fn choose(seed: int): int {\n"
            "    var value: int = seed;\n"
            "    {\n"
            "        var value: int = 1;\n"
            "        return value;\n"
            "    }\n"
            "}\n"
            "fn use(): int {\n"
            "    var value: int = 1;\n"
            "    return value;\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    SZrFileRange position;
    const SZrParserSemanticSymbolQuery *choose;

    sourceName = ZrCore_String_CreateFromNative(g_state, "visible_symbols_source.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    position = symbol_source_position(source, sourceName, "value", 2U);
    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "value"));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "seed"));

    ZrCore_Array_Free(g_state, &symbols);
    position = symbol_source_position(source, sourceName, "value", 4U);
    ZrCore_Array_Construct(&symbols);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    choose = symbol_find_visible_name(&symbols, "choose");
    TEST_ASSERT_NOT_NULL(choose);
    TEST_ASSERT_NOT_NULL(choose->signatureDisplay);
    TEST_ASSERT_EQUAL_STRING(
            "choose(seed: int): int",
            ZrCore_String_GetNativeString(choose->signatureDisplay));

    ZrCore_Array_Free(g_state, &symbols);
    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_visible_symbols_projects_extern_block_declarations(void) {
    const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "    fn NativeAdd(lhs: i32, rhs: i32): i32;\n"
            "    delegate Callback(value: i32): void;\n"
            "    struct NativePoint { var x: i32; }\n"
            "    enum Mode { Off }\n"
            "}\n"
            "fn use(): i32 { return NativeAdd(1, 2); }\n";
    SZrSemanticContext *context;
    SZrString *sourceName;
    SZrString *nativeAddName;
    SZrString *callbackName;
    SZrString *pointName;
    SZrString *modeName;
    SZrString *useName;
    SZrString *nativeAddSignature;
    SZrAstNode *ast;
    SZrAstNode *externBlock;
    SZrAstNode *useDeclaration;
    SZrAstNode *nativeAddDeclaration;
    SZrAstNode *callbackDeclaration;
    SZrAstNode *pointDeclaration;
    SZrAstNode *modeDeclaration;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    SZrFileRange position;
    const SZrParserSemanticSymbolQuery *nativeAdd;
    const SZrParserSemanticSymbolQuery *callback;
    const SZrParserSemanticSymbolQuery *point;
    const SZrParserSemanticSymbolQuery *mode;

    sourceName = ZrCore_String_CreateFromNative(g_state, "visible_symbols_extern_block.zr");
    nativeAddName = ZrCore_String_CreateFromNative(g_state, "NativeAdd");
    callbackName = ZrCore_String_CreateFromNative(g_state, "Callback");
    pointName = ZrCore_String_CreateFromNative(g_state, "NativePoint");
    modeName = ZrCore_String_CreateFromNative(g_state, "Mode");
    useName = ZrCore_String_CreateFromNative(g_state, "use");
    nativeAddSignature = ZrCore_String_CreateFromNative(
            g_state, "NativeAdd(lhs: i32, rhs: i32): i32");
    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_NOT_NULL(nativeAddName);
    TEST_ASSERT_NOT_NULL(callbackName);
    TEST_ASSERT_NOT_NULL(pointName);
    TEST_ASSERT_NOT_NULL(modeName);
    TEST_ASSERT_NOT_NULL(useName);
    TEST_ASSERT_NOT_NULL(nativeAddSignature);

    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(2U, (TZrUInt32)ast->data.script.statements->count);
    externBlock = ast->data.script.statements->nodes[0];
    useDeclaration = ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(externBlock);
    TEST_ASSERT_NOT_NULL(useDeclaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_BLOCK, externBlock->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, useDeclaration->type);
    TEST_ASSERT_NOT_NULL(externBlock->data.externBlock.declarations);
    TEST_ASSERT_EQUAL_UINT32(
            4U, (TZrUInt32)externBlock->data.externBlock.declarations->count);
    nativeAddDeclaration = externBlock->data.externBlock.declarations->nodes[0];
    callbackDeclaration = externBlock->data.externBlock.declarations->nodes[1];
    pointDeclaration = externBlock->data.externBlock.declarations->nodes[2];
    modeDeclaration = externBlock->data.externBlock.declarations->nodes[3];
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_FUNCTION_DECLARATION, nativeAddDeclaration->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_DELEGATE_DECLARATION, callbackDeclaration->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, pointDeclaration->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ENUM_DECLARATION, modeDeclaration->type);

    context = ZrParser_SemanticContext_New(g_state);
    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_EQUAL_UINT32(
            1201U,
            ZrParser_Semantic_RegisterSymbolWithId(
                    context,
                    1201U,
                    nativeAddName,
                    ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                    101U,
                    ZR_SEMANTIC_ID_INVALID,
                    nativeAddDeclaration,
                    nativeAddDeclaration->location));
    TEST_ASSERT_EQUAL_UINT32(
            1202U,
            ZrParser_Semantic_RegisterSymbolWithId(
                    context,
                    1202U,
                    callbackName,
                    ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                    102U,
                    ZR_SEMANTIC_ID_INVALID,
                    callbackDeclaration,
                    callbackDeclaration->location));
    TEST_ASSERT_EQUAL_UINT32(
            1203U,
            ZrParser_Semantic_RegisterSymbolWithId(
                    context,
                    1203U,
                    pointName,
                    ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                    103U,
                    ZR_SEMANTIC_ID_INVALID,
                    pointDeclaration,
                    pointDeclaration->location));
    TEST_ASSERT_EQUAL_UINT32(
            1204U,
            ZrParser_Semantic_RegisterSymbolWithId(
                    context,
                    1204U,
                    modeName,
                    ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                    104U,
                    ZR_SEMANTIC_ID_INVALID,
                    modeDeclaration,
                    modeDeclaration->location));
    TEST_ASSERT_EQUAL_UINT32(
            1205U,
            ZrParser_Semantic_RegisterSymbolWithId(
                    context,
                    1205U,
                    useName,
                    ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                    105U,
                    ZR_SEMANTIC_ID_INVALID,
                    useDeclaration,
                    useDeclaration->location));
    symbol_append_reference(context,
                            nativeAddDeclaration,
                            ZR_SEMANTIC_REFERENCE_DECLARATION,
                            1201U,
                            101U,
                            nativeAddDeclaration->location,
                            nativeAddDeclaration->location,
                            ZR_TRUE,
                            ZR_TRUE,
                            nativeAddName,
                            nativeAddSignature);
    symbol_append_reference(context,
                            callbackDeclaration,
                            ZR_SEMANTIC_REFERENCE_DECLARATION,
                            1202U,
                            102U,
                            callbackDeclaration->location,
                            callbackDeclaration->location,
                            ZR_TRUE,
                            ZR_TRUE,
                            callbackName,
                            ZR_NULL);
    symbol_append_reference(context,
                            pointDeclaration,
                            ZR_SEMANTIC_REFERENCE_DECLARATION,
                            1203U,
                            103U,
                            pointDeclaration->location,
                            pointDeclaration->location,
                            ZR_TRUE,
                            ZR_TRUE,
                            pointName,
                            ZR_NULL);
    symbol_append_reference(context,
                            modeDeclaration,
                            ZR_SEMANTIC_REFERENCE_DECLARATION,
                            1204U,
                            104U,
                            modeDeclaration->location,
                            modeDeclaration->location,
                            ZR_TRUE,
                            ZR_TRUE,
                            modeName,
                            ZR_NULL);
    symbol_append_reference(context,
                            useDeclaration,
                            ZR_SEMANTIC_REFERENCE_DECLARATION,
                            1205U,
                            105U,
                            useDeclaration->location,
                            useDeclaration->location,
                            ZR_TRUE,
                            ZR_TRUE,
                            useName,
                            ZR_NULL);
    TEST_ASSERT_TRUE(ZrParser_Semantic_BuildSourceScopeFacts(context, ast));

    position = symbol_source_position(source, sourceName, "return", 0U);
    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            context, position, ZR_NULL, &options, &symbols));
    nativeAdd = symbol_find_visible_name(&symbols, "NativeAdd");
    callback = symbol_find_visible_name(&symbols, "Callback");
    point = symbol_find_visible_name(&symbols, "NativePoint");
    mode = symbol_find_visible_name(&symbols, "Mode");
    TEST_ASSERT_NOT_NULL(nativeAdd);
    TEST_ASSERT_NOT_NULL(callback);
    TEST_ASSERT_NOT_NULL(point);
    TEST_ASSERT_NOT_NULL(mode);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, nativeAdd->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_TYPE, callback->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_TYPE, point->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_TYPE, mode->kind);
    TEST_ASSERT_NOT_NULL(nativeAdd->signatureDisplay);
    TEST_ASSERT_EQUAL_STRING(
            "NativeAdd(lhs: i32, rhs: i32): i32",
            ZrCore_String_GetNativeString(nativeAdd->signatureDisplay));

    ZrCore_Array_Free(g_state, &symbols);
    ZrParser_SemanticContext_Free(context);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_visible_symbols_does_not_leak_for_initializer(void) {
    const TZrChar *source =
            "fn loop_scope(seed: int): int {\n"
            "    for (var step: int = 0; step < seed; step = step + 1) {\n"
            "    }\n"
            "    var after: int = seed;\n"
            "    return after;\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    SZrFileRange position;

    sourceName = ZrCore_String_CreateFromNative(g_state, "visible_symbols_loop.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    position = symbol_source_position(source, sourceName, "after", 0U);
    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "step"));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "seed"));

    ZrCore_Array_Free(g_state, &symbols);
    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_visible_symbols_projects_source_type_declarations(void) {
    const TZrChar *source =
            "struct Point { var x: int; }\n"
            "class Meter { }\n"
            "interface Readable { fn read(): int; }\n"
            "fn probe(): int { return 0; }\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    SZrFileRange position;

    sourceName = ZrCore_String_CreateFromNative(g_state, "visible_symbols_types.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    position = symbol_source_position(source, sourceName, "probe", 0U);
    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "Point"));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "Meter"));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "Readable"));

    ZrCore_Array_Free(g_state, &symbols);
    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_visible_symbols_projects_source_type_generic_parameter(void) {
    const TZrChar *source =
            "struct Box<T> { }\n"
            "class Crate<U> { }\n"
            "interface Readable<V> { }\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    const SZrParserSemanticSymbolQuery *box;
    const SZrParserSemanticSymbolQuery *crate;
    const SZrParserSemanticSymbolQuery *parameter;
    const SZrCanonicalTypeNode *parameterType;
    const SZrParserSemanticSymbolQuery *readable;
    SZrFileRange position;

    sourceName = ZrCore_String_CreateFromNative(g_state, "visible_symbols_generic_type.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3U, (TZrUInt32)ast->data.script.statements->count);
    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.structDeclaration.generic);
    TEST_ASSERT_NOT_NULL(declaration->data.structDeclaration.generic->params);
    TEST_ASSERT_EQUAL_UINT32(
            1U,
            (TZrUInt32)declaration->data.structDeclaration.generic->params->count);
    TEST_ASSERT_NOT_NULL(declaration->data.structDeclaration.generic->params->nodes[0]);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_PARAMETER,
            declaration->data.structDeclaration.generic->params->nodes[0]->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    position = symbol_source_position(source, sourceName, "Box", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "T"));

    position = symbol_source_position(source, sourceName, "T", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "T"));
    box = symbol_find_visible_name(&symbols, "Box");
    parameter = symbol_find_visible_name(&symbols, "T");
    TEST_ASSERT_NOT_NULL(box);
    TEST_ASSERT_NOT_NULL(parameter);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, parameter->symbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, parameter->typeId);
    TEST_ASSERT_EQUAL_UINT32(box->symbolId, parameter->ownerSymbolId);
    parameterType = ZrParser_CanonicalType_Find(cs.semanticContext, parameter->typeId);
    TEST_ASSERT_NOT_NULL(parameterType);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_PARAMETER, parameterType->kind);

    position = symbol_source_position(source, sourceName, "U", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "T"));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "U"));
    crate = symbol_find_visible_name(&symbols, "Crate");
    parameter = symbol_find_visible_name(&symbols, "U");
    TEST_ASSERT_NOT_NULL(crate);
    TEST_ASSERT_NOT_NULL(parameter);
    TEST_ASSERT_EQUAL_UINT32(crate->symbolId, parameter->ownerSymbolId);

    position = symbol_source_position(source, sourceName, "V", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "U"));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "V"));
    readable = symbol_find_visible_name(&symbols, "Readable");
    parameter = symbol_find_visible_name(&symbols, "V");
    TEST_ASSERT_NOT_NULL(readable);
    TEST_ASSERT_NOT_NULL(parameter);
    TEST_ASSERT_EQUAL_UINT32(readable->symbolId, parameter->ownerSymbolId);

    ZrCore_Array_Free(g_state, &symbols);
    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_visible_symbols_projects_source_function_generic_parameter(void) {
    const TZrChar *source = "fn identity<T>(value: T): T { return value; }\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    const SZrParserSemanticSymbolQuery *function;
    const SZrParserSemanticSymbolQuery *parameter;
    const SZrCanonicalTypeNode *parameterType;
    SZrFileRange position;

    sourceName = ZrCore_String_CreateFromNative(g_state, "visible_symbols_generic_function.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)ast->data.script.statements->count);
    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.functionDeclaration.generic);
    TEST_ASSERT_NOT_NULL(declaration->data.functionDeclaration.generic->params);
    TEST_ASSERT_EQUAL_UINT32(
            1U,
            (TZrUInt32)declaration->data.functionDeclaration.generic->params->count);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    position = symbol_source_position(source, sourceName, "identity", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "T"));

    position = symbol_source_position(source, sourceName, "T", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "T"));
    function = symbol_find_visible_name(&symbols, "identity");
    parameter = symbol_find_visible_name(&symbols, "T");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(parameter);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, parameter->symbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, parameter->typeId);
    TEST_ASSERT_EQUAL_UINT32(function->symbolId, parameter->ownerSymbolId);
    parameterType = ZrParser_CanonicalType_Find(cs.semanticContext, parameter->typeId);
    TEST_ASSERT_NOT_NULL(parameterType);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_PARAMETER, parameterType->kind);

    ZrCore_Array_Free(g_state, &symbols);
    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_visible_symbols_projects_source_method_generic_parameter(void) {
    const TZrChar *source =
            "struct Box<T> { fn echo<U>(value: U): U { return value; } }\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *methodNode;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    const SZrSemanticSymbolRecord *method;
    const SZrParserSemanticSymbolQuery *parameter;
    const SZrCanonicalTypeNode *parameterType;
    SZrFileRange position;

    sourceName = ZrCore_String_CreateFromNative(g_state, "visible_symbols_generic_method.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]->data.structDeclaration.members);
    methodNode = ast->data.script.statements->nodes[0]->data.structDeclaration.members->nodes[0];
    TEST_ASSERT_NOT_NULL(methodNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_METHOD, methodNode->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    method = symbol_find_registered_node(cs.semanticContext, methodNode);
    TEST_ASSERT_NOT_NULL(method);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, method->id);

    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    position = symbol_source_position(source, sourceName, "echo", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "U"));

    position = symbol_source_position(source, sourceName, "U", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "U"));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "T"));
    parameter = symbol_find_visible_name(&symbols, "U");
    TEST_ASSERT_NOT_NULL(parameter);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, parameter->symbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, parameter->typeId);
    TEST_ASSERT_EQUAL_UINT32(method->id, parameter->ownerSymbolId);
    parameterType = ZrParser_CanonicalType_Find(cs.semanticContext, parameter->typeId);
    TEST_ASSERT_NOT_NULL(parameterType);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_PARAMETER, parameterType->kind);

    ZrCore_Array_Free(g_state, &symbols);
    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_visible_symbols_projects_source_class_method_generic_parameter(void) {
    const TZrChar *source =
            "class Crate<T> { fn echo<U>(value: U): U { return value; } }\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *methodNode;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    const SZrSemanticSymbolRecord *method;
    const SZrParserSemanticSymbolQuery *parameter;
    const SZrCanonicalTypeNode *parameterType;
    SZrFileRange position;

    sourceName = ZrCore_String_CreateFromNative(g_state, "visible_symbols_generic_class_method.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]->data.classDeclaration.members);
    methodNode = ast->data.script.statements->nodes[0]->data.classDeclaration.members->nodes[0];
    TEST_ASSERT_NOT_NULL(methodNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_METHOD, methodNode->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    method = symbol_find_registered_node(cs.semanticContext, methodNode);
    TEST_ASSERT_NOT_NULL(method);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, method->id);

    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    position = symbol_source_position(source, sourceName, "echo", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "U"));

    position = symbol_source_position(source, sourceName, "U", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "U"));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "T"));
    parameter = symbol_find_visible_name(&symbols, "U");
    TEST_ASSERT_NOT_NULL(parameter);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, parameter->symbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, parameter->typeId);
    TEST_ASSERT_EQUAL_UINT32(method->id, parameter->ownerSymbolId);
    parameterType = ZrParser_CanonicalType_Find(cs.semanticContext, parameter->typeId);
    TEST_ASSERT_NOT_NULL(parameterType);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_PARAMETER, parameterType->kind);

    ZrCore_Array_Free(g_state, &symbols);
    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_visible_symbols_projects_source_const_generic_parameter(void) {
    const TZrChar *source = "struct Matrix<const N: int> { }\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    const SZrSemanticSymbolRecord *type;
    const SZrParserSemanticSymbolQuery *parameter;
    const SZrCanonicalTypeNode *parameterType;
    SZrFileRange position;

    sourceName = ZrCore_String_CreateFromNative(g_state, "visible_symbols_const_generic.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)ast->data.script.statements->count);
    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, declaration->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    type = symbol_find_registered_node(cs.semanticContext, declaration);
    TEST_ASSERT_NOT_NULL(type);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, type->id);

    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    position = symbol_source_position(source, sourceName, "Matrix", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "N"));

    position = symbol_source_position(source, sourceName, "N", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "N"));
    parameter = symbol_find_visible_name(&symbols, "N");
    TEST_ASSERT_NOT_NULL(parameter);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, parameter->symbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, parameter->typeId);
    TEST_ASSERT_EQUAL_UINT32(type->id, parameter->ownerSymbolId);
    parameterType = ZrParser_CanonicalType_Find(cs.semanticContext, parameter->typeId);
    TEST_ASSERT_NOT_NULL(parameterType);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_PARAMETER, parameterType->kind);

    ZrCore_Array_Free(g_state, &symbols);
    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_visible_symbols_projects_source_interface_method_generic_parameter(void) {
    const TZrChar *source =
            "interface Readable<T> { fn echo<U>(value: U): U; }\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *methodNode;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    const SZrSemanticSymbolRecord *method;
    const SZrParserSemanticSymbolQuery *parameter;
    const SZrCanonicalTypeNode *parameterType;
    SZrFileRange position;

    sourceName = ZrCore_String_CreateFromNative(
            g_state, "visible_symbols_generic_interface_method.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]->data.interfaceDeclaration.members);
    methodNode = ast->data.script.statements->nodes[0]->data.interfaceDeclaration.members->nodes[0];
    TEST_ASSERT_NOT_NULL(methodNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTERFACE_METHOD_SIGNATURE, methodNode->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    method = symbol_find_registered_node(cs.semanticContext, methodNode);
    TEST_ASSERT_NOT_NULL(method);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, method->id);

    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    position = symbol_source_position(source, sourceName, "echo", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "U"));

    position = symbol_source_position(source, sourceName, "U", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "U"));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "T"));
    parameter = symbol_find_visible_name(&symbols, "U");
    TEST_ASSERT_NOT_NULL(parameter);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, parameter->symbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, parameter->typeId);
    TEST_ASSERT_EQUAL_UINT32(method->id, parameter->ownerSymbolId);
    parameterType = ZrParser_CanonicalType_Find(cs.semanticContext, parameter->typeId);
    TEST_ASSERT_NOT_NULL(parameterType);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_PARAMETER, parameterType->kind);

    ZrCore_Array_Free(g_state, &symbols);
    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_visible_symbols_projects_direct_import_alias(void) {
    const TZrChar *source =
            "var math = import(\"zr.math\");\n"
            "fn probe(): int { return 0; }\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *importDeclaration;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    const SZrSemanticSymbolRecord *importSymbol;
    const SZrSemanticVisibleSymbolFact *importFact;
    const SZrParserSemanticSymbolQuery *visibleImport;
    SZrFileRange position;

    sourceName = ZrCore_String_CreateFromNative(g_state, "visible_symbols_import_alias.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(g_state->global));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(2U, (TZrUInt32)ast->data.script.statements->count);
    importDeclaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(importDeclaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, importDeclaration->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    importSymbol = symbol_find_registered_node(cs.semanticContext, importDeclaration);
    TEST_ASSERT_NOT_NULL(importSymbol);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_VARIABLE, importSymbol->kind);
    importFact = symbol_find_visible_fact(cs.semanticContext, importSymbol->id);
    TEST_ASSERT_NOT_NULL(importFact);
    TEST_ASSERT_TRUE(importFact->isImport);
    TEST_ASSERT_TRUE(importFact->isAlias);

    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    position = symbol_source_position(source, sourceName, "return", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "math"));

    options.includeImports = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "math"));
    visibleImport = symbol_find_visible_name(&symbols, "math");
    TEST_ASSERT_NOT_NULL(visibleImport);
    TEST_ASSERT_EQUAL_UINT32(importSymbol->id, visibleImport->symbolId);

    ZrCore_Array_Free(g_state, &symbols);
    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_symbol_at_projects_native_module_function_identity(void) {
    const TZrChar *source =
            "var math = import(\"zr.math\");\n"
            "fn probe(): float { return math.abs(-3.0); }\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    const SZrSemanticReferenceFact *reference;
    SZrParserSemanticSymbolQuery symbolAt;

    sourceName = ZrCore_String_CreateFromNative(
            g_state, "symbol_at_native_module_function.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(g_state->global));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    reference = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            cs.semanticContext,
            symbol_source_position(source, sourceName, "abs", 0U),
            ZR_SEMANTIC_REFERENCE_CALL);
    TEST_ASSERT_NOT_NULL(reference);
    TEST_ASSERT_TRUE(reference->isResolved);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, reference->symbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, reference->typeId);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)reference->declarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)reference->declarationRange.end.offset);
    memset(&symbolAt, 0, sizeof(symbolAt));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_SymbolAt(
            cs.semanticContext,
            symbol_source_position(source, sourceName, "abs", 0U),
            ZR_NULL,
            &symbolAt));
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, symbolAt.symbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, symbolAt.typeId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, symbolAt.kind);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbolAt.declarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbolAt.declarationRange.end.offset);

    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_symbol_at_projects_native_generic_receiver_declaration_identity(void) {
    const TZrChar *source =
            "var api = import(\"semantic.generic_identity\");\n"
            "var device = new api.NativeEchoDevice();\n"
            "var inferred = device.echo(1);\n"
            "var explicit = device.echo<string>(\"text\");\n";
    const TZrChar *inferredCallText = strstr(source, "device.echo(1)");
    const TZrChar *explicitCallText =
            strstr(source, "device.echo<string>(\"text\")");
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    const SZrSemanticReferenceFact *inferred;
    const SZrSemanticReferenceFact *explicitCall;
    SZrParserSemanticSymbolQuery inferredSymbol;
    SZrParserSemanticSymbolQuery explicitSymbol;
    SZrParserSemanticCallQuery inferredQuery;
    SZrParserSemanticCallQuery explicitQuery;
    SZrFileRange inferredCallPosition;
    SZrFileRange explicitCallPosition;
    TZrChar inferredLabel[128];
    TZrChar explicitLabel[128];

    TEST_ASSERT_NOT_NULL(inferredCallText);
    TEST_ASSERT_NOT_NULL(explicitCallText);
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "symbol_at_native_generic_receiver.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_RegisterModule(
            g_state->global, &kSymbolGenericEchoModule));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    inferred = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            cs.semanticContext,
            symbol_source_position(source, sourceName, "echo", 0U),
            ZR_SEMANTIC_REFERENCE_CALL);
    explicitCall = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            cs.semanticContext,
            symbol_source_position(source, sourceName, "echo", 1U),
            ZR_SEMANTIC_REFERENCE_CALL);
    TEST_ASSERT_NOT_NULL(inferred);
    TEST_ASSERT_NOT_NULL(explicitCall);
    TEST_ASSERT_TRUE(inferred->isResolved);
    TEST_ASSERT_TRUE(explicitCall->isResolved);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, inferred->symbolId);
    TEST_ASSERT_EQUAL_UINT32(inferred->symbolId, explicitCall->symbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(inferred->typeId, explicitCall->typeId);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)inferred->declarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)explicitCall->declarationRange.start.offset);

    memset(&inferredCallPosition, 0, sizeof(inferredCallPosition));
    inferredCallPosition.source = sourceName;
    inferredCallPosition.start.offset =
            (TZrSize)(inferredCallText - source + strlen("device.echo("));
    inferredCallPosition.end = inferredCallPosition.start;
    memset(&explicitCallPosition, 0, sizeof(explicitCallPosition));
    explicitCallPosition.source = sourceName;
    explicitCallPosition.start.offset =
            (TZrSize)(explicitCallText - source + strlen("device.echo<string>("));
    explicitCallPosition.end = explicitCallPosition.start;
    memset(&inferredQuery, 0, sizeof(inferredQuery));
    memset(&explicitQuery, 0, sizeof(explicitQuery));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext,
            inferredCallPosition,
            ZR_NULL,
            &inferredQuery));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext,
            explicitCallPosition,
            ZR_NULL,
            &explicitQuery));
    TEST_ASSERT_TRUE(inferredQuery.hasResolvedTarget);
    TEST_ASSERT_TRUE(explicitQuery.hasResolvedTarget);
    TEST_ASSERT_EQUAL_UINT32(inferred->symbolId, inferredQuery.targetSymbolId);
    TEST_ASSERT_EQUAL_UINT32(inferredQuery.targetSymbolId, explicitQuery.targetSymbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(
            inferredQuery.callableTypeId, explicitQuery.callableTypeId);
    TEST_ASSERT_EQUAL_UINT32(
            0U, (TZrUInt32)inferredQuery.targetDeclarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT32(
            0U, (TZrUInt32)explicitQuery.targetDeclarationRange.start.offset);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &inferredQuery, inferredLabel, sizeof(inferredLabel)));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &explicitQuery, explicitLabel, sizeof(explicitLabel)));
    TEST_ASSERT_EQUAL_STRING("fn echo<T>(value: int): int", inferredLabel);
    TEST_ASSERT_EQUAL_STRING("fn echo<T>(value: string): string", explicitLabel);

    memset(&inferredSymbol, 0, sizeof(inferredSymbol));
    memset(&explicitSymbol, 0, sizeof(explicitSymbol));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_SymbolAt(
            cs.semanticContext,
            symbol_source_position(source, sourceName, "echo", 0U),
            ZR_NULL,
            &inferredSymbol));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_SymbolAt(
            cs.semanticContext,
            symbol_source_position(source, sourceName, "echo", 1U),
            ZR_NULL,
            &explicitSymbol));
    TEST_ASSERT_EQUAL_UINT32(inferredSymbol.symbolId, explicitSymbol.symbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(inferredSymbol.typeId, explicitSymbol.typeId);

    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_visible_symbols_projects_destructured_import_and_type_value_aliases(void) {
    const TZrChar *source =
            "var {Vec3: Vector3} = import(\"zr.math\");\n"
            "var MatrixType = int[][];\n"
            "fn probe(): int { return 0; }\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *destructuredDeclaration;
    SZrAstNode *typeValueDeclaration;
    SZrAstNode *bindingNode;
    const SZrSemanticSymbolRecord *destructuredSymbol;
    const SZrSemanticSymbolRecord *typeValueSymbol;
    const SZrSemanticVisibleSymbolFact *destructuredFact;
    const SZrSemanticVisibleSymbolFact *typeValueFact;
    const SZrSemanticReferenceFact *destructuredReference;
    const SZrSemanticReferenceFact *typeValueReference;
    const SZrParserSemanticSymbolQuery *visibleImport;
    const SZrParserSemanticSymbolQuery *visibleTypeAlias;
    SZrParserSemanticSymbolQuery symbolAt;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    SZrFileRange position;

    sourceName = ZrCore_String_CreateFromNative(
            g_state, "visible_symbols_destructured_import_alias.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(g_state->global));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3U, (TZrUInt32)ast->data.script.statements->count);
    destructuredDeclaration = ast->data.script.statements->nodes[0];
    typeValueDeclaration = ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(destructuredDeclaration);
    TEST_ASSERT_NOT_NULL(typeValueDeclaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, destructuredDeclaration->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, typeValueDeclaration->type);
    TEST_ASSERT_NOT_NULL(destructuredDeclaration->data.variableDeclaration.pattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DESTRUCTURING_OBJECT,
                          destructuredDeclaration->data.variableDeclaration.pattern->type);
    TEST_ASSERT_NOT_NULL(destructuredDeclaration->data.variableDeclaration.pattern->data.destructuringObject.keys);
    TEST_ASSERT_EQUAL_UINT32(
            1U,
            (TZrUInt32)destructuredDeclaration->data.variableDeclaration.pattern->data.destructuringObject.keys->count);
    bindingNode = destructuredDeclaration->data.variableDeclaration.pattern->data.destructuringObject.keys->nodes[0];
    TEST_ASSERT_NOT_NULL(bindingNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_KEY_VALUE_PAIR, bindingNode->type);
    bindingNode = bindingNode->data.keyValuePair.key;
    TEST_ASSERT_NOT_NULL(bindingNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, bindingNode->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    destructuredSymbol = symbol_find_registered_node(cs.semanticContext, bindingNode);
    typeValueSymbol = symbol_find_registered_node(cs.semanticContext, typeValueDeclaration);
    TEST_ASSERT_NOT_NULL(destructuredSymbol);
    TEST_ASSERT_NOT_NULL(typeValueSymbol);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_VARIABLE, destructuredSymbol->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_VARIABLE, typeValueSymbol->kind);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, destructuredSymbol->typeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, typeValueSymbol->typeId);
    destructuredFact = symbol_find_visible_fact(cs.semanticContext, destructuredSymbol->id);
    typeValueFact = symbol_find_visible_fact(cs.semanticContext, typeValueSymbol->id);
    TEST_ASSERT_NOT_NULL(destructuredFact);
    TEST_ASSERT_NOT_NULL(typeValueFact);
    TEST_ASSERT_TRUE(destructuredFact->isImport);
    TEST_ASSERT_TRUE(destructuredFact->isAlias);
    TEST_ASSERT_FALSE(typeValueFact->isImport);
    TEST_ASSERT_TRUE(typeValueFact->isAlias);

    destructuredReference = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            cs.semanticContext,
            symbol_source_position(source, sourceName, "Vec3", 0U),
            ZR_SEMANTIC_REFERENCE_DECLARATION);
    typeValueReference = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            cs.semanticContext,
            symbol_source_position(source, sourceName, "MatrixType", 0U),
            ZR_SEMANTIC_REFERENCE_DECLARATION);
    TEST_ASSERT_NOT_NULL(destructuredReference);
    TEST_ASSERT_NOT_NULL(typeValueReference);
    TEST_ASSERT_EQUAL_UINT32(destructuredSymbol->id, destructuredReference->symbolId);
    TEST_ASSERT_EQUAL_UINT32(typeValueSymbol->id, typeValueReference->symbolId);
    memset(&symbolAt, 0, sizeof(symbolAt));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_SymbolAt(
            cs.semanticContext,
            symbol_source_position(source, sourceName, "Vec3", 0U),
            ZR_NULL,
            &symbolAt));
    TEST_ASSERT_EQUAL_UINT32(destructuredSymbol->id, symbolAt.symbolId);
    TEST_ASSERT_EQUAL_UINT32(destructuredSymbol->typeId, symbolAt.typeId);
    memset(&symbolAt, 0, sizeof(symbolAt));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_SymbolAt(
            cs.semanticContext,
            symbol_source_position(source, sourceName, "MatrixType", 0U),
            ZR_NULL,
            &symbolAt));
    TEST_ASSERT_EQUAL_UINT32(typeValueSymbol->id, symbolAt.symbolId);
    TEST_ASSERT_EQUAL_UINT32(typeValueSymbol->typeId, symbolAt.typeId);

    position = symbol_source_position(source, sourceName, "return", 0U);
    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "Vec3"));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "MatrixType"));

    options.includeImports = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "Vec3"));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "MatrixType"));
    visibleImport = symbol_find_visible_name(&symbols, "Vec3");
    visibleTypeAlias = symbol_find_visible_name(&symbols, "MatrixType");
    TEST_ASSERT_NOT_NULL(visibleImport);
    TEST_ASSERT_NOT_NULL(visibleTypeAlias);
    TEST_ASSERT_EQUAL_UINT32(destructuredSymbol->id, visibleImport->symbolId);
    TEST_ASSERT_EQUAL_UINT32(typeValueSymbol->id, visibleTypeAlias->symbolId);

    ZrCore_Array_Free(g_state, &symbols);
    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_visible_symbols_projects_source_type_members(void) {
    const TZrChar *source =
            "class Meter {\n"
            "    var reading: int;\n"
            "    static var total: int;\n"
            "    fn instance(): int { return 0; }\n"
            "    static fn factory(): int { return 0; }\n"
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *classNode;
    SZrAstNode *readingNode;
    SZrAstNode *totalNode;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    const SZrSemanticSymbolRecord *classSymbol;
    const SZrSemanticSymbolRecord *reading;
    const SZrSemanticSymbolRecord *total;
    const SZrParserSemanticSymbolQuery *visibleReading;
    SZrFileRange position;

    sourceName = ZrCore_String_CreateFromNative(g_state, "visible_symbols_type_members.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)ast->data.script.statements->count);
    classNode = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(classNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classNode->type);
    TEST_ASSERT_NOT_NULL(classNode->data.classDeclaration.members);
    TEST_ASSERT_EQUAL_UINT32(4U, (TZrUInt32)classNode->data.classDeclaration.members->count);
    readingNode = classNode->data.classDeclaration.members->nodes[0];
    totalNode = classNode->data.classDeclaration.members->nodes[1];
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_FIELD, readingNode->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_FIELD, totalNode->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    classSymbol = symbol_find_registered_node(cs.semanticContext, classNode);
    reading = symbol_find_registered_node(cs.semanticContext, readingNode);
    total = symbol_find_registered_node(cs.semanticContext, totalNode);
    TEST_ASSERT_NOT_NULL(classSymbol);
    TEST_ASSERT_NOT_NULL(reading);
    TEST_ASSERT_NOT_NULL(total);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_FIELD, reading->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_FIELD, total->kind);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, reading->id);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, total->id);

    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    position = symbol_source_position(source, sourceName, "return", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "reading"));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "total"));

    options.includeReceiverMembers = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "reading"));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "total"));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "instance"));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "factory"));
    visibleReading = symbol_find_visible_name(&symbols, "reading");
    TEST_ASSERT_NOT_NULL(visibleReading);
    TEST_ASSERT_EQUAL_UINT32(reading->id, visibleReading->symbolId);
    TEST_ASSERT_EQUAL_UINT32(classSymbol->id, visibleReading->ownerSymbolId);

    position = symbol_source_position(source, sourceName, "return", 1U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "reading"));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "total"));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)symbol_count_visible_name(&symbols, "instance"));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)symbol_count_visible_name(&symbols, "factory"));

    ZrCore_Array_Free(g_state, &symbols);
    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_visible_symbols_projects_source_struct_and_interface_members(void) {
    const TZrChar *source =
            "struct Point { var x: int; fn read(): int { return 0; } }\n"
            "interface Readable { var ready: int; fn read(): int; }\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *pointNode;
    SZrAstNode *readableNode;
    SZrAstNode *pointFieldNode;
    SZrAstNode *interfaceFieldNode;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    const SZrSemanticSymbolRecord *pointField;
    const SZrSemanticSymbolRecord *interfaceField;
    const SZrParserSemanticSymbolQuery *visiblePointField;
    const SZrParserSemanticSymbolQuery *visibleInterfaceField;
    SZrFileRange position;

    sourceName = ZrCore_String_CreateFromNative(
            g_state, "visible_symbols_struct_interface_members.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(2U, (TZrUInt32)ast->data.script.statements->count);
    pointNode = ast->data.script.statements->nodes[0];
    readableNode = ast->data.script.statements->nodes[1];
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, pointNode->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTERFACE_DECLARATION, readableNode->type);
    pointFieldNode = pointNode->data.structDeclaration.members->nodes[0];
    interfaceFieldNode = readableNode->data.interfaceDeclaration.members->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_FIELD, pointFieldNode->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTERFACE_FIELD_DECLARATION, interfaceFieldNode->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    pointField = symbol_find_registered_node(cs.semanticContext, pointFieldNode);
    interfaceField = symbol_find_registered_node(cs.semanticContext, interfaceFieldNode);
    TEST_ASSERT_NOT_NULL(pointField);
    TEST_ASSERT_NOT_NULL(interfaceField);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_FIELD, pointField->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_FIELD, interfaceField->kind);

    ZrCore_Array_Construct(&symbols);
    memset(&options, 0, sizeof(options));
    options.includeReceiverMembers = ZR_TRUE;
    position = symbol_source_position(source, sourceName, "return", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    visiblePointField = symbol_find_visible_name(&symbols, "x");
    TEST_ASSERT_NOT_NULL(visiblePointField);
    TEST_ASSERT_EQUAL_UINT32(pointField->id, visiblePointField->symbolId);

    position = symbol_source_position(source, sourceName, "read", 1U);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_VisibleSymbols(
            cs.semanticContext, position, ZR_NULL, &options, &symbols));
    visibleInterfaceField = symbol_find_visible_name(&symbols, "ready");
    TEST_ASSERT_NOT_NULL(visibleInterfaceField);
    TEST_ASSERT_EQUAL_UINT32(interfaceField->id, visibleInterfaceField->symbolId);

    ZrCore_Array_Free(g_state, &symbols);
    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

#include "test_semantic_query_declared_symbol_cases.h"

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_symbol_at_projects_resolved_reference_identity);
    RUN_TEST(test_symbol_at_fails_closed_for_unresolved_reference);
    RUN_TEST(test_visible_symbols_uses_scope_facts_for_shadowing_and_options);
    RUN_TEST(test_visible_symbols_excludes_instance_members_from_static_scope);
    RUN_TEST(test_visible_symbols_project_compiled_source_scope_facts);
    RUN_TEST(test_visible_symbols_projects_extern_block_declarations);
    RUN_TEST(test_visible_symbols_does_not_leak_for_initializer);
    RUN_TEST(test_visible_symbols_projects_source_type_declarations);
    RUN_TEST(test_visible_symbols_projects_source_type_generic_parameter);
    RUN_TEST(test_visible_symbols_projects_source_function_generic_parameter);
    RUN_TEST(test_visible_symbols_projects_source_method_generic_parameter);
    RUN_TEST(test_visible_symbols_projects_source_class_method_generic_parameter);
    RUN_TEST(test_visible_symbols_projects_source_const_generic_parameter);
    RUN_TEST(test_visible_symbols_projects_source_interface_method_generic_parameter);
    RUN_TEST(test_visible_symbols_projects_direct_import_alias);
    RUN_TEST(test_symbol_at_projects_native_module_function_identity);
    RUN_TEST(test_symbol_at_projects_native_generic_receiver_declaration_identity);
    RUN_TEST(test_visible_symbols_projects_destructured_import_and_type_value_aliases);
    RUN_TEST(test_visible_symbols_projects_source_type_members);
    RUN_TEST(test_visible_symbols_projects_source_struct_and_interface_members);
    RUN_TEST(test_declared_symbols_projects_exact_snapshot_declarations);
    return UNITY_END();
}
