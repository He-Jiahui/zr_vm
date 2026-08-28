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
#include "zr_vm_library/file.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/writer.h"
#include "path_support.h"

#include "../../zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h"
#include "../../zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.h"

typedef struct SZrParityTimer {
    clock_t startTime;
    clock_t endTime;
} SZrParityTimer;

typedef struct SZrParityBinaryFixture {
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    TZrChar binaryPath[ZR_TESTS_PATH_MAX];
} SZrParityBinaryFixture;

static int g_failures = 0;

#define TEST_START(summary) do { \
    timer.startTime = clock(); \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while (0)

#define TEST_PASS(timerValue, summary) do { \
    (timerValue).endTime = clock(); \
    printf("Pass - Cost Time:%.3fms - %s\n", \
           ((double)((timerValue).endTime - (timerValue).startTime) / CLOCKS_PER_SEC) * 1000.0, \
           summary); \
    fflush(stdout); \
} while (0)

#define TEST_FAIL(timerValue, summary, reason) do { \
    (timerValue).endTime = clock(); \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", \
           ((double)((timerValue).endTime - (timerValue).startTime) / CLOCKS_PER_SEC) * 1000.0, \
           summary, reason); \
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
        if (pointer != ZR_NULL && (TZrPtr)pointer >= (TZrPtr)0x1000 &&
            originalSize > 0 && originalSize < 1024 * 1024 * 1024) {
            free(pointer);
        }
        return ZR_NULL;
    }
    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }
    if ((TZrPtr)pointer >= (TZrPtr)0x1000 && originalSize > 0 &&
        originalSize < 1024 * 1024 * 1024) {
        return realloc(pointer, newSize);
    }
    return malloc(newSize);
}

static TZrBool find_position(const TZrChar *content,
                             const TZrChar *needle,
                             TZrSize occurrence,
                             TZrInt32 offset,
                             SZrLspPosition *outPosition) {
    const TZrChar *match;
    const TZrChar *cursor;
    TZrSize currentOccurrence = 0;
    TZrInt32 line = 0;
    TZrInt32 character = 0;

    if (content == ZR_NULL || needle == ZR_NULL || outPosition == ZR_NULL) {
        return ZR_FALSE;
    }
    match = strstr(content, needle);
    while (match != ZR_NULL && currentOccurrence < occurrence) {
        match = strstr(match + 1, needle);
        currentOccurrence++;
    }
    if (match == ZR_NULL) {
        return ZR_FALSE;
    }
    for (cursor = content; cursor < match; cursor++) {
        if (*cursor == '\n') {
            line++;
            character = 0;
        } else {
            character++;
        }
    }
    outPosition->line = line;
    outPosition->character = character + offset;
    return ZR_TRUE;
}

static SZrString *create_file_uri(SZrState *state, const TZrChar *path) {
    TZrChar buffer[ZR_TESTS_PATH_MAX * 2];
    TZrSize pathLength;
    TZrSize writeIndex = 0;

    if (state == ZR_NULL || path == ZR_NULL) {
        return ZR_NULL;
    }
    pathLength = strlen(path);
    if (pathLength + 16 >= sizeof(buffer)) {
        return ZR_NULL;
    }
#ifdef ZR_VM_PLATFORM_IS_WIN
    memcpy(buffer, "file:///", 8);
    writeIndex = 8;
#else
    memcpy(buffer, "file://", 7);
    writeIndex = 7;
#endif
    for (TZrSize index = 0; index < pathLength; index++) {
        buffer[writeIndex++] = path[index] == '\\' ? '/' : path[index];
    }
    buffer[writeIndex] = '\0';
    return ZrCore_String_Create(state, buffer, writeIndex);
}

static TZrBool write_text_file(const TZrChar *path,
                               const TZrChar *content,
                               TZrSize length) {
    FILE *file;
    size_t written;

    if (path == ZR_NULL || content == ZR_NULL ||
        !ZrTests_Path_EnsureParentDirectory(path)) {
        return ZR_FALSE;
    }
    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    written = fwrite(content, 1, (size_t)length, file);
    fclose(file);
    return written == (size_t)length;
}

