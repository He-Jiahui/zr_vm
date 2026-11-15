#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "interface/lsp_interface_internal.h"
#include "lsp_query_result_cleanup.h"
#include "runtime_support.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s:%d: %s\n", __func__, __LINE__, #condition); \
        return ZR_FALSE; \
    } \
} while (0)

typedef struct Fixture {
    SZrState *state;
    SZrLspContext *context;
    SZrSemanticContext *semantic;
    SZrString *uri;
    const char *text;
} Fixture;

/* Text matching only locates known test tokens; binding expectations below are
 * checked against producer SymbolIds, never inferred from spelling. */
static SZrLspPosition token_position(const char *text, const char *token, TZrSize occurrence) {
    const char *match = strstr(text, token);
    SZrLspPosition position = {0, 0};
    for (TZrSize i = 0U; match != ZR_NULL && i < occurrence; ++i) {
        match = strstr(match + strlen(token), token);
    }
    assert(match != ZR_NULL);
    for (const char *cursor = text; cursor < match; ++cursor) {
        if (*cursor == '\n') {
            ++position.line;
            position.character = 0;
        } else {
            ++position.character;
        }
    }
    return position;
}

static SZrFileRange token_range(Fixture *fixture, const char *token, TZrSize occurrence) {
    SZrLspPosition position = token_position(fixture->text, token, occurrence);
    SZrFileRange range = {0};
    range.start = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            fixture->context, fixture->uri, position);
    position.character += (TZrInt32)strlen(token);
    range.end = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            fixture->context, fixture->uri, position);
    range.source = fixture->uri;
    return range;
}

static TZrSymbolId symbol_id(Fixture *fixture, const char *token, TZrSize occurrence) {
    SZrParserSemanticSymbolQuery query = {0};
    if (!ZrParser_SemanticQuery_SymbolAt(fixture->semantic,
                token_range(fixture, token, occurrence), ZR_NULL, &query)) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    return query.symbolId;
}

static TZrBool expect_reference(Fixture *fixture, const char *token, TZrSize occurrence,
                                TZrSymbolId expected, EZrSemanticReferenceKind kind) {
    SZrSemanticContext *context = fixture->semantic;
    SZrFileRange range = token_range(fixture, token, occurrence);
    const SZrSemanticReferenceFact *declaration =
            ZrParser_SemanticQuery_DeclarationOf(context, expected, ZR_NULL);
    TZrBool found = ZR_FALSE;
    REQUIRE(declaration != ZR_NULL);
    for (TZrSize i = 0; i < context->referenceFacts.length; ++i) {
        const SZrSemanticReferenceFact *fact = ZrCore_Array_Get(&context->referenceFacts, i);
        if (fact->range.start.offset != range.start.offset ||
            fact->range.end.offset != range.end.offset || fact->kind != kind) {
            continue;
        }
        REQUIRE(fact->isResolved && fact->symbolId == expected);
        REQUIRE(fact->originKind == ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION);
        REQUIRE(fact->declarationRange.start.offset == declaration->range.start.offset);
        REQUIRE(fact->typeId != ZR_SEMANTIC_ID_INVALID);
        found = ZR_TRUE;
    }
    REQUIRE(found);
    return ZR_TRUE;
}

static TZrBool expect_projection(Fixture *fixture, const char *token, TZrSize occurrence,
                                 TZrSize count) {
    SZrArray locations = {0};
    SZrArray renamed = {0};
    SZrLspPosition position = token_position(fixture->text, token, occurrence);
    TZrSymbolId expected = symbol_id(fixture, token, occurrence);
    SZrString *name = ZR_STRING_LITERAL(fixture->state, "renamedIndex");
    TZrBool passed = ZrLanguageServer_Lsp_FindReferences(fixture->state, fixture->context,
            fixture->uri, position, ZR_TRUE, &locations) && locations.length == count &&
            ZrLanguageServer_Lsp_Rename(fixture->state, fixture->context,
                    fixture->uri, position, name, &renamed) && renamed.length == count;
    for (TZrSize i = 0U; passed && i < renamed.length; ++i) {
        const SZrLspLocation *location = *(SZrLspLocation **)ZrCore_Array_Get(&renamed, i);
        SZrLspPosition start = location->range.start;
        SZrFileRange range = {0};
        SZrParserSemanticSymbolQuery query = {0};
        range.start = ZrLanguageServer_Lsp_GetDocumentFilePosition(fixture->context, fixture->uri, start);
        range.end = range.start;
        range.source = fixture->uri;
        passed = ZrCore_String_Equal(location->uri, fixture->uri) &&
                 location->range.end.line == start.line &&
                 location->range.end.character == start.character + (TZrInt32)strlen(token) &&
                 ZrParser_SemanticQuery_SymbolAt(fixture->semantic, range, ZR_NULL, &query) &&
                 query.symbolId == expected;
        for (TZrSize j = 0U; passed && j < i; ++j) {
            const SZrLspLocation *previous = *(SZrLspLocation **)ZrCore_Array_Get(&renamed, j);
            passed = previous->range.start.line != start.line ||
                     previous->range.start.character != start.character;
        }
    }
    free_local_reference_projection_results(fixture->state, &locations, ZR_NULL);
    free_local_reference_projection_results(fixture->state, &renamed, ZR_NULL);
    REQUIRE(passed);
    return ZR_TRUE;
}

