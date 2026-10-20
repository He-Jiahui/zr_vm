#include "unity.h"
#include "harness/runtime_support.h"
#include "parser/parser_internal.h"
#include "parser/parser_ownership.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"

#include <stdio.h>
#include <string.h>

enum {
    PRIMARY_TRACKED_ALLOCATION_LIMIT = 1024,
    PRIMARY_ROOT_PREWARM_COUNT = 64
};

typedef enum EZrPrimaryFixtureKind {
    ZR_PRIMARY_FIXTURE_NEW,
    ZR_PRIMARY_FIXTURE_EMPTY,
    ZR_PRIMARY_FIXTURE_FULL
} EZrPrimaryFixtureKind;

typedef struct SZrPrimaryFixture {
    SZrParserState parser;
    SZrParserComptimeBudget *ledger;
    SZrAstNode *base;
    SZrAstNode *member;
    SZrAstNode *pendingProperty;
    SZrAstNode *result;
    SZrSyntaxContextTable *contextTable;
    SZrTokenStream *stream;
    TZrSize rootsBefore;
    TZrBool parserInitialized;
} SZrPrimaryFixture;

typedef struct SZrPrimaryRejection {
    TZrBool rejected;
    TZrBool error;
    TZrBool fatal;
    TZrBool baseFreed;
    TZrBool memberFreed;
    EZrParserComptimeBudgetResource exceeded;
    TZrUInt64 before;
    TZrUInt64 allowance;
    TZrUInt64 usage;
    TZrUInt64 limit;
    TZrUInt64 requested;
    TZrSize liveAfterParser;
    TZrSize rootsAfterParser;
} SZrPrimaryRejection;

static SZrState *g_state;
static SZrPrimaryFixture g_fixture;
static FZrAllocator g_allocator;
static TZrPtr g_liveAllocations[PRIMARY_TRACKED_ALLOCATION_LIMIT];
static TZrSize g_liveAllocationSizes[PRIMARY_TRACKED_ALLOCATION_LIMIT];
static TZrSize g_liveCount;
static TZrBool g_trackAllocations;
static TZrBool g_trackingOverflow;