static TZrBool prepare_binary_fixture(SZrState *state,
                                      SZrParityBinaryFixture *fixture) {
    static const TZrChar *projectContent =
            "{\n"
            "  \"name\": \"semantic_query_parity\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\"\n"
            "}\n";
    static const TZrChar *binarySource =
            "pub class Meter {\n"
            "    pub static property shared: int {\n"
            "        get { return 7; }\n"
            "    }\n"
            "}\n"
            "pub var binarySeed = fn(): int => 40;\n";
    static const TZrChar *mainContent =
            "var binary = import(\"semantic_query_provider\");\n"
            "var local = binary.binarySeed();\n"
            "var observed = binary.Meter.shared;\n"
            "return local + observed;\n";
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRoot[ZR_TESTS_PATH_MAX];
    TZrChar binaryRoot[ZR_TESTS_PATH_MAX];
    TZrChar *separator;
    SZrString *sourceName = ZR_NULL;
    SZrFunction *function = ZR_NULL;
    SZrBinaryWriterOptions options;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || fixture == ZR_NULL ||
        !ZrTests_Path_GetGeneratedArtifact(
                "language_server",
                "semantic_query_parity",
                "semantic_query_parity",
                ".zrp",
                fixture->projectPath,
                sizeof(fixture->projectPath))) {
        return ZR_FALSE;
    }
    snprintf(rootPath, sizeof(rootPath), "%s", fixture->projectPath);
    separator = strrchr(rootPath, '/');
    if (separator == ZR_NULL) {
        separator = strrchr(rootPath, '\\');
    }
    if (separator == ZR_NULL) {
        return ZR_FALSE;
    }
    *separator = '\0';
    ZrLibrary_File_PathJoin(rootPath, "src", sourceRoot);
    ZrLibrary_File_PathJoin(rootPath, "bin", binaryRoot);
    ZrLibrary_File_PathJoin(sourceRoot, "main.zr", fixture->mainPath);
    ZrLibrary_File_PathJoin(
            binaryRoot, "semantic_query_provider.zro", fixture->binaryPath);

    if (!write_text_file(
                fixture->projectPath, projectContent, strlen(projectContent)) ||
        !write_text_file(fixture->mainPath, mainContent, strlen(mainContent)) ||
        !ZrTests_Path_EnsureParentDirectory(fixture->binaryPath)) {
        return ZR_FALSE;
    }
    sourceName = ZrCore_String_Create(
            state, fixture->binaryPath, strlen(fixture->binaryPath));
    function = sourceName != ZR_NULL
                       ? ZrParser_Source_Compile(
                                 state,
                                 binarySource,
                                 strlen(binarySource),
                                 sourceName)
                       : ZR_NULL;
    memset(&options, 0, sizeof(options));
    options.moduleName = "semantic_query_provider";
    if (function != ZR_NULL) {
        success = ZrParser_Writer_WriteBinaryFileWithOptions(
                state, function, fixture->binaryPath, &options);
        ZrCore_Function_Free(state, function);
    }
    return success;
}

