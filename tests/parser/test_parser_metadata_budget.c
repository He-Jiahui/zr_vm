#include "unity.h"
#include "harness/runtime_support.h"
#include "parser/parser_internal.h"
#include "parser/parser_property_migration.h"
#include "parser/parser_ownership.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/token_stream.h"

#include <string.h>

enum { METADATA_TRACKED_ALLOCATION_LIMIT = 256 };

typedef struct SZrMetadataFixture {
    SZrParserState parser;
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudget *ledger;
    SZrSyntaxContextTable *table;
    SZrTokenStream *stream;
    SZrAstNodeArray *arguments;
    SZrArray *names;
    SZrArray *markers;
    SZrType *type;
    SZrGenericDeclaration *generic;
    SZrAstNode *property;
    SZrLegacyPropertyMigrationCollection migration;
    TZrBool parserInitialized;
    TZrSize rootsBefore;
} SZrMetadataFixture;

typedef struct SZrMetadataRejection {
    TZrBool rejected;
    TZrBool fatal;
    TZrBool outputsCleared;
    EZrParserComptimeBudgetResource exceeded;
    TZrUInt64 before;
    TZrUInt64 usage;
    TZrUInt64 limit;
} SZrMetadataRejection;

static SZrState *g_state;
static SZrMetadataFixture g_fixture;
static FZrAllocator g_allocator;
static TZrPtr g_liveAllocations[METADATA_TRACKED_ALLOCATION_LIMIT];
static TZrSize g_liveCount;
static TZrBool g_trackAllocations;
static TZrBool g_trackingOverflow;
static TZrUInt32 g_migrationDiagnostics;
static TZrUInt32 g_migrationFixes;

/* Observe real native allocations without replacing parser allocation behavior. */
static TZrPtr metadata_allocator(
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
        g_liveAllocations[index] = g_liveAllocations[--g_liveCount];
    }
    if (newSize != 0u && result != ZR_NULL) {
        if (g_liveCount == METADATA_TRACKED_ALLOCATION_LIMIT) {
            g_trackingOverflow = ZR_TRUE;
        } else {
            g_liveAllocations[g_liveCount++] = result;
        }
    }
    return result;
}

