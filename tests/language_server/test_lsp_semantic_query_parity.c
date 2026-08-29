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

static TZrBool hierarchy_item_name_equals(
        const SZrLspHierarchyItem *item,
        const TZrChar *expected) {
    return item != ZR_NULL && item->name != ZR_NULL && expected != ZR_NULL &&
           strcmp(ZrCore_String_GetNativeString(item->name), expected) == 0;
}

static void test_local_type_hierarchy_uses_canonical_relations(
        SZrState *state) {
    static const TZrChar *content =
            "class Base {}\n"
            "class SameName {}\n"
            "class Derived : Base {}\n"
            "class Other : SameName {}\n";
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrString *tamperedName = ZR_NULL;
    SZrLspPosition derivedPosition;
    SZrLspPosition basePosition;
    SZrArray derivedItems = {0};
    SZrArray baseItems = {0};
    SZrArray supertypes = {0};
    SZrArray subtypes = {0};
    SZrArray unresolved = {0};
    SZrArray stale = {0};
    SZrLspHierarchyItem *derivedItem = ZR_NULL;
    SZrLspHierarchyItem *baseItem = ZR_NULL;
    SZrLspHierarchyItem *superItem = ZR_NULL;
    SZrLspHierarchyItem *subItem = ZR_NULL;
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrSymbolTable *detachedSymbolTable = ZR_NULL;
    SZrSemanticReferenceFact *derivedDeclaration = ZR_NULL;
    const TZrChar *failure = "type hierarchy preparation";
    TZrBool valid = ZR_FALSE;

    TEST_START("LSP Local Type Hierarchy Uses Canonical Relations");
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///semantic_query_local_type_hierarchy.zr",
            strlen("file:///semantic_query_local_type_hierarchy.zr"));
    tamperedName = ZrCore_String_Create(
            state, "SameName", strlen("SameName"));
    if (context == ZR_NULL || uri == ZR_NULL || tamperedName == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !find_position(content, "class Derived", 0U, 6, &derivedPosition) ||
        !find_position(content, "class Base", 0U, 6, &basePosition)) {
        goto cleanup;
    }
    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(
            state, context, uri);
    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL) {
        failure = "semantic snapshot for symbol-table detachment";
        goto cleanup;
    }
    detachedSymbolTable = analyzer->symbolTable;
    analyzer->symbolTable = ZR_NULL;
    if (!ZrLanguageServer_Lsp_PrepareTypeHierarchy(
                state, context, uri, derivedPosition, &derivedItems) ||
        derivedItems.length != 1U ||
        !ZrLanguageServer_Lsp_PrepareTypeHierarchy(
                state, context, uri, basePosition, &baseItems) ||
        baseItems.length != 1U) {
        goto cleanup;
    }
    derivedItem = *(SZrLspHierarchyItem **)ZrCore_Array_Get(&derivedItems, 0U);
    baseItem = *(SZrLspHierarchyItem **)ZrCore_Array_Get(&baseItems, 0U);
    if (derivedItem == ZR_NULL || baseItem == ZR_NULL ||
        !derivedItem->hasSemanticIdentity ||
        !baseItem->hasSemanticIdentity ||
        derivedItem->semanticId == ZR_SEMANTIC_ID_INVALID ||
        derivedItem->semanticTypeId == ZR_SEMANTIC_ID_INVALID ||
        derivedItem->semanticVersion != 1U ||
        baseItem->semanticId == ZR_SEMANTIC_ID_INVALID ||
        baseItem->semanticTypeId == ZR_SEMANTIC_ID_INVALID ||
        baseItem->semanticVersion != 1U) {
        failure = "stable hierarchy identity";
        goto cleanup;
    }

    derivedDeclaration = (SZrSemanticReferenceFact *)
            ZrParser_SemanticQuery_DeclarationOf(
                    analyzer->semanticContext,
                    derivedItem->semanticId,
                    ZR_NULL);
    if (derivedDeclaration == ZR_NULL ||
        !derivedDeclaration->isResolved) {
        failure = "resolved declaration for exactness check";
        goto cleanup;
    }

    derivedItem->name = tamperedName;
    baseItem->name = tamperedName;
    if (!ZrLanguageServer_Lsp_GetTypeHierarchySupertypes(
                state, context, derivedItem, &supertypes) ||
        supertypes.length != 1U ||
        !ZrLanguageServer_Lsp_GetTypeHierarchySubtypes(
                state, context, baseItem, &subtypes) ||
        subtypes.length != 1U) {
        failure = "canonical BaseTypesOf/DerivedTypesOf projection";
        goto cleanup;
    }
    superItem = *(SZrLspHierarchyItem **)ZrCore_Array_Get(&supertypes, 0U);
    subItem = *(SZrLspHierarchyItem **)ZrCore_Array_Get(&subtypes, 0U);
    if (!hierarchy_item_name_equals(superItem, "Base") ||
        !hierarchy_item_name_equals(subItem, "Derived") ||
        superItem->semanticId != baseItem->semanticId ||
        subItem->semanticId != derivedItem->semanticId) {
        failure = "exact canonical hierarchy targets";
        goto cleanup;
    }

    derivedDeclaration->isResolved = ZR_FALSE;
    (void)ZrLanguageServer_Lsp_GetTypeHierarchySupertypes(
            state, context, derivedItem, &unresolved);
    derivedDeclaration->isResolved = ZR_TRUE;
    if (unresolved.length != 0U) {
        failure = "unresolved declaration did not fail closed";
        goto cleanup;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 2U)) {
        failure = "version update";
        goto cleanup;
    }
    (void)ZrLanguageServer_Lsp_GetTypeHierarchySupertypes(
            state, context, derivedItem, &stale);
    if (stale.length != 0U) {
        failure = "stale hierarchy item did not fail closed";
        goto cleanup;
    }
    valid = ZR_TRUE;

