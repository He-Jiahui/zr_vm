#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_text_file_owned(const char *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    if (path == NULL) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)fileSize + 1u);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    if (fileSize > 0 && fread(buffer, 1u, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[fileSize] = '\0';
    fclose(file);
    return buffer;
}

static char *read_repo_text_file_owned(const char *relativePath) {
    const char *sourceFile = __FILE__;
    const char *marker;
    char path[1024];
    size_t rootLength;
    size_t relativeLength;

    if (relativePath == NULL) {
        return NULL;
    }

    marker = strstr(sourceFile, "tests/language_server/test_lsp_source_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\language_server\\test_lsp_source_contracts.c");
    }
    if (marker == NULL) {
        return read_text_file_owned(relativePath);
    }

    rootLength = (size_t)(marker - sourceFile);
    relativeLength = strlen(relativePath);
    if (rootLength + relativeLength + 1u >= sizeof(path)) {
        return NULL;
    }

    memcpy(path, sourceFile, rootLength);
    memcpy(path + rootLength, relativePath, relativeLength + 1u);
    return read_text_file_owned(path);
}

static int g_failures = 0;

static void assert_text_contains(const char *text, const char *needle) {
    if (strstr(text, needle) == NULL) {
        printf("Missing source contract text: %s\n", needle);
        g_failures++;
    }
}

static void assert_text_contains_none(const char *text, const char *needle) {
    if (strstr(text, needle) != NULL) {
        printf("Unexpected source contract text: %s\n", needle);
        g_failures++;
    }
}

static int text_range_contains(const char *start, const char *end, const char *needle) {
    size_t needleLength;

    if (start == NULL || end == NULL || needle == NULL || end < start) {
        return 0;
    }

    needleLength = strlen(needle);
    if (needleLength == 0) {
        return 1;
    }

    for (const char *cursor = start; cursor + needleLength <= end; cursor++) {
        if (strncmp(cursor, needle, needleLength) == 0) {
            return 1;
        }
    }
    return 0;
}

static const char *find_next_text(const char *text, const char *needle) {
    const char *first;

    if (text == NULL || needle == NULL) {
        return NULL;
    }

    first = strstr(text, needle);
    if (first == NULL) {
        return NULL;
    }
    return strstr(first + strlen(needle), needle);
}

static void assert_text_section_contains(const char *sectionName,
                                         const char *start,
                                         const char *end,
                                         const char *needle) {
    if (start == NULL || end == NULL || end <= start) {
        printf("Missing source contract section: %s\n", sectionName);
        g_failures++;
        return;
    }
    if (!text_range_contains(start, end, needle)) {
        printf("Missing source contract text in %s: %s\n", sectionName, needle);
        g_failures++;
    }
}

static void assert_text_section_contains_none(const char *sectionName,
                                              const char *start,
                                              const char *end,
                                              const char *needle) {
    if (start == NULL || end == NULL || end <= start) {
        printf("Missing source contract section: %s\n", sectionName);
        g_failures++;
        return;
    }
    if (text_range_contains(start, end, needle)) {
        printf("Unexpected source contract text in %s: %s\n", sectionName, needle);
        g_failures++;
    }
}

static void test_import_chain_location_conversion_does_not_use_static_append_state(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_import_chain.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_semantic_import_chain.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_Lsp_RangeFromFileRangeForDocument");
    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "ZrLanguageServer_LspRange_FromFileRangeWithContent");
    assert_text_contains_none(source, "ZrLanguageServer_LspRange_FromFileRange(range)");
    assert_text_contains_none(source, "g_semanticImportChainAppend");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_semantic_query_location_conversion_uses_shared_document_helper(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_semantic_query.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_Lsp_RangeFromFileRangeForDocument");
    assert_text_contains_none(source, "semantic_query_lsp_range_from_file_range");
    assert_text_contains_none(source, "semantic_query_get_document_content");
    assert_text_contains_none(source, "ZrLanguageServer_LspRange_FromFileRangeWithContent");

    free(source);
}

static void test_lsp_interface_range_conversion_uses_shared_document_helper(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_interface.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_Lsp_RangeFromFileRangeForDocument");
    assert_text_contains_none(source, "lsp_range_from_file_range_for_document");
    assert_text_contains_none(source, "ZrLanguageServer_LspRange_FromFileRange(");
    assert_text_contains_none(source, "ZrLanguageServer_LspRange_FromFileRangeWithContent");

    free(source);
}