/* Forward every request unchanged; quota rejection comes from the owner ledger. */
static TZrPtr primary_allocator(
        TZrPtr userData, TZrPtr pointer, TZrSize oldSize,
        TZrSize newSize, TZrInt64 type) {
    TZrSize index = g_liveCount;
    TZrPtr result;
    TZrBool trackedType = type == ZR_MEMORY_NATIVE_TYPE_ARRAY ||
            type == ZR_MEMORY_NATIVE_TYPE_NONE;

    if (g_trackAllocations && trackedType && pointer != ZR_NULL) {
        for (index = 0u; index < g_liveCount; index++) {
            if (g_liveAllocations[index] == pointer) {
                break;
            }
        }
    }
    result = g_allocator(userData, pointer, oldSize, newSize, type);
    if (!g_trackAllocations || !trackedType ||
        (newSize != 0u && result == ZR_NULL)) {
        return result;
    }
    if (index < g_liveCount) {
        g_liveCount--;
        g_liveAllocations[index] = g_liveAllocations[g_liveCount];
        g_liveAllocationSizes[index] = g_liveAllocationSizes[g_liveCount];
    }
    if (newSize != 0u && result != ZR_NULL) {
        if (g_liveCount == PRIMARY_TRACKED_ALLOCATION_LIMIT) {
            g_trackingOverflow = ZR_TRUE;
        } else {
            g_liveAllocations[g_liveCount] = result;
            g_liveAllocationSizes[g_liveCount++] = newSize;
        }
    }
    return result;
}
static TZrBool allocation_is_live(TZrPtr pointer) {
    TZrSize index;
    for (index = 0u; index < g_liveCount; index++) {
        if (g_liveAllocations[index] == pointer) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void fixture_free(void) {
    /* Failure fixtures may retain either input or return a root which owns both. */
    if (allocation_is_live(g_fixture.result)) {
        ZrParser_Ast_Free(g_state, g_fixture.result);
    }
    if (allocation_is_live(g_fixture.base)) {
        ZrParser_Ast_Free(g_state, g_fixture.base);
    }
    if (allocation_is_live(g_fixture.member)) {
        ZrParser_Ast_Free(g_state, g_fixture.member);
    }
    if (allocation_is_live(g_fixture.pendingProperty)) {
        ZrParser_Ast_Free(g_state, g_fixture.pendingProperty);
    }
    if (g_fixture.parserInitialized) {
        ZrParser_State_Free(&g_fixture.parser);
    }
    ZrParser_TokenStream_Release(g_fixture.stream);
    ZrParser_SyntaxContextTable_Release(g_fixture.contextTable);
    memset(&g_fixture, 0, sizeof(g_fixture));
}

void setUp(void) {
    SZrGcRootHandle roots[PRIMARY_ROOT_PREWARM_COUNT];
    TZrSize index;
    TZrSize rootsBefore;

    memset(&g_fixture, 0, sizeof(g_fixture));
    g_liveCount = 0u;
    g_trackAllocations = ZR_FALSE;
    g_trackingOverflow = ZR_FALSE;
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    /* The GC domain retains reusable root-table capacity until state teardown.
     * Initialize that infrastructure before observing parser-owned storage. */
    rootsBefore = ZrCore_GcDomain_GetRootCount(g_state);
    for (index = 0u; index < PRIMARY_ROOT_PREWARM_COUNT; index++) {
        TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Create(
                g_state, (SZrRawObject *)g_state, &roots[index]));
    }
    for (index = 0u; index < PRIMARY_ROOT_PREWARM_COUNT; index++) {
        ZrCore_GcRootHandle_Release(g_state, &roots[index]);
    }
    TEST_ASSERT_EQUAL_UINT64(rootsBefore, ZrCore_GcDomain_GetRootCount(g_state));
    g_allocator = g_state->global->allocator;
    g_state->global->allocator = primary_allocator;
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        fixture_free();
        g_trackAllocations = ZR_FALSE;
        g_state->global->allocator = g_allocator;
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static void create_fixture_member(void) {
    g_fixture.pendingProperty = parse_identifier(&g_fixture.parser);
    TEST_ASSERT_NOT_NULL(g_fixture.pendingProperty);
    g_fixture.member = create_ast_node(
            &g_fixture.parser, ZR_AST_MEMBER_EXPRESSION,
            g_fixture.pendingProperty->location);
    TEST_ASSERT_NOT_NULL(g_fixture.member);
    g_fixture.member->data.memberExpression.property =
            g_fixture.pendingProperty;
    g_fixture.member->data.memberExpression.accessMode =
            ZR_POSTFIX_ACCESS_DIRECT;
    g_fixture.pendingProperty = ZR_NULL;
}

static void fixture_open(EZrPrimaryFixtureKind kind) {
    const char *source = kind == ZR_PRIMARY_FIXTURE_FULL
            ? "base previous candidate" : "base candidate";
    SZrAstNode *primary;

    g_fixture.rootsBefore = ZrCore_GcDomain_GetRootCount(g_state);
    g_trackAllocations = ZR_FALSE;
    ZrParser_State_Init(
            &g_fixture.parser, g_state, source, strlen(source), ZR_NULL);
    g_fixture.parserInitialized = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(g_fixture.parser.lexer);
    TEST_ASSERT_FALSE(g_fixture.parser.hasError);
    g_fixture.parser.suppressErrorOutput = ZR_TRUE;
    g_fixture.ledger = ZrParser_Ownership_Budget(
            g_fixture.parser.lexer->ownership);
    TEST_ASSERT_NOT_NULL(g_fixture.ledger);
    g_trackAllocations = ZR_TRUE;

    g_fixture.base = parse_identifier(&g_fixture.parser);
    TEST_ASSERT_NOT_NULL(g_fixture.base);
    if (kind != ZR_PRIMARY_FIXTURE_NEW) {
        primary = create_ast_node(
                &g_fixture.parser, ZR_AST_PRIMARY_EXPRESSION,
                g_fixture.base->location);
        TEST_ASSERT_NOT_NULL(primary);
        primary->data.primaryExpression.property = g_fixture.base;
        g_fixture.base = primary;
    }
    if (kind == ZR_PRIMARY_FIXTURE_FULL) {
        g_fixture.base->data.primaryExpression.members =
                parser_ast_array_new(&g_fixture.parser, 1u);
        TEST_ASSERT_NOT_NULL(g_fixture.base->data.primaryExpression.members);
        create_fixture_member();
        TEST_ASSERT_TRUE(parser_ast_array_add(
                &g_fixture.parser,
                g_fixture.base->data.primaryExpression.members,
                g_fixture.member));
        g_fixture.member = ZR_NULL;
        TEST_ASSERT_EQUAL_UINT64(
                g_fixture.base->data.primaryExpression.members->capacity,
                g_fixture.base->data.primaryExpression.members->count);
    }
    create_fixture_member();
    TEST_ASSERT_FALSE(g_fixture.parser.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, g_fixture.parser.lexer->t.token);
    TEST_ASSERT_EQUAL_PTR(
            g_fixture.parser.lexer->ownership, g_fixture.base->ownership);
    TEST_ASSERT_EQUAL_PTR(g_fixture.base->ownership, g_fixture.member->ownership);
    TEST_ASSERT_GREATER_THAN_UINT64_MESSAGE(
            g_fixture.rootsBefore, ZrCore_GcDomain_GetRootCount(g_state),
            "AST strings must be rooted while the parser fixture is live");
}

static void expect_unrestricted_append(EZrPrimaryFixtureKind kind) {
    TZrSize rootsBefore;
    TZrSize expectedCount = kind == ZR_PRIMARY_FIXTURE_FULL ? 2u : 1u;
    SZrAstNode *property;
    SZrAstNode *previous = ZR_NULL;

    fixture_open(kind);
    rootsBefore = g_fixture.rootsBefore;
    property = kind == ZR_PRIMARY_FIXTURE_NEW ? g_fixture.base
            : g_fixture.base->data.primaryExpression.property;
    if (kind == ZR_PRIMARY_FIXTURE_FULL) {
        previous = g_fixture.base->data.primaryExpression.members->nodes[0];
    }
    g_fixture.result = append_primary_member(
            &g_fixture.parser, g_fixture.base, g_fixture.member,
            g_fixture.base->location);
    TEST_ASSERT_NOT_NULL(g_fixture.result);
    TEST_ASSERT_FALSE(g_fixture.parser.hasError);
    TEST_ASSERT_FALSE(g_fixture.parser.hasFatalError);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, g_fixture.result->type);
    TEST_ASSERT_EQUAL_PTR(
            property, g_fixture.result->data.primaryExpression.property);
    TEST_ASSERT_NOT_NULL(g_fixture.result->data.primaryExpression.members);
    TEST_ASSERT_EQUAL_UINT64(expectedCount,
            g_fixture.result->data.primaryExpression.members->count);
    TEST_ASSERT_EQUAL_PTR(g_fixture.member,
            g_fixture.result->data.primaryExpression.members->nodes[expectedCount - 1u]);
    if (kind != ZR_PRIMARY_FIXTURE_NEW) {
        TEST_ASSERT_EQUAL_PTR(g_fixture.base, g_fixture.result);
    }
    if (kind == ZR_PRIMARY_FIXTURE_FULL) {
        TEST_ASSERT_EQUAL_PTR(previous,
                g_fixture.result->data.primaryExpression.members->nodes[0]);
    }
    fixture_free();
    g_trackAllocations = ZR_FALSE;
    TEST_ASSERT_FALSE(g_trackingOverflow);
    TEST_ASSERT_EQUAL_UINT64(0u, g_liveCount);
    TEST_ASSERT_EQUAL_UINT64(rootsBefore, ZrCore_GcDomain_GetRootCount(g_state));
}

static void expect_heap_rejection(
        EZrPrimaryFixtureKind kind, TZrUInt64 allowance) {
    SZrPrimaryRejection rejection;
    TZrSize rootsBefore;

    fixture_open(kind);
    rootsBefore = g_fixture.rootsBefore;
    memset(&rejection, 0, sizeof(rejection));
    rejection.before = g_fixture.ledger->usage.heapBytes;
    rejection.allowance = allowance;
    rejection.limit = rejection.before + allowance;
    g_fixture.ledger->limits.heapBytes = rejection.limit;
    g_fixture.result = append_primary_member(
            &g_fixture.parser, g_fixture.base, g_fixture.member,
            g_fixture.base->location);
    rejection.rejected = g_fixture.result == ZR_NULL;
    rejection.error = g_fixture.parser.hasError;
    rejection.fatal = g_fixture.parser.hasFatalError;
    rejection.baseFreed = !allocation_is_live(g_fixture.base);
    rejection.memberFreed = !allocation_is_live(g_fixture.member);
    rejection.exceeded = g_fixture.ledger->exceededResource;
    rejection.usage = g_fixture.ledger->usage.heapBytes;
    rejection.requested = g_fixture.ledger->requestedUsage;

    /* Observe helper cleanup before fallback cleanup can hide retained inputs. */
    ZrParser_State_Free(&g_fixture.parser);
    g_fixture.parserInitialized = ZR_FALSE;
    rejection.liveAfterParser = g_liveCount;
    rejection.rootsAfterParser = ZrCore_GcDomain_GetRootCount(g_state);
    fixture_free();
    g_trackAllocations = ZR_FALSE;

    TEST_ASSERT_FALSE_MESSAGE(g_trackingOverflow, "Allocation observer must not overflow");
    TEST_ASSERT_TRUE_MESSAGE(rejection.rejected,
            "Fatal primary append failure must return NULL, not a partial base");
    TEST_ASSERT_TRUE(rejection.error);
    TEST_ASSERT_TRUE_MESSAGE(rejection.fatal, "Heap quota failure must remain fatal");
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, rejection.exceeded);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(rejection.before + rejection.allowance,
            rejection.usage, "A rejected reservation must charge the attempted heap bytes");
    TEST_ASSERT_GREATER_THAN_UINT64_MESSAGE(rejection.limit, rejection.requested,
            "The failed reservation must record its requested heap total");
    TEST_ASSERT_TRUE_MESSAGE(rejection.baseFreed,
            "Rejected append must free the base and its previously adopted children");
    TEST_ASSERT_TRUE_MESSAGE(rejection.memberFreed,
            "Rejected append must free the incoming member subtree");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0u, rejection.liveAfterParser,
            "Primary append failure must not retain native parser storage");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(rootsBefore, rejection.rootsAfterParser,
            "Primary append failure must release all AST ownership roots");
}