static TZrBool query_snapshot_is_stable(SZrState *state,
                                        SZrLspContext *lsp,
                                        SZrString *uri,
                                        const TZrChar *content,
                                        const TZrChar *callNeedle,
                                        TZrInt32 callOffset,
                                        const TZrChar *propertyNeedle,
                                        TZrInt32 propertyOffset) {
    SZrSemanticAnalyzer *analyzer;
    SZrLspPosition localPosition;
    SZrLspPosition callPosition;
    SZrLspPosition propertyPosition;
    SZrFilePosition localFilePosition;
    SZrFilePosition callFilePosition;
    SZrFilePosition propertyFilePosition;
    SZrFileRange localRange;
    SZrFileRange callRange;
    SZrFileRange propertyRange;
    SZrInferredType firstType = {0};
    SZrInferredType secondType = {0};
    SZrParserSemanticTypeQuery firstCanonical = {0};
    SZrParserSemanticTypeQuery secondCanonical = {0};
    SZrParserSemanticCallQuery firstCall = {0};
    SZrParserSemanticCallQuery secondCall = {0};
    SZrParserSemanticPropertyQuery firstProperty = {0};
    SZrParserSemanticPropertyQuery secondProperty = {0};
    SZrParserSemanticQueryFacts firstFacts = {0};
    SZrParserSemanticQueryFacts secondFacts = {0};
    SZrParserSemanticQueryDiagnostics firstDiagnostics = {0};
    SZrParserSemanticQueryDiagnostics secondDiagnostics = {0};
    const SZrSemanticReferenceFact *firstDefinition;
    const SZrSemanticReferenceFact *secondDefinition;
    const SZrSemanticReferenceFact *declaration;
    SZrArray firstReferences = {0};
    SZrArray secondReferences = {0};
    const SZrSemanticReferenceFact *const *firstReferencePtr;
    const SZrSemanticReferenceFact *const *secondReferencePtr;
    const SZrSemanticReferenceFact *firstReference;
    const SZrSemanticReferenceFact *secondReference;
    TZrChar firstSignature[256] = {0};
    TZrChar secondSignature[256] = {0};
    const TZrChar *failure = "initialization";
    TZrBool valid = ZR_FALSE;

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, lsp, uri);
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        !find_position(content, "return local", 0U, 7, &localPosition) ||
        !find_position(content, callNeedle, 0U, callOffset, &callPosition) ||
        !find_position(
                content, propertyNeedle, 0U, propertyOffset, &propertyPosition)) {
        failure = "analyzer or fixture position";
        goto cleanup;
    }
    localFilePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            lsp, uri, localPosition);
    callFilePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            lsp, uri, callPosition);
    propertyFilePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            lsp, uri, propertyPosition);
    localRange = ZrParser_FileRange_Create(localFilePosition, localFilePosition, uri);
    callRange = ZrParser_FileRange_Create(callFilePosition, callFilePosition, uri);
    propertyRange = ZrParser_FileRange_Create(
            propertyFilePosition, propertyFilePosition, uri);

    if (!ZrParser_SemanticQuery_TypeAt(
                analyzer->semanticContext, localRange, ZR_NULL, &firstType) ||
        !ZrParser_SemanticQuery_TypeAt(
                analyzer->semanticContext, localRange, ZR_NULL, &secondType)) {
        failure = "TypeAt";
        goto cleanup;
    }
    if (!ZrParser_SemanticQuery_CanonicalTypeAt(
                analyzer->semanticContext,
                localRange,
                ZR_NULL,
                &firstCanonical) ||
        !ZrParser_SemanticQuery_CanonicalTypeAt(
                analyzer->semanticContext,
                localRange,
                ZR_NULL,
                &secondCanonical) ||
        firstCanonical.typeId == ZR_SEMANTIC_ID_INVALID ||
        firstCanonical.typeId != secondCanonical.typeId ||
        firstCanonical.expression != secondCanonical.expression ||
        firstCanonical.reference != secondCanonical.reference) {
        failure = "CanonicalTypeAt";
        goto cleanup;
    }
    if (!ZrParser_SemanticQuery_CallAt(
                analyzer->semanticContext, callRange, ZR_NULL, &firstCall) ||
        !ZrParser_SemanticQuery_CallAt(
                analyzer->semanticContext, callRange, ZR_NULL, &secondCall) ||
        firstCall.callableTypeId == ZR_SEMANTIC_ID_INVALID ||
        firstCall.callableTypeId != secondCall.callableTypeId ||
        firstCall.expression != secondCall.expression ||
        firstCall.reference != secondCall.reference) {
        failure = "CallAt";
        goto cleanup;
    }
    if (!ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext,
                &firstCall,
                firstSignature,
                sizeof(firstSignature)) ||
        !ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext,
                &secondCall,
                secondSignature,
                sizeof(secondSignature)) ||
        firstSignature[0] == '\0' ||
        strcmp(firstSignature, secondSignature) != 0) {
        failure = "FormatCall";
        goto cleanup;
    }
    if (!ZrParser_SemanticQuery_FactsAt(
                analyzer->semanticContext, localRange, ZR_NULL, &firstFacts) ||
        !ZrParser_SemanticQuery_FactsAt(
                analyzer->semanticContext, localRange, ZR_NULL, &secondFacts) ||
        firstFacts.expression != secondFacts.expression ||
        firstFacts.reference != secondFacts.reference) {
        failure = "FactsAt";
        goto cleanup;
    }
    if (!ZrParser_SemanticQuery_PropertyAt(
                analyzer->semanticContext,
                propertyRange,
                ZR_NULL,
                &firstProperty) ||
        !ZrParser_SemanticQuery_PropertyAt(
                analyzer->semanticContext,
                propertyRange,
                ZR_NULL,
                &secondProperty) ||
        firstProperty.propertySymbolId == ZR_SEMANTIC_ID_INVALID ||
        firstProperty.propertyTypeId == ZR_SEMANTIC_ID_INVALID ||
        firstProperty.propertySymbolId != secondProperty.propertySymbolId ||
        firstProperty.propertyTypeId != secondProperty.propertyTypeId) {
        failure = "PropertyAt";
        goto cleanup;
    }

    firstDefinition = ZrParser_SemanticQuery_DefinitionOf(
            analyzer->semanticContext, localRange, ZR_NULL);
    secondDefinition = ZrParser_SemanticQuery_DefinitionOf(
            analyzer->semanticContext, localRange, ZR_NULL);
    declaration = firstDefinition != ZR_NULL
                          ? ZrParser_SemanticQuery_DeclarationOf(
                                    analyzer->semanticContext,
                                    firstDefinition->symbolId,
                                    ZR_NULL)
                          : ZR_NULL;
    if (firstDefinition == ZR_NULL || firstDefinition != secondDefinition ||
        declaration == ZR_NULL ||
        !ZrParser_SemanticQuery_ReferencesOf(
                analyzer->semanticContext,
                firstDefinition->symbolId,
                ZR_NULL,
                &firstReferences) ||
        !ZrParser_SemanticQuery_ReferencesOf(
                analyzer->semanticContext,
                firstDefinition->symbolId,
                ZR_NULL,
                &secondReferences) ||
        firstReferences.length == 0 ||
        firstReferences.length != secondReferences.length) {
        failure = "DefinitionOf, DeclarationOf, or ReferencesOf";
        goto cleanup;
    }
    firstReferencePtr = (const SZrSemanticReferenceFact *const *)ZrCore_Array_Get(
            &firstReferences, 0U);
    secondReferencePtr = (const SZrSemanticReferenceFact *const *)ZrCore_Array_Get(
            &secondReferences, 0U);
    firstReference = firstReferencePtr != ZR_NULL ? *firstReferencePtr : ZR_NULL;
    secondReference = secondReferencePtr != ZR_NULL ? *secondReferencePtr : ZR_NULL;
    if (firstReference == ZR_NULL || firstReference != secondReference ||
        !ZrParser_SemanticQuery_MaterializeDiagnostics(
                analyzer->semanticContext, ZR_NULL) ||
        !ZrParser_SemanticQuery_Diagnostics(
                analyzer->semanticContext, ZR_NULL, &firstDiagnostics) ||
        !ZrParser_SemanticQuery_Diagnostics(
                analyzer->semanticContext, ZR_NULL, &secondDiagnostics) ||
        firstDiagnostics.items != secondDiagnostics.items ||
        firstDiagnostics.count != secondDiagnostics.count) {
        failure = "reference view or Diagnostics";
        goto cleanup;
    }
    valid = ZR_TRUE;