static void test_lsp_shared_document_helpers_do_not_use_legacy_fallbacks(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c");
    const char *rangeStart;
    const char *rangeEnd;
    const char *positionStart;
    const char *positionEnd;

    if (source == NULL) {
        printf("FAIL: could not read lsp_interface_support.c\n");
        g_failures++;
        return;
    }

    rangeStart = strstr(source, "SZrLspRange ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(");
    rangeEnd = rangeStart != NULL
                   ? strstr(rangeStart, "SZrLspPosition ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(")
                   : NULL;
    positionStart = rangeEnd;
    positionEnd = positionStart != NULL
                      ? strstr(positionStart, "static void lsp_append_diagnostic_internal(")
                      : NULL;

    assert_text_section_contains("ZrLanguageServer_Lsp_RangeFromFileRangeForDocument",
                                 rangeStart,
                                 rangeEnd,
                                 "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_section_contains_none("ZrLanguageServer_Lsp_RangeFromFileRangeForDocument",
                                      rangeStart,
                                      rangeEnd,
                                      "return ZrLanguageServer_LspRange_FromFileRange(range);");
    assert_text_section_contains("ZrLanguageServer_Lsp_PositionFromFilePositionForDocument",
                                 positionStart,
                                 positionEnd,
                                 "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_section_contains_none("ZrLanguageServer_Lsp_PositionFromFilePositionForDocument",
                                      positionStart,
                                      positionEnd,
                                      "return ZrLanguageServer_LspPosition_FromFilePosition(position);");

    free(source);
}

static void test_lsp_document_file_position_has_no_legacy_fallback(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c");
    const char *positionStart;
    const char *positionEnd;

    if (source == NULL) {
        printf("FAIL: could not read lsp_interface.c\n");
        g_failures++;
        return;
    }

    positionStart = strstr(source, "SZrFilePosition ZrLanguageServer_Lsp_GetDocumentFilePosition(");
    positionEnd = positionStart != NULL
                      ? strstr(positionStart, "TZrBool ZrLanguageServer_Lsp_UpdateDocumentCore(")
                      : NULL;

    assert_text_section_contains("ZrLanguageServer_Lsp_GetDocumentFilePosition",
                                 positionStart,
                                 positionEnd,
                                 "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_section_contains("ZrLanguageServer_Lsp_GetDocumentFilePosition",
                                 positionStart,
                                 positionEnd,
                                 "ZrParser_FilePosition_Create(0, 0, 0)");
    assert_text_section_contains_none("ZrLanguageServer_Lsp_GetDocumentFilePosition",
                                      positionStart,
                                      positionEnd,
                                      "return ZrLanguageServer_LspPosition_ToFilePosition(position);");

    free(source);
}

static void test_lsp_no_content_position_range_apis_are_removed(void) {
    char *headerSource = read_repo_text_file_owned(
        "zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h");
    char *positionSource = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_position.c");

    if (headerSource == NULL || positionSource == NULL) {
        printf("FAIL: could not read LSP position API source files\n");
        g_failures++;
        free(headerSource);
        free(positionSource);
        return;
    }

    assert_text_contains_none(headerSource, "ZrLanguageServer_LspRange_FromFileRange(SZrFileRange");
    assert_text_contains_none(headerSource, "ZrLanguageServer_LspRange_ToFileRange(SZrLspRange");
    assert_text_contains_none(headerSource, "ZrLanguageServer_LspPosition_FromFilePosition(SZrFilePosition");
    assert_text_contains_none(headerSource, "ZrLanguageServer_LspPosition_ToFilePosition(SZrLspPosition");
    assert_text_contains_none(positionSource, "ZrLanguageServer_LspRange_FromFileRange(SZrFileRange");
    assert_text_contains_none(positionSource, "ZrLanguageServer_LspRange_ToFileRange(SZrLspRange");
    assert_text_contains_none(positionSource, "ZrLanguageServer_LspPosition_FromFilePosition(SZrFilePosition");
    assert_text_contains_none(positionSource, "ZrLanguageServer_LspPosition_ToFilePosition(SZrLspPosition");

    free(headerSource);
    free(positionSource);
}