static void test_new_primary_member_succeeds_without_limit(void) {
    expect_unrestricted_append(ZR_PRIMARY_FIXTURE_NEW);
}

static void test_existing_empty_primary_succeeds_without_limit(void) {
    expect_unrestricted_append(ZR_PRIMARY_FIXTURE_EMPTY);
}

static void test_existing_full_primary_succeeds_without_limit(void) {
    expect_unrestricted_append(ZR_PRIMARY_FIXTURE_FULL);
}

static void test_wrapper_ast_heap_rejection_frees_inputs(void) {
    expect_heap_rejection(ZR_PRIMARY_FIXTURE_NEW, 0u);
}

static void test_wrapper_members_heap_rejection_frees_inputs(void) {
    expect_heap_rejection(ZR_PRIMARY_FIXTURE_NEW, sizeof(SZrAstNode));
}

static void test_existing_empty_members_heap_rejection_frees_inputs(void) {
    expect_heap_rejection(ZR_PRIMARY_FIXTURE_EMPTY, 0u);
}

static void test_existing_members_growth_heap_rejection_frees_inputs(void) {
    expect_heap_rejection(ZR_PRIMARY_FIXTURE_FULL, 0u);
}

static void expect_expression_node_rejection(
        const char *source, TZrUInt64 nodeLimit) {
    TZrBool rejected;
    TZrBool fatal;
    EZrParserComptimeBudgetResource exceeded;
    TZrSize rootsBefore;

    g_trackAllocations = ZR_TRUE;
    ZrParser_State_Init(
            &g_fixture.parser, g_state, source, strlen(source), ZR_NULL);
    g_fixture.parserInitialized = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(g_fixture.parser.lexer);
    g_fixture.ledger = ZrParser_Ownership_Budget(
            g_fixture.parser.lexer->ownership);
    TEST_ASSERT_NOT_NULL(g_fixture.ledger);
    g_fixture.result = parse_expression(&g_fixture.parser);
    TEST_ASSERT_NOT_NULL(g_fixture.result);
    TEST_ASSERT_FALSE(g_fixture.parser.hasError);
    TEST_ASSERT_FALSE(g_fixture.parser.hasFatalError);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, g_fixture.parser.lexer->t.token);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(nodeLimit + 1u,
            g_fixture.ledger->usage.nodeCount,
            "The quota must reject the final wrapper, after all input nodes fit");
    fixture_free();
    TEST_ASSERT_EQUAL_UINT64(0u, g_liveCount);

    rootsBefore = ZrCore_GcDomain_GetRootCount(g_state);
    g_trackAllocations = ZR_TRUE;
    ZrParser_State_Init(
            &g_fixture.parser, g_state, source, strlen(source), ZR_NULL);
    g_fixture.parserInitialized = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(g_fixture.parser.lexer);
    g_fixture.parser.suppressErrorOutput = ZR_TRUE;
    g_fixture.ledger = ZrParser_Ownership_Budget(
            g_fixture.parser.lexer->ownership);
    TEST_ASSERT_NOT_NULL(g_fixture.ledger);
    g_fixture.ledger->limits.nodeCount = nodeLimit;
    g_fixture.result = parse_expression(&g_fixture.parser);
    rejected = g_fixture.result == ZR_NULL;
    fatal = g_fixture.parser.hasFatalError;
    exceeded = g_fixture.ledger->exceededResource;
    fixture_free();
    g_trackAllocations = ZR_FALSE;

    TEST_ASSERT_TRUE(rejected);
    TEST_ASSERT_TRUE(fatal);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT, exceeded);
    TEST_ASSERT_FALSE(g_trackingOverflow);
    TEST_ASSERT_EQUAL_UINT64(0u, g_liveCount);
    TEST_ASSERT_EQUAL_UINT64(rootsBefore, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_call_wrapper_rejection_consumes_base_once(void) {
    /* The ledger also counts discarded identifier probes before publication. */
    expect_expression_node_rejection("base(argument)", 5u);
}

static void test_optional_call_wrapper_rejection_consumes_base_once(void) {
    expect_expression_node_rejection("base?.(argument)", 5u);
}

static void test_new_constructor_rejection_frees_target_and_arguments(void) {
    expect_expression_node_rejection("new Target(argument)", 3u);
}

static void test_own_constructor_rejection_frees_target_and_arguments(void) {
    expect_expression_node_rejection("own Target(argument)", 3u);
}

static void test_reference_constructor_rejection_frees_target(void) {
    expect_expression_node_rejection("ref target", 2u);
}

static void primary_source_fixture_open(const char *source) {
    g_fixture.rootsBefore = ZrCore_GcDomain_GetRootCount(g_state);
    g_trackAllocations = ZR_TRUE;
    ZrParser_State_Init(
            &g_fixture.parser, g_state, source, strlen(source), ZR_NULL);
    g_fixture.parserInitialized = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(g_fixture.parser.lexer);
    TEST_ASSERT_FALSE(g_fixture.parser.hasError);
    g_fixture.parser.suppressErrorOutput = ZR_TRUE;
    g_fixture.ledger = ZrParser_Ownership_Budget(
            g_fixture.parser.lexer->ownership);
    TEST_ASSERT_NOT_NULL(g_fixture.ledger);
}

static void primary_stream_fixture_open(const char *source) {
    g_fixture.rootsBefore = ZrCore_GcDomain_GetRootCount(g_state);
    g_trackAllocations = ZR_TRUE;
    g_fixture.contextTable = ZrParser_SyntaxContextTable_Create(
            g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_fixture.contextTable);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            g_state, g_fixture.contextTable,
            "primary.zr", sizeof("primary.zr") - 1u,
            source, strlen(source), ZR_NULL, &g_fixture.stream));
    TEST_ASSERT_NOT_NULL(g_fixture.stream);
    ZrParser_State_InitWithTokenStream(
            &g_fixture.parser, g_state, g_fixture.stream, ZR_NULL);
    g_fixture.parserInitialized = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(g_fixture.parser.lexer);
    TEST_ASSERT_FALSE(g_fixture.parser.hasError);
    g_fixture.parser.suppressErrorOutput = ZR_TRUE;
    g_fixture.ledger = ZrParser_Ownership_Budget(
            g_fixture.parser.lexer->ownership);
    TEST_ASSERT_NOT_NULL(g_fixture.ledger);
    ZrParser_TokenStream_Release(g_fixture.stream);
    g_fixture.stream = ZR_NULL;
    ZrParser_SyntaxContextTable_Release(g_fixture.contextTable);
    g_fixture.contextTable = ZR_NULL;
}