static void free_metadata_array(SZrArray **array) {
    if (*array == ZR_NULL) {
        return;
    }
    ZrCore_Array_Free(g_state, *array);
    ZrCore_Memory_RawFreeWithType(
            g_state->global, *array, sizeof(SZrArray),
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    *array = ZR_NULL;
}

static void fixture_free(void) {
    if (g_fixture.migration.entries.isValid &&
        g_fixture.parserInitialized) {
        parser_property_migration_collection_publish_and_free(
                &g_fixture.parser, &g_fixture.migration);
    }
    ZrParser_Ast_Free(g_state, g_fixture.property);
    free_ast_node_array_with_elements(g_state, g_fixture.arguments);
    free_metadata_array(&g_fixture.names);
    free_metadata_array(&g_fixture.markers);
    free_owned_type(g_state, g_fixture.type);
    free_generic_declaration(g_state, g_fixture.generic);
    if (g_fixture.parserInitialized) {
        ZrParser_State_Free(&g_fixture.parser);
    }
    ZrParser_TokenStream_Release(g_fixture.stream);
    ZrParser_SyntaxContextTable_Release(g_fixture.table);
    memset(&g_fixture, 0, sizeof(g_fixture));
}

void setUp(void) {
    memset(&g_fixture, 0, sizeof(g_fixture));
    g_liveCount = 0u;
    g_trackAllocations = ZR_FALSE;
    g_trackingOverflow = ZR_FALSE;
    g_migrationDiagnostics = 0u;
    g_migrationFixes = 0u;
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    g_allocator = g_state->global->allocator;
    g_state->global->allocator = metadata_allocator;
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

static void init_budget(SZrParserComptimeBudget *budget) {
    SZrParserComptimeBudgetLimits limits;
    memset(&limits, 0, sizeof(limits));
    limits.fuel = ZR_PARSER_COMPTIME_DEFAULT_FUEL;
    limits.callDepth = ZR_PARSER_COMPTIME_DEFAULT_CALL_DEPTH;
    limits.heapBytes = ZR_PARSER_COMPTIME_DEFAULT_HEAP_BYTES;
    limits.aggregateCount = ZR_PARSER_COMPTIME_DEFAULT_AGGREGATE_COUNT;
    limits.generatedDeclarationCount =
            ZR_PARSER_COMPTIME_DEFAULT_GENERATED_DECLARATION_COUNT;
    limits.diagnosticCount = ZR_PARSER_COMPTIME_DEFAULT_DIAGNOSTIC_COUNT;
    limits.tokenCount = ZR_PARSER_COMPTIME_DEFAULT_TOKEN_COUNT;
    limits.nodeCount = ZR_PARSER_COMPTIME_DEFAULT_NODE_COUNT;
    limits.matchingCount = ZR_PARSER_COMPTIME_DEFAULT_MATCHING_COUNT;
    limits.expansionCount = ZR_PARSER_COMPTIME_DEFAULT_EXPANSION_COUNT;
    ZrParser_ComptimeBudget_Init(budget, &limits);
}

static void fixture_open(const char *source, TZrBool native) {
    SZrParserCursor cursor;
    SZrString *sourceName;

    g_fixture.rootsBefore = ZrCore_GcDomain_GetRootCount(g_state);
    sourceName = ZrCore_String_CreateFromNative(g_state, "metadata.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    if (native) {
        g_fixture.table = ZrParser_SyntaxContextTable_Create(
                g_state->global, ZR_NULL);
        TEST_ASSERT_NOT_NULL(g_fixture.table);
        TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
                g_state, g_fixture.table,
                "metadata.zr", sizeof("metadata.zr") - 1u,
                source, strlen(source), ZR_NULL, &g_fixture.stream));
        init_budget(&g_fixture.budget);
        ZrParser_State_InitWithTokenStreamBudget(
                &g_fixture.parser, g_state, g_fixture.stream,
                sourceName, &g_fixture.budget);
    } else {
        ZrParser_State_Init(
                &g_fixture.parser, g_state, source, strlen(source), sourceName);
    }
    g_fixture.parserInitialized = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(g_fixture.parser.lexer);
    g_fixture.parser.suppressErrorOutput = ZR_TRUE;
    g_fixture.ledger = ZrParser_Ownership_Budget(
            g_fixture.parser.lexer->ownership);
    TEST_ASSERT_NOT_NULL(g_fixture.ledger);
    if (native) {
        TEST_ASSERT_EQUAL_PTR(&g_fixture.budget, g_fixture.ledger);
    }

    /* Warm ownership/root capacity. Native cursors rematerialize token strings
     * on replay, so later heap limits also cover that decoding work. */
    save_parser_cursor(&g_fixture.parser, &cursor);
    while (g_fixture.parser.lexer->t.token != ZR_TK_EOS) {
        ZrParser_Lexer_Next(g_fixture.parser.lexer);
    }
    TEST_ASSERT_FALSE(g_fixture.parser.hasError);
    restore_parser_cursor(&g_fixture.parser, &cursor);
    g_liveCount = 0u;
    g_trackingOverflow = ZR_FALSE;
    g_trackAllocations = ZR_TRUE;
}

static void fixture_close_and_check_cleanup(void) {
    TZrSize rootsBefore = g_fixture.rootsBefore;
    fixture_free();
    g_trackAllocations = ZR_FALSE;
    TEST_ASSERT_FALSE_MESSAGE(g_trackingOverflow, "Allocation observer must not overflow");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(
            0u, g_liveCount, "Parser must deep-free all newly allocated native storage");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(
            rootsBefore, ZrCore_GcDomain_GetRootCount(g_state),
            "Failed metadata adoption must not retain parser ownership roots");
}

static TZrUInt64 ast_array_storage(TZrSize capacity) {
    return (TZrUInt64)sizeof(SZrAstNodeArray) +
            (TZrUInt64)capacity * (TZrUInt64)sizeof(SZrAstNode *);
}

static TZrUInt64 metadata_array_storage(TZrSize elementSize, TZrSize capacity) {
    return (TZrUInt64)sizeof(SZrArray) +
            (TZrUInt64)elementSize * (TZrUInt64)capacity;
}

static SZrMetadataRejection set_allowance(TZrUInt64 allowance) {
    SZrMetadataRejection rejection;
    memset(&rejection, 0, sizeof(rejection));
    rejection.before = g_fixture.ledger->usage.heapBytes;
    rejection.limit = rejection.before + allowance;
    g_fixture.ledger->limits.heapBytes = rejection.limit;
    return rejection;
}