static void test_lsp_interface_identifier_scan_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c");
    const char *charStart;
    const char *charEnd;
    const char *rangeStart;
    const char *rangeEnd;

    if (source == NULL) {
        printf("FAIL: could not read lsp_interface.c\n");
        g_failures++;
        return;
    }

    charStart = find_next_text(source, "static TZrBool lsp_position_is_identifier_char(");
    charEnd = charStart != NULL
                  ? strstr(charStart, "static SZrFilePosition lsp_file_position_from_offset(")
                  : NULL;
    rangeStart = find_next_text(source, "static TZrBool lsp_try_get_identifier_range_at_position(");
    rangeEnd = rangeStart != NULL
                   ? strstr(rangeStart, "static void lsp_normalize_rename_location_ranges(")
                   : NULL;

    assert_text_section_contains("lsp_position_is_identifier_char",
                                 charStart,
                                 charEnd,
                                 "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_section_contains_none("lsp_position_is_identifier_char",
                                      charStart,
                                      charEnd,
                                      "fileVersion->content");
    assert_text_section_contains("lsp_try_get_identifier_range_at_position",
                                 rangeStart,
                                 rangeEnd,
                                 "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_section_contains_none("lsp_try_get_identifier_range_at_position",
                                      rangeStart,
                                      rangeEnd,
                                      "fileVersion->content");

    free(source);
}

static void test_lsp_interface_completion_code_span_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c");
    const char *completionStart;
    const char *completionEnd;

    if (source == NULL) {
        printf("FAIL: could not read lsp_interface.c\n");
        g_failures++;
        return;
    }

    completionStart = strstr(source, "TZrBool ZrLanguageServer_Lsp_GetCompletion(");
    completionEnd = completionStart != NULL
                        ? strstr(completionStart, "TZrBool ZrLanguageServer_Lsp_GetHover(")
                        : NULL;

    assert_text_section_contains("ZrLanguageServer_Lsp_GetCompletion",
                                 completionStart,
                                 completionEnd,
                                 "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_section_contains_none("ZrLanguageServer_Lsp_GetCompletion",
                                      completionStart,
                                      completionEnd,
                                      "fileVersion->content");

    free(source);
}

static void test_lsp_interface_hover_documentation_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c");
    const char *hoverStart;
    const char *hoverEnd;

    if (source == NULL) {
        printf("FAIL: could not read lsp_interface.c\n");
        g_failures++;
        return;
    }

    hoverStart = strstr(source, "TZrBool ZrLanguageServer_Lsp_GetHover(");
    hoverEnd = hoverStart != NULL
                   ? strstr(hoverStart, "TZrBool ZrLanguageServer_Lsp_GetRichHover(")
                   : NULL;

    assert_text_section_contains("ZrLanguageServer_Lsp_GetHover",
                                 hoverStart,
                                 hoverEnd,
                                 "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_section_contains_none("ZrLanguageServer_Lsp_GetHover",
                                      hoverStart,
                                      hoverEnd,
                                      "fileVersion->content");

    free(source);
}

static void test_lsp_inlay_position_conversion_uses_shared_document_helper(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/interface/lsp_inlay_hints.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_inlay_hints.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_Lsp_PositionFromFilePositionForDocument");
    assert_text_contains_none(source, "lsp_inlay_position_from_file_position");
    assert_text_contains_none(source, "ZrLanguageServer_LspPosition_FromFilePositionWithContent");

    free(source);
}

static void test_project_navigation_position_conversion_uses_interface_helper(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_project_navigation.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_Lsp_GetDocumentFilePosition");
    assert_text_contains_none(source, "ZrLanguageServer_LspPosition_ToFilePosition");

    free(source);
}

static void test_project_navigation_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_project_navigation.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_project_refresh_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned("zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_project.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_metadata_provider_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_metadata_provider.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_semantic_query_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_semantic_query.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_incremental_parser_parse_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c");
    const char *parseStart;
    const char *parseEnd;

    if (source == NULL) {
        printf("FAIL: could not read incremental_parser.c\n");
        g_failures++;
        return;
    }

    parseStart = strstr(source, "TZrBool ZrLanguageServer_IncrementalParser_Parse(");
    parseEnd = parseStart != NULL
                   ? strstr(parseStart, "SZrAstNode *ZrLanguageServer_IncrementalParser_GetAST(")
                   : NULL;

    assert_text_section_contains("ZrLanguageServer_IncrementalParser_Parse",
                                 parseStart,
                                 parseEnd,
                                 "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_section_contains_none("ZrLanguageServer_IncrementalParser_Parse",
                                      parseStart,
                                      parseEnd,
                                      "fileVersion->content");
    assert_text_section_contains_none("ZrLanguageServer_IncrementalParser_Parse",
                                      parseStart,
                                      parseEnd,
                                      "fileVersion->contentLength");

    free(source);
}

