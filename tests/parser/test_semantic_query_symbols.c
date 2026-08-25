#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"

#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

static SZrState *g_state;

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
            "}\n";
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrArray symbols;
    SZrParserSemanticVisibleSymbolOptions options;
    SZrFileRange position;

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
    symbol_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_symbol_at_projects_resolved_reference_identity);
    RUN_TEST(test_symbol_at_fails_closed_for_unresolved_reference);
    RUN_TEST(test_visible_symbols_uses_scope_facts_for_shadowing_and_options);
    RUN_TEST(test_visible_symbols_excludes_instance_members_from_static_scope);
    RUN_TEST(test_visible_symbols_project_compiled_source_scope_facts);
    RUN_TEST(test_visible_symbols_does_not_leak_for_initializer);
    RUN_TEST(test_visible_symbols_projects_source_type_declarations);
    RUN_TEST(test_visible_symbols_projects_source_type_generic_parameter);
    RUN_TEST(test_visible_symbols_projects_source_function_generic_parameter);
    RUN_TEST(test_visible_symbols_projects_source_method_generic_parameter);
    RUN_TEST(test_visible_symbols_projects_source_class_method_generic_parameter);
    return UNITY_END();
}