static void capture_rejection(SZrMetadataRejection *rejection, TZrBool rejected) {
    rejection->rejected = rejected;
    rejection->fatal = g_fixture.parser.hasFatalError;
    rejection->outputsCleared = g_fixture.names == ZR_NULL &&
            g_fixture.markers == ZR_NULL;
    rejection->exceeded = g_fixture.ledger->exceededResource;
    rejection->usage = g_fixture.ledger->usage.heapBytes;
}

static void assert_heap_rejection(const SZrMetadataRejection *rejection) {
    TEST_ASSERT_TRUE_MESSAGE(
            rejection->rejected, "Metadata storage must spend the shared heap budget");
    TEST_ASSERT_TRUE_MESSAGE(rejection->fatal, "Heap quota failure must remain fatal");
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, rejection->exceeded);
    TEST_ASSERT_TRUE(rejection->usage >= rejection->before);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(rejection->limit, rejection->usage);
}

static void parse_call_arguments(TZrBool markers) {
    TEST_ASSERT_TRUE(consume_token(&g_fixture.parser, ZR_TK_LPAREN));
    g_fixture.arguments = parse_argument_list(
            &g_fixture.parser, &g_fixture.names,
            markers ? &g_fixture.markers : ZR_NULL);
}

static void assert_arguments_parse(TZrSize count, TZrBool markers) {
    TEST_ASSERT_NOT_NULL_MESSAGE(g_fixture.arguments, "Argument fixture must parse without a quota");
    TEST_ASSERT_FALSE(g_fixture.parser.hasError);
    TEST_ASSERT_FALSE(g_fixture.parser.hasFatalError);
    TEST_ASSERT_EQUAL_UINT64(count, g_fixture.arguments->count);
    TEST_ASSERT_EQUAL_INT(ZR_TK_RPAREN, g_fixture.parser.lexer->t.token);
    if (count != 0u) {
        TEST_ASSERT_NOT_NULL(g_fixture.names);
        TEST_ASSERT_EQUAL_UINT64(count, g_fixture.names->length);
    } else {
        TEST_ASSERT_NULL(g_fixture.names);
    }
    if (markers) {
        TEST_ASSERT_NOT_NULL(g_fixture.markers);
        TEST_ASSERT_TRUE(g_fixture.markers->isValid);
        TEST_ASSERT_EQUAL_UINT64(count, g_fixture.markers->length);
    }
}

static TZrUInt64 parse_call_heap_usage(
        const char *source, TZrSize count, TZrBool names,
        TZrSize *outNameCapacity) {
    TZrUInt64 before;
    TZrUInt64 usage;

    fixture_open(source, ZR_TRUE);
    before = g_fixture.ledger->usage.heapBytes;
    TEST_ASSERT_TRUE(consume_token(&g_fixture.parser, ZR_TK_LPAREN));
    g_fixture.arguments = parse_argument_list(
            &g_fixture.parser, names ? &g_fixture.names : ZR_NULL, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_fixture.arguments);
    TEST_ASSERT_FALSE(g_fixture.parser.hasError);
    TEST_ASSERT_FALSE(g_fixture.parser.hasFatalError);
    TEST_ASSERT_EQUAL_UINT64(count, g_fixture.arguments->count);
    TEST_ASSERT_EQUAL_INT(ZR_TK_RPAREN, g_fixture.parser.lexer->t.token);
    usage = g_fixture.ledger->usage.heapBytes - before;
    if (names) {
        TEST_ASSERT_NOT_NULL(g_fixture.names);
        TEST_ASSERT_EQUAL_UINT64(count, g_fixture.names->length);
        *outNameCapacity = g_fixture.names->capacity;
    } else {
        TEST_ASSERT_NULL(g_fixture.names);
    }
    fixture_close_and_check_cleanup();
    return usage;
}