static void capture_primary_probe_rejection(SZrPrimaryRejection *rejection) {
    rejection->rejected = g_fixture.result == ZR_NULL;
    rejection->error = g_fixture.parser.hasError;
    rejection->fatal = g_fixture.parser.hasFatalError;
    rejection->baseFreed = !allocation_is_live(g_fixture.base);
    rejection->exceeded = g_fixture.ledger->exceededResource;
    rejection->usage = g_fixture.ledger->usage.nodeCount;
    rejection->requested = g_fixture.ledger->requestedUsage;
    ZrParser_State_Free(&g_fixture.parser);
    g_fixture.parserInitialized = ZR_FALSE;
    rejection->liveAfterParser = g_liveCount;
    rejection->rootsAfterParser = ZrCore_GcDomain_GetRootCount(g_state);
    fixture_free();
    g_trackAllocations = ZR_FALSE;
}

static void assert_primary_probe_rejection(
        const SZrPrimaryRejection *rejection, TZrSize rootsBefore) {
    TEST_ASSERT_TRUE_MESSAGE(rejection->rejected,
            "Fatal parser allocation failure must return NULL");
    TEST_ASSERT_TRUE(rejection->error);
    TEST_ASSERT_TRUE_MESSAGE(rejection->fatal,
            "Cursor restoration must preserve a fatal node quota failure");
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT, rejection->exceeded);
    TEST_ASSERT_EQUAL_UINT64(rejection->limit, rejection->usage);
    TEST_ASSERT_EQUAL_UINT64(rejection->limit + 1u, rejection->requested);
    TEST_ASSERT_FALSE(g_trackingOverflow);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0u, rejection->liveAfterParser,
            "Failed switch trial must release native parser storage");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(rootsBefore, rejection->rootsAfterParser,
            "Failed switch trial must release AST ownership roots");
}

