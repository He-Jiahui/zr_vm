/* RED coverage for native syntax-context hygiene and token ownership. */

#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_parser/syntax_context.h"
#include "zr_vm_parser/token_stream.h"

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

static SZrSyntaxContextTable *create_context_table(void) {
    SZrSyntaxContextTable *table;

    table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(table);
    return table;
}

static TZrBool register_test_expansion(
        SZrSyntaxContextTable *table,
        TZrUInt64 stableIdentity,
        TZrSyntaxExpansionId *outExpansionId) {
    static const TZrChar definitionModule[] = "macro.provider";
    static const TZrChar callSiteSource[] = "caller.zr";
    static const TZrChar definitionSiteSource[] = "definition.zr";
    SZrSyntaxExpansionInfo info;

    if (table == ZR_NULL || outExpansionId == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&info, 0, sizeof(info));
    info.stableIdentity = stableIdentity;
    info.definitionModule = definitionModule;
    info.definitionModuleLength = sizeof(definitionModule) - 1U;
    info.callSiteSource = callSiteSource;
    info.callSiteSourceLength = sizeof(callSiteSource) - 1U;
    info.definitionSiteSource = definitionSiteSource;
    info.definitionSiteSourceLength = sizeof(definitionSiteSource) - 1U;
    info.callSiteSpan = ZrParser_TokenSpan_None();
    info.definitionSiteSpan = ZrParser_TokenSpan_None();
    info.callSiteContext = ZrParser_SyntaxContext_TableRoot(table);
    info.definitionSiteContext = ZrParser_SyntaxContext_TableRoot(table);
    return ZrParser_SyntaxContextTable_RegisterExpansion(
            table, &info, outExpansionId);
}

static SZrTokenStream *stream_from_text(
        SZrSyntaxContextTable *table,
        const TZrChar *sourceName,
        TZrSize sourceNameLength,
        const TZrChar *source,
        TZrSize sourceLength) {
    SZrTokenStream *stream = ZR_NULL;

    TEST_ASSERT_NOT_NULL(table);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            g_state,
            table,
            sourceName,
            sourceNameLength,
            source,
            sourceLength,
            ZR_NULL,
            &stream));
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_IsSealed(stream));
    return stream;
}

static void test_cross_table_import_and_concat_keep_equality_table_sensitive(void) {
    SZrSyntaxContextTable *sourceTable = create_context_table();
    SZrSyntaxContextTable *destinationTable = create_context_table();
    TZrSyntaxExpansionId expansionId = 0U;
    SZrSyntaxContext sourceMixed;
    SZrSyntaxContext imported;
    SZrSyntaxContext importedAgain;
    SZrSyntaxContext destinationRoot;
    SZrSyntaxContext sourceRoot;
    SZrSyntaxContext unrelated;
    SZrTokenStream *left;
    SZrTokenStream *right;
    SZrTokenStream *joined = ZR_NULL;
    const SZrTokenTree *leftTree;
    const SZrTokenTree *rightTree;
    SZrSyntaxContext leftContext;
    SZrSyntaxContext rightContext;

    TEST_ASSERT_TRUE(register_test_expansion(
            sourceTable, UINT64_C(0xCA11), &expansionId));
    sourceMixed = ZrParser_SyntaxContext_MixedSite(sourceTable, expansionId);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(sourceMixed));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Import(
            destinationTable, sourceMixed, &imported));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(imported));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsSameTable(
            imported, ZrParser_SyntaxContext_TableRoot(destinationTable)));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(sourceMixed, imported));
    TEST_ASSERT_EQUAL_UINT64(ZrParser_SyntaxContext_StableHash(sourceMixed),
            ZrParser_SyntaxContext_StableHash(imported));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Import(
            destinationTable, sourceMixed, &importedAgain));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(imported, importedAgain));
    sourceRoot = ZrParser_SyntaxContext_TableRoot(sourceTable);
    destinationRoot = ZrParser_SyntaxContext_TableRoot(destinationTable);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(sourceRoot, destinationRoot));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            sourceRoot, ZrParser_SyntaxContext_Root()));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(imported, destinationRoot));
    TZrSyntaxExpansionId unrelatedId = 0U;
    TEST_ASSERT_TRUE(register_test_expansion(
            destinationTable, UINT64_C(0xCA15), &unrelatedId));
    unrelated = ZrParser_SyntaxContext_ApplyMark(
            destinationRoot, unrelatedId, ZR_SYNTAX_OPAQUE);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(unrelated));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(imported, unrelated));

    left = stream_from_text(
            sourceTable, "left.zr", sizeof("left.zr") - 1U,
            "left", sizeof("left") - 1U);
    right = stream_from_text(
            destinationTable, "right.zr", sizeof("right.zr") - 1U,
            "right", sizeof("right") - 1U);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Concat(
            left, right, ZR_NULL, &joined));
    TEST_ASSERT_NOT_NULL(joined);
    TEST_ASSERT_EQUAL_UINT(2U, (unsigned)ZrParser_TokenStream_Count(joined));
    leftTree = ZrParser_TokenStream_At(joined, 0U);
    rightTree = ZrParser_TokenStream_At(joined, 1U);
    TEST_ASSERT_NOT_NULL(leftTree);
    TEST_ASSERT_NOT_NULL(rightTree);
    leftContext = ZrParser_TokenTree_Context(leftTree);
    rightContext = ZrParser_TokenTree_Context(rightTree);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(leftContext));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(rightContext));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsSameTable(leftContext, rightContext));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsSameTable(
            leftContext,
            ZrParser_SyntaxContext_TableRoot(
                    ZrParser_TokenStream_ContextTable(joined))));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_IsSameTable(leftContext, sourceMixed));

    ZrParser_TokenStream_Release(left);
    ZrParser_TokenStream_Release(right);
    ZrParser_SyntaxContextTable_Release(sourceTable);
    ZrParser_SyntaxContextTable_Release(destinationTable);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_NOT_NULL(ZrParser_TokenStream_At(joined, 0U));
    TEST_ASSERT_NOT_NULL(ZrParser_TokenStream_At(joined, 1U));
    ZrParser_TokenStream_Release(joined);
}