cleanup:
    if (!valid) {
        printf(
                "  query parity stage=%s expressions=%zu references=%zu properties=%zu diagnostics=%zu\n",
                failure,
                analyzer != ZR_NULL && analyzer->semanticContext != ZR_NULL
                        ? (size_t)analyzer->semanticContext->expressionFacts.length
                        : 0U,
                analyzer != ZR_NULL && analyzer->semanticContext != ZR_NULL
                        ? (size_t)analyzer->semanticContext->referenceFacts.length
                        : 0U,
                analyzer != ZR_NULL && analyzer->semanticContext != ZR_NULL
                        ? (size_t)analyzer->semanticContext->propertyContracts.length
                        : 0U,
                analyzer != ZR_NULL && analyzer->semanticContext != ZR_NULL
                        ? (size_t)analyzer->semanticContext->queryDiagnostics.length
                        : 0U);
    }
    ZrCore_Array_Free(state, &firstReferences);
    ZrCore_Array_Free(state, &secondReferences);
    return valid;
}

static void test_source_semantic_query_snapshot_parity(SZrState *state) {
    static const TZrChar *content =
            "class Meter {\n"
            "    pri var stored: int = 7;\n"
            "    pub property value: int {\n"
            "        get { return this.stored; }\n"
            "    }\n"
            "}\n"
            "fn seed(value: int): int { return value; }\n"
            "fn read(meter: Meter): int {\n"
            "    var local = seed(meter.value);\n"
            "    return local;\n"
            "}\n";
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;

    TEST_START("LSP Semantic Query Source Snapshot Parity");
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///semantic_query_parity_source.zr",
            strlen("file:///semantic_query_parity_source.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !query_snapshot_is_stable(
                state,
                context,
                uri,
                content,
                "seed(meter.value)",
                0,
                "meter.value",
                6)) {
        TEST_FAIL(
                timer,
                "LSP Semantic Query Source Snapshot Parity",
                "Source query results were unavailable or changed across read-only calls");
    } else {
        TEST_PASS(timer, "LSP Semantic Query Source Snapshot Parity");
    }
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void free_local_reference_projection_results(
        SZrState *state,
        SZrArray *locations,
        SZrArray *highlights) {
    TZrSize index;

    if (state == ZR_NULL) {
        return;
    }
    if (locations != ZR_NULL && locations->isValid) {
        for (index = 0U; index < locations->length; index++) {
            SZrLspLocation **slot =
                    (SZrLspLocation **)ZrCore_Array_Get(locations, index);
            if (slot != ZR_NULL && *slot != ZR_NULL) {
                ZrCore_Memory_RawFree(
                        state->global, *slot, sizeof(SZrLspLocation));
            }
        }
        ZrCore_Array_Free(state, locations);
    }
    if (highlights != ZR_NULL && highlights->isValid) {
        for (index = 0U; index < highlights->length; index++) {
            SZrLspDocumentHighlight **slot =
                    (SZrLspDocumentHighlight **)ZrCore_Array_Get(
                            highlights, index);
            if (slot != ZR_NULL && *slot != ZR_NULL) {
                ZrCore_Memory_RawFree(
                        state->global,
                        *slot,
                        sizeof(SZrLspDocumentHighlight));
            }
        }
        ZrCore_Array_Free(state, highlights);
    }
}