static void test_incremental_parser_content_uses_versioned_refcounted_block(void) {
    char *headerSource = read_repo_text_file_owned(
        "zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h");
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c");

    if (headerSource == NULL || source == NULL) {
        printf("FAIL: could not read incremental parser content ownership sources\n");
        g_failures++;
        free(headerSource);
        free(source);
        return;
    }

    assert_text_contains(headerSource, "typedef struct SZrFileVersionContentBlock");
    assert_text_contains(headerSource, "SZrFileVersionContentBlock *textBlock");
    assert_text_contains(headerSource, "SZrFileVersionContentBlock *contentBlock");
    assert_text_contains(headerSource, "TZrSize contentGeneration");
    assert_text_contains(headerSource, "TZrSize refCount");
    assert_text_contains(source, "content_block_retain(");
    assert_text_contains(source, "content_block_release(");
    assert_text_contains_none(source, "fileVersion->content");
    assert_text_contains_none(source, "fileVersion->contentLength");

    free(headerSource);
    free(source);
}

static void test_editor_features_use_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_editor_features.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_token_metadata_hover_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/lsp_token_metadata.c");
    const char *hoverStart;
    const char *hoverEnd;

    if (source == NULL) {
        printf("FAIL: could not read lsp_token_metadata.c\n");
        g_failures++;
        return;
    }

    hoverStart = strstr(source, "TZrBool ZrLanguageServer_Lsp_TryGetMetaMethodHover(");
    hoverEnd = hoverStart != NULL ? source + strlen(source) : NULL;

    assert_text_section_contains("ZrLanguageServer_Lsp_TryGetMetaMethodHover",
                                 hoverStart,
                                 hoverEnd,
                                 "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_section_contains_none("ZrLanguageServer_Lsp_TryGetMetaMethodHover",
                                      hoverStart,
                                      hoverEnd,
                                      "fileVersion->content");

    free(source);
}

static void test_semantic_tokens_source_scan_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_tokens.c");
    const char *tokensStart;
    const char *tokensEnd;

    if (source == NULL) {
        printf("FAIL: could not read lsp_semantic_tokens.c\n");
        g_failures++;
        return;
    }

    tokensStart = strstr(source, "TZrBool ZrLanguageServer_Lsp_GetSemanticTokens(");
    tokensEnd = tokensStart != NULL
                    ? strstr(tokensStart, "TZrSize ZrLanguageServer_Lsp_SemanticTokenTypeCount(")
                    : NULL;

    assert_text_section_contains("ZrLanguageServer_Lsp_GetSemanticTokens",
                                 tokensStart,
                                 tokensEnd,
                                 "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_section_contains_none("ZrLanguageServer_Lsp_GetSemanticTokens",
                                      tokensStart,
                                      tokensEnd,
                                      "fileVersion->content");

    free(source);
}

static void test_folding_ranges_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/lsp_folding_ranges.c");
    const char *foldingStart;
    const char *foldingEnd;

    if (source == NULL) {
        printf("FAIL: could not read lsp_folding_ranges.c\n");
        g_failures++;
        return;
    }

    foldingStart = strstr(source, "TZrBool ZrLanguageServer_Lsp_GetFoldingRanges(");
    foldingEnd = foldingStart != NULL ? source + strlen(source) : NULL;

    assert_text_section_contains("ZrLanguageServer_Lsp_GetFoldingRanges",
                                 foldingStart,
                                 foldingEnd,
                                 "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_section_contains_none("ZrLanguageServer_Lsp_GetFoldingRanges",
                                      foldingStart,
                                      foldingEnd,
                                      "fileVersion->content");

    free(source);
}

