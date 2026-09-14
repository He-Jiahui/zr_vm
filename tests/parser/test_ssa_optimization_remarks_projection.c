#include "zr_vm_core/optimization_remark.h"
#include "commands/explain_optimize_command.h"
#include "semantic/lsp_optimization_remarks.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static SZrOptimizationRemark projection_remark(void) {
    SZrOptimizationRemark remark;
    ZrCore_OptimizationRemark_Init(&remark);
    remark.moduleHash = 0x10u;
    remark.siteKey = 0x99u;
    remark.irHash = 0x20u;
    remark.sourceVersion = 0u;
    remark.sourceId = 7u;
    remark.sourceRange.startOffset = 0u;
    remark.sourceRange.endOffset = 4u;
    (void)strcpy(remark.pass, "vectorize");
    (void)strcpy(remark.module, "demo");
    remark.status = ZR_OPTIMIZATION_REMARK_MISSED;
    remark.reason = ZR_OPTIMIZATION_REMARK_REASON_CODE_BUDGET;
    remark.backendMask = ZR_OPTIMIZATION_REMARK_BACKEND_JIT;
    remark.evidence = ZR_OPTIMIZATION_REMARK_EVIDENCE_ESTIMATED;
    remark.estimatedCost = 11u;
    remark.before = 1u;
    remark.after = 2u;
    return remark;
}

static void read_file(FILE *file, char *buffer, size_t capacity) {
    size_t count;
    assert(file != NULL && buffer != NULL && capacity > 0u);
    assert(fseek(file, 0L, SEEK_SET) == 0);
    count = fread(buffer, 1u, capacity - 1u, file);
    buffer[count] = '\0';
}

static void test_cli_options_and_projection(void) {
    const TZrChar *arguments[] = {
        "--json", "--module", "demo", "--reason", "code_budget",
        "--backend", "jit", "--range", "0:4", "--limit", "1"
    };
    SZrCliExplainOptimizeOptions options;
    SZrOptimizationRemarkStore store;
    SZrOptimizationRemark remark = projection_remark();
    char error[128];
    char output[2048];
    FILE *file;

    assert(ZrCli_ExplainOptimizeOptions_Parse(
                (int)(sizeof(arguments) / sizeof(arguments[0])), arguments,
                &options, error, sizeof(error)));
    assert(options.json == ZR_TRUE);
    assert(options.hasModule == ZR_TRUE && strcmp(options.module, "demo") == 0);
    assert(options.reasonMask == ZR_OPTIMIZATION_REMARK_REASON_MASK(
                ZR_OPTIMIZATION_REMARK_REASON_CODE_BUDGET));
    assert(options.backendMask == ZR_OPTIMIZATION_REMARK_BACKEND_JIT);
    assert(options.hasSourceRange == ZR_TRUE &&
           options.sourceRange.endOffset == 4u);
    assert(options.pageLimit == 1u);

    ZrCore_OptimizationRemarks_StoreInit(&store);
    assert(ZrCore_OptimizationRemarks_Append(&store, &remark, NULL));
    file = tmpfile();
    assert(file != NULL);
    assert(ZrCli_ExplainOptimize_RunStore(&store, &options, file, stderr) == 0);
    read_file(file, output, sizeof(output));
    assert(strstr(output, "\"schemaVersion\":1") != NULL);
    assert(strstr(output, "\"siteKey\":153") != NULL);
    assert(strstr(output, "\"module\":\"demo\"") != NULL);
    assert(strstr(output, "\"reason\":\"code_budget\"") != NULL);
    (void)fclose(file);

    /* Text and JSON are projections of the same identity, including the
     * address-free site key and canonical source id. */
    options.json = ZR_FALSE;
    file = tmpfile();
    assert(file != NULL);
    assert(ZrCli_ExplainOptimize_RunStore(&store, &options, file, stderr) == 0);
    read_file(file, output, sizeof(output));
    assert(strstr(output, "code_budget") != NULL);
    assert(strstr(output, "source 7 [0..4]") != NULL);
    assert(strstr(output, "site=153") != NULL);
    (void)fclose(file);
    ZrCore_OptimizationRemarks_StoreFree(&store);
}

static TZrBool always_cancel(void *userData) {
    (void)userData;
    return ZR_TRUE;
}