static void test_mixed_site_normalization_differs_for_local_and_item(void) {
    SZrSyntaxContextTable *table = create_context_table();
    TZrSyntaxExpansionId expansionId = 0U;
    SZrSyntaxContext mixed;
    SZrSyntaxContext local;
    SZrSyntaxContext item;
    SZrSyntaxContext root;

    TEST_ASSERT_TRUE(register_test_expansion(
            table, UINT64_C(0xCA12), &expansionId));
    mixed = ZrParser_SyntaxContext_MixedSite(table, expansionId);
    root = ZrParser_SyntaxContext_TableRoot(table);
    local = ZrParser_SyntaxContext_Normalize(
            mixed, ZR_SYNTAX_NAMESPACE_LOCAL);
    item = ZrParser_SyntaxContext_Normalize(
            mixed, ZR_SYNTAX_NAMESPACE_ITEM);

    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(mixed));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(local));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(item));
    /* Semi-opaque mixed-site marks are retained for local bindings. */
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(mixed, local));
    /* Item bindings strip semi-opaque marks and resolve at the call site. */
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(root, item));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(local, item));

    ZrParser_SyntaxContextTable_Release(table);
}

static void test_expansion_names_are_copied_and_invalid_ids_are_rejected(void) {
    SZrSyntaxContextTable *table = create_context_table();
    TZrSyntaxExpansionId expansionId = 0U;
    SZrSyntaxContext invalid;
    SZrSyntaxContext imported;
    const SZrSyntaxExpansionInfo *stored;
    static const TZrChar moduleName[] = "macro.provider";
    static const TZrChar callSource[] = "caller.zr";
    static const TZrChar definitionSource[] = "definition.zr";
    SZrSyntaxExpansionInfo info;
    TZrSyntaxExpansionId copiedId = 0U;

    TEST_ASSERT_TRUE(register_test_expansion(
            table, UINT64_C(0xCA13), &expansionId));
    stored = ZrParser_SyntaxContextTable_Expansion(table, expansionId);
    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(0xCA13), stored->stableIdentity);
    TEST_ASSERT_EQUAL_MEMORY(
            "macro.provider", stored->definitionModule,
            sizeof("macro.provider") - 1U);
    TEST_ASSERT_EQUAL_MEMORY(
            "caller.zr", stored->callSiteSource,
            sizeof("caller.zr") - 1U);
    TEST_ASSERT_EQUAL_MEMORY(
            "definition.zr", stored->definitionSiteSource,
            sizeof("definition.zr") - 1U);

    memset(&info, 0, sizeof(info));
    info.stableIdentity = UINT64_C(0xCA14);
    info.definitionModule = moduleName;
    info.definitionModuleLength = sizeof(moduleName) - 1U;
    info.callSiteSource = callSource;
    info.callSiteSourceLength = sizeof(callSource) - 1U;
    info.definitionSiteSource = definitionSource;
    info.definitionSiteSourceLength = sizeof(definitionSource) - 1U;
    info.callSiteSpan = ZrParser_TokenSpan_None();
    info.definitionSiteSpan = ZrParser_TokenSpan_None();
    info.callSiteContext = ZrParser_SyntaxContext_TableRoot(table);
    info.definitionSiteContext = ZrParser_SyntaxContext_TableRoot(table);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            table, &info, &copiedId));
    stored = ZrParser_SyntaxContextTable_Expansion(table, copiedId);
    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_TRUE(moduleName != stored->definitionModule);
    TEST_ASSERT_TRUE(callSource != stored->callSiteSource);
    TEST_ASSERT_TRUE(definitionSource != stored->definitionSiteSource);
    TEST_ASSERT_EQUAL_MEMORY(moduleName, stored->definitionModule,
                             sizeof(moduleName) - 1U);

    invalid.table = table;
    invalid.markId = UINT32_MAX;
    TEST_ASSERT_NULL(ZrParser_SyntaxContextTable_Expansion(table, 0U));
    TEST_ASSERT_NULL(ZrParser_SyntaxContextTable_Expansion(
            table, copiedId + 1U));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_IsValid(invalid));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Import(
            table, invalid, &imported));
    ZrParser_SyntaxContextTable_Release(table);
}