static void test_local_reference_consumers_use_canonical_facts(
        SZrState *state) {
    static const TZrChar *content =
            "fn read(): int {\n"
            "    var value = 1;\n"
            "    value = 2;\n"
            "    return value;\n"
            "}\n";
    static const TZrChar *updatedContent =
            "fn read(): int {\n"
            "    var value = 1;\n"
            "    value = 2;\n"
            "    value = 3;\n"
            "    return value;\n"
            "}\n";
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrLspPosition position;
    SZrLspSemanticQuery query;
    SZrReferenceTracker *tracker = ZR_NULL;
    TZrSymbolId symbolId = ZR_SEMANTIC_ID_INVALID;
    SZrArray locations = {0};
    SZrArray highlights = {0};
    TZrSize readHighlightCount = 0U;
    TZrSize writeHighlightCount = 0U;
    TZrChar failureBuffer[192] = {0};
    const TZrChar *failure = "initial resolution";
    TZrBool valid = ZR_FALSE;

    TEST_START("LSP Local Reference Consumers Use Canonical Facts");
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///semantic_query_local_references.zr",
            strlen("file:///semantic_query_local_references.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !find_position(content, "return value", 0U, 7, &position) ||
        !ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, position, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL ||
        query.symbol == ZR_NULL ||
        query.symbol->semanticId == ZR_SEMANTIC_ID_INVALID ||
        query.analyzer == ZR_NULL ||
        query.analyzer->referenceTracker == ZR_NULL) {
        goto cleanup;
    }

    tracker = query.analyzer->referenceTracker;
    query.analyzer->referenceTracker = ZR_NULL;
    failure = "canonical reference projection";
    {
        TZrBool referencesAppended =
                ZrLanguageServer_LspSemanticQuery_AppendReferences(
                        state, context, &query, ZR_TRUE, &locations);
        TZrBool highlightsAppended =
                ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(
                        state, context, &query, &highlights);
        if (!referencesAppended || locations.length != 3U ||
            !highlightsAppended || highlights.length != 3U) {
            TZrSize matchingFactCount = 0U;
            TZrSize resolvedUseCount = 0U;
            for (TZrSize index = 0U;
                 index < query.analyzer->semanticContext->referenceFacts.length;
                 index++) {
                const SZrSemanticReferenceFact *fact =
                        (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                                &query.analyzer->semanticContext->referenceFacts,
                                index);
                if (fact != ZR_NULL &&
                    fact->symbolId == query.symbol->semanticId) {
                    matchingFactCount++;
                    if (fact->isResolved &&
                        fact->kind != ZR_SEMANTIC_REFERENCE_DECLARATION) {
                        resolvedUseCount++;
                    }
                }
            }
            snprintf(
                    failureBuffer,
                    sizeof(failureBuffer),
                    "canonical projection references=%d/%zu highlights=%d/%zu facts=%zu uses=%zu id=%llu",
                    referencesAppended,
                    (size_t)locations.length,
                    highlightsAppended,
                    (size_t)highlights.length,
                    (size_t)matchingFactCount,
                    (size_t)resolvedUseCount,
                    (unsigned long long)query.symbol->semanticId);
            failure = failureBuffer;
            goto cleanup;
        }
    }
    for (TZrSize index = 0U; index < highlights.length; index++) {
        SZrLspDocumentHighlight **slot =
                (SZrLspDocumentHighlight **)ZrCore_Array_Get(
                        &highlights, index);
        if (slot == ZR_NULL || *slot == ZR_NULL) {
            goto cleanup;
        }
        if ((*slot)->kind == 2) {
            readHighlightCount++;
        } else if ((*slot)->kind == 3) {
            writeHighlightCount++;
        }
    }
    if (readHighlightCount != 1U || writeHighlightCount != 2U) {
        goto cleanup;
    }

    free_local_reference_projection_results(state, &locations, &highlights);
    query.analyzer->referenceTracker = tracker;
    tracker = ZR_NULL;
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    failure = "updated snapshot re-resolution";
    {
        TZrBool updated = ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                updatedContent,
                strlen(updatedContent),
                2U);
        TZrBool positioned =
                find_position(updatedContent, "return value", 0U, 7, &position);
        TZrBool resolved = updated && positioned &&
                ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                        state, context, uri, position, &query);
        if (!resolved ||
            query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL ||
            query.symbol == ZR_NULL || query.analyzer == ZR_NULL ||
            query.analyzer->referenceTracker == ZR_NULL) {
            snprintf(
                    failureBuffer,
                    sizeof(failureBuffer),
                    "updated=%d positioned=%d resolved=%d kind=%d analyzer=%p symbol=%p tracker=%p",
                    updated,
                    positioned,
                    resolved,
                    query.kind,
                    (void *)query.analyzer,
                    (void *)query.symbol,
                    query.analyzer != ZR_NULL
                            ? (void *)query.analyzer->referenceTracker
                            : ZR_NULL);
            failure = failureBuffer;
            goto cleanup;
        }
    }
    tracker = query.analyzer->referenceTracker;
    query.analyzer->referenceTracker = ZR_NULL;
    if (!ZrLanguageServer_LspSemanticQuery_AppendReferences(
                state, context, &query, ZR_TRUE, &locations) ||
        locations.length != 4U ||
        !ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(
                state, context, &query, &highlights) ||
        highlights.length != 4U) {
        goto cleanup;
    }

    free_local_reference_projection_results(state, &locations, &highlights);
    failure = "unresolved SymbolId fail-closed";
    symbolId = query.symbol->semanticId;
    query.symbol->semanticId = ZR_SEMANTIC_ID_INVALID;
    if (ZrLanguageServer_LspSemanticQuery_AppendReferences(
                state, context, &query, ZR_TRUE, &locations) ||
        locations.length != 0U) {
        query.symbol->semanticId = symbolId;
        goto cleanup;
    }
    query.symbol->semanticId = symbolId;
    valid = ZR_TRUE;