static void test_switch_struct_object_rejection_consumes_value(void) {
    SZrPrimaryRejection rejection;
    TZrSize rootsBefore;
    EZrToken restoredToken;

    primary_source_fixture_open("Variant {})");
    rootsBefore = g_fixture.rootsBefore;
    g_fixture.base = parse_identifier(&g_fixture.parser);
    TEST_ASSERT_NOT_NULL(g_fixture.base);
    TEST_ASSERT_EQUAL_INT(ZR_TK_LBRACE, g_fixture.parser.lexer->t.token);
    memset(&rejection, 0, sizeof(rejection));
    rejection.limit = g_fixture.ledger->usage.nodeCount;
    TEST_ASSERT_EQUAL_UINT64(1u, rejection.limit);
    g_fixture.ledger->limits.nodeCount = rejection.limit;
    /* The identifier fits; the empty payload's object node is rejected. */
    g_fixture.result = try_parse_switch_struct_variant_payload_case(
            &g_fixture.parser, g_fixture.base);
    restoredToken = g_fixture.parser.lexer->t.token;
    capture_primary_probe_rejection(&rejection);

    assert_primary_probe_rejection(&rejection, rootsBefore);
    TEST_ASSERT_TRUE_MESSAGE(rejection.baseFreed,
            "Fatal struct trial must consume its identifier value");
    TEST_ASSERT_EQUAL_INT(ZR_TK_LBRACE, restoredToken);
}

static void test_switch_move_wrapper_rejection_preserves_fatal(void) {
    SZrPrimaryRejection rejection;
    TZrSize rootsBefore;
    EZrToken restoredToken;

    primary_source_fixture_open("Choice.Item(move payload)");
    rootsBefore = g_fixture.rootsBefore;
    memset(&rejection, 0, sizeof(rejection));
    rejection.limit = 3u;
    g_fixture.ledger->limits.nodeCount = rejection.limit;
    /* Choice, Item and MEMBER fit; PRIMARY is the rejected fourth node. */
    g_fixture.result = try_parse_switch_move_variant_pattern_case(
            &g_fixture.parser);
    restoredToken = g_fixture.parser.lexer->t.token;
    capture_primary_probe_rejection(&rejection);

    assert_primary_probe_rejection(&rejection, rootsBefore);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, restoredToken);
}

static void test_call_succeeds_without_node_limit(void) {
    TZrSize rootsBefore;
    SZrAstNode *call;

    primary_source_fixture_open("base(argument)");
    rootsBefore = g_fixture.rootsBefore;
    g_fixture.result = parse_expression(&g_fixture.parser);
    TEST_ASSERT_NOT_NULL(g_fixture.result);
    TEST_ASSERT_FALSE(g_fixture.parser.hasError);
    TEST_ASSERT_FALSE(g_fixture.parser.hasFatalError);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, g_fixture.parser.lexer->t.token);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, g_fixture.result->type);
    TEST_ASSERT_NOT_NULL(g_fixture.result->data.primaryExpression.property);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL,
            g_fixture.result->data.primaryExpression.property->type);
    TEST_ASSERT_NOT_NULL(g_fixture.result->data.primaryExpression.members);
    TEST_ASSERT_EQUAL_UINT64(
            1u, g_fixture.result->data.primaryExpression.members->count);
    call = g_fixture.result->data.primaryExpression.members->nodes[0];
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, call->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_POSTFIX_ACCESS_DIRECT, call->data.functionCall.accessMode);
    TEST_ASSERT_NOT_NULL(call->data.functionCall.args);
    TEST_ASSERT_EQUAL_UINT64(1u, call->data.functionCall.args->count);
    TEST_ASSERT_NOT_NULL(call->data.functionCall.args->nodes[0]);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL,
            call->data.functionCall.args->nodes[0]->type);
    TEST_ASSERT_EQUAL_UINT64(6u, g_fixture.ledger->usage.nodeCount);
    fixture_free();
    g_trackAllocations = ZR_FALSE;

    TEST_ASSERT_FALSE(g_trackingOverflow);
    TEST_ASSERT_EQUAL_UINT64(0u, g_liveCount);
    TEST_ASSERT_EQUAL_UINT64(rootsBefore, ZrCore_GcDomain_GetRootCount(g_state));
}