cleanup:
    if (derivedDeclaration != ZR_NULL) {
        derivedDeclaration->isResolved = ZR_TRUE;
    }
    if (analyzer != ZR_NULL && detachedSymbolTable != ZR_NULL) {
        analyzer->symbolTable = detachedSymbolTable;
    }
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &derivedItems);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &baseItems);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &supertypes);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &subtypes);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &unresolved);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &stale);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (valid) {
        TEST_PASS(timer, "LSP Local Type Hierarchy Uses Canonical Relations");
    } else {
        TEST_FAIL(
                timer,
                "LSP Local Type Hierarchy Uses Canonical Relations",
                failure);
    }
}

static void test_local_call_hierarchy_uses_canonical_edges(
        SZrState *state) {
    static const TZrChar *content =
            "fn helper(value: int): int { return value; }\n"
            "fn decoy(value: int): int { return value; }\n"
            "fn run(value: int): int {\n"
            "    var first = helper(value);\n"
            "    return first + helper(value);\n"
            "}\n";
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrString *tamperedRunName = ZR_NULL;
    SZrString *tamperedHelperName = ZR_NULL;
    SZrLspPosition runPosition;
    SZrLspPosition helperPosition;
    SZrArray runItems = {0};
    SZrArray helperItems = {0};
    SZrArray outgoing = {0};
    SZrArray incoming = {0};
    SZrArray stale = {0};
    SZrArray outgoingEdges = {0};
    SZrArray incomingEdges = {0};
    SZrLspSemanticQuery runQuery;
    SZrLspSemanticQuery helperQuery;
    SZrLspHierarchyItem *runItem = ZR_NULL;
    SZrLspHierarchyItem *helperItem = ZR_NULL;
    SZrLspHierarchyCall *outgoingCall = ZR_NULL;
    SZrLspHierarchyCall *incomingCall = ZR_NULL;
    const TZrChar *failure = "call hierarchy preparation";
    TZrBool valid = ZR_FALSE;

    TEST_START("LSP Local Call Hierarchy Uses Canonical Edges");
    ZrLanguageServer_LspSemanticQuery_Init(&runQuery);
    ZrLanguageServer_LspSemanticQuery_Init(&helperQuery);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///semantic_query_local_call_hierarchy.zr",
            strlen("file:///semantic_query_local_call_hierarchy.zr"));
    tamperedRunName = ZrCore_String_Create(
            state, "helper", strlen("helper"));
    tamperedHelperName = ZrCore_String_Create(
            state, "decoy", strlen("decoy"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        tamperedRunName == ZR_NULL || tamperedHelperName == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !find_position(content, "fn run", 0U, 3, &runPosition) ||
        !find_position(content, "fn helper", 0U, 3, &helperPosition) ||
        !ZrLanguageServer_Lsp_PrepareCallHierarchy(
                state, context, uri, runPosition, &runItems) ||
        runItems.length != 1U ||
        !ZrLanguageServer_Lsp_PrepareCallHierarchy(
                state, context, uri, helperPosition, &helperItems) ||
        helperItems.length != 1U) {
        goto cleanup;
    }
    runItem = *(SZrLspHierarchyItem **)ZrCore_Array_Get(&runItems, 0U);
    helperItem = *(SZrLspHierarchyItem **)ZrCore_Array_Get(&helperItems, 0U);
    if (runItem == ZR_NULL || helperItem == ZR_NULL ||
        !runItem->hasSemanticIdentity || !helperItem->hasSemanticIdentity ||
        runItem->semanticId == ZR_SEMANTIC_ID_INVALID ||
        runItem->semanticTypeId == ZR_SEMANTIC_ID_INVALID ||
        runItem->semanticVersion != 1U ||
        helperItem->semanticId == ZR_SEMANTIC_ID_INVALID ||
        helperItem->semanticTypeId == ZR_SEMANTIC_ID_INVALID ||
        helperItem->semanticVersion != 1U) {
        failure = "stable callable hierarchy identity";
        goto cleanup;
    }
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, runPosition, &runQuery) ||
        !ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, helperPosition, &helperQuery) ||
        runQuery.analyzer == ZR_NULL ||
        runQuery.analyzer->semanticContext == ZR_NULL ||
        helperQuery.analyzer != runQuery.analyzer) {
        failure = "canonical callable query snapshot";
        goto cleanup;
    }
    if (!ZrParser_SemanticQuery_OutgoingCalls(
                runQuery.analyzer->semanticContext,
                runItem->semanticId,
                ZR_NULL,
                &outgoingEdges)) {
        failure = "canonical parser outgoing call-edge availability";
        goto cleanup;
    }
    if (outgoingEdges.length != 2U) {
        failure = "canonical parser outgoing call-edge availability";
        goto cleanup;
    }
    if (!ZrParser_SemanticQuery_IncomingCalls(
                runQuery.analyzer->semanticContext,
                helperItem->semanticId,
                ZR_NULL,
                &incomingEdges) ||
        incomingEdges.length != 2U) {
        failure = "canonical parser incoming call-edge availability";
        goto cleanup;
    }

    runItem->name = tamperedRunName;
    helperItem->name = tamperedHelperName;
    if (!ZrLanguageServer_Lsp_GetCallHierarchyOutgoingCalls(
                state, context, runItem, &outgoing) ||
        outgoing.length != 1U ||
        !ZrLanguageServer_Lsp_GetCallHierarchyIncomingCalls(
                state, context, helperItem, &incoming) ||
        incoming.length != 1U) {
        failure = "canonical outgoing/incoming call edge projection";
        goto cleanup;
    }
    outgoingCall = *(SZrLspHierarchyCall **)ZrCore_Array_Get(&outgoing, 0U);
    incomingCall = *(SZrLspHierarchyCall **)ZrCore_Array_Get(&incoming, 0U);
    if (outgoingCall == ZR_NULL || incomingCall == ZR_NULL ||
        outgoingCall->item == ZR_NULL || incomingCall->item == ZR_NULL ||
        !hierarchy_item_name_equals(outgoingCall->item, "helper") ||
        !hierarchy_item_name_equals(incomingCall->item, "run") ||
        outgoingCall->item->semanticId != helperItem->semanticId ||
        incomingCall->item->semanticId != runItem->semanticId ||
        outgoingCall->fromRanges.length != 2U ||
        incomingCall->fromRanges.length != 2U) {
        failure = "exact grouped call hierarchy targets and callsites";
        goto cleanup;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 2U)) {
        failure = "version update";
        goto cleanup;
    }
    (void)ZrLanguageServer_Lsp_GetCallHierarchyOutgoingCalls(
            state, context, runItem, &stale);
    if (stale.length != 0U) {
        failure = "stale call hierarchy item did not fail closed";
        goto cleanup;
    }
    valid = ZR_TRUE;

