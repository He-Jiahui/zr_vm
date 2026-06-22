//
// Focused LSP semantic query diagnostic publication tests.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server.h"

typedef struct SZrTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

static int g_failures = 0;

#define TEST_START(summary) do { \
    timer.startTime = clock(); \
    printf("Unit Test - %s\n", summary); \
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
    g_failures++; \
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

static const TZrChar *test_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

static TZrBool diagnostic_array_contains_code(SZrArray *diagnostics, const TZrChar *expectedCode) {
    TZrSize index;

    if (diagnostics == ZR_NULL || expectedCode == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        const TZrChar *code = (diagnosticPtr != ZR_NULL &&
                               *diagnosticPtr != ZR_NULL &&
                               (*diagnosticPtr)->code != ZR_NULL)
                                  ? test_string_text((*diagnosticPtr)->code)
                                  : ZR_NULL;
        if (code != ZR_NULL && strcmp(code, expectedCode) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrSize diagnostic_array_count_code(SZrArray *diagnostics, const TZrChar *expectedCode) {
    TZrSize count = 0;

    if (diagnostics == ZR_NULL || expectedCode == ZR_NULL) {
        return 0;
    }

    for (TZrSize index = 0; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        const TZrChar *code = (diagnosticPtr != ZR_NULL &&
                               *diagnosticPtr != ZR_NULL &&
                               (*diagnosticPtr)->code != ZR_NULL)
                                  ? test_string_text((*diagnosticPtr)->code)
                                  : ZR_NULL;
        if (code != ZR_NULL && strcmp(code, expectedCode) == 0) {
            count++;
        }
    }

    return count;
}

static const SZrLspDiagnostic *diagnostic_array_find_code(SZrArray *diagnostics, const TZrChar *expectedCode) {
    if (diagnostics == ZR_NULL || expectedCode == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        const TZrChar *code = (diagnosticPtr != ZR_NULL &&
                               *diagnosticPtr != ZR_NULL &&
                               (*diagnosticPtr)->code != ZR_NULL)
                                  ? test_string_text((*diagnosticPtr)->code)
                                  : ZR_NULL;
        if (code != ZR_NULL && strcmp(code, expectedCode) == 0) {
            return *diagnosticPtr;
        }
    }

    return ZR_NULL;
}

static void test_lsp_diagnostics_publish_definite_assignment_related_information(SZrState *state) {
    const TZrChar *summary = "LSP Diagnostics Publish Definite Assignment Related Information";
    const TZrChar *uriText = "file:///semantic_query_definite_assignment_related.zr";
    const TZrChar *content =
        "func choose(flag: bool): int {\n"
        "    var seed: int;\n"
        "    if (flag) {\n"
        "        seed = 1;\n"
        "    }\n"
        "    return seed;\n"
        "}\n";
    const SZrLspDiagnostic *diagnostic;
    const SZrLspDiagnosticRelatedInformation *related;
    const TZrChar *relatedMessage;
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;
    TZrChar reason[256];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare definite assignment related fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    if (diagnostic_array_count_code(&diagnostics, "possibly_uninitialized_read") != 1) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected one possibly uninitialized read diagnostic; diagnostics=%llu",
                 (unsigned long long)diagnostics.length);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    diagnostic = diagnostic_array_find_code(&diagnostics, "possibly_uninitialized_read");
    if (diagnostic == ZR_NULL ||
        diagnostic->severity != 2 ||
        !diagnostic->relatedInformation.isValid ||
        diagnostic->relatedInformation.length != 1) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected one related location for the declaration");
        return;
    }

    related = (const SZrLspDiagnosticRelatedInformation *)ZrCore_Array_Get(
            (SZrArray *)&diagnostic->relatedInformation,
            0);
    relatedMessage = related != ZR_NULL ? test_string_text(related->message) : ZR_NULL;
    if (related == ZR_NULL ||
        relatedMessage == ZR_NULL ||
        strcmp(relatedMessage, "Variable declaration is here") != 0 ||
        related->location.range.start.line != 1 ||
        related->location.range.start.character != 8) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected related information to point at the seed declaration");
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_diagnostics_publish_semantic_query_unreachable_branch(SZrState *state) {
    const TZrChar *summary = "LSP Diagnostics Publish Semantic Query Unreachable Branch";
    const TZrChar *uriText = "file:///semantic_query_unreachable_branch.zr";
    const TZrChar *content =
        "func choose(): int {\n"
        "    return true ? 1 : 2;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;
    TZrChar reason[256];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare constant conditional branch fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    if (!diagnostic_array_contains_code(&diagnostics, "unreachable_code")) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected semantic query diagnostic code unreachable_code; diagnostics=%llu",
                 (unsigned long long)diagnostics.length);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_diagnostics_publish_interval_logical_unreachable_branch(SZrState *state) {
    const TZrChar *summary = "LSP Diagnostics Publish Interval Logical Unreachable Branch";
    const TZrChar *uriText = "file:///semantic_query_interval_logical_branch.zr";
    const TZrChar *content =
        "func choose(seed: u8): int {\n"
        "    if (seed < 300) {\n"
        "        return 1;\n"
        "    } else {\n"
        "        return 2;\n"
        "    }\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;
    TZrChar reason[256];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare interval logical branch fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    if (!diagnostic_array_contains_code(&diagnostics, "unreachable_code")) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected interval logical fact to publish unreachable_code; diagnostics=%llu",
                 (unsigned long long)diagnostics.length);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_diagnostics_publish_numeric_overflow(SZrState *state) {
    const TZrChar *summary = "LSP Diagnostics Publish Numeric Overflow";
    const TZrChar *uriText = "file:///semantic_query_numeric_overflow.zr";
    const TZrChar *content =
        "func overflow(): int {\n"
        "    return 9223372036854775807 + 1;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;
    TZrChar reason[256];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare numeric overflow fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    if (!diagnostic_array_contains_code(&diagnostics, "numeric_overflow")) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected semantic query diagnostic code numeric_overflow; diagnostics=%llu",
                 (unsigned long long)diagnostics.length);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_diagnostics_publish_array_bounds(SZrState *state) {
    const TZrChar *summary = "LSP Diagnostics Publish Array Bounds";
    const TZrChar *uriText = "file:///semantic_query_array_bounds.zr";
    const TZrChar *content =
        "func pick(): int {\n"
        "    var values = [1, 2];\n"
        "    return values[2];\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;
    TZrChar reason[256];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare array bounds fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    if (!diagnostic_array_contains_code(&diagnostics, "array_index_out_of_bounds")) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected semantic query diagnostic code array_index_out_of_bounds; diagnostics=%llu",
                 (unsigned long long)diagnostics.length);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_diagnostics_publish_interval_array_bounds(SZrState *state) {
    const TZrChar *summary = "LSP Diagnostics Publish Interval Array Bounds";
    const TZrChar *uriText = "file:///semantic_query_interval_array_bounds.zr";
    const TZrChar *content =
        "func pick(index: u8): int {\n"
        "    var values = [1, 2];\n"
        "    var maybe = values[index];\n"
        "    return values[index + 2];\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;
    TZrChar reason[256];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare interval array bounds fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    if (diagnostic_array_count_code(&diagnostics, "array_index_out_of_bounds") != 1) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected one interval array bounds diagnostic; diagnostics=%llu",
                 (unsigned long long)diagnostics.length);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_diagnostics_publish_possible_interval_array_bounds(SZrState *state) {
    const TZrChar *summary = "LSP Diagnostics Publish Possible Interval Array Bounds";
    const TZrChar *uriText = "file:///semantic_query_possible_interval_array_bounds.zr";
    const TZrChar *content =
        "func pick(index: u8): int {\n"
        "    var values = [1, 2];\n"
        "    return values[index];\n"
        "}\n";
    const SZrLspDiagnostic *diagnostic;
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;
    TZrChar reason[256];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare possible interval array bounds fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    if (diagnostic_array_count_code(&diagnostics, "array_index_may_be_out_of_bounds") != 1) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected one possible interval array bounds diagnostic; diagnostics=%llu",
                 (unsigned long long)diagnostics.length);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    if (diagnostic_array_count_code(&diagnostics, "array_index_out_of_bounds") != 0) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Possible interval bounds should not publish a definite bounds error");
        return;
    }

    diagnostic = diagnostic_array_find_code(&diagnostics, "array_index_may_be_out_of_bounds");
    if (diagnostic == ZR_NULL || diagnostic->severity != 2) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected possible interval bounds diagnostic severity Warning");
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_diagnostics_publish_primitive_integer_array_bounds(SZrState *state) {
    const TZrChar *summary = "LSP Diagnostics Publish Primitive Integer Array Bounds";
    const TZrChar *uriText = "file:///semantic_query_primitive_integer_array_bounds.zr";
    const TZrChar *content =
        "func pick(index: int): int {\n"
        "    var values = [1, 2];\n"
        "    return values[index];\n"
        "}\n";
    const SZrLspDiagnostic *diagnostic;
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;
    TZrChar reason[256];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare primitive integer array bounds fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    if (diagnostic_array_count_code(&diagnostics, "array_index_may_be_out_of_bounds") != 1) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected one primitive integer array bounds diagnostic; diagnostics=%llu",
                 (unsigned long long)diagnostics.length);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    if (diagnostic_array_count_code(&diagnostics, "array_index_out_of_bounds") != 0) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Primitive integer bounds should not publish a definite bounds error");
        return;
    }

    diagnostic = diagnostic_array_find_code(&diagnostics, "array_index_may_be_out_of_bounds");
    if (diagnostic == ZR_NULL || diagnostic->severity != 2) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected primitive integer bounds diagnostic severity Warning");
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_diagnostics_publish_array_min_max_bounds(SZrState *state) {
    const TZrChar *summary = "LSP Diagnostics Publish Array Min Max Bounds";
    const TZrChar *uriText = "file:///semantic_query_array_min_max_bounds.zr";
    const TZrChar *content =
        "func maybe(index: u8): int {\n"
        "    var values: int[1 .. 3] = [1, 2];\n"
        "    return values[index];\n"
        "}\n"
        "func definite(): int {\n"
        "    var values: int[1 .. 3] = [1, 2];\n"
        "    return values[3];\n"
        "}\n";
    const SZrLspDiagnostic *definiteDiagnostic;
    const SZrLspDiagnostic *possibleDiagnostic;
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;
    TZrChar reason[256];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare array min max bounds fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    if (diagnostic_array_count_code(&diagnostics, "array_index_may_be_out_of_bounds") != 1) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected one possible array min max bounds diagnostic; diagnostics=%llu",
                 (unsigned long long)diagnostics.length);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    if (diagnostic_array_count_code(&diagnostics, "array_index_out_of_bounds") != 1) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected one definite array min max bounds diagnostic; diagnostics=%llu",
                 (unsigned long long)diagnostics.length);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    possibleDiagnostic = diagnostic_array_find_code(&diagnostics, "array_index_may_be_out_of_bounds");
    definiteDiagnostic = diagnostic_array_find_code(&diagnostics, "array_index_out_of_bounds");
    if (possibleDiagnostic == ZR_NULL || possibleDiagnostic->severity != 2 ||
        definiteDiagnostic == ZR_NULL || definiteDiagnostic->severity != 1) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected warning and error severities for array min max bounds");
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_diagnostics_publish_min_only_array_negative_interval(SZrState *state) {
    const TZrChar *summary = "LSP Diagnostics Publish Min Only Array Negative Interval";
    const TZrChar *uriText = "file:///semantic_query_min_only_array_negative_interval.zr";
    const TZrChar *content =
        "func maybe(index: int): int {\n"
        "    var values: int[1 ..] = [1, 2];\n"
        "    return values[index];\n"
        "}\n"
        "func positive(index: u8): int {\n"
        "    var values: int[1 ..] = [1, 2];\n"
        "    return values[index];\n"
        "}\n";
    const SZrLspDiagnostic *diagnostic;
    const TZrChar *message;
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;
    TZrChar reason[256];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare min only array negative interval fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    if (diagnostic_array_count_code(&diagnostics, "array_index_may_be_out_of_bounds") != 1) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected one min only array negative interval diagnostic; diagnostics=%llu",
                 (unsigned long long)diagnostics.length);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    if (diagnostic_array_count_code(&diagnostics, "array_index_out_of_bounds") != 0) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Min only array negative interval should not publish a definite bounds error");
        return;
    }

    diagnostic = diagnostic_array_find_code(&diagnostics, "array_index_may_be_out_of_bounds");
    message = diagnostic != ZR_NULL ? test_string_text(diagnostic->message) : ZR_NULL;
    if (diagnostic == ZR_NULL ||
        diagnostic->severity != 2 ||
        message == ZR_NULL ||
        strstr(message, "may be negative") == ZR_NULL ||
        strstr(message, "array max size") != ZR_NULL) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  summary,
                  "Expected warning severity and a negative-only message without array max size");
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_diagnostics_publish_non_integer_array_index(SZrState *state) {
    const TZrChar *summary = "LSP Diagnostics Publish Non Integer Array Index";
    const TZrChar *uriText = "file:///semantic_query_array_index_type_mismatch.zr";
    const TZrChar *content =
        "func pick(): int {\n"
        "    var values = [1, 2];\n"
        "    return values[\"name\"];\n"
        "}\n";
    const SZrLspDiagnostic *diagnostic;
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;
    TZrChar reason[256];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare non integer array index fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Diagnostics request failed");
        return;
    }

    if (diagnostic_array_count_code(&diagnostics, "array_index_type_mismatch") != 1) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected one non integer array index diagnostic; diagnostics=%llu",
                 (unsigned long long)diagnostics.length);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    if (diagnostic_array_count_code(&diagnostics, "array_index_out_of_bounds") != 0 ||
        diagnostic_array_count_code(&diagnostics, "array_index_may_be_out_of_bounds") != 0) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected no bounds diagnostics for non integer array index");
        return;
    }

    diagnostic = diagnostic_array_find_code(&diagnostics, "array_index_type_mismatch");
    if (diagnostic == ZR_NULL || diagnostic->severity != 1) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected error severity for non integer array index");
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;

    printf("ZR VM LSP Semantic Query Diagnostic Tests\n");
    printf("=========================================\n\n");

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

    test_lsp_diagnostics_publish_definite_assignment_related_information(state);
    test_lsp_diagnostics_publish_semantic_query_unreachable_branch(state);
    test_lsp_diagnostics_publish_interval_logical_unreachable_branch(state);
    test_lsp_diagnostics_publish_numeric_overflow(state);
    test_lsp_diagnostics_publish_array_bounds(state);
    test_lsp_diagnostics_publish_interval_array_bounds(state);
    test_lsp_diagnostics_publish_possible_interval_array_bounds(state);
    test_lsp_diagnostics_publish_primitive_integer_array_bounds(state);
    test_lsp_diagnostics_publish_array_min_max_bounds(state);
    test_lsp_diagnostics_publish_min_only_array_negative_interval(state);
    test_lsp_diagnostics_publish_non_integer_array_index(state);

    ZrCore_GlobalState_Free(global);

    printf("\n==========\n");
    printf("All LSP Semantic Query Diagnostic Tests Completed\n");
    printf("==========\n");
    return g_failures == 0 ? 0 : 1;
}