static void test_token_origin_survives_caller_release_and_full_gc(void) {
    TZrChar sourceName[] = "caller_origin.zr";
    TZrChar source[] = "originName";
    SZrSyntaxContextTable *table = create_context_table();
    SZrTokenStream *stream;
    const SZrTokenTree *tree;
    SZrTokenText text;
    SZrTokenSpan span;
    SZrSyntaxContext context;
    TZrSize length = 0U;
    const TZrChar *bytes;
    const TZrChar *name;

    stream = stream_from_text(
            table, sourceName, sizeof(sourceName) - 1U,
            source, sizeof(source) - 1U);
    sourceName[0] = 'X';
    source[0] = 'X';
    ZrParser_SyntaxContextTable_Release(table);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);

    TEST_ASSERT_EQUAL_UINT(1U, (unsigned)ZrParser_TokenStream_Count(stream));
    tree = ZrParser_TokenStream_At(stream, 0U);
    TEST_ASSERT_NOT_NULL(tree);
    text = ZrParser_TokenTree_Text(tree);
    bytes = ZrParser_TokenStream_TextBytes(stream, text);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_EQUAL_UINT64(sizeof("originName") - 1U, (uint64_t)text.length);
    TEST_ASSERT_EQUAL_MEMORY("originName", bytes, sizeof("originName") - 1U);
    name = ZrParser_TokenStream_SourceName(stream, text.sourceId, &length);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_UINT64(sizeof("caller_origin.zr") - 1U, (uint64_t)length);
    TEST_ASSERT_EQUAL_MEMORY(
            "caller_origin.zr", name, sizeof("caller_origin.zr") - 1U);
    span = ZrParser_TokenTree_Span(tree);
    TEST_ASSERT_TRUE(ZrParser_TokenSpan_IsValid(span));
    TEST_ASSERT_EQUAL_UINT32(text.sourceId, span.sourceId);
    TEST_ASSERT_EQUAL_UINT64(0U, (uint64_t)span.startOffset);
    TEST_ASSERT_EQUAL_UINT64(sizeof("originName") - 1U,
                             (uint64_t)span.endOffset);
    context = ZrParser_TokenTree_Context(tree);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(context));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsSameTable(
            context, ZrParser_SyntaxContext_TableRoot(
                    ZrParser_TokenStream_ContextTable(stream))));
    TEST_ASSERT_NULL(ZrParser_TokenStream_At(
            stream, ZrParser_TokenStream_Count(stream)));
    TEST_ASSERT_NULL(ZrParser_TokenStream_SourceBytes(
            stream, ZR_TOKEN_SOURCE_INVALID, ZR_NULL));

    ZrParser_TokenStream_Release(stream);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cross_table_import_and_concat_keep_equality_table_sensitive);
    RUN_TEST(test_mixed_site_normalization_differs_for_local_and_item);
    RUN_TEST(test_expansion_names_are_copied_and_invalid_ids_are_rejected);
    RUN_TEST(test_token_origin_survives_caller_release_and_full_gc);
    return UNITY_END();
}
