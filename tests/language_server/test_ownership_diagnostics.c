//
// Focused ownership diagnostic regression tests.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "zr_vm_language_server.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"

typedef struct SZrTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

static int test_failures = 0;

#define TEST_START(summary) do { \
    timer.startTime = clock(); \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while (0)

#define TEST_INFO(summary, details) do { \
    printf("Testing %s:\n %s\n", summary, details); \
    fflush(stdout); \
} while (0)

#define TEST_PASS(timerValue, summary) do { \
    (timerValue).endTime = clock(); \
    double elapsed = ((double)((timerValue).endTime - (timerValue).startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary); \
    fflush(stdout); \
} while (0)

#define TEST_FAIL(timerValue, summary, reason) do { \
    (timerValue).endTime = clock(); \
    double elapsed = ((double)((timerValue).endTime - (timerValue).startTime) / CLOCKS_PER_SEC) * 1000.0; \
    test_failures++; \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason); \
    fflush(stdout); \
} while (0)

#define TEST_DIVIDER() do { \
    printf("----------\n"); \
    fflush(stdout); \
} while (0)

static TZrPtr test_allocator(TZrPtr userData,
                             TZrPtr pointer,
                             TZrSize originalSize,
                             TZrSize newSize,
                             TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL &&
            (TZrPtr)pointer >= (TZrPtr)0x1000 &&
            originalSize > 0 &&
            originalSize < 1024 * 1024 * 1024) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }
    if ((TZrPtr)pointer >= (TZrPtr)0x1000 &&
        originalSize > 0 &&
        originalSize < 1024 * 1024 * 1024) {
        return realloc(pointer, newSize);
    }
    return malloc(newSize);
}

static SZrFileRange file_range_for_nth_substring(const TZrChar *content,
                                                 const TZrChar *needle,
                                                 TZrSize occurrence) {
    const TZrChar *cursor = content;
    const TZrChar *found = ZR_NULL;
    TZrSize remaining = occurrence;
    TZrSize offset = 0;
    SZrFilePosition position = ZrParser_FilePosition_Create(0, 1, 1);

    while (cursor != ZR_NULL && *cursor != '\0') {
        found = strstr(cursor, needle);
        if (found == ZR_NULL) {
            break;
        }
        if (remaining == 0) {
            offset = (TZrSize)(found - content);
            break;
        }
        remaining--;
        cursor = found + 1;
    }

    for (TZrSize index = 0; index < offset && content[index] != '\0'; index++) {
        position.offset++;
        if (content[index] == '\n') {
            position.line++;
            position.column = 1;
        } else {
            position.column++;
        }
    }

    return ZrParser_FileRange_Create(position, position, ZR_NULL);
}

static TZrBool diagnostic_string_contains(SZrString *value, const TZrChar *fragment) {
    const TZrChar *text;

    if (value == ZR_NULL || fragment == ZR_NULL) {
        return ZR_FALSE;
    }

    text = ZrCore_String_GetNativeString(value);
    return text != ZR_NULL && strstr(text, fragment) != ZR_NULL;
}

static const TZrChar *test_string_text(SZrString *value) {
    return value != ZR_NULL ? ZrCore_String_GetNativeString(value) : ZR_NULL;
}

static TZrBool test_string_equals(SZrString *value, const TZrChar *expected) {
    const TZrChar *text = test_string_text(value);
    return text != ZR_NULL && expected != ZR_NULL && strcmp(text, expected) == 0;
}

static TZrBool ownership_fact_message_contains(const SZrSemanticOwnershipFact *fact, const TZrChar *fragment) {
    return fact != ZR_NULL && diagnostic_string_contains(fact->diagnosticMessage, fragment);
}