static void test_name_metadata_initial_heap_delta(void) {
    const char *source = "(name: 1)";
    TZrSize nameCapacity = 0u;
    TZrUInt64 withoutNames = parse_call_heap_usage(
            source, 1u, ZR_FALSE, &nameCapacity);
    TZrUInt64 withNames = parse_call_heap_usage(
            source, 1u, ZR_TRUE, &nameCapacity);

    TEST_ASSERT_EQUAL_UINT64(ZR_PARSER_INITIAL_CAPACITY_TINY, nameCapacity);
    TEST_ASSERT_GREATER_THAN_UINT64(withoutNames, withNames);
    TEST_ASSERT_EQUAL_UINT64(
            metadata_array_storage(sizeof(SZrString *), nameCapacity),
            withNames - withoutNames);
}

static void test_name_metadata_growth_heap_delta(void) {
    const char *source = "(a: 1, b: 2, c: 3, d: 4, e: 5)";
    TZrSize nameCapacity = 0u;
    TZrUInt64 withoutNames = parse_call_heap_usage(
            source, 5u, ZR_FALSE, &nameCapacity);
    TZrUInt64 withNames = parse_call_heap_usage(
            source, 5u, ZR_TRUE, &nameCapacity);

    TEST_ASSERT_GREATER_THAN_UINT64(ZR_PARSER_INITIAL_CAPACITY_TINY, nameCapacity);
    TEST_ASSERT_GREATER_THAN_UINT64(withoutNames, withNames);
    TEST_ASSERT_EQUAL_UINT64(
            metadata_array_storage(
                    sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_TINY) +
                    (TZrUInt64)nameCapacity * sizeof(SZrString *),
            withNames - withoutNames);
}

static void test_named_argument_metadata_spends_shared_heap(void) {
    const char *source = "(name: 1)";
    SZrMetadataRejection rejection;
    SZrString **name;

    fixture_open(source, ZR_TRUE);
    parse_call_arguments(ZR_FALSE);
    assert_arguments_parse(1u, ZR_FALSE);
    name = (SZrString **)ZrCore_Array_Get(g_fixture.names, 0u);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("name", ZrCore_String_GetNativeString(*name));
    fixture_close_and_check_cleanup();

    fixture_open(source, ZR_TRUE);
    rejection = set_allowance(
            ast_array_storage(ZR_PARSER_INITIAL_CAPACITY_TINY) + sizeof(SZrAstNode));
    parse_call_arguments(ZR_FALSE);
    capture_rejection(&rejection, g_fixture.arguments == ZR_NULL);
    fixture_close_and_check_cleanup();
    assert_heap_rejection(&rejection);
    TEST_ASSERT_TRUE_MESSAGE(rejection.outputsCleared, "Failed names must not escape to the caller");
}

static void test_empty_call_marker_metadata_spends_shared_heap(void) {
    SZrMetadataRejection rejection;

    fixture_open("()", ZR_TRUE);
    parse_call_arguments(ZR_TRUE);
    assert_arguments_parse(0u, ZR_TRUE);
    fixture_close_and_check_cleanup();

    fixture_open("()", ZR_TRUE);
    rejection = set_allowance(ast_array_storage(ZR_PARSER_INITIAL_CAPACITY_TINY));
    parse_call_arguments(ZR_TRUE);
    capture_rejection(&rejection, g_fixture.arguments == ZR_NULL);
    fixture_close_and_check_cleanup();
    assert_heap_rejection(&rejection);
    TEST_ASSERT_TRUE_MESSAGE(rejection.outputsCleared, "Failed markers must not escape to the caller");
}

static void test_argument_name_growth_spends_shared_heap(void) {
    const char *source = "(a: 1, b: 2, c: 3, d: 4, e: 5)";
    TZrUInt64 allowance;
    TZrSize argumentCapacity;
    SZrMetadataRejection rejection;

    fixture_open(source, ZR_TRUE);
    parse_call_arguments(ZR_FALSE);
    assert_arguments_parse(5u, ZR_FALSE);
    argumentCapacity = g_fixture.arguments->capacity;
    TEST_ASSERT_GREATER_THAN_UINT64(ZR_PARSER_INITIAL_CAPACITY_TINY, argumentCapacity);
    TEST_ASSERT_GREATER_THAN_UINT64(ZR_PARSER_INITIAL_CAPACITY_TINY, g_fixture.names->capacity);
    fixture_close_and_check_cleanup();

    allowance = ast_array_storage(ZR_PARSER_INITIAL_CAPACITY_TINY) +
            (TZrUInt64)argumentCapacity * sizeof(SZrAstNode *) +
            5u * (TZrUInt64)sizeof(SZrAstNode) +
            metadata_array_storage(sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_TINY);
    fixture_open(source, ZR_TRUE);
    rejection = set_allowance(allowance);
    parse_call_arguments(ZR_FALSE);
    capture_rejection(&rejection, g_fixture.arguments == ZR_NULL);
    fixture_close_and_check_cleanup();
    assert_heap_rejection(&rejection);
    TEST_ASSERT_TRUE(rejection.outputsCleared);
}