static TZrBool expect_condition_facts(Fixture *fixture) {
    SZrSemanticContext *context = fixture->semantic;
    SZrFileRange range = token_range(fixture, "< ", 0U);
    const SZrSemanticExpressionFact *binary = ZrParser_SemanticFacts_FindExpressionAtPosition(context, range);
    const SZrSemanticExpressionFact *left;
    const SZrSemanticExpressionFact *right;
    REQUIRE(binary != ZR_NULL && binary->node != ZR_NULL);
    REQUIRE(binary->node->type == ZR_AST_BINARY_EXPRESSION);
    left = ZrParser_SemanticFacts_FindExpressionByNode(context, binary->node->data.binaryExpression.left);
    right = ZrParser_SemanticFacts_FindExpressionByNode(context, binary->node->data.binaryExpression.right);
    REQUIRE(left != ZR_NULL && right != ZR_NULL);
    REQUIRE(left->inferredType.baseType == ZR_VALUE_TYPE_INT64);
    REQUIRE(!left->inferredType.hasRangeConstraint ||
            left->inferredType.minValue < left->inferredType.maxValue);
    REQUIRE(right->inferredType.baseType == ZR_VALUE_TYPE_INT64);
    REQUIRE(binary->inferredType.baseType == ZR_VALUE_TYPE_BOOL);
    REQUIRE(left->typeId != 0U && right->typeId != 0U && binary->typeId != 0U);
    REQUIRE(left->exactness == ZR_SEMANTIC_FACT_EXACT &&
            right->exactness == ZR_SEMANTIC_FACT_EXACT && binary->exactness == ZR_SEMANTIC_FACT_EXACT);
    return ZR_TRUE;
}

static TZrBool expect_no_diagnostics(Fixture *fixture) {
    SZrArray diagnostics = {0};
    TZrBool passed = ZrLanguageServer_Lsp_GetDiagnostics(fixture->state, fixture->context,
            fixture->uri, &diagnostics) && diagnostics.length == 0U;
    ZrLanguageServer_Lsp_FreeDiagnostics(fixture->state, &diagnostics);
    REQUIRE(passed);
    return ZR_TRUE;
}

static TZrBool check_while(Fixture *fixture) {
    TZrSymbolId outer = symbol_id(fixture, "index", 0U);
    TZrSymbolId inner = symbol_id(fixture, "index", 5U);
    REQUIRE(outer != 0U && inner != 0U && outer != inner);
    REQUIRE(expect_condition_facts(fixture));
    for (TZrSize i = 1U; i <= 4U; ++i)
        REQUIRE(expect_reference(fixture, "index", i, outer, ZR_SEMANTIC_REFERENCE_READ));
    REQUIRE(expect_reference(fixture, "index", 6U, inner, ZR_SEMANTIC_REFERENCE_READ));
    REQUIRE(expect_reference(fixture, "index", 7U, outer, ZR_SEMANTIC_REFERENCE_WRITE));
    REQUIRE(expect_projection(fixture, "index", 0U, 6U));
    REQUIRE(expect_projection(fixture, "index", 1U, 6U));
    REQUIRE(expect_projection(fixture, "index", 5U, 2U));
    return expect_no_diagnostics(fixture);
}