cleanup:
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &runItems);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &helperItems);
    ZrLanguageServer_Lsp_FreeHierarchyCalls(state, &outgoing);
    ZrLanguageServer_Lsp_FreeHierarchyCalls(state, &incoming);
    ZrLanguageServer_Lsp_FreeHierarchyCalls(state, &stale);
    if (outgoingEdges.isValid) {
        ZrCore_Array_Free(state, &outgoingEdges);
    }
    if (incomingEdges.isValid) {
        ZrCore_Array_Free(state, &incomingEdges);
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &runQuery);
    ZrLanguageServer_LspSemanticQuery_Free(state, &helperQuery);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (valid) {
        TEST_PASS(timer, "LSP Local Call Hierarchy Uses Canonical Edges");
    } else {
        TEST_FAIL(
                timer,
                "LSP Local Call Hierarchy Uses Canonical Edges",
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

static void test_source_hover_consumes_canonical_symbol_fact_without_analyzer_state(
        SZrState *state) {
    static const TZrChar *content =
            "fn run(seed: int): int {\n"
            "    var result = seed + 1;\n"
            "    return result;\n"
            "}\n";
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrFileVersion *fileVersion = ZR_NULL;
    SZrSymbolTable *savedSymbolTable = ZR_NULL;
    SZrReferenceTracker *savedReferenceTracker = ZR_NULL;
    SZrAstNode *savedAnalyzerAst = ZR_NULL;
    SZrLspPosition usePosition;
    SZrLspSemanticQuery query;
    SZrLspHover *hover = ZR_NULL;
    TZrBool passed = ZR_FALSE;

    TEST_START("LSP Source Hover Consumes Canonical Symbol Fact Without Analyzer State");
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///canonical_source_hover_fact.zr",
            strlen("file:///canonical_source_hover_fact.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !find_position(content, "return result;", 0U, 7, &usePosition)) {
        TEST_FAIL(
                timer,
                "LSP Source Hover Consumes Canonical Symbol Fact Without Analyzer State",
                "Could not prepare canonical source hover fixture");
        goto cleanup;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        fileVersion == ZR_NULL || fileVersion->ast == ZR_NULL) {
        TEST_FAIL(
                timer,
                "LSP Source Hover Consumes Canonical Symbol Fact Without Analyzer State",
                "Source fixture did not expose a semantic snapshot");
        goto cleanup;
    }

    savedSymbolTable = analyzer->symbolTable;
    savedReferenceTracker = analyzer->referenceTracker;
    savedAnalyzerAst = analyzer->ast;
    analyzer->symbolTable = ZR_NULL;
    analyzer->referenceTracker = ZR_NULL;
    analyzer->ast = ZR_NULL;

    ZrLanguageServer_LspSemanticQuery_Init(&query);
    passed = ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                     state, context, uri, usePosition, &query) &&
             query.kind == ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL &&
             query.hasCanonicalSymbol &&
             query.canonicalSymbol.symbolId != ZR_SEMANTIC_ID_INVALID &&
             ZrLanguageServer_LspSemanticQuery_BuildHover(
                     state, context, &query, &hover) &&
             hover != ZR_NULL;
    if (passed && hover->contents.length > 0U) {
        SZrString **text = (SZrString **)ZrCore_Array_Get(&hover->contents, 0U);
        passed = text != ZR_NULL && *text != ZR_NULL &&
                 strstr(ZrCore_String_GetNativeString(*text), "result") != ZR_NULL &&
                 strstr(ZrCore_String_GetNativeString(*text), "int") != ZR_NULL;
    } else {
        passed = ZR_FALSE;
    }

    analyzer->symbolTable = savedSymbolTable;
    savedSymbolTable = ZR_NULL;
    analyzer->referenceTracker = savedReferenceTracker;
    savedReferenceTracker = ZR_NULL;
    analyzer->ast = savedAnalyzerAst;
    savedAnalyzerAst = ZR_NULL;
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    if (passed) {
        TEST_PASS(
                timer,
                "LSP Source Hover Consumes Canonical Symbol Fact Without Analyzer State");
    } else {
        TEST_FAIL(
                timer,
                "LSP Source Hover Consumes Canonical Symbol Fact Without Analyzer State",
                "Source hover did not consume the parser SymbolAt fact after analyzer state was detached");
    }

cleanup:
    if (savedSymbolTable != ZR_NULL && analyzer != ZR_NULL) {
        analyzer->symbolTable = savedSymbolTable;
    }
    if (savedReferenceTracker != ZR_NULL && analyzer != ZR_NULL) {
        analyzer->referenceTracker = savedReferenceTracker;
    }
    if (savedAnalyzerAst != ZR_NULL && analyzer != ZR_NULL) {
        analyzer->ast = savedAnalyzerAst;
    }
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

#include "test_lsp_semantic_call_hierarchy_cases.h"
#include "test_lsp_canonical_completion_cases.h"
#include "test_lsp_external_member_reference_identity_cases.h"

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
    test_local_type_hierarchy_uses_canonical_relations(state);
    test_local_call_hierarchy_uses_canonical_edges(state);
    test_local_method_call_hierarchy_uses_canonical_edges(state);
    test_local_lambda_call_hierarchy_uses_canonical_edges(state);
    test_binary_semantic_query_snapshot_parity(state);
    test_native_semantic_query_snapshot_parity(state);
    test_canonical_visible_symbol_completion_survives_symbol_table_detachment(state);
    test_source_hover_consumes_canonical_symbol_fact_without_analyzer_state(state);
    test_external_member_references_reject_mismatched_declaration_identity(state);
    ZrCore_GlobalState_Free(global);
    return g_failures == 0 ? 0 : 1;
}