static void test_lsp_projection_version_and_generation(void) {
    const TZrChar content[] = "🙂x\n";
    SZrOptimizationRemark remark = projection_remark();
    SZrLspOptimizationRemark projected;
    SZrOptimizationRemarkDiagnostic diagnostic;
    SZrOptimizationRemarkStore store;
    SZrLspOptimizationRemarkRequest request;
    SZrLspOptimizationRemarkPage page;
    SZrLspOptimizationRemarkCache cache;
    TZrUInt64 generation;

    assert(ZrLanguageServer_LspOptimizationRemark_ProjectVersioned(
                &remark, ZR_TRUE, 0u, content, sizeof(content) - 1u,
                &projected, &diagnostic) == ZR_LSP_OPTIMIZATION_REMARK_OK);
    assert(projected.sourceVersion == 0u);
    assert(projected.siteKey == 0x99u);
    assert(projected.range.start.line == 0 &&
           projected.range.start.character == 0);
    assert(projected.range.end.line == 0 &&
           projected.range.end.character == 2);
    assert(strcmp(projected.reason, "code_budget") == 0);
    assert(ZrLanguageServer_LspOptimizationRemark_ProjectVersioned(
                &remark, ZR_TRUE, 1u, content, sizeof(content) - 1u,
                &projected, &diagnostic) == ZR_LSP_OPTIMIZATION_REMARK_STALE);

    /* LSP positions are signed 32-bit integers; a malformed/out-of-snapshot
     * byte offset must clamp rather than wrap to a negative coordinate. */
    remark.sourceRange.startOffset = UINT32_MAX;
    remark.sourceRange.endOffset = UINT32_MAX;
    assert(ZrLanguageServer_LspOptimizationRemark_ProjectVersioned(
                &remark, ZR_TRUE, 0u, ZR_NULL, 0u,
                &projected, &diagnostic) == ZR_LSP_OPTIMIZATION_REMARK_OK);
    assert(projected.range.start.character == INT32_MAX &&
           projected.range.end.character == INT32_MAX);
    remark = projection_remark();

    ZrCore_OptimizationRemarks_StoreInit(&store);
    assert(ZrCore_OptimizationRemarks_Append(&store, &remark, &diagnostic));
    ZrLanguageServer_LspOptimizationRemarkRequest_Init(&request);
    request.hasDocumentVersion = ZR_TRUE;
    request.documentVersion = 0u;
    request.content = content;
    request.contentLength = sizeof(content) - 1u;
    assert(ZrLanguageServer_LspOptimizationRemarks_Query(
                &store, &request, &page, &diagnostic) == ZR_LSP_OPTIMIZATION_REMARK_OK);
    assert(page.count == 1u && page.items[0].sourceVersion == 0u);
    ZrLanguageServer_LspOptimizationRemarkPage_Free(&page);

    request.documentVersion = 1u;
    assert(ZrLanguageServer_LspOptimizationRemarks_Query(
                &store, &request, &page, &diagnostic) == ZR_LSP_OPTIMIZATION_REMARK_STALE);
    assert(page.stale == ZR_TRUE);
    ZrLanguageServer_LspOptimizationRemarkPage_Free(&page);

    request.documentVersion = 0u;
    request.isCancelled = always_cancel;
    assert(ZrLanguageServer_LspOptimizationRemarks_Query(
                &store, &request, &page, &diagnostic) == ZR_LSP_OPTIMIZATION_REMARK_CANCELLED);
    assert(page.cancelled == ZR_TRUE);
    ZrLanguageServer_LspOptimizationRemarkPage_Free(&page);

    ZrLanguageServer_LspOptimizationRemarkCache_Init(&cache);
    ZrLanguageServer_LspOptimizationRemarkCache_Begin(&cache, 0u);
    generation = cache.generation;
    assert(ZrLanguageServer_LspOptimizationRemarkCache_PublishGeneration(
                &cache, 0u, generation));
    assert(ZrLanguageServer_LspOptimizationRemarkCache_AcceptGeneration(
                &cache, 0u, generation));
    ZrLanguageServer_LspOptimizationRemarkCache_Begin(&cache, 1u);
    assert(!ZrLanguageServer_LspOptimizationRemarkCache_AcceptGeneration(
                &cache, 0u, generation));
    assert(!ZrLanguageServer_LspOptimizationRemarkCache_PublishGeneration(
                &cache, 1u, generation));
    ZrCore_OptimizationRemarks_StoreFree(&store);
}