static TZrBool check_for(Fixture *fixture) {
    TZrSymbolId outer = symbol_id(fixture, "index", 0U);
    TZrSymbolId header = symbol_id(fixture, "index", 1U);
    REQUIRE(outer != 0U && header != 0U && outer != header);
    REQUIRE(expect_condition_facts(fixture));
    REQUIRE(expect_reference(fixture, "index", 2U, header, ZR_SEMANTIC_REFERENCE_READ));
    REQUIRE(expect_reference(fixture, "index", 3U, header, ZR_SEMANTIC_REFERENCE_WRITE));
    REQUIRE(expect_reference(fixture, "index", 4U, header, ZR_SEMANTIC_REFERENCE_READ));
    REQUIRE(expect_reference(fixture, "index", 5U, outer, ZR_SEMANTIC_REFERENCE_READ));
    REQUIRE(expect_projection(fixture, "index", 0U, 2U));
    REQUIRE(expect_projection(fixture, "index", 2U, 4U));
    return expect_no_diagnostics(fixture);
}

static TZrBool check_for_outer(Fixture *fixture) {
    TZrSymbolId outer = symbol_id(fixture, "index", 0U);
    REQUIRE(expect_condition_facts(fixture));
    REQUIRE(expect_reference(fixture, "index", 1U, outer, ZR_SEMANTIC_REFERENCE_WRITE));
    REQUIRE(expect_reference(fixture, "index", 2U, outer, ZR_SEMANTIC_REFERENCE_READ));
    REQUIRE(expect_reference(fixture, "index", 3U, outer, ZR_SEMANTIC_REFERENCE_WRITE));
    REQUIRE(expect_reference(fixture, "index", 4U, outer, ZR_SEMANTIC_REFERENCE_READ));
    REQUIRE(expect_reference(fixture, "index", 5U, outer, ZR_SEMANTIC_REFERENCE_READ));
    REQUIRE(expect_projection(fixture, "index", 2U, 6U));
    return expect_no_diagnostics(fixture);
}

static TZrBool check_foreach(Fixture *fixture) {
    TZrSymbolId outer = symbol_id(fixture, "index", 0U);
    TZrSymbolId inner = symbol_id(fixture, "index", 2U);
    TZrSymbolId values = symbol_id(fixture, "values", 0U);
    TZrSymbolId item = symbol_id(fixture, "item", 0U);
    REQUIRE(outer != 0U && inner != 0U && outer != inner);
    REQUIRE(expect_reference(fixture, "item", 1U, item, ZR_SEMANTIC_REFERENCE_READ));
    REQUIRE(expect_reference(fixture, "values", 1U, values, ZR_SEMANTIC_REFERENCE_READ));
    REQUIRE(expect_reference(fixture, "index", 1U, outer, ZR_SEMANTIC_REFERENCE_READ));
    REQUIRE(expect_reference(fixture, "index", 3U, inner, ZR_SEMANTIC_REFERENCE_READ));
    REQUIRE(expect_reference(fixture, "index", 4U, outer, ZR_SEMANTIC_REFERENCE_WRITE));
    REQUIRE(expect_projection(fixture, "index", 0U, 3U));
    REQUIRE(expect_projection(fixture, "index", 2U, 2U));
    REQUIRE(expect_projection(fixture, "values", 1U, 4U));
    return expect_no_diagnostics(fixture);
}

static TZrBool check_immutable(Fixture *fixture) {
    TZrSymbolId input = symbol_id(fixture, "input", 0U);
    TZrSymbolId ceiling = symbol_id(fixture, "ceiling", 0U);
    REQUIRE(input != 0U && ceiling != 0U && input != ceiling);
    REQUIRE(expect_reference(fixture, "input", 1U, input, ZR_SEMANTIC_REFERENCE_READ));
    REQUIRE(expect_reference(fixture, "input", 2U, input, ZR_SEMANTIC_REFERENCE_READ));
    REQUIRE(expect_reference(fixture, "ceiling", 1U, ceiling, ZR_SEMANTIC_REFERENCE_READ));
    REQUIRE(expect_projection(fixture, "input", 1U, 3U));
    REQUIRE(expect_projection(fixture, "ceiling", 1U, 2U));
    return expect_no_diagnostics(fixture);
}