static SZrDiagnostic *find_diagnostic_by_code_and_line(SZrSemanticAnalyzer *analyzer,
                                                       const TZrChar *code,
                                                       TZrInt32 line) {
    TZrSize index;

    if (analyzer == ZR_NULL || code == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < analyzer->diagnostics.length; index++) {
        SZrDiagnostic **diagnosticPtr =
            (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, index);
        if (diagnosticPtr == ZR_NULL || *diagnosticPtr == ZR_NULL || (*diagnosticPtr)->code == ZR_NULL) {
            continue;
        }
        if (strcmp(ZrCore_String_GetNativeStringShort((*diagnosticPtr)->code), code) == 0 &&
            (*diagnosticPtr)->location.start.line == line) {
            return *diagnosticPtr;
        }
    }

    return ZR_NULL;
}

static TZrSize count_diagnostics_by_code_and_line(
        SZrSemanticAnalyzer *analyzer,
        const TZrChar *code,
        TZrInt32 line) {
    TZrSize count = 0;

    if (analyzer == ZR_NULL || code == ZR_NULL) {
        return 0;
    }
    for (TZrSize index = 0; index < analyzer->diagnostics.length; index++) {
        SZrDiagnostic **diagnosticPtr =
                (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, index);
        if (diagnosticPtr != ZR_NULL && *diagnosticPtr != ZR_NULL &&
            (*diagnosticPtr)->code != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeStringShort((*diagnosticPtr)->code), code) == 0 &&
            (*diagnosticPtr)->location.start.line == line) {
            count++;
        }
    }
    return count;
}

static const SZrLspDiagnostic *find_lsp_diagnostic_by_code(SZrArray *diagnostics,
                                                           const TZrChar *code) {
    TZrSize index;

    if (diagnostics == ZR_NULL || code == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr =
            (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        if (diagnosticPtr != ZR_NULL &&
            *diagnosticPtr != ZR_NULL &&
            (*diagnosticPtr)->code != ZR_NULL &&
            test_string_equals((*diagnosticPtr)->code, code)) {
            return *diagnosticPtr;
        }
    }

    return ZR_NULL;
}

static void test_semantic_analyzer_reports_loaned_return_escape(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Loaned Return Escape");

    TEST_INFO("Ownership loan escape in return statements",
              "Returning ref owner as a ref result must emit loan_escape with cause, suggestion, and ownership fact");

    {
        const TZrChar *testCode =
            "resource class Resource {\n"
            "}\n"
            "fn leak(resource: Unique<Resource>): ref Resource {\n"
            "    return ref resource;\n"
            "}\n";
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_loaned_return_escape_test.zr", 37);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;
        const SZrDiagnosticRelatedInformation *sourceRelated;
        const SZrDiagnosticRelatedInformation *endRelated;
        const SZrSemanticOwnershipFact *fact;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer, "Semantic Analyzer Reports Loaned Return Escape", "Failed to create semantic analyzer");
            return;
        }
        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, "Semantic Analyzer Reports Loaned Return Escape", "Failed to parse test code");
            return;
        }
        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, "Semantic Analyzer Reports Loaned Return Escape", "Failed to analyze AST");
            return;
        }

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "loan_escape", 4);
        if (diagnostic == ZR_NULL ||
            count_diagnostics_by_code_and_line(analyzer, "loan_escape", 4) != 1 ||
            diagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
            diagnostic->location.start.column != 16 ||
            diagnostic->location.end.column != 24) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Loaned Return Escape",
                      "Expected loan_escape diagnostic for loaned return escape");
            return;
        }
        if (!diagnostic_string_contains(diagnostic->message, "Loaned value cannot escape") ||
            !diagnostic_string_contains(diagnostic->cause, "ref") ||
            !diagnostic_string_contains(diagnostic->suggestion, "Return")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Loaned Return Escape",
                      "Expected loan escape diagnostic to include message, cause, and suggestion");
            return;
        }

        if (!diagnostic->relatedInformation.isValid || diagnostic->relatedInformation.length != 2) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Loaned Return Escape",
                      "Expected loan escape to include source and source-lifetime-end related information");
            return;
        }
        sourceRelated = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
                &diagnostic->relatedInformation,
                0);
        endRelated = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
                &diagnostic->relatedInformation,
                1);
        if (sourceRelated == ZR_NULL ||
            !test_string_equals(sourceRelated->message, "Loan source is here") ||
            sourceRelated->location.start.line != 4 ||
            sourceRelated->location.start.column != 16 ||
            sourceRelated->location.end.line != 4 ||
            sourceRelated->location.end.column != 24 ||
            endRelated == ZR_NULL ||
            !test_string_equals(endRelated->message, "Source lifetime ends here") ||
            endRelated->location.start.line != 5 ||
            endRelated->location.start.offset != endRelated->location.end.offset) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Loaned Return Escape",
                      "Expected loan related information to identify the owner use and enclosing body end");
            return;
        }

        fact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "resource;", 0));
        if (fact == ZR_NULL ||
            fact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_ERROR ||
            fact->qualifier != ZR_OWNERSHIP_QUALIFIER_LOANED ||
            !fact->isViolation ||
            !ownership_fact_message_contains(fact, "Loaned") ||
            fact->relatedNode == ZR_NULL ||
            fact->relatedNode == fact->node ||
            fact->relatedNode->location.start.line != 4 ||
            fact->relatedNode->location.start.column != 16) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Loaned Return Escape",
                      "Expected loan escape ownership fact to cover the ownership builtin");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Loaned Return Escape");
}