static void expect_member_segment_rejection(
        const char *source, TZrUInt64 nodeLimit) {
    SZrPrimaryRejection rejection;
    TZrSize rootsBefore;

    primary_source_fixture_open(source);
    rootsBefore = g_fixture.rootsBefore;
    g_fixture.base = parse_identifier(&g_fixture.parser);
    TEST_ASSERT_NOT_NULL(g_fixture.base);
    memset(&rejection, 0, sizeof(rejection));
    rejection.limit = nodeLimit;
    g_fixture.ledger->limits.nodeCount = nodeLimit;
    g_fixture.result = parse_member_access(&g_fixture.parser, g_fixture.base);
    capture_primary_probe_rejection(&rejection);

    assert_primary_probe_rejection(&rejection, rootsBefore);
    TEST_ASSERT_TRUE(rejection.baseFreed);
}

static void test_generic_call_node_rejection_frees_arguments(void) {
    expect_member_segment_rejection("base<1>(argument)", 4u);
}

static void test_direct_member_node_rejection_consumes_base(void) {
    expect_member_segment_rejection("base.name", 2u);
}

static void test_optional_member_node_rejection_consumes_base(void) {
    expect_member_segment_rejection("base?.name", 2u);
}

static void test_index_member_node_rejection_consumes_base(void) {
    expect_member_segment_rejection("base[index]", 3u);
}

static void test_prototype_constructor_rejection_frees_arguments(void) {
    SZrPrimaryRejection rejection;
    TZrSize rootsBefore;

    primary_source_fixture_open("base(argument)");
    rootsBefore = g_fixture.rootsBefore;
    g_fixture.pendingProperty = parse_identifier(&g_fixture.parser);
    TEST_ASSERT_NOT_NULL(g_fixture.pendingProperty);
    g_fixture.base = create_prototype_reference_node(
            &g_fixture.parser, g_fixture.pendingProperty,
            g_fixture.pendingProperty->location);
    TEST_ASSERT_NOT_NULL(g_fixture.base);
    g_fixture.pendingProperty = ZR_NULL;
    TEST_ASSERT_EQUAL_INT(ZR_TK_LPAREN, g_fixture.parser.lexer->t.token);
    memset(&rejection, 0, sizeof(rejection));
    rejection.limit = 4u;
    g_fixture.ledger->limits.nodeCount = rejection.limit;
    g_fixture.result = parse_postfix_call_segment(
            &g_fixture.parser, g_fixture.base, g_fixture.base->location,
            g_fixture.base->location, ZR_POSTFIX_ACCESS_DIRECT);
    capture_primary_probe_rejection(&rejection);

    assert_primary_probe_rejection(&rejection, rootsBefore);
    TEST_ASSERT_TRUE(rejection.baseFreed);
}

static void test_switch_branches_succeed_and_release_storage(void) {
    TZrSize rootsBefore;

    primary_source_fixture_open("switch (subject) { (first) {} () {} (last) {} }");
    rootsBefore = g_fixture.rootsBefore;
    g_fixture.result = parse_switch_expression(&g_fixture.parser);
    TEST_ASSERT_NOT_NULL(g_fixture.result);
    TEST_ASSERT_FALSE(g_fixture.parser.hasError);
    TEST_ASSERT_FALSE(g_fixture.parser.hasFatalError);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, g_fixture.parser.lexer->t.token);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SWITCH_EXPRESSION, g_fixture.result->type);
    TEST_ASSERT_EQUAL_UINT64(2u, g_fixture.result->data.switchExpression.cases->count);
    TEST_ASSERT_NOT_NULL(g_fixture.result->data.switchExpression.defaultCase);
    fixture_free();
    g_trackAllocations = ZR_FALSE;
    TEST_ASSERT_FALSE(g_trackingOverflow);
    TEST_ASSERT_EQUAL_UINT64(0u, g_liveCount);
    TEST_ASSERT_EQUAL_UINT64(rootsBefore, ZrCore_GcDomain_GetRootCount(g_state));
}

static void expect_switch_wrapper_rejection(const char *source) {
    SZrPrimaryRejection rejection;
    TZrUInt64 nodeCount;
    TZrSize rootsBefore;

    primary_source_fixture_open(source);
    g_fixture.result = parse_switch_expression(&g_fixture.parser);
    TEST_ASSERT_NOT_NULL(g_fixture.result);
    TEST_ASSERT_FALSE(g_fixture.parser.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, g_fixture.parser.lexer->t.token);
    nodeCount = g_fixture.ledger->usage.nodeCount;
    TEST_ASSERT_GREATER_THAN_UINT64(2u, nodeCount);
    fixture_free();
    TEST_ASSERT_EQUAL_UINT64(0u, g_liveCount);

    primary_source_fixture_open(source);
    rootsBefore = g_fixture.rootsBefore;
    memset(&rejection, 0, sizeof(rejection));
    /* Reject the branch wrapper after its block, before the final switch node. */
    rejection.limit = nodeCount - 2u;
    g_fixture.ledger->limits.nodeCount = rejection.limit;
    g_fixture.result = parse_switch_expression(&g_fixture.parser);
    capture_primary_probe_rejection(&rejection);
    assert_primary_probe_rejection(&rejection, rootsBefore);
}

static void test_switch_default_wrapper_rejection_releases_block(void) {
    expect_switch_wrapper_rejection("switch (subject) { () {} }");
}

static void test_switch_case_wrapper_rejection_releases_block_and_value(void) {
    expect_switch_wrapper_rejection("switch (subject) { (value) {} }");
}