static void test_argument_marker_growth_spends_shared_heap(void) {
    const char *source = "(ref a, out b, ref c, out d, ref e)";
    TZrUInt64 allowance;
    TZrSize argumentCapacity;
    TZrSize nameCapacity;
    SZrMetadataRejection rejection;
    SZrCallArgumentSyntax *marker;

    fixture_open(source, ZR_TRUE);
    parse_call_arguments(ZR_TRUE);
    assert_arguments_parse(5u, ZR_TRUE);
    argumentCapacity = g_fixture.arguments->capacity;
    nameCapacity = g_fixture.names->capacity;
    TEST_ASSERT_GREATER_THAN_UINT64(ZR_PARSER_INITIAL_CAPACITY_TINY, g_fixture.markers->capacity);
    marker = (SZrCallArgumentSyntax *)ZrCore_Array_Get(g_fixture.markers, 1u);
    TEST_ASSERT_NOT_NULL(marker);
    TEST_ASSERT_EQUAL_INT(ZR_CALL_ARGUMENT_MARKER_OUT, marker->marker);
    fixture_close_and_check_cleanup();

    allowance = ast_array_storage(ZR_PARSER_INITIAL_CAPACITY_TINY) +
            (TZrUInt64)argumentCapacity * sizeof(SZrAstNode *) +
            5u * (TZrUInt64)sizeof(SZrAstNode) +
            metadata_array_storage(sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_TINY) +
            (TZrUInt64)nameCapacity * sizeof(SZrString *) +
            metadata_array_storage(sizeof(SZrCallArgumentSyntax), ZR_PARSER_INITIAL_CAPACITY_TINY);
    fixture_open(source, ZR_TRUE);
    rejection = set_allowance(allowance);
    parse_call_arguments(ZR_TRUE);
    capture_rejection(&rejection, g_fixture.arguments == ZR_NULL);
    fixture_close_and_check_cleanup();
    assert_heap_rejection(&rejection);
    TEST_ASSERT_TRUE(rejection.outputsCleared);
}

static void test_type_shell_spends_shared_heap(void) {
    SZrMetadataRejection rejection;

    fixture_open("Probe", ZR_TRUE);
    g_fixture.type = parse_type(&g_fixture.parser);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_fixture.type, "Type fixture must parse without a quota");
    TEST_ASSERT_FALSE(g_fixture.parser.hasError);
    TEST_ASSERT_NOT_NULL(g_fixture.type->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, g_fixture.type->name->type);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, g_fixture.parser.lexer->t.token);
    fixture_close_and_check_cleanup();

    fixture_open("Probe", ZR_TRUE);
    rejection = set_allowance(sizeof(SZrAstNode));
    g_fixture.type = parse_type(&g_fixture.parser);
    capture_rejection(&rejection, g_fixture.type == ZR_NULL);
    fixture_close_and_check_cleanup();
    assert_heap_rejection(&rejection);
}

static void test_generic_declaration_shell_spends_shared_heap(void) {
    SZrMetadataRejection rejection;

    fixture_open("<T>", ZR_TRUE);
    g_fixture.generic = parse_generic_declaration(&g_fixture.parser, ZR_FALSE);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_fixture.generic, "Generic fixture must parse without a quota");
    TEST_ASSERT_FALSE(g_fixture.parser.hasError);
    TEST_ASSERT_NOT_NULL(g_fixture.generic->params);
    TEST_ASSERT_EQUAL_UINT64(1u, g_fixture.generic->params->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PARAMETER, g_fixture.generic->params->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, g_fixture.parser.lexer->t.token);
    fixture_close_and_check_cleanup();

    fixture_open("<T>", ZR_TRUE);
    rejection = set_allowance(
            ast_array_storage(ZR_PARSER_INITIAL_CAPACITY_TINY) +
            2u * (TZrUInt64)sizeof(SZrAstNode));
    g_fixture.generic = parse_generic_declaration(&g_fixture.parser, ZR_FALSE);
    capture_rejection(&rejection, g_fixture.generic == ZR_NULL);
    fixture_close_and_check_cleanup();
    assert_heap_rejection(&rejection);
}