static void test_document_links_uses_content_snapshot_for_open_documents(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/lsp_document_links.c");
    const char *linksStart;
    const char *linksEnd;

    if (source == NULL) {
        printf("FAIL: could not read lsp_document_links.c\n");
        g_failures++;
        return;
    }

    linksStart = strstr(source, "TZrBool ZrLanguageServer_Lsp_GetDocumentLinks(");
    linksEnd = linksStart != NULL ? source + strlen(source) : NULL;

    assert_text_section_contains("ZrLanguageServer_Lsp_GetDocumentLinks",
                                 linksStart,
                                 linksEnd,
                                 "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_section_contains_none("ZrLanguageServer_Lsp_GetDocumentLinks",
                                      linksStart,
                                      linksEnd,
                                      "fileVersion->content");

    free(source);
}

static void test_signature_help_code_span_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c");
    const char *signatureStart;
    const char *signatureEnd;

    if (source == NULL) {
        printf("FAIL: could not read lsp_signature_help.c\n");
        g_failures++;
        return;
    }

    signatureStart = strstr(source, "TZrBool ZrLanguageServer_Lsp_GetSignatureHelp(");
    signatureEnd = signatureStart != NULL ? source + strlen(source) : NULL;

    assert_text_section_contains("ZrLanguageServer_Lsp_GetSignatureHelp",
                                 signatureStart,
                                 signatureEnd,
                                 "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_section_contains_none("ZrLanguageServer_Lsp_GetSignatureHelp",
                                      signatureStart,
                                      signatureEnd,
                                      "fileVersion->content");

    free(source);
}

