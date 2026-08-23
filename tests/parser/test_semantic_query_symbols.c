#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"

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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_symbol_at_projects_resolved_reference_identity);
    RUN_TEST(test_symbol_at_fails_closed_for_unresolved_reference);
    return UNITY_END();
}