static void capture_migration_diagnostic(
        TZrPtr userData, const SZrStructuredDiagnostic *diagnostic, EZrToken token) {
    const char *code;
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(token);
    if (diagnostic == ZR_NULL || diagnostic->code == ZR_NULL) {
        return;
    }
    code = ZrCore_String_GetNativeString(diagnostic->code);
    if (code != ZR_NULL && strcmp(code, "legacy_property_syntax") == 0) {
        g_migrationDiagnostics++;
        g_migrationFixes += (TZrUInt32)diagnostic->fixes.length;
    }
}

static void prepare_property_migration(void) {
    /* Migration replacements deliberately read the original source spelling. */
    fixture_open("get value: int;", ZR_FALSE);
    g_fixture.parser.enableLegacyMigrationParsing = ZR_TRUE;
    g_fixture.parser.structuredErrorCallback = capture_migration_diagnostic;
    g_migrationDiagnostics = 0u;
    g_migrationFixes = 0u;
    g_fixture.property = parse_class_property(&g_fixture.parser);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_fixture.property, "Migration fixture must parse before publication");
    TEST_ASSERT_FALSE(g_fixture.parser.hasError);
    TEST_ASSERT_FALSE(g_fixture.parser.hasFatalError);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_PROPERTY, g_fixture.property->type);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, g_fixture.parser.lexer->t.token);
    parser_property_migration_collection_init(g_state, &g_fixture.migration);
    TEST_ASSERT_TRUE(parser_property_migration_collection_append(
            g_state, &g_fixture.migration, g_fixture.property));
    g_fixture.property = ZR_NULL;
}

static void expect_property_publication_heap_rejection(TZrUInt64 allowance) {
    SZrMetadataRejection rejection;
    TZrBool entriesFreed;
    TZrUInt32 published;

    prepare_property_migration();
    parser_property_migration_collection_publish_and_free(
            &g_fixture.parser, &g_fixture.migration);
    TEST_ASSERT_TRUE(g_fixture.parser.hasError);
    TEST_ASSERT_FALSE(g_fixture.parser.hasFatalError);
    TEST_ASSERT_EQUAL_UINT32(1u, g_migrationDiagnostics);
    TEST_ASSERT_EQUAL_UINT32(1u, g_migrationFixes);
    fixture_close_and_check_cleanup();

    prepare_property_migration();
    rejection = set_allowance(allowance);
    parser_property_migration_collection_publish_and_free(
            &g_fixture.parser, &g_fixture.migration);
    capture_rejection(&rejection, g_fixture.parser.hasFatalError);
    entriesFreed = !g_fixture.migration.entries.isValid &&
            g_fixture.migration.entries.head == ZR_NULL;
    published = g_migrationDiagnostics;
    fixture_close_and_check_cleanup();
    assert_heap_rejection(&rejection);
    TEST_ASSERT_TRUE_MESSAGE(entriesFreed, "Rejected diagnostics must release adopted property ASTs");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, published, "No migration fix may escape a rejected heap reservation");
}

static void test_property_migration_handled_flags_spend_shared_heap(void) {
    expect_property_publication_heap_rejection(0u);
}

static void test_property_migration_replacement_spends_shared_heap(void) {
    expect_property_publication_heap_rejection(sizeof(TZrBool));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_named_argument_metadata_spends_shared_heap);
    RUN_TEST(test_empty_call_marker_metadata_spends_shared_heap);
    RUN_TEST(test_argument_name_growth_spends_shared_heap);
    RUN_TEST(test_argument_marker_growth_spends_shared_heap);
    RUN_TEST(test_type_shell_spends_shared_heap);
    RUN_TEST(test_generic_declaration_shell_spends_shared_heap);
    RUN_TEST(test_property_migration_handled_flags_spend_shared_heap);
    RUN_TEST(test_property_migration_replacement_spends_shared_heap);
    RUN_TEST(test_name_metadata_initial_heap_delta);
    RUN_TEST(test_name_metadata_growth_heap_delta);
    return UNITY_END();
}