static void test_code_action_imports_use_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/lsp_code_action_imports.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_code_action_imports.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_super_navigation_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/lsp_super_navigation.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_super_navigation.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_code_actions_use_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/lsp_code_actions.c");
    const char *actionsStart;
    const char *actionsEnd;

    if (source == NULL) {
        printf("FAIL: could not read lsp_code_actions.c\n");
        g_failures++;
        return;
    }

    actionsStart = strstr(source, "TZrBool ZrLanguageServer_Lsp_GetCodeActions(");
    actionsEnd = actionsStart != NULL ? source + strlen(source) : NULL;

    assert_text_section_contains("ZrLanguageServer_Lsp_GetCodeActions",
                                 actionsStart,
                                 actionsEnd,
                                 "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_hierarchy_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/lsp_hierarchy.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_hierarchy.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_stdio_document_color_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/stdio/stdio_document_color.c");

    if (source == NULL) {
        printf("FAIL: could not read stdio_document_color.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_stdio_completion_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/stdio/stdio_completion.c");

    if (source == NULL) {
        printf("FAIL: could not read stdio_completion.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_stdio_moniker_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/stdio/stdio_moniker.c");

    if (source == NULL) {
        printf("FAIL: could not read stdio_moniker.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_stdio_inline_completion_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/stdio/stdio_inline_completion.c");

    if (source == NULL) {
        printf("FAIL: could not read stdio_inline_completion.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_stdio_linked_editing_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/stdio/stdio_linked_editing.c");

    if (source == NULL) {
        printf("FAIL: could not read stdio_linked_editing.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_stdio_diagnostics_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/stdio/stdio_diagnostics.c");

    if (source == NULL) {
        printf("FAIL: could not read stdio_diagnostics.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_stdio_documents_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/stdio/stdio_documents.c");

    if (source == NULL) {
        printf("FAIL: could not read stdio_documents.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_stdio_inline_value_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/stdio/stdio_inline_value.c");

    if (source == NULL) {
        printf("FAIL: could not read stdio_inline_value.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

static void test_stdio_position_encoding_uses_content_snapshot(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/stdio/stdio_position_encoding.c");

    if (source == NULL) {
        printf("FAIL: could not read stdio_position_encoding.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains_none(source, "fileVersion->content");

    free(source);
}

int main(void) {
    printf("==========\n");
    printf("Language Server - LSP Source Contract Tests\n");
    printf("==========\n\n");

    test_import_chain_location_conversion_does_not_use_static_append_state();
    test_semantic_query_location_conversion_uses_shared_document_helper();
    test_lsp_interface_range_conversion_uses_shared_document_helper();
    test_lsp_shared_document_helpers_do_not_use_legacy_fallbacks();
    test_lsp_document_file_position_has_no_legacy_fallback();
    test_lsp_no_content_position_range_apis_are_removed();
    test_lsp_interface_identifier_scan_uses_content_snapshot();
    test_lsp_interface_completion_code_span_uses_content_snapshot();
    test_lsp_interface_hover_documentation_uses_content_snapshot();
    test_lsp_inlay_position_conversion_uses_shared_document_helper();
    test_project_navigation_position_conversion_uses_interface_helper();
    test_project_navigation_uses_content_snapshot();
    test_project_refresh_uses_content_snapshot();
    test_metadata_provider_uses_content_snapshot();
    test_semantic_query_uses_content_snapshot();
    test_incremental_parser_parse_uses_content_snapshot();
    test_incremental_parser_content_uses_versioned_refcounted_block();
    test_editor_features_use_content_snapshot();
    test_token_metadata_hover_uses_content_snapshot();
    test_semantic_tokens_source_scan_uses_content_snapshot();
    test_folding_ranges_uses_content_snapshot();
    test_document_links_uses_content_snapshot_for_open_documents();
    test_signature_help_code_span_uses_content_snapshot();
    test_code_action_imports_use_content_snapshot();
    test_super_navigation_uses_content_snapshot();
    test_code_actions_use_content_snapshot();
    test_hierarchy_uses_content_snapshot();
    test_stdio_document_color_uses_content_snapshot();
    test_stdio_completion_uses_content_snapshot();
    test_stdio_moniker_uses_content_snapshot();
    test_stdio_inline_completion_uses_content_snapshot();
    test_stdio_linked_editing_uses_content_snapshot();
    test_stdio_diagnostics_uses_content_snapshot();
    test_stdio_documents_uses_content_snapshot();
    test_stdio_inline_value_uses_content_snapshot();
    test_stdio_position_encoding_uses_content_snapshot();

    if (g_failures != 0) {
        printf("\nFAILED: %d LSP source contract test failure(s)\n", g_failures);
        return 1;
    }

    printf("PASS: Import-chain location conversion avoids static append state\n");
    printf("PASS: Semantic query location conversion uses shared document helper\n");
    printf("PASS: LSP interface range conversion uses shared document helper\n");
    printf("PASS: LSP shared document helpers avoid legacy fallbacks\n");
    printf("PASS: LSP document file position avoids legacy fallback\n");
    printf("PASS: LSP no-content position/range APIs are removed\n");
    printf("PASS: LSP interface identifier scan uses content snapshot\n");
    printf("PASS: LSP interface completion code span uses content snapshot\n");
    printf("PASS: LSP interface hover documentation uses content snapshot\n");
    printf("PASS: LSP inlay position conversion uses shared document helper\n");
    printf("PASS: Project navigation position conversion uses interface helper\n");
    printf("PASS: Project navigation uses content snapshot\n");
    printf("PASS: Project refresh uses content snapshot\n");
    printf("PASS: Metadata provider uses content snapshot\n");
    printf("PASS: Semantic query uses content snapshot\n");
    printf("PASS: Incremental parser parse uses content snapshot\n");
    printf("PASS: Incremental parser content uses versioned refcounted block\n");
    printf("PASS: Editor features use content snapshot\n");
    printf("PASS: Token metadata hover uses content snapshot\n");
    printf("PASS: Semantic tokens source scan uses content snapshot\n");
    printf("PASS: Folding ranges use content snapshot\n");
    printf("PASS: Document links use content snapshot for open documents\n");
    printf("PASS: Signature help code span uses content snapshot\n");
    printf("PASS: Code action imports use content snapshot\n");
    printf("PASS: Super navigation uses content snapshot\n");
    printf("PASS: Code actions use content snapshot\n");
    printf("PASS: Hierarchy uses content snapshot\n");
    printf("PASS: stdio document color uses content snapshot\n");
    printf("PASS: stdio completion uses content snapshot\n");
    printf("PASS: stdio moniker uses content snapshot\n");
    printf("PASS: stdio inline completion uses content snapshot\n");
    printf("PASS: stdio linked editing uses content snapshot\n");
    printf("PASS: stdio diagnostics uses content snapshot\n");
    printf("PASS: stdio documents uses content snapshot\n");
    printf("PASS: stdio inline value uses content snapshot\n");
    printf("PASS: stdio position encoding uses content snapshot\n");
    printf("\nPASSED: LSP source contract tests\n");
    return 0;
}