cleanup:
    if (query.analyzer != ZR_NULL && tracker != ZR_NULL) {
        query.analyzer->referenceTracker = tracker;
    }
    free_local_reference_projection_results(state, &locations, &highlights);
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (valid) {
        TEST_PASS(timer, "LSP Local Reference Consumers Use Canonical Facts");
    } else {
        TEST_FAIL(
                timer,
                "LSP Local Reference Consumers Use Canonical Facts",
                failure);
    }
}

static void test_local_implementation_consumer_uses_canonical_relations(
        SZrState *state) {
    static const TZrChar *content =
            "interface Readable { fn read(): int; }\n"
            "class Device : Readable {\n"
            "    pub fn read(): int { return 1; }\n"
            "}\n"
            "class Other {\n"
            "    pub fn read(): int { return 2; }\n"
            "}\n";
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrLspPosition position;
    SZrLspSemanticQuery query;
    SZrArray locations = {0};
    SZrLspLocation *location = ZR_NULL;
    TZrChar failureBuffer[256] = {0};
    const TZrChar *failure = "implementation resolution";
    TZrBool valid = ZR_FALSE;

    TEST_START("LSP Local Implementation Consumer Uses Canonical Relations");
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///semantic_query_local_implementation.zr",
            strlen("file:///semantic_query_local_implementation.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !find_position(content, "interface Readable", 0U, 11, &position) ||
        !ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, position, &query)) {
        goto cleanup;
    }
    if (!ZrLanguageServer_Lsp_GetImplementation(
                state, context, uri, position, &locations)) {
        TZrSize matchingRelations = 0U;
        if (query.analyzer != ZR_NULL &&
            query.analyzer->semanticContext != ZR_NULL &&
            query.symbol != ZR_NULL) {
            for (TZrSize index = 0U;
                 index < query.analyzer->semanticContext->relationFacts.length;
                 index++) {
                const SZrSemanticRelationFact *relation =
                        (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                                &query.analyzer->semanticContext->relationFacts,
                                index);
                if (relation != ZR_NULL &&
                    relation->targetSymbolId == query.symbol->semanticId &&
                    (relation->kind == ZR_SEMANTIC_RELATION_IMPLEMENTATION ||
                     relation->kind == ZR_SEMANTIC_RELATION_OVERRIDE)) {
                    matchingRelations++;
                }
            }
        }
        snprintf(
                failureBuffer,
                sizeof(failureBuffer),
                "implementation query kind=%d symbol=%p id=%llu relations=%zu matching=%zu",
                query.kind,
                (void *)query.symbol,
                (unsigned long long)(query.symbol != ZR_NULL
                        ? query.symbol->semanticId
                        : ZR_SEMANTIC_ID_INVALID),
                (size_t)(query.analyzer != ZR_NULL &&
                                 query.analyzer->semanticContext != ZR_NULL
                        ? query.analyzer->semanticContext->relationFacts.length
                        : 0U),
                (size_t)matchingRelations);
        failure = failureBuffer;
        goto cleanup;
    }
    failure = "exact implementation relation";
    if (locations.length != 1U) {
        goto cleanup;
    }
    {
        SZrLspLocation **slot =
                (SZrLspLocation **)ZrCore_Array_Get(&locations, 0U);
        location = slot != ZR_NULL ? *slot : ZR_NULL;
    }
    if (location == ZR_NULL ||
        !ZrLanguageServer_Lsp_StringsEqual(location->uri, uri) ||
        location->range.start.line != 1 ||
        location->range.start.character != 0 ||
        location->range.end.line != 3 ||
        location->range.end.character != 1) {
        goto cleanup;
    }
    valid = ZR_TRUE;