static void expect_switch_syntax_failure(const char *source) {
    SZrPrimaryRejection rejection;
    TZrSize rootsBefore;

    primary_source_fixture_open(source);
    rootsBefore = g_fixture.rootsBefore;
    memset(&rejection, 0, sizeof(rejection));
    g_fixture.result = parse_switch_expression(&g_fixture.parser);
    capture_primary_probe_rejection(&rejection);
    TEST_ASSERT_TRUE(rejection.rejected);
    TEST_ASSERT_TRUE(rejection.error);
    TEST_ASSERT_FALSE(rejection.fatal);
    TEST_ASSERT_FALSE(g_trackingOverflow);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0u, rejection.liveAfterParser,
            "Rejected switch must release earlier case and default subtrees");
    TEST_ASSERT_EQUAL_UINT64(rootsBefore, rejection.rootsAfterParser);
}

static void test_switch_bad_later_header_releases_adopted_branches(void) {
    expect_switch_syntax_failure(
            "switch (subject) { (first) {} () {} (last {} }");
}

static void test_switch_missing_later_body_releases_adopted_branches(void) {
    expect_switch_syntax_failure(
            "switch (subject) { (first) {} () {} (last) }");
}

static void test_switch_duplicate_default_is_rejected_without_leaks(void) {
    expect_switch_syntax_failure("switch (subject) { () {} () {} }");
}

static void expect_generic_constructors(TZrBool fromStream) {
    static const char *sources[] = {
        "new Target()",
        "new container.Pair<int, string>(2, \"even\")",
        "new container.Array<Pair<int, string>>()",
        "new container.Array<Pair<int, string> >()",
        "new container.Array<Array<Pair<int, string>>>()",
        "xs.add(new container.Pair<int, string>(2, \"even\"))"
    };
    TZrSize index;

    for (index = 0u; index < sizeof(sources) / sizeof(sources[0]); index++) {
        TZrSize rootsBefore;

        if (fromStream) {
            primary_stream_fixture_open(sources[index]);
        } else {
            primary_source_fixture_open(sources[index]);
        }
        rootsBefore = g_fixture.rootsBefore;
        g_fixture.result = parse_expression(&g_fixture.parser);
        TEST_ASSERT_NOT_NULL_MESSAGE(g_fixture.result,
                g_fixture.parser.errorMessage != ZR_NULL
                        ? g_fixture.parser.errorMessage : sources[index]);
        TEST_ASSERT_FALSE_MESSAGE(g_fixture.parser.hasError, sources[index]);
        TEST_ASSERT_FALSE(g_fixture.parser.hasFatalError);
        TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, g_fixture.parser.lexer->t.token);
        fixture_free();
        g_trackAllocations = ZR_FALSE;
        TEST_ASSERT_FALSE(g_trackingOverflow);
        TEST_ASSERT_EQUAL_UINT64(0u, g_liveCount);
        TEST_ASSERT_EQUAL_UINT64(rootsBefore, ZrCore_GcDomain_GetRootCount(g_state));
    }
}

static void test_nested_generic_constructor_succeeds_and_releases_storage(void) {
    expect_generic_constructors(ZR_FALSE);
}

static void test_token_stream_generic_constructor_succeeds_and_releases_storage(void) {
    expect_generic_constructors(ZR_TRUE);
}

