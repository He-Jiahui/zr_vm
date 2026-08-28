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

static void test_binary_metadata_coordinate_projection_is_explicitly_scoped(void) {
    char *coordinateSource = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/interface/lsp_binary_metadata_coordinates.c");
    char *semanticQuerySource = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c");
    char *projectNavigationSource = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c");

    if (coordinateSource == NULL || semanticQuerySource == NULL || projectNavigationSource == NULL) {
        printf("FAIL: could not read binary metadata coordinate projection sources\n");
        g_failures++;
        free(coordinateSource);
        free(semanticQuerySource);
        free(projectNavigationSource);
        return;
    }

    assert_text_contains(coordinateSource, "ZrLanguageServer_Lsp_TryRangeFromBinaryMetadataCoordinates");
    assert_text_contains(coordinateSource, "ZrLanguageServer_Lsp_TryFilePositionFromBinaryMetadataCoordinates");
    assert_text_contains(coordinateSource, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
    assert_text_contains(semanticQuerySource, "sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA");
    assert_text_contains(semanticQuerySource, "ZrLanguageServer_Lsp_TryRangeFromBinaryMetadataCoordinates");
    assert_text_contains(projectNavigationSource, "sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA");
    assert_text_contains(projectNavigationSource, "ZrLanguageServer_Lsp_TryRangeFromBinaryMetadataCoordinates");
    assert_text_contains(projectNavigationSource,
                         "ZrLanguageServer_Lsp_TryFilePositionFromBinaryMetadataCoordinates");

    free(coordinateSource);
    free(semanticQuerySource);
    free(projectNavigationSource);
}

static void test_descriptor_metadata_coordinate_projection_is_explicitly_scoped(void) {
    char *coordinateSource = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/interface/lsp_descriptor_metadata_coordinates.c");
    char *semanticQuerySource = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c");
    char *projectNavigationSource = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c");

    if (coordinateSource == NULL || semanticQuerySource == NULL || projectNavigationSource == NULL) {
        printf("FAIL: could not read descriptor metadata coordinate projection sources\n");
        g_failures++;
        free(coordinateSource);
        free(semanticQuerySource);
        free(projectNavigationSource);
        return;
    }

    assert_text_contains(coordinateSource, "ZrLanguageServer_Lsp_TryRangeFromDescriptorMetadataCoordinates");
    assert_text_contains_none(coordinateSource, "TryRangeFromBinaryMetadataCoordinates");
    assert_text_contains(semanticQuerySource,
                         "sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN");
    assert_text_contains(semanticQuerySource,
                         "ZrLanguageServer_Lsp_TryRangeFromDescriptorMetadataCoordinates");
    assert_text_contains(projectNavigationSource,
                         "sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN");
    assert_text_contains(projectNavigationSource,
                         "ZrLanguageServer_Lsp_TryRangeFromDescriptorMetadataCoordinates");

    free(coordinateSource);
    free(semanticQuerySource);
    free(projectNavigationSource);
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
    char *callHierarchy = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_call_hierarchy.c");
    char *typeHierarchy = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_type_hierarchy.c");

    if (source == NULL || callHierarchy == NULL || typeHierarchy == NULL) {
        printf("FAIL: could not read hierarchy sources\n");
        g_failures++;
        free(source);
        free(callHierarchy);
        free(typeHierarchy);
        return;
    }

    assert_text_contains(
        callHierarchy, "ZrLanguageServer_Lsp_GetDocumentFileVersion");
    assert_text_contains(
        typeHierarchy, "ZrLanguageServer_Lsp_GetDocumentFileVersion");
    assert_text_contains_none(source, "fileVersion->content");
    assert_text_contains_none(callHierarchy, "fileVersion->content");
    assert_text_contains_none(typeHierarchy, "fileVersion->content");

    free(source);
    free(callHierarchy);
    free(typeHierarchy);
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

static void test_stdio_diagnostics_uses_shared_diagnostic_store(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/stdio/stdio_diagnostics.c");

    if (source == NULL) {
        printf("FAIL: could not read stdio_diagnostics.c\n");
        g_failures++;
        return;
    }

    assert_text_contains(source, "ZrLanguageServer_LspDiagnosticStore_BuildResultId");
    assert_text_contains_none(source, "ZrLanguageServer_LspSemanticSnapshot_GetActive");
    assert_text_contains_none(source, "ZrLanguageServer_LspSemanticSnapshot_Acquire");
    assert_text_contains_none(source, "ZrLanguageServer_LspSemanticSnapshot_FormatResultId");
    assert_text_contains_none(source, "ZrLanguageServer_LspSemanticSnapshot_Release");
    assert_text_contains_none(source, "ZrLanguageServer_FileVersionContentSnapshot_Acquire");
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

static void test_wasm_diagnostics_use_canonical_projection(void) {
    char *cmake = read_repo_text_file_owned(
        "zr_vm_language_server/CMakeLists.txt");
    char *exports = read_repo_text_file_owned(
        "zr_vm_language_server/wasm/wasm_exports.cpp");
    char *projection = read_repo_text_file_owned(
        "zr_vm_language_server/wasm/wasm_diagnostic_json.cpp");

    if (cmake == NULL || exports == NULL || projection == NULL) {
        printf("FAIL: could not read WASM diagnostic projection sources\n");
        g_failures++;
        free(cmake);
        free(exports);
        free(projection);
        return;
    }

    assert_text_contains(cmake, "_wasm_ZrLspGetDiagnosticReport");
    assert_text_contains(cmake, "_wasm_ZrLspGetWorkspaceDiagnosticReports");
    assert_text_contains(exports, "#include \"wasm_diagnostic_json.h\"");
    assert_text_contains(exports, "ZrLanguageServer_Wasm_SerializeDiagnostics");
    assert_text_contains_none(exports, "static cJSON* serialize_diagnostics");
    assert_text_contains(projection, "ZR_LSP_FIELD_RELATED_INFORMATION");
    assert_text_contains(projection, "ZR_LSP_FIELD_FIXES");
    assert_text_contains(projection, "ZR_LSP_FIELD_DESCRIPTOR_ID");
    assert_text_contains(projection, "ZR_LSP_FIELD_CODE_DESCRIPTION");
    assert_text_contains(projection, "ZR_LSP_FIELD_NO_FIX_REASON");
    assert_text_contains(projection, "ZrLanguageServer_Lsp_DiagnosticNoFixReasonName");

    free(cmake);
    free(exports);
    free(projection);
}

static void test_type_mismatch_diagnostics_use_compiler_query_projection(void) {
    char *typecheck = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c");
    char *support = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c");
    const char *consumerStart;
    const char *consumerEnd;

    if (typecheck == NULL || support == NULL) {
        printf("FAIL: could not read semantic analyzer type mismatch sources\n");
        g_failures++;
        free(typecheck);
        free(support);
        return;
    }

    assert_text_contains(
        typecheck,
        "ZrParser_AssignmentCompatibility_CheckDetailed");
    assert_text_contains(
        typecheck,
        "semantic_publish_current_compiler_diagnostic");
    assert_text_contains(
        typecheck,
        "expr->type != ZR_AST_ASSIGNMENT_EXPRESSION");
    assert_text_contains_none(
        typecheck,
        "ZrLanguageServer_SemanticAnalyzer_ReportTypeMismatch");
    assert_text_contains_none(
        typecheck,
        "semantic_analyzer_type_mismatch_diagnostics.h");
    assert_text_contains_none(
        typecheck,
        "semantic_check_method_call(");
    assert_text_contains_none(
        typecheck,
        "semantic_call_matches_parameters(");
    assert_text_contains_none(
        typecheck,
        "Type mismatch in method call");

    consumerStart = strstr(
        support,
        "void ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic");
    consumerEnd = consumerStart != NULL
        ? strstr(consumerStart + 1, "TZrBool ZrLanguageServer_SemanticAnalyzer_InferExactExpressionType")
        : NULL;
    assert_text_section_contains(
        "compiler error query consumer",
        consumerStart,
        consumerEnd,
        "ZrLanguageServer_SemanticAnalyzer_PublishCurrentCompilerQueryDiagnostic");
    assert_text_section_contains_none(
        "compiler error query consumer",
        consumerStart,
        consumerEnd,
        "ZrLanguageServer_Diagnostic_FromStructured");
    assert_text_section_contains_none(
        "compiler error query consumer",
        consumerStart,
        consumerEnd,
        "ZrLanguageServer_SemanticAnalyzer_AddDiagnostic");

    free(typecheck);
    free(support);
}

static void test_reachability_diagnostics_use_semantic_query_projection(void) {
    char *typecheck = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c");
    char *reachability = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_reachability.c");
    char *unionPatterns = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_union_patterns.c");

    if (typecheck == NULL || reachability == NULL || unionPatterns == NULL) {
        printf("FAIL: could not read semantic analyzer reachability sources\n");
        g_failures++;
        free(typecheck);
        free(reachability);
        free(unionPatterns);
        return;
    }

    assert_text_contains(typecheck, "semantic_record_reachability_fact");
    assert_text_contains(reachability, "semantic_control_record_unreachable_fact");
    assert_text_contains(unionPatterns, "ZrParser_SemanticFacts_AppendReachability");
    assert_text_contains_none(typecheck, "\"unreachable_branch\"");
    assert_text_contains_none(typecheck, "\"short_circuit_unreachable\"");
    assert_text_contains_none(typecheck, "\"unreachable_code\"");
    assert_text_contains_none(reachability, "\"unreachable_loop_body\"");
    assert_text_contains_none(unionPatterns, "\"unreachable_union_switch_default\"");

    free(typecheck);
    free(reachability);
    free(unionPatterns);
}

static void test_const_assignment_diagnostics_use_semantic_query_projection(void) {
    char *typecheck = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c");

    if (typecheck == NULL) {
        printf("FAIL: could not read semantic analyzer typecheck source\n");
        g_failures++;
        free(typecheck);
        return;
    }

    assert_text_contains(
        typecheck,
        "ZrParser_ConstAssignment_PublishDiagnostic");
    assert_text_contains_none(typecheck, "\"const_assignment\"");
    assert_text_contains_none(typecheck, "Cannot assign to const");
    assert_text_contains_none(
        typecheck,
        "ZrParser_SemanticQuery_SymbolAt");
    assert_text_contains_none(
        typecheck,
        "ZrParser_ConstAssignment_EvaluateContext");
    assert_text_contains_none(
        typecheck,
        "ZrParser_ConstAssignment_BuildDiagnostic");
    assert_text_contains_none(
        typecheck,
        "ZrParser_SemanticFacts_AppendDiagnostic");
    assert_text_contains_none(
        typecheck,
        "ZrLanguageServer_SemanticAnalyzer_ProjectConstAssignment");

    free(typecheck);
}

static void test_variance_diagnostics_use_parser_query_projection(void) {
    char *typecheck = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c");

    if (typecheck == NULL) {
        printf("FAIL: could not read semantic analyzer typecheck source\n");
        g_failures++;
        return;
    }

    assert_text_contains(
        typecheck,
        "ZrParser_Variance_PublishInterfaceDiagnostics");
    assert_text_contains_none(
        typecheck,
        "ZrParser_Variance_InterfaceViolationAt");
    assert_text_contains_none(
        typecheck,
        "ZrParser_Variance_BuildDiagnostic");
    assert_text_contains_none(
        typecheck,
        "ZrParser_SemanticFacts_AppendDiagnostic");
    assert_text_contains_none(
        typecheck,
        "ZrLanguageServer_SemanticAnalyzer_ValidateInterfaceVarianceRules");
    assert_text_contains_none(
        typecheck,
        "ZrLanguageServer_SemanticAnalyzer_AddDiagnostic");
    assert_text_contains_none(
        typecheck,
        "semantic_validate_interface_type_variance");
    assert_text_contains_none(typecheck, "\"invalid_variance\"");

    free(typecheck);
}

static void test_interface_const_field_diagnostics_use_parser_query_projection(void) {
    char *symbols = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c");

    if (symbols == NULL) {
        printf("FAIL: could not read semantic analyzer symbols source\n");
        g_failures++;
        return;
    }

    assert_text_contains(
        symbols,
        "ZrParser_InterfaceContract_ConstFieldViolationAt");
    assert_text_contains(
        symbols,
        "ZrParser_InterfaceContract_BuildConstFieldDiagnostic");
    assert_text_contains(
        symbols,
        "ZrParser_SemanticFacts_AppendDiagnostic");
    assert_text_contains_none(
        symbols,
        "Interface field '%s' is const, but implementation field is not const");
    assert_text_contains_none(symbols, "TODO: \u5982\u679c\u5b57\u6bb5\u672a\u627e\u5230");

    free(symbols);
}

static void test_unresolved_reference_diagnostics_use_parser_query_projection(void) {
    char *materializer = read_repo_text_file_owned(
        "zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_unresolved_diagnostics.c");
    char *projection = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c");
    char *typecheck = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c");

    if (materializer == NULL || projection == NULL || typecheck == NULL) {
        printf("FAIL: could not read unresolved-reference diagnostic sources\n");
        g_failures++;
        free(materializer);
        free(projection);
        free(typecheck);
        return;
    }

    assert_text_contains(materializer, "fact->isResolved");
    assert_text_contains(materializer, "candidate->isResolved");
    assert_text_contains(materializer, "&candidate->range, &fact->range");
    assert_text_contains(materializer, "\"unresolved_reference\"");
    assert_text_contains(materializer, "\"member_not_found\"");
    assert_text_contains(
        materializer,
        "ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION");
    assert_text_contains(
        projection,
        "ZrParser_SemanticQuery_MaterializeDiagnostics");
    assert_text_contains(
        projection,
        "ZrLanguageServer_Diagnostic_FromStructured");
    assert_text_contains_none(projection, "\"unresolved_reference\"");
    assert_text_contains_none(projection, "\"member_not_found\"");
    assert_text_contains_none(typecheck, "\"unresolved_reference\"");
    assert_text_contains_none(typecheck, "\"member_not_found\"");

    free(materializer);
    free(projection);
    free(typecheck);
}

static void test_named_call_compatibility_uses_parser_inference_projection(void) {
    char *typecheck = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c");

    if (typecheck == NULL) {
        printf("FAIL: could not read semantic analyzer typecheck source\n");
        g_failures++;
        return;
    }

    assert_text_contains(
        typecheck,
        "semantic_check_primary_call_with_parser_inference");
    assert_text_contains_none(typecheck, "ZrParser_TypeEnvironment_LookupFunction");
    assert_text_contains_none(typecheck, "ZrParser_FunctionCallOverload_Resolve");
    assert_text_contains_none(typecheck, "ZrParser_FunctionCallCompatibility_Check");
    assert_text_contains_none(typecheck, "\"Type mismatch in function call\"");

    free(typecheck);
}

static void test_assignment_ownership_uses_parser_diagnostic_projection(void) {
    char *typecheck = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c");

    if (typecheck == NULL) {
        printf("FAIL: could not read semantic analyzer typecheck source\n");
        g_failures++;
        return;
    }

    assert_text_contains(
        typecheck,
        "semantic_publish_current_compiler_diagnostic");
    assert_text_contains_none(
        typecheck,
        "semantic_emit_ownership_compatibility_diagnostic");

    free(typecheck);
}

static void test_reference_tracker_uses_canonical_identity_and_snapshot_source(void) {
    char *tracker = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c");
    char *analyzer = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c");
    char *querySource = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_source.c");

    if (tracker == NULL || analyzer == NULL || querySource == NULL) {
        printf("FAIL: could not read reference identity sources\n");
        g_failures++;
        free(tracker);
        free(analyzer);
        free(querySource);
        return;
    }

    assert_text_contains(tracker, "reference->symbolId = symbol->semanticId");
    assert_text_contains(tracker, "ZrCore_Value_InitAsUInt");
    assert_text_contains_none(tracker, "symbol->name");
    assert_text_contains_none(tracker, "ZrCore_Value_InitAsRawObject");
    assert_text_contains(
        analyzer,
        "ZrLanguageServer_SemanticAnalyzer_BindQuerySource");
    assert_text_contains(querySource, "analyzer->ast->location.source");

    free(tracker);
    free(analyzer);
    free(querySource);
}

static void test_local_reference_consumers_use_parser_relation_queries(void) {
    char *referenceQuery = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c");
    char *semanticQuery = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c");
    char *projectNavigation = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c");

    if (referenceQuery == NULL || semanticQuery == NULL || projectNavigation == NULL) {
        printf("FAIL: could not read local reference consumer sources\n");
        g_failures++;
        free(referenceQuery);
        free(semanticQuery);
        free(projectNavigation);
        return;
    }

    assert_text_contains(
        referenceQuery, "ZrParser_SemanticQuery_ReferencesOf");
    assert_text_contains(
        referenceQuery, "ZrParser_SemanticQuery_DeclarationOf");
    assert_text_contains(
        referenceQuery, "ZrLanguageServer_SemanticAnalyzer_BindQuerySource");
    assert_text_contains_none(referenceQuery, "referenceTracker");
    assert_text_contains_none(referenceQuery, "symbol->name");
    assert_text_contains_none(
        semanticQuery, "semantic_query_normalize_symbol_reference_range");
    assert_text_contains(
        projectNavigation,
        "ZrLanguageServer_LspSemanticReferenceQuery_AppendReferencesForSymbol");
    assert_text_contains_none(
        projectNavigation, "ReferenceTracker_FindReferences");

    free(referenceQuery);
    free(semanticQuery);
    free(projectNavigation);
}

static void test_local_definition_consumer_uses_snapshot_source(void) {
    char *definitionQuery = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_definition_query.c");
    char *semanticQuery = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c");

    if (definitionQuery == NULL || semanticQuery == NULL) {
        printf("FAIL: could not read local definition consumer source\n");
        g_failures++;
        free(definitionQuery);
        free(semanticQuery);
        return;
    }

    assert_text_contains(
        definitionQuery, "ZrParser_SemanticQuery_DefinitionsOf");
    assert_text_contains(
        definitionQuery, "ZrParser_SemanticQuery_DeclarationOf");
    assert_text_contains(
        definitionQuery, "ZrLanguageServer_SemanticAnalyzer_BindQuerySource");
    assert_text_contains_none(
        definitionQuery,
        "definitionRange.source != ZR_NULL ? definitionRange.source : query->uri");
    assert_text_contains_none(
        definitionQuery, "query->symbol->location.source");
    assert_text_contains_none(
        semanticQuery, "semantic_query_symbol_lookup_range");
    assert_text_contains_none(
        semanticQuery, "semantic_query_try_enum_member_name_range");

    free(definitionQuery);
    free(semanticQuery);
}

static void test_local_implementation_consumer_uses_parser_relations(void) {
    char *implementationQuery = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_implementation_query.c");
    char *editorFeatures = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c");
    char *analysis = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c");

    if (implementationQuery == NULL || editorFeatures == NULL ||
        analysis == NULL) {
        printf("FAIL: could not read local implementation consumer sources\n");
        g_failures++;
        free(implementationQuery);
        free(editorFeatures);
        free(analysis);
        return;
    }

    assert_text_contains(
        implementationQuery, "ZrParser_SemanticQuery_ImplementationsOf");
    assert_text_contains(
        implementationQuery, "ZrLanguageServer_SemanticAnalyzer_BindQuerySource");
    assert_text_contains_none(implementationQuery, "symbol->name");
    assert_text_contains_none(implementationQuery, "memberName");
    assert_text_contains_none(implementationQuery, "referenceTracker");
    assert_text_contains_none(implementationQuery, "strstr");
    assert_text_contains(
        editorFeatures,
        "ZrLanguageServer_LspSemanticImplementationQuery_Append");
    assert_text_contains(
        analysis,
        "ZrParser_SemanticRelations_PublishCompilerContracts");

    free(implementationQuery);
    free(editorFeatures);
    free(analysis);
}

static void test_local_type_hierarchy_uses_parser_relations(void) {
    char *hierarchyQuery = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_type_hierarchy.c");
    char *hierarchy = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/lsp_hierarchy.c");
    char *stdioParser = read_repo_text_file_owned(
        "zr_vm_language_server/stdio/stdio_hierarchy.c");
    char *stdioJson = read_repo_text_file_owned(
        "zr_vm_language_server/stdio/stdio_editor_features_json.c");

    if (hierarchyQuery == NULL || hierarchy == NULL ||
        stdioParser == NULL || stdioJson == NULL) {
        printf("FAIL: could not read canonical type hierarchy sources\n");
        g_failures++;
        free(hierarchyQuery);
        free(hierarchy);
        free(stdioParser);
        free(stdioJson);
        return;
    }

    assert_text_contains(
        hierarchyQuery, "ZrParser_SemanticQuery_BaseTypesOf");
    assert_text_contains(
        hierarchyQuery, "ZrParser_SemanticQuery_DerivedTypesOf");
    assert_text_contains(
        hierarchyQuery, "ZrLanguageServer_LspSemanticQuery_ResolveAtPosition");
    assert_text_contains(
        hierarchyQuery, "ZrLanguageServer_SemanticAnalyzer_BindQuerySource");
    assert_text_contains_none(hierarchyQuery, "GetDocumentSymbols");
    assert_text_contains_none(hierarchyQuery, "referenceTracker");
    assert_text_contains_none(
        hierarchyQuery, "lsp_hierarchy_string_text");
    assert_text_contains_none(
        hierarchyQuery, "symbol_name_matches");
    assert_text_contains_none(hierarchyQuery, "strcmp");
    assert_text_contains_none(hierarchyQuery, "memcmp");
    assert_text_contains(
        hierarchy, "ZrLanguageServer_LspSemanticTypeHierarchy_Prepare");
    assert_text_contains(
        hierarchy, "ZrLanguageServer_LspSemanticTypeHierarchy_AppendSupertypes");
    assert_text_contains(
        hierarchy, "ZrLanguageServer_LspSemanticTypeHierarchy_AppendSubtypes");
    assert_text_contains_none(hierarchy, "lsp_hierarchy_type_header_contains_base");
    assert_text_contains(stdioParser, "hasSemanticIdentity");
    assert_text_contains(stdioParser, "semanticVersion");
    assert_text_contains(stdioJson, "hasSemanticIdentity");
    assert_text_contains(stdioJson, "semanticTypeId");

    free(hierarchyQuery);
    free(hierarchy);
    free(stdioParser);
    free(stdioJson);
}

static void test_local_call_hierarchy_uses_parser_edges(void) {
    char *hierarchyQuery = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_call_hierarchy.c");
    char *hierarchy = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/lsp_hierarchy.c");

    if (hierarchyQuery == NULL || hierarchy == NULL) {
        printf("FAIL: could not read canonical call hierarchy sources\n");
        g_failures++;
        free(hierarchyQuery);
        free(hierarchy);
        return;
    }

    assert_text_contains(
        hierarchyQuery, "ZrParser_SemanticQuery_OutgoingCalls");
    assert_text_contains(
        hierarchyQuery, "ZrParser_SemanticQuery_IncomingCalls");
    assert_text_contains(
        hierarchyQuery, "ZrParser_SemanticQuery_DeclarationOf");
    assert_text_contains(
        hierarchyQuery, "ZrParser_Semantic_FindSymbolById");
    assert_text_contains(hierarchyQuery, "ZR_AST_LAMBDA_EXPRESSION");
    assert_text_contains(
        hierarchyQuery, "ZrLanguageServer_LspSemanticQuery_ResolveAtPosition");
    assert_text_contains(
        hierarchyQuery, "ZrLanguageServer_SemanticAnalyzer_BindQuerySource");
    assert_text_contains_none(hierarchyQuery, "GetDocumentSymbols");
    assert_text_contains_none(hierarchyQuery, "referenceTracker");
    assert_text_contains_none(
        hierarchyQuery, "ZrParser_Semantic_FindSymbolByNameAndKind");
    assert_text_contains_none(hierarchyQuery, "strcmp");
    assert_text_contains_none(hierarchyQuery, "memcmp");
    assert_text_contains(
        hierarchy, "ZrLanguageServer_LspSemanticCallHierarchy_Prepare");
    assert_text_contains(
        hierarchy, "ZrLanguageServer_LspSemanticCallHierarchy_AppendIncoming");
    assert_text_contains(
        hierarchy, "ZrLanguageServer_LspSemanticCallHierarchy_AppendOutgoing");
    assert_text_contains_none(
        hierarchy, "lsp_hierarchy_scan_symbol_for_named_calls");
    assert_text_contains_none(
        hierarchy, "lsp_hierarchy_find_callable_symbol");
    assert_text_contains_none(hierarchy, "lsp_editor_offset_is_code");

    free(hierarchyQuery);
    free(hierarchy);
}

static void test_extern_callable_decorators_use_parser_diagnostic_projection(void) {
    char *typecheck = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c");

    if (typecheck == NULL) {
        printf("FAIL: could not read semantic analyzer typecheck source\n");
        g_failures++;
        return;
    }

    assert_text_contains(
        typecheck,
        "ZrParser_Compiler_ValidateExternCallableDecorators");
    assert_text_contains(
        typecheck,
        "ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic");
    assert_text_contains_none(
        typecheck,
        "semantic_validate_extern_callable_decorators");
    assert_text_contains_none(typecheck, "allowedCallconvs");

    free(typecheck);
}

#include "test_lsp_source_contract_duplicate_diagnostic_cases.h"
#include "test_lsp_source_contract_extern_enum_decorator_cases.h"
#include "test_lsp_source_contract_extern_struct_decorator_cases.h"
#include "test_lsp_source_contract_ffi_wrapper_decorator_cases.h"
#include "test_lsp_source_contract_extern_parameter_decorator_cases.h"
#include "test_lsp_source_contract_initializer_annotation_cases.h"
#include "test_lsp_source_contract_return_type_cases.h"
#include "test_lsp_source_contract_exact_type_diagnostic_cases.h"
#include "test_lsp_source_contract_no_local_diagnostic_api_cases.h"
#include "test_lsp_source_contract_inlay_declaration_cases.h"

int main(void) {
    printf("==========\n");
    printf("Language Server - LSP Source Contract Tests\n");
    printf("==========\n\n");

    test_import_chain_location_conversion_does_not_use_static_append_state();
    test_semantic_query_location_conversion_uses_shared_document_helper();
    test_binary_metadata_coordinate_projection_is_explicitly_scoped();
    test_descriptor_metadata_coordinate_projection_is_explicitly_scoped();
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
    test_stdio_diagnostics_uses_shared_diagnostic_store();
    test_stdio_documents_uses_content_snapshot();
    test_stdio_inline_value_uses_content_snapshot();
    test_stdio_position_encoding_uses_content_snapshot();
    test_wasm_diagnostics_use_canonical_projection();
    test_type_mismatch_diagnostics_use_compiler_query_projection();
    test_reachability_diagnostics_use_semantic_query_projection();
    test_const_assignment_diagnostics_use_semantic_query_projection();
    test_variance_diagnostics_use_parser_query_projection();
    test_interface_const_field_diagnostics_use_parser_query_projection();
    test_unresolved_reference_diagnostics_use_parser_query_projection();
    test_named_call_compatibility_uses_parser_inference_projection();
    test_assignment_ownership_uses_parser_diagnostic_projection();
    test_reference_tracker_uses_canonical_identity_and_snapshot_source();
    test_local_reference_consumers_use_parser_relation_queries();
    test_local_definition_consumer_uses_snapshot_source();
    test_local_implementation_consumer_uses_parser_relations();
    test_local_type_hierarchy_uses_parser_relations();
    test_local_call_hierarchy_uses_parser_edges();
    test_inlay_uses_canonical_declaration_query();
    test_extern_callable_decorators_use_parser_diagnostic_projection();
    test_extern_enum_decorators_use_parser_diagnostic_projection();
    test_extern_struct_decorators_use_parser_diagnostic_projection();
    test_ffi_wrapper_decorators_use_parser_diagnostic_projection();
    test_extern_parameter_decorators_use_parser_diagnostic_projection();
    test_duplicate_type_uses_parser_diagnostic_projection();
    test_initializer_annotation_uses_parser_diagnostic_projection();
    test_return_type_inference_uses_parser_diagnostic_projection();
    test_cannot_infer_exact_type_uses_parser_diagnostic_projection();
    test_semantic_analyzer_has_no_unstructured_diagnostic_escape_hatch();

    if (g_failures != 0) {
        printf("\nFAILED: %d LSP source contract test failure(s)\n", g_failures);
        return 1;
    }

    printf("PASS: Import-chain location conversion avoids static append state\n");
    printf("PASS: Semantic query location conversion uses shared document helper\n");
    printf("PASS: Binary metadata coordinate projection is explicitly scoped\n");
    printf("PASS: Descriptor metadata coordinate projection is explicitly scoped\n");
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
    printf("PASS: stdio diagnostics uses shared diagnostic store\n");
    printf("PASS: stdio documents uses content snapshot\n");
    printf("PASS: stdio inline value uses content snapshot\n");
    printf("PASS: stdio position encoding uses content snapshot\n");
    printf("PASS: WASM diagnostics use canonical projection\n");
    printf("PASS: Type mismatch diagnostics use compiler query projection\n");
    printf("PASS: Reachability diagnostics use semantic query projection\n");
    printf("PASS: Variance diagnostics use parser query projection\n");
    printf("PASS: Interface const-field diagnostics use parser query projection\n");
    printf("PASS: Unresolved-reference diagnostics use parser query projection\n");
    printf("PASS: Named-call compatibility uses parser inference projection\n");
    printf("PASS: Assignment ownership uses parser diagnostic projection\n");
    printf("PASS: Reference tracker uses SymbolId and snapshot source identity\n");
    printf("PASS: Local references and highlights use parser relation queries\n");
    printf("PASS: Local definition uses analyzer snapshot source identity\n");
    printf("PASS: Local implementation uses parser relation queries\n");
    printf("PASS: Local type hierarchy uses parser relation queries\n");
    printf("PASS: Local call hierarchy uses parser call-edge queries\n");
    printf("PASS: Inlay hints use canonical declaration queries\n");
    printf("PASS: Extern callable decorators use parser diagnostic projection\n");
    printf("PASS: Duplicate type uses parser diagnostic projection\n");
    printf("PASS: Return type inference uses parser diagnostic projection\n");
    printf("PASS: Semantic analyzer has no unstructured diagnostic escape hatch\n");
    printf("\nPASSED: LSP source contract tests\n");
    return 0;
}