cleanup:
    free_local_reference_projection_results(state, &locations, ZR_NULL);
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (valid) {
        TEST_PASS(
                timer,
                "LSP Local Implementation Consumer Uses Canonical Relations");
    } else {
        TEST_FAIL(
                timer,
                "LSP Local Implementation Consumer Uses Canonical Relations",
                failure);
    }
}

static void test_binary_semantic_query_snapshot_parity(SZrState *state) {
    SZrParityTimer timer;
    SZrParityBinaryFixture fixture = {0};
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    TZrChar *content = ZR_NULL;
    TZrSize contentLength = 0;

    TEST_START("LSP Semantic Query Binary Snapshot Parity");
    if (!prepare_binary_fixture(state, &fixture)) {
        TEST_FAIL(
                timer,
                "LSP Semantic Query Binary Snapshot Parity",
                "Could not generate the binary semantic-query fixture");
        return;
    }
    content = ZrTests_ReadTextFile(fixture.mainPath, &contentLength);
    context = ZrLanguageServer_LspContext_New(state);
    uri = create_file_uri(state, fixture.mainPath);
    if (content == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, contentLength, 1U) ||
        !query_snapshot_is_stable(
                state,
                context,
                uri,
                content,
                "binary.binarySeed()",
                7,
                "Meter.shared",
                6)) {
        TEST_FAIL(
                timer,
                "LSP Semantic Query Binary Snapshot Parity",
                "Binary query results were unavailable or changed across read-only calls");
    } else {
        TEST_PASS(timer, "LSP Semantic Query Binary Snapshot Parity");
    }
    free(content);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_native_semantic_query_snapshot_parity(SZrState *state) {
    static const TZrChar *content =
            "var {LinkedList} = import(\"zr.container\");\n"
            "var list: LinkedList<int> = null;\n"
            "var local = list.addLast(1);\n"
            "var {Pool, PoolReadRef} = import(\"zr.pooling\");\n"
            "var pool = init Pool<int>();\n"
            "var guard: PoolReadRef<int>;\n"
            "pool.tryRead(out guard);\n"
            "var observed = guard.value;\n"
            "return local;\n";
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;

    TEST_START("LSP Semantic Query Native Snapshot Parity");
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///semantic_query_parity_native.zr",
            strlen("file:///semantic_query_parity_native.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !query_snapshot_is_stable(
                state,
                context,
                uri,
                content,
                "list.addLast(1)",
                5,
                "guard.value",
                6)) {
        TEST_FAIL(
                timer,
                "LSP Semantic Query Native Snapshot Parity",
                "Native descriptor query results were unavailable or changed across read-only calls");
    } else {
        TEST_PASS(timer, "LSP Semantic Query Native Snapshot Parity");
    }
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        return 1;
    }
    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);
    test_source_semantic_query_snapshot_parity(state);
    test_local_reference_consumers_use_canonical_facts(state);
    test_local_implementation_consumer_uses_canonical_relations(state);
    test_binary_semantic_query_snapshot_parity(state);
    test_native_semantic_query_snapshot_parity(state);
    ZrCore_GlobalState_Free(global);
    return g_failures == 0 ? 0 : 1;
}