static void expect_closing_angle_consumption(TZrBool fromStream) {
    TZrSize rootsBefore;

    if (fromStream) {
        primary_stream_fixture_open(">> next");
    } else {
        primary_source_fixture_open(">> next");
    }
    rootsBefore = g_fixture.rootsBefore;
    TEST_ASSERT_EQUAL_INT(ZR_TK_RIGHT_SHIFT, g_fixture.parser.lexer->t.token);
    TEST_ASSERT_TRUE(consume_type_closing_angle(&g_fixture.parser));
    TEST_ASSERT_EQUAL_INT(ZR_TK_GREATER_THAN, g_fixture.parser.lexer->t.token);
    TEST_ASSERT_EQUAL_UINT64(1u, g_fixture.parser.lexer->t.rawLength);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(1u,
            g_fixture.parser.lexer->t.span.startOffset,
            "Consuming one closing angle must leave the second piece current");
    TEST_ASSERT_EQUAL_UINT64(2u, g_fixture.parser.lexer->t.span.endOffset);
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, peek_token(&g_fixture.parser));
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, peek_token(&g_fixture.parser));
    TEST_ASSERT_TRUE(consume_type_closing_angle(&g_fixture.parser));
    TEST_ASSERT_EQUAL_INT(ZR_TK_IDENTIFIER, g_fixture.parser.lexer->t.token);
    g_fixture.result = parse_identifier(&g_fixture.parser);
    TEST_ASSERT_NOT_NULL(g_fixture.result);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, g_fixture.parser.lexer->t.token);
    fixture_free();
    g_trackAllocations = ZR_FALSE;
    TEST_ASSERT_FALSE(g_trackingOverflow);
    TEST_ASSERT_EQUAL_UINT64(0u, g_liveCount);
    TEST_ASSERT_EQUAL_UINT64(rootsBefore, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_text_closing_angle_consumes_first_split_piece(void) {
    expect_closing_angle_consumption(ZR_FALSE);
}

static void test_token_stream_closing_angle_consumes_first_split_piece(void) {
    expect_closing_angle_consumption(ZR_TRUE);
}

static void expect_complete_script(const char *source, TZrBool fromStream) {
    TZrSize rootsBefore;
    char storageMessage[128];

    if (fromStream) {
        primary_stream_fixture_open(source);
    } else {
        primary_source_fixture_open(source);
    }
    rootsBefore = g_fixture.rootsBefore;
    g_fixture.result = ZrParser_ParseWithState(&g_fixture.parser);
    TEST_ASSERT_FALSE_MESSAGE(g_fixture.parser.hasError,
            g_fixture.parser.errorMessage != ZR_NULL
                    ? g_fixture.parser.errorMessage : source);
    TEST_ASSERT_FALSE(g_fixture.parser.hasFatalError);
    TEST_ASSERT_NOT_NULL(g_fixture.result);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, g_fixture.parser.lexer->t.token);
    fixture_free();
    g_trackAllocations = ZR_FALSE;
    TEST_ASSERT_FALSE(g_trackingOverflow);
    snprintf(storageMessage, sizeof(storageMessage),
            "Remaining parser allocations: %zu; first size: %zu; AST kind: %d",
            (size_t)g_liveCount,
            g_liveCount != 0u ? (size_t)g_liveAllocationSizes[0] : 0u,
            g_liveCount != 0u && g_liveAllocationSizes[0] == sizeof(SZrAstNode)
                    ? (int)((SZrAstNode *)g_liveAllocations[0])->type : -1);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0u, g_liveCount, storageMessage);
    TEST_ASSERT_EQUAL_UINT64(rootsBefore, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_source_import_member_chain_preserves_parser_success(void) {
    expect_complete_script(
            "import(\"helper\").toolkit.math.greet();", ZR_FALSE);
}

static void test_token_stream_import_member_chain_preserves_parser_success(void) {
    expect_complete_script(
            "import(\"helper\").toolkit.math.greet();", ZR_TRUE);
}

static void test_source_explicit_generic_call_preserves_parser_success(void) {
    expect_complete_script(
            "let {Array} = import(\"zr.container\");\n"
            "fn map<TIn, TOut>(source: Array<TIn>): Array<TOut> { return source; }\n"
            "map<int, string>(values);", ZR_FALSE);
}

static void test_token_stream_explicit_generic_call_preserves_parser_success(void) {
    expect_complete_script(
            "let {Array} = import(\"zr.container\");\n"
            "fn map<TIn, TOut>(source: Array<TIn>): Array<TOut> { return source; }\n"
            "map<int, string>(values);", ZR_TRUE);
}

static void test_generic_call_alone_releases_probe_storage(void) {
    expect_complete_script("map<int, string>(values);", ZR_FALSE);
}

static void test_generic_function_alone_releases_parser_storage(void) {
    expect_complete_script(
            "fn map<TIn, TOut>(source: Array<TIn>): Array<TOut> { return source; }",
            ZR_FALSE);
}

static void test_destructured_import_alone_releases_parser_storage(void) {
    expect_complete_script("let {Array} = import(\"zr.container\");", ZR_FALSE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_new_primary_member_succeeds_without_limit);
    RUN_TEST(test_existing_empty_primary_succeeds_without_limit);
    RUN_TEST(test_existing_full_primary_succeeds_without_limit);
    RUN_TEST(test_wrapper_ast_heap_rejection_frees_inputs);
    RUN_TEST(test_wrapper_members_heap_rejection_frees_inputs);
    RUN_TEST(test_existing_empty_members_heap_rejection_frees_inputs);
    RUN_TEST(test_existing_members_growth_heap_rejection_frees_inputs);
    RUN_TEST(test_call_wrapper_rejection_consumes_base_once);
    RUN_TEST(test_optional_call_wrapper_rejection_consumes_base_once);
    RUN_TEST(test_new_constructor_rejection_frees_target_and_arguments);
    RUN_TEST(test_own_constructor_rejection_frees_target_and_arguments);
    RUN_TEST(test_reference_constructor_rejection_frees_target);
    RUN_TEST(test_switch_struct_object_rejection_consumes_value);
    RUN_TEST(test_switch_move_wrapper_rejection_preserves_fatal);
    RUN_TEST(test_call_succeeds_without_node_limit);
    RUN_TEST(test_generic_call_node_rejection_frees_arguments);
    RUN_TEST(test_direct_member_node_rejection_consumes_base);
    RUN_TEST(test_optional_member_node_rejection_consumes_base);
    RUN_TEST(test_index_member_node_rejection_consumes_base);
    RUN_TEST(test_prototype_constructor_rejection_frees_arguments);
    RUN_TEST(test_switch_branches_succeed_and_release_storage);
    RUN_TEST(test_switch_default_wrapper_rejection_releases_block);
    RUN_TEST(test_switch_case_wrapper_rejection_releases_block_and_value);
    RUN_TEST(test_switch_bad_later_header_releases_adopted_branches);
    RUN_TEST(test_switch_missing_later_body_releases_adopted_branches);
    RUN_TEST(test_switch_duplicate_default_is_rejected_without_leaks);
    RUN_TEST(test_nested_generic_constructor_succeeds_and_releases_storage);
    RUN_TEST(test_token_stream_generic_constructor_succeeds_and_releases_storage);
    RUN_TEST(test_text_closing_angle_consumes_first_split_piece);
    RUN_TEST(test_token_stream_closing_angle_consumes_first_split_piece);
    RUN_TEST(test_source_import_member_chain_preserves_parser_success);
    RUN_TEST(test_token_stream_import_member_chain_preserves_parser_success);
    RUN_TEST(test_source_explicit_generic_call_preserves_parser_success);
    RUN_TEST(test_token_stream_explicit_generic_call_preserves_parser_success);
    RUN_TEST(test_generic_call_alone_releases_probe_storage);
    RUN_TEST(test_generic_function_alone_releases_parser_storage);
    RUN_TEST(test_destructured_import_alone_releases_parser_storage);
    return UNITY_END();
}