static void test_cli_rejects_invalid_filters(void) {
    static const TZrChar *const unknownOption[] = {"--not-a-filter"};
    static const TZrChar *const duplicateModule[] = {
        "--module", "demo", "--module", "other"
    };
    static const TZrChar *const reversedRange[] = {"--range", "8:2"};
    static const TZrChar *const emptyModule[] = {"--module="};
    static const TZrChar *const zeroLimit[] = {"--limit", "0"};
    static const TZrChar *const trailingMaskComma[] = {
        "--reason", "code_budget,"
    };
    SZrCliExplainOptimizeOptions options;
    char error[128];

    assert(!ZrCli_ExplainOptimizeOptions_Parse(
                1, unknownOption, &options, error, sizeof(error)));
    assert(error[0] != '\0');
    assert(!ZrCli_ExplainOptimizeOptions_Parse(
                (int)(sizeof(duplicateModule) / sizeof(duplicateModule[0])),
                duplicateModule, &options, error, sizeof(error)));
    assert(!ZrCli_ExplainOptimizeOptions_Parse(
                2, reversedRange, &options, error, sizeof(error)));
    assert(!ZrCli_ExplainOptimizeOptions_Parse(
                1, emptyModule, &options, error, sizeof(error)));
    assert(!ZrCli_ExplainOptimizeOptions_Parse(
                2, zeroLimit, &options, error, sizeof(error)));
    assert(!ZrCli_ExplainOptimizeOptions_Parse(
                2, trailingMaskComma, &options, error, sizeof(error)));
}

static void test_lsp_rejects_invalid_query_and_unrelated_stale_candidate(void) {
    const TZrChar content[] = "x";
    SZrOptimizationRemark remark = projection_remark();
    SZrOptimizationRemarkStore store;
    SZrLspOptimizationRemarkRequest request;
    SZrLspOptimizationRemarkPage page = {0};
    SZrOptimizationRemarkDiagnostic diagnostic;

    ZrCore_OptimizationRemarks_StoreInit(&store);
    assert(ZrCore_OptimizationRemarks_Append(&store, &remark, &diagnostic));
    ZrLanguageServer_LspOptimizationRemarkRequest_Init(&request);
    request.content = content;
    request.contentLength = sizeof(content) - 1u;
    request.hasSourceRange = ZR_TRUE;
    request.sourceRange.startOffset = 4u;
    request.sourceRange.endOffset = 1u;
    assert(ZrLanguageServer_LspOptimizationRemarks_Query(
                &store, &request, &page, &diagnostic) ==
           ZR_LSP_OPTIMIZATION_REMARK_INVALID);
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_QUERY);
    ZrLanguageServer_LspOptimizationRemarkPage_Free(&page);

    ZrLanguageServer_LspOptimizationRemarkRequest_Init(&request);
    request.content = content;
    request.contentLength = sizeof(content) - 1u;
    request.hasPass = ZR_TRUE;
    (void)memset(request.pass, 'x', sizeof(request.pass));
    assert(ZrLanguageServer_LspOptimizationRemarks_Query(
                &store, &request, &page, &diagnostic) ==
           ZR_LSP_OPTIMIZATION_REMARK_INVALID);
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_QUERY);
    ZrLanguageServer_LspOptimizationRemarkPage_Free(&page);

    /* A version mismatch in another module is not evidence that this query
     * is stale; the stale probe must retain all identity filters. */
    ZrLanguageServer_LspOptimizationRemarkRequest_Init(&request);
    request.content = content;
    request.contentLength = sizeof(content) - 1u;
    request.hasDocumentVersion = ZR_TRUE;
    request.documentVersion = 9u;
    request.hasModule = ZR_TRUE;
    (void)strcpy(request.module, "other-module");
    assert(ZrLanguageServer_LspOptimizationRemarks_Query(
                &store, &request, &page, &diagnostic) ==
           ZR_LSP_OPTIMIZATION_REMARK_OK);
    assert(page.count == 0u && page.totalMatches == 0u &&
           page.droppedCount == 0u &&
           page.stale == ZR_FALSE);
    ZrLanguageServer_LspOptimizationRemarkPage_Free(&page);

    assert(ZrLanguageServer_LspOptimizationRemark_ProjectVersioned(
                ZR_NULL, ZR_TRUE, 0u, content, sizeof(content) - 1u,
                ZR_NULL, &diagnostic) == ZR_LSP_OPTIMIZATION_REMARK_INVALID);
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT);
    ZrCore_OptimizationRemarks_StoreFree(&store);
}

int main(void) {
    test_cli_options_and_projection();
    test_lsp_projection_version_and_generation();
    test_cli_rejects_invalid_filters();
    test_lsp_rejects_invalid_query_and_unrelated_stale_candidate();
    return 0;
}
