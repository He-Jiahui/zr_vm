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

static void test_semantic_analyzer_reports_loaned_return_escape(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Loaned Return Escape");

    TEST_INFO("Ownership loan escape in return statements",
              "Returning %loan(owner) as a %loaned result must emit loan_escape with cause, suggestion, and ownership fact");

    {
        const TZrChar *testCode =
            "class Resource {\n"
            "}\n"
            "leak(resource: %unique Resource): %loaned Resource {\n"
            "    return %loan(resource);\n"
            "}\n";
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_loaned_return_escape_test.zr", 37);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;
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
        if (diagnostic == ZR_NULL || diagnostic->severity != ZR_DIAGNOSTIC_ERROR) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Loaned Return Escape",
                      "Expected loan_escape diagnostic for loaned return escape");
            return;
        }
        if (!diagnostic_string_contains(diagnostic->message, "Loaned value cannot escape") ||
            !diagnostic_string_contains(diagnostic->cause, "%loan") ||
            !diagnostic_string_contains(diagnostic->suggestion, "Return")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Loaned Return Escape",
                      "Expected loan escape diagnostic to include message, cause, and suggestion");
            return;
        }

        fact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            file_range_for_nth_substring(testCode, "%loan(resource)", 0));
        if (fact == ZR_NULL ||
            fact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_ERROR ||
            fact->qualifier != ZR_OWNERSHIP_QUALIFIER_LOANED ||
            !fact->isViolation ||
            !ownership_fact_message_contains(fact, "Loaned")) {
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

    ZrCore_GlobalState_Free(global);

    printf("\n==========\n");
    printf("All Ownership Diagnostics Tests Completed\n");
    printf("==========\n");
    return 0;
}