static void test_semantic_analyzer_reports_borrowed_return_escape(SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Reports Borrowed Return Escape";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn leak(resource: Unique<Resource>): ref readonly Resource {\n"
        "    return ref resource;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *diagnostic;
    const SZrDiagnosticRelatedInformation *sourceRelated;
    const SZrDiagnosticRelatedInformation *endRelated;

    TEST_START(summary);
    TEST_INFO("Ownership borrow escape in return statements",
              "Returning ref owner must retain the borrow source and source lifetime end");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(state,
                                      "ownership_borrowed_return_escape_test.zr",
                                      strlen("ownership_borrowed_return_escape_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare borrowed return escape fixture");
        return;
    }

    diagnostic = find_diagnostic_by_code_and_line(analyzer, "borrow_escape", 4);
    if (diagnostic == ZR_NULL ||
        count_diagnostics_by_code_and_line(analyzer, "borrow_escape", 4) != 1 ||
        diagnostic->location.start.column != 16 ||
        diagnostic->location.end.column != 24 ||
        !diagnostic->relatedInformation.isValid ||
        diagnostic->relatedInformation.length != 2) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Expected borrow_escape with two related locations");
        return;
    }

    sourceRelated = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
            &diagnostic->relatedInformation,
            0);
    endRelated = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
            &diagnostic->relatedInformation,
            1);
    if (sourceRelated == ZR_NULL ||
        !test_string_equals(sourceRelated->message, "Borrow source is here") ||
        sourceRelated->location.start.line != 4 ||
        sourceRelated->location.start.column != 16 ||
        sourceRelated->location.end.line != 4 ||
        sourceRelated->location.end.column != 24 ||
        endRelated == ZR_NULL ||
        !test_string_equals(endRelated->message, "Source lifetime ends here") ||
        endRelated->location.start.line != 5 ||
        endRelated->location.start.offset != endRelated->location.end.offset) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Borrow escape related-information chain was not precise");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_lsp_translates_loan_escape_related_information(SZrState *state) {
    const TZrChar *summary = "LSP Translates Loan Escape Related Information";
    const TZrChar *uriText = "file:///ownership_loan_escape_related.zr";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn leak(resource: Unique<Resource>): ref Resource {\n"
        "    return ref resource;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;
    const SZrLspDiagnostic *diagnostic;
    const SZrLspDiagnosticRelatedInformation *sourceRelated;
    const SZrLspDiagnosticRelatedInformation *endRelated;

    TEST_START(summary);
    TEST_INFO("Ownership related information at the LSP boundary",
              "Loan source and lifetime-end ranges must use zero-based LSP coordinates");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, testCode, strlen(testCode), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare LSP loan escape fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    diagnostic = find_lsp_diagnostic_by_code(&diagnostics, "loan_escape");
    if (diagnostic == ZR_NULL ||
        !diagnostic->relatedInformation.isValid ||
        diagnostic->relatedInformation.length != 2) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected LSP loan_escape with two related locations");
        return;
    }

    sourceRelated = (const SZrLspDiagnosticRelatedInformation *)ZrCore_Array_Get(
            (SZrArray *)&diagnostic->relatedInformation,
            0);
    endRelated = (const SZrLspDiagnosticRelatedInformation *)ZrCore_Array_Get(
            (SZrArray *)&diagnostic->relatedInformation,
            1);
    if (sourceRelated == ZR_NULL ||
        !test_string_equals(sourceRelated->message, "Loan source is here") ||
        sourceRelated->location.range.start.line != 3 ||
        sourceRelated->location.range.start.character != 15 ||
        sourceRelated->location.range.end.line != 3 ||
        sourceRelated->location.range.end.character != 23 ||
        endRelated == ZR_NULL ||
        !test_string_equals(endRelated->message, "Source lifetime ends here") ||
        endRelated->location.range.start.line != 4 ||
        endRelated->location.range.start.line != endRelated->location.range.end.line ||
        endRelated->location.range.start.character != endRelated->location.range.end.character) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "LSP related locations did not preserve source and lifetime-end anchors");
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_reports_use_after_unique_move(SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Reports Use After Unique Move";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn consume(value: Unique<Resource>): int { return 0; }\n"
        "fn use(resource: Unique<Resource>): int {\n"
        "    consume(resource);\n"
        "    resource;\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *diagnostic;
    const SZrDiagnosticRelatedInformation *related;
    const SZrSemanticOwnershipFact *moveFact;
    const SZrSemanticOwnershipFact *violationFact;

    TEST_START(summary);
    TEST_INFO("Forward unique ownership flow",
              "Passing a unique value by value must move it and reject a later read with the move point attached");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(state,
                                      "ownership_use_after_move_test.zr",
                                      strlen("ownership_use_after_move_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare use-after-move fixture");
        return;
    }

    diagnostic = find_diagnostic_by_code_and_line(analyzer, "use_after_move", 6);
    if (diagnostic == ZR_NULL ||
        diagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
        !diagnostic_string_contains(diagnostic->cause, "moved") ||
        !diagnostic->relatedInformation.isValid ||
        diagnostic->relatedInformation.length != 1) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Expected use_after_move with one move-point related location");
        return;
    }

    related = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
            &diagnostic->relatedInformation,
            0);
    if (related == ZR_NULL ||
        !test_string_equals(related->message, "Value was moved here") ||
        related->location.start.line != 5 ||
        related->location.start.column != 13 ||
        related->location.end.line != 5 ||
        related->location.end.column != 21) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Move-point related information did not cover the moved argument token");
        return;
    }

    moveFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "resource", 2));
    violationFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "resource", 3));
    if (moveFact == ZR_NULL ||
        moveFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_MOVE ||
        moveFact->qualifier != ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
        moveFact->symbolId == ZR_SEMANTIC_ID_INVALID ||
        moveFact->isViolation ||
        violationFact == ZR_NULL ||
        violationFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_ERROR ||
        !violationFact->isViolation ||
        violationFact->symbolId != moveFact->symbolId ||
        violationFact->relatedNode != moveFact->node) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Expected linked MOVE and ERROR ownership facts for the unique symbol");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_lsp_reports_possible_path_use_after_move(SZrState *state) {
    const TZrChar *summary = "LSP Reports Possible-Path Use After Move";
    const TZrChar *uriText = "file:///ownership_possible_path_use_after_move.zr";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn consume(value: Unique<Resource>): int { return 0; }\n"
        "fn use(resource: Unique<Resource>, flag: bool): int {\n"
        "    if (flag) { consume(resource); }\n"
        "    resource;\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;
    const SZrLspDiagnostic *diagnostic;
    const SZrLspDiagnosticRelatedInformation *related;

    TEST_START(summary);
    TEST_INFO("Ownership-flow branch join",
              "A read after a branch-local move must preserve the move path and related location at the LSP boundary");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, testCode, strlen(testCode), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare branch ownership-flow fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    diagnostic = find_lsp_diagnostic_by_code(&diagnostics, "use_after_move");
    if (diagnostic == ZR_NULL ||
        diagnostic->severity != 1 ||
        diagnostic->range.start.line != 5 ||
        diagnostic->range.start.character != 4 ||
        !diagnostic->relatedInformation.isValid ||
        diagnostic->relatedInformation.length != 1) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected branch-join use_after_move at the later read");
        return;
    }

    related = (const SZrLspDiagnosticRelatedInformation *)ZrCore_Array_Get(
            (SZrArray *)&diagnostic->relatedInformation,
            0);
    if (related == ZR_NULL ||
        !test_string_equals(related->message, "Value was moved here") ||
        related->location.range.start.line != 4 ||
        related->location.range.start.character != 24 ||
        related->location.range.end.line != 4 ||
        related->location.range.end.character != 32) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected LSP related information to identify the branch-local move argument");
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_reports_use_after_unique_assignment_move(SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Reports Use After Unique Assignment Move";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn use(resource: Unique<Resource>): int {\n"
        "    var next: Unique<Resource> = resource;\n"
        "    resource;\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *diagnostic;
    const SZrDiagnosticRelatedInformation *related;

    TEST_START(summary);
    TEST_INFO("Unique assignment ownership flow",
              "Assigning a unique value to another variable must move it before a later read");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(state,
                                      "ownership_assignment_use_after_move_test.zr",
                                      strlen("ownership_assignment_use_after_move_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare assignment-move fixture");
        return;
    }

    diagnostic = find_diagnostic_by_code_and_line(analyzer, "use_after_move", 5);
    if (diagnostic == ZR_NULL ||
        !diagnostic->relatedInformation.isValid ||
        diagnostic->relatedInformation.length != 1) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Expected assignment use_after_move with the assignment source attached");
        return;
    }

    related = (const SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(
            &diagnostic->relatedInformation,
            0);
    if (related == ZR_NULL ||
        related->location.start.line != 4 ||
        related->location.start.column != 34 ||
        related->location.end.column != 42) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Assignment move point did not cover the unique source token");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_does_not_move_unique_ref_argument(SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Does Not Move Unique Ref Argument";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn inspect(value: ref Unique<Resource>): int { return 0; }\n"
        "fn use(resource: Unique<Resource>): int {\n"
        "    inspect(ref resource);\n"
        "    resource;\n"
        "    return 0;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *diagnostic;
    const SZrSemanticOwnershipFact *fact;

    TEST_START(summary);
    TEST_INFO("Ownership passing-mode boundary",
              "A ref argument aliases the unique value and must not consume it as a by-value move");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(state,
                                      "ownership_ref_argument_not_move_test.zr",
                                      strlen("ownership_ref_argument_not_move_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare ref-argument fixture");
        return;
    }

    diagnostic = find_diagnostic_by_code_and_line(analyzer, "use_after_move", 6);
    fact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "resource", 1));
    if (diagnostic != ZR_NULL ||
        (fact != ZR_NULL && fact->kind == ZR_SEMANTIC_OWNERSHIP_FACT_MOVE)) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Ref argument was incorrectly consumed as a unique move");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_allows_caller_reference_passthrough(
        SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Allows Caller Reference Passthrough";
    const TZrChar *testCode =
        "resource class Resource {\n"
        "}\n"
        "fn passthrough(resource: ref Resource): ref Resource {\n"
        "    return ref resource;\n"
        "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *borrowDiagnostic;
    SZrDiagnostic *loanDiagnostic;

    TEST_START(summary);
    TEST_INFO("Canonical caller reference escape bound",
              "A ref parameter already tied to the caller may be returned without an ownership escape diagnostic");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state,
            "ownership_reference_passthrough_test.zr",
            strlen("ownership_reference_passthrough_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        TEST_FAIL(timer, summary, "Failed to prepare caller reference fixture");
        return;
    }

    borrowDiagnostic = find_diagnostic_by_code_and_line(
            analyzer, "borrow_escape", 4);
    loanDiagnostic = find_diagnostic_by_code_and_line(
            analyzer, "loan_escape", 4);
    if (borrowDiagnostic != ZR_NULL || loanDiagnostic != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Caller reference passthrough was treated as an owner escape");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

#include "test_ownership_diagnostics_region_cases.h"
#include "test_ownership_diagnostics_owner_set_cases.h"
#include "test_ownership_diagnostics_using_body_cases.h"
#include "test_ownership_diagnostics_weak_receiver_cases.h"

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;

    printf("ZR VM Language Server Ownership Diagnostics Tests\n");
    printf("===============================================\n\n");

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL) {
        printf("Failed to create global state\n");
        return 1;
    }

    state = global->mainThreadState;
    if (state == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        printf("Failed to get main state\n");
        return 1;
    }
    ZrCore_GlobalState_InitRegistry(state, global);
    ZrVmLibMath_Register(global);
    ZrVmLibSystem_Register(global);
    ZrVmLibContainer_Register(global);

    test_semantic_analyzer_reports_loaned_return_escape(state);
    TEST_DIVIDER();
    test_semantic_analyzer_reports_borrowed_return_escape(state);
    TEST_DIVIDER();
    test_semantic_analyzer_allows_caller_reference_passthrough(state);
    TEST_DIVIDER();
    test_lsp_translates_loan_escape_related_information(state);
    TEST_DIVIDER();
    test_semantic_analyzer_reports_use_after_unique_move(state);
    TEST_DIVIDER();
    test_lsp_reports_possible_path_use_after_move(state);
    TEST_DIVIDER();
    test_semantic_analyzer_reports_use_after_unique_assignment_move(state);
    TEST_DIVIDER();
    test_semantic_analyzer_does_not_move_unique_ref_argument(state);
    TEST_DIVIDER();
    test_semantic_analyzer_records_borrow_and_loan_regions(state);
    TEST_DIVIDER();
    test_semantic_analyzer_reports_borrow_after_owner_release(state);
    TEST_DIVIDER();
    test_lsp_reports_possible_path_borrow_after_owner_release(state);
    TEST_DIVIDER();
    test_semantic_analyzer_releases_using_owner_at_scope_exit(state);
    TEST_DIVIDER();
    test_semantic_analyzer_releases_using_borrow_at_scope_exit(state);
    TEST_DIVIDER();
    test_semantic_analyzer_links_weak_use_to_possible_owner_release(state);
    TEST_DIVIDER();
    test_semantic_analyzer_rebinds_borrowed_alias_owner(state);
    TEST_DIVIDER();
    test_semantic_analyzer_rebinds_loaned_alias_owner(state);
    TEST_DIVIDER();
    test_semantic_analyzer_rebinds_weak_alias_owner(state);
    TEST_DIVIDER();
    test_semantic_analyzer_joins_borrowed_alias_owner_set(state);
    TEST_DIVIDER();
    test_semantic_analyzer_joins_loaned_alias_owner_set(state);
    TEST_DIVIDER();
    test_semantic_analyzer_joins_weak_alias_owner_set(state);
    TEST_DIVIDER();
    test_semantic_analyzer_ignores_release_outside_alias_owner_set(state);
    TEST_DIVIDER();
    test_semantic_analyzer_tracks_borrow_rebind_in_using_body(state);
    TEST_DIVIDER();
    test_semantic_analyzer_tracks_unique_move_in_using_body(state);
    TEST_DIVIDER();
    test_semantic_analyzer_guards_direct_weak_receiver_after_owner_release(state);
    TEST_DIVIDER();
    test_semantic_analyzer_guards_rebound_direct_weak_receiver(state);
    TEST_DIVIDER();

    ZrCore_GlobalState_Free(global);

    printf("\n==========\n");
    printf("All Ownership Diagnostics Tests Completed\n");
    printf("==========\n");
    return test_failures == 0 ? 0 : 1;
}