static TZrBool check_unresolved(Fixture *fixture) {
    SZrArray renamed = {0};
    SZrString *name = ZR_STRING_LITERAL(fixture->state, "renamedIndex");
    SZrLspPosition position = token_position(fixture->text, "index", 0U);
    TZrBool resolved = ZrLanguageServer_Lsp_Rename(fixture->state, fixture->context,
            fixture->uri, position, name, &renamed);
    TZrSize count = renamed.length;
    free_local_reference_projection_results(fixture->state, &renamed, ZR_NULL);
    REQUIRE(symbol_id(fixture, "index", 0U) == ZR_SEMANTIC_ID_INVALID);
    REQUIRE(!resolved && count == 0U);
    REQUIRE(expect_projection(fixture, "index", 1U, 2U));
    return ZR_TRUE;
}

static TZrBool run_case(SZrState *state, const char *label, const char *text,
                        TZrBool (*check)(Fixture *)) {
    Fixture fixture = {0};
    TZrBool passed = ZR_FALSE;
    fixture.state = state;
    fixture.text = text;
    fixture.context = ZrLanguageServer_LspContext_New(state);
    fixture.uri = ZR_STRING_LITERAL(state, "file:///loop-identity.zr");
    if (fixture.context != ZR_NULL && fixture.uri != ZR_NULL &&
        ZrLanguageServer_Lsp_UpdateDocument(state, fixture.context, fixture.uri, text, strlen(text), 1U)) {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(
                state, fixture.context, fixture.uri);
        if (analyzer != ZR_NULL && analyzer->semanticContext != ZR_NULL) {
            fixture.semantic = analyzer->semanticContext;
            passed = check(&fixture);
        }
    }
    ZrLanguageServer_LspContext_Free(state, fixture.context);
    printf("%s: %s\n", passed ? "PASS" : "FAIL", label);
    return passed;
}

int main(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrBool passed = ZR_TRUE;
    if (state == ZR_NULL) return 1;
    passed = run_case(state, "while condition, computed read/write keys and nested shadow",
        "let container = import(\"zr.container\");\n"
        "fn scan(values: container.Array<int>): int {\n"
        "    var index = 0;\n    var total = 0;\n"
        "    while (index < values.length) {\n"
        "        total += values[index];\n        values[index] = index;\n"
        "        {\n            let index = 1;\n            total += values[index];\n        }\n"
        "        index += 1;\n    }\n    return total;\n}\n", check_while) && passed;
    passed = run_case(state, "for preheader, header binding/condition/step/body and restored outer scope",
        "fn scan(limit: int): int {\n    var index = 9;\n    var total = 0;\n"
        "    for (var index = 0; index < limit; index += 1) {\n"
        "        total += index;\n    }\n    return index + total;\n}\n", check_for) && passed;
    passed = run_case(state, "for expression initializer keeps the outer identity in every clause",
        "fn scan(limit: int): int {\n    var index = 9;\n    var total = 0;\n"
        "    for (index = 0; index < limit; index += 1) {\n"
        "        total += index;\n    }\n    return index + total;\n}\n", check_for_outer) && passed;
    passed = run_case(state, "foreach iterable, loop-carried outer binding and nested shadow",
        "fn each(values: int[]): int {\n    var index = 0;\n    var total = 0;\n"
        "    for (var item in values) {\n        total += item + values[index];\n"
        "        {\n            let index = 1;\n            total += values[index];\n        }\n"
        "        index += 1;\n    }\n    return total;\n}\n", check_foreach) && passed;
    passed = run_case(state, "typed foreach still publishes the iterable reference",
        "fn each(values: int[]): int {\n    var index = 0;\n    var total = 0;\n"
        "    for (var item: int in values) {\n        total += item + values[index];\n"
        "        {\n            let index = 1;\n            total += values[index];\n        }\n"
        "        index += 1;\n    }\n    return total;\n}\n", check_foreach) && passed;
    passed = run_case(state, "const and in parameter identity is not widened or rebound",
        "fn inspect(input: in int): int {\n    let ceiling = 2;\n    var total = 0;\n"
        "    while (input < ceiling) {\n        total += input;\n        break;\n    }\n"
        "    return total;\n}\n", check_immutable) && passed;
    passed = run_case(state, "unresolved condition cannot bind the same spelling declared inside the loop",
        "fn bad(): void {\n    while (index < 2) {\n"
        "        var index = 0;\n        index += 1;\n    }\n}\n", check_unresolved) && passed;
    ZrTests_Runtime_State_Destroy(state);
    return passed ? 0 : 1;
}
