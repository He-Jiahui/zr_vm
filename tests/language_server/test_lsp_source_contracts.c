#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 源码契约用例通过磁盘上的当前实现核对跨模块调用边界；调用方负责释放返回的文本。 */
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

/* 优先依据 __FILE__ 拼仓根；无法识别源路径时才从运行目录解析相对路径。 */
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

/* 各片段用例共享失败计数，main 在全部契约探针完成后统一给构建系统退出状态。 */
static int g_failures = 0;

/* TODO: 全文件字符串命中也可能来自注释或字面量；需用语法定位或行为测试补证关键契约。 */
static void assert_text_contains(const char *text, const char *needle) {
    if (strstr(text, needle) == NULL) {
        printf("Missing source contract text: %s\n", needle);
        g_failures++;
    }
}

/* 防止旧实现入口重新出现；调用者已确认文件读取成功。 */
static void assert_text_contains_none(const char *text, const char *needle) {
    if (strstr(text, needle) != NULL) {
        printf("Unexpected source contract text: %s\n", needle);
        g_failures++;
    }
}

/* 把探针限定到调用者选定的源码区间，避免别的函数满足同名契约。end 是区间末端。 */
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

/* 标识符扫描和 rename 用例借第二次命中跳过静态前向声明，定位实现区间。 */
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

/* 对局部消费者做正向约束，并把缺失边界也算作测试失败。 */
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

/* 对同一局部区间排除旧路径，防止重构后旁路重新参与请求处理。 */
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

/* 导入链位置转换应以版本化内容为准，避免静态追加状态跨请求污染。 */
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
    assert_text_contains_none(source, "semantic_import_chain_string_text");

    free(source);
}

/* 虚拟文档构造器只暴露实际调用的查询路径，避免遗留 API 与当前投影分叉。 */
static void test_virtual_document_builder_has_no_dead_query_surface(void) {
    char *source = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/lsp_virtual_documents.c");
    char *header = read_repo_text_file_owned(
            "zr_vm_language_server/src/zr_vm_language_server/lsp_virtual_documents.h");

    if (source == NULL || header == NULL) {
        printf("FAIL: could not read LSP virtual document sources\n");
        g_failures++;
        free(source);
        free(header);
        return;
    }

    assert_text_contains_none(source, "virtual_builder_append_format");
    assert_text_contains_none(
            source,
            "ZrLanguageServer_LspVirtualDocuments_FindModuleLinkDeclaration");
    assert_text_contains_none(
            header,
            "ZrLanguageServer_LspVirtualDocuments_FindModuleLinkDeclaration");
    assert_text_contains_none(
            source,
            "ZrLanguageServer_LspVirtualDocuments_FindTypeDeclaration");
    assert_text_contains_none(
            header,
            "ZrLanguageServer_LspVirtualDocuments_FindTypeDeclaration");

    free(source);
    free(header);
}

/* 语义查询的文件范围必须经共享文档转换，保持编辑器坐标与文档版本一致。 */
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

/* 二进制元数据的源范围不等同当前打开的文本，投影入口须显式选取坐标语境。 */
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

/* 描述符元数据坐标与本地文档坐标分属不同来源，检查调用方显式选路。 */
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

/* 公共 LSP 接口将文件范围投影为协议范围时复用文档内容感知的转换。 */
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

/* 共享转换助手不得退回无内容的旧路径，以免 UTF-16 范围失去当前版本依据。 */
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

/* 文档位置映射应遵从内容快照，防止按旧字节偏移解释客户端位置。 */
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

/* 公共声明和实现均不应再提供缺少文档内容的坐标转换捷径。 */
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

/* 标识符扫描依赖请求所绑定的快照，防止编辑期间扫描另一版本内容。 */
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

/* 补全词段范围须与调用时的文档快照配对，避免替换区间漂移。 */
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

/* 悬停文档取词与范围应来自同一快照，以免展示已失效符号说明。 */
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

/* inlay hint 的语义范围进入协议层时使用共享的版本感知坐标投影。 */
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

/* 项目导航不应自行恢复无内容坐标换算，以免跨文件定位口径不一致。 */
static void test_project_navigation_has_no_legacy_position_conversion(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c");

    if (source == NULL) {
        printf("FAIL: could not read lsp_project_navigation.c\n");
        g_failures++;
        return;
    }

    assert_text_contains_none(source, "ZrLanguageServer_LspPosition_ToFilePosition");

    free(source);
}

/* 项目级导航消费每个文件版本的内容快照，而非直接读可变文本。 */
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

/* 项目刷新沿版本化内容更新索引，避免边刷新边读取被替换的文档缓冲区。 */
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

/* 元数据提供器查询源码范围时持有快照，维持导入目标与位置的一致视图。 */
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

/* 语义查询在请求期间固定源文本版本，使符号事实与坐标指向同一内容。 */
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

/* 增量解析使用已获取的版本快照，避免解析期间编辑替换底层缓冲。 */
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

/* 版本块的引用计数契约保护解析与读请求重叠时的文本生命周期。 */
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

/* 编辑器特性读取同一版本内容，保证返回范围与已发布语义事实相配。 */
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

/* token 元数据悬停必须对快照取词，防止元数据 token 与现行文本错位。 */
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

/* 语义 token 扫描固定文档版本，便于增量编辑期间输出稳定协议范围。 */
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

/* token 的符号类别应由 parser 语义身份决定，避免独立扫描重新推断名称。 */
static void test_semantic_tokens_use_canonical_symbol_queries(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_tokens.c");
    char *canonicalSource = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_token_canonical.c");
    const char *declarationsStart;
    const char *declarationsEnd;
    const char *canonicalStart;
    const char *canonicalEnd;
    const char *scanStart;
    const char *scanEnd;

    if (source == NULL || canonicalSource == NULL) {
        printf("FAIL: could not read lsp_semantic_tokens.c\n");
        g_failures++;
        free(source);
        free(canonicalSource);
        return;
    }

    declarationsStart = strstr(source, "static void semantic_token_add_symbol_tokens(");
    declarationsEnd = declarationsStart != NULL
                          ? strstr(declarationsStart, "static TZrBool semantic_token_is_meta_method(")
                          : NULL;
    assert_text_section_contains("semantic_token_add_symbol_tokens",
                                 declarationsStart,
                                 declarationsEnd,
                                 "ZrParser_SemanticQuery_DeclaredSymbols");
    assert_text_section_contains_none("semantic_token_add_symbol_tokens",
                                      declarationsStart,
                                      declarationsEnd,
                                      "analyzer->symbolTable");

    canonicalStart = strstr(
        canonicalSource, "TZrInt32 ZrLanguageServer_LspSemanticToken_ResolveCanonical(");
    canonicalEnd = canonicalStart != NULL
                       ? canonicalSource + strlen(canonicalSource)
                       : NULL;
    assert_text_section_contains("ZrLanguageServer_LspSemanticToken_ResolveCanonical",
                                 canonicalStart,
                                 canonicalEnd,
                                 "ZrParser_SemanticQuery_SymbolAt");
    assert_text_section_contains("ZrLanguageServer_LspSemanticToken_ResolveCanonical",
                                 canonicalStart,
                                 canonicalEnd,
                                 "ZrParser_SemanticQuery_CanonicalTypeAt");
    assert_text_section_contains_none("ZrLanguageServer_LspSemanticToken_ResolveCanonical",
                                      canonicalStart,
                                      canonicalEnd,
                                      "SemanticAnalyzer_ResolveTypeAtPosition");

    scanStart = strstr(source, "static void semantic_token_scan_source(");
    scanEnd = scanStart != NULL
                  ? strstr(scanStart, "TZrBool ZrLanguageServer_Lsp_GetSemanticTokens(")
                  : NULL;
    assert_text_section_contains("semantic_token_scan_source",
                                 scanStart,
                                 scanEnd,
                                 "ZrLanguageServer_LspSemanticToken_ResolveCanonical");
    assert_text_section_contains_none("semantic_token_scan_source",
                                      scanStart,
                                      scanEnd,
                                      "ZrLanguageServer_LspSemanticQuery_ResolveAtPosition");
    assert_text_section_contains_none("semantic_token_scan_source",
                                      scanStart,
                                      scanEnd,
                                      "semantic_token_resolve_metadata_chain_member");
    assert_text_section_contains_none("semantic_token_scan_source",
                                      scanStart,
                                      scanEnd,
                                      "ZrLanguageServer_LspSemanticImportChain_ResolveLinkedMember");
    assert_text_section_contains_none("semantic_token_scan_source",
                                      scanStart,
                                      scanEnd,
                                      "ZrLanguageServer_SymbolTable_LookupAtPosition");
    /* Wire legend values are checked by stdio_optional_capabilities_smoke.js. */
    free(source);
    free(canonicalSource);
}

/* 折叠范围在请求时绑定版本化文本，以免行边界随编辑移动。 */
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

/* 打开文档的链接扫描读快照，避免链接文字与返回范围跨版本。 */
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

/* 签名帮助使用请求快照识别调用词段，使参数位次和范围同源。 */
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

/* 自动导入修复的插入位置根据快照选择，避免旧文档补丁覆盖当前编辑。 */
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

/* super 跳转从快照解析接收者上下文，防止跨版本选择错误父类。 */
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

/* code action 依据生成诊断的文档快照生成编辑，保持修复范围可应用。 */
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

/* 层次查询经文档版本访问源码，不直接读取可变内容缓冲区。 */
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

/* stdio 补全在请求生命周期内从快照提取文本，避免后续编辑改变候选范围。 */
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

/* stdio moniker 的符号位置应绑定快照，维持外部身份与请求文本一致。 */
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

/* 内联补全取用快照，避免建议文本基于不同版本的前缀。 */
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

/* 关联编辑范围需要固定当前文档版本，防止成对替换位置漂移。 */
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

/* stdio 诊断通过共享存储读取 analyzer 结果，不再单独构造不一致的诊断集合。 */
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

/* 文档同步处理在版本边界取快照，避免发布跨版本的文本与语义状态。 */
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

/* inline value 的位置与取值都应由同一内容版本导出。 */
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

/* stdio 坐标编码转换依赖文档快照，防止把 UTF-16 位置解释为旧版字节。 */
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

/* WASM 导出须复用协议诊断序列化器，避免浏览器接口与 stdio 字段语义分叉。 */
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

/* 类型不匹配由 compiler 兼容性判断产出，LSP 只消费结构化诊断投影。 */
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
    assert_text_contains_none(typecheck, "semantic_member_property_text");
    assert_text_contains_none(typecheck, "semantic_identifier_node_text");

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

/* 不可达事实先进入 parser 语义事实，客户端诊断再由统一查询物化。 */
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

/* const 赋值违规保留 parser 的规则所有权，LSP 不另行编码判定。 */
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

/* 接口方差校验由 parser 发布诊断，避免 analyzer 重复维护类型规则。 */
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

/* 接口 const 字段约束由 parser 构造诊断，符号收集仅消费该结果。 */
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
        "ZrParser_InterfaceContract_PublishConstFieldDiagnostics");
    assert_text_contains_none(
        symbols,
        "ZrParser_InterfaceContract_ConstFieldViolationAt");
    assert_text_contains_none(
        symbols,
        "ZrParser_InterfaceContract_BuildConstFieldDiagnostic");
    assert_text_contains_none(
        symbols,
        "ZrParser_SemanticFacts_AppendDiagnostic");
    assert_text_contains_none(
        symbols,
        "Interface field '%s' is const, but implementation field is not const");
    assert_text_contains_none(symbols, "TODO: \u5982\u679c\u5b57\u6bb5\u672a\u627e\u5230");

    free(symbols);
}

/* 未解析引用由 parser 结合符号事实物化，LSP 只把结构化诊断投影到协议。 */
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

/* 命名调用的兼容性以 parser 推断为准，防止 LSP 重做一套重载匹配。 */
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

/* 赋值所有权错误来自 compiler 诊断；表达式推断不应绕过该统一入口。 */
static void test_assignment_ownership_uses_parser_diagnostic_projection(void) {
    char *typecheck = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c");
    const char *assignment;
    const char *nextCase;

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
    assert_text_contains_none(typecheck, "semantic_analyzer_expected_type.h");
    assignment = strstr(typecheck, "case ZR_AST_ASSIGNMENT_EXPRESSION:");
    nextCase = assignment != NULL ? strstr(assignment, "case ZR_AST_FUNCTION_CALL:") : NULL;
    assert_text_section_contains("assignment inference", assignment, nextCase,
                                 "semantic_infer_node_type(");
    assert_text_section_contains_none("assignment inference", assignment, nextCase,
                                      "ZrParser_ExpressionType_Infer");
    assert_text_section_contains_none("assignment inference", assignment, nextCase,
                                      "ZrParser_AssignmentCompatibility_CheckDetailed");

    free(typecheck);
}

/* 引用记录绑定语义 ID 与快照源，避免按名称合并不同作用域或版本的符号。 */
static void test_reference_tracker_uses_canonical_identity_and_snapshot_source(void) {
    char *tracker = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c");
    char *symbolTable = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/symbol_table.c");
    char *symbolTableHeader = read_repo_text_file_owned(
        "zr_vm_language_server/include/zr_vm_language_server/symbol_table.h");
    char *analyzer = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c");
    char *querySource = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_source.c");

    if (tracker == NULL || symbolTable == NULL || symbolTableHeader == NULL || analyzer == NULL ||
        querySource == NULL) {
        printf("FAIL: could not read reference identity sources\n");
        g_failures++;
        free(tracker);
        free(symbolTable);
        free(symbolTableHeader);
        free(analyzer);
        free(querySource);
        return;
    }

    assert_text_contains(tracker, "reference->symbolId = symbol->semanticId");
    assert_text_contains_none(tracker, "symbol->name");
    assert_text_contains_none(tracker, "ZrCore_Value_InitAsRawObject");
    assert_text_contains_none(tracker, "symbolToReferencesMap");
    assert_text_contains_none(tracker, "ZrLanguageServer_ReferenceTracker_FindReferences");
    assert_text_contains_none(tracker, "ZrLanguageServer_ReferenceTracker_GetReferenceCount");
    assert_text_contains_none(tracker, "ZrLanguageServer_ReferenceTracker_GetReferenceLocations");
    assert_text_contains_none(symbolTable, "ZrLanguageServer_Symbol_GetReferenceCount");
    assert_text_contains_none(symbolTableHeader, "ZrLanguageServer_SymbolTable_AddSymbol(");
    assert_text_contains_none(symbolTable, "ZrLanguageServer_SymbolTable_AddSymbol(");
    assert_text_contains(
        analyzer,
        "ZrLanguageServer_SemanticAnalyzer_BindQuerySource");
    assert_text_contains(querySource, "analyzer->ast->location.source");

    free(tracker);
    free(symbolTable);
    free(symbolTableHeader);
    free(analyzer);
    free(querySource);
}

/* 本地引用与高亮消费 parser 关系查询，不能退回名称或独立 tracker 扫描。 */
static void test_local_reference_consumers_use_parser_relation_queries(void) {
    char *referenceQuery = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c");
    char *referenceQueryHeader = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.h");
    char *semanticQuery = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c");
    const char *appendReferencesStart;
    const char *appendReferencesEnd;

    if (referenceQuery == NULL || referenceQueryHeader == NULL || semanticQuery == NULL) {
        printf("FAIL: could not read local reference consumer sources\n");
        g_failures++;
        free(referenceQuery);
        free(referenceQueryHeader);
        free(semanticQuery);
        return;
    }

    assert_text_contains(
        referenceQuery, "ZrParser_SemanticQuery_ReferencesOf");
    assert_text_contains(
        referenceQuery, "ZrParser_SemanticQuery_DeclarationOf");
    assert_text_contains(
        referenceQuery, "ZrParser_SemanticQuery_ExternalReferences");
    assert_text_contains(
        referenceQuery, "ZrLanguageServer_LspExternalTargetIdentity_MatchesReference");
    assert_text_contains(
        referenceQuery, "ZrLanguageServer_SemanticAnalyzer_BindQuerySource");
    assert_text_contains_none(referenceQuery, "referenceTracker");
    assert_text_contains_none(referenceQuery, "symbol->name");
    assert_text_contains_none(
        referenceQuery,
        "ZrLanguageServer_LspSemanticReferenceQuery_AppendReferencesForSymbol");
    assert_text_contains_none(
        referenceQueryHeader,
        "ZrLanguageServer_LspSemanticReferenceQuery_AppendReferencesForSymbol");
    appendReferencesStart = strstr(
        semanticQuery,
        "ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendReferences(");
    appendReferencesEnd = appendReferencesStart != NULL
                              ? strstr(appendReferencesStart,
                                       "ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(")
                              : NULL;
    assert_text_section_contains_none(
        "ZrLanguageServer_LspSemanticQuery_AppendReferences",
        appendReferencesStart,
        appendReferencesEnd,
        "query->symbol->name");
    assert_text_contains_none(
        semanticQuery, "semantic_query_normalize_symbol_reference_range");

    free(referenceQuery);
    free(referenceQueryHeader);
    free(semanticQuery);
}

/* 跨快照引用必须比较 provider 代次与外部目标 token，防止同名对象串线。 */
static void test_cross_snapshot_references_use_external_identity_queries(void) {
    char *crossSnapshot = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_cross_snapshot_references.c");
    char *externalMetadata = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_external_metadata_identity.c");
    char *externalIdentity = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_target_identity.c");

    if (crossSnapshot == NULL || externalMetadata == NULL || externalIdentity == NULL) {
        printf("FAIL: could not read cross-snapshot reference sources\n");
        g_failures++;
        free(crossSnapshot);
        free(externalMetadata);
        free(externalIdentity);
        return;
    }

    assert_text_contains(
        crossSnapshot, "ZrParser_SemanticQuery_ExternalReferences");
    assert_text_contains(
        crossSnapshot,
        "ZrLanguageServer_LspExternalMetadataIdentity_ResolveDeclaration");
    assert_text_contains_none(crossSnapshot, "CollectImportBindings");
    assert_text_contains_none(crossSnapshot, "memberName");
    assert_text_contains_none(crossSnapshot, "strstr");
    assert_text_contains(
        crossSnapshot, "ZrLanguageServer_LspCrossSnapshotReferences_AppendExternal");
    assert_text_contains(crossSnapshot, "externalProviderGeneration");
    assert_text_contains(
        crossSnapshot, "ZrLanguageServer_LspExternalTargetIdentity_MatchesReference");
    assert_text_contains(externalIdentity, "externalProviderGeneration");
    assert_text_contains(externalIdentity, "externalMetadataToken");
    assert_text_contains(externalIdentity, "externalSignatureToken");
    assert_text_contains(externalIdentity, "externalSignatureHash");
    assert_text_contains(externalIdentity, "externalTargetKind");
    assert_text_contains_none(externalMetadata, "declarationNode");
    assert_text_contains_none(externalMetadata, "strcmp");
    assert_text_contains_none(externalMetadata, "memberName");

    free(crossSnapshot);
    free(externalMetadata);
    free(externalIdentity);
}

/* import 链末端成员先按外部身份命中，再装载元数据；无本地 AST 时仍可解析。 */
static void test_import_chain_terminal_member_uses_external_identity(void) {
    char *semanticQuery = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c");
    char *externalMetadata = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_external_metadata_identity.c");
    const char *resolverStart;
    const char *resolverEnd;
    const char *resolveAtPosition;
    const char *canonicalExternalCall;
    const char *astGate;
    const char *identitySelection;
    const char *memberHydration;

    if (semanticQuery == NULL || externalMetadata == NULL) {
        printf("FAIL: could not read import-chain external identity sources\n");
        g_failures++;
        free(semanticQuery);
        free(externalMetadata);
        return;
    }

    resolverStart = strstr(
        semanticQuery,
        "semantic_query_resolve_canonical_external_member_target(");
    resolverEnd = resolverStart != NULL
        ? strstr(resolverStart, "static TZrBool semantic_query_try_resolve_canonical_symbol(")
        : NULL;
    assert_text_section_contains(
        "semantic_query_resolve_canonical_external_member_target",
        resolverStart,
        resolverEnd,
        "ZrLanguageServer_LspExternalMetadataIdentity_ResolveMember");
    assert_text_contains(
        externalMetadata,
        "ZR_SEMANTIC_EXTERNAL_TARGET_MODULE");
    assert_text_section_contains_none(
        "semantic_query_resolve_canonical_external_member_target",
        resolverStart,
        resolverEnd,
        "ZrLanguageServer_LspSemanticImportChain_ResolveAtRange");
    assert_text_section_contains_none(
        "semantic_query_resolve_canonical_external_member_target",
        resolverStart,
        resolverEnd,
        "analyzer->ast");

    /* 此顺序是无 AST 的外部成员仍可定位的前提，不能仅验证两个调用各自存在。 */
    resolveAtPosition = strstr(
        semanticQuery,
        "ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(");
    canonicalExternalCall = resolveAtPosition != NULL
        ? strstr(resolveAtPosition,
                 "semantic_query_resolve_canonical_external_member_target(")
        : NULL;
    astGate = resolveAtPosition != NULL
        ? strstr(resolveAtPosition, "if (analyzer->ast != ZR_NULL)")
        : NULL;
    if (canonicalExternalCall == NULL || astGate == NULL ||
        canonicalExternalCall >= astGate) {
        printf("FAIL: canonical external member consumer must run before the AST import-chain gate\n");
        g_failures++;
    }

    /* 先按身份筛选再 hydration，防止同名 metadata 候选被错误装载。 */
    identitySelection = strstr(
        externalMetadata,
        "if (!external_metadata_identity_matches(analyzer, identity, candidate))");
    memberHydration = strstr(
        externalMetadata,
        "ZrLanguageServer_LspMetadataProvider_ResolveImportedMember(");
    if (identitySelection == NULL || memberHydration == NULL ||
        identitySelection >= memberHydration) {
        printf("FAIL: external member metadata must be selected by exact identity before hydration\n");
        g_failures++;
    }
    assert_text_contains_none(externalMetadata, "strcmp");

    free(semanticQuery);
    free(externalMetadata);
}

/* import 原点跳转沿 parser 发布的关系与元数据 URI，避免重新按别名搜索。 */
static void test_import_origin_definition_consumer_uses_parser_relations(void) {
    char *relationQuery = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_relation_query.c");
    char *semanticQuery = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c");
    char *parserImportQuery = read_repo_text_file_owned(
        "zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_imports.c");
    char *analyzerAnalysis = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c");
    const char *resolveStart;
    const char *resolveEnd;
    const char *literalResolver;
    const char *astGate;
    const char *moduleLiteralBranch;
    const char *memberBranch;

    if (relationQuery == NULL || semanticQuery == NULL ||
        parserImportQuery == NULL || analyzerAnalysis == NULL) {
        printf("FAIL: could not read canonical import-origin definition sources\n");
        g_failures++;
        free(relationQuery);
        free(semanticQuery);
        free(parserImportQuery);
        free(analyzerAnalysis);
        return;
    }

    assert_text_contains(
        relationQuery, "ZrParser_SemanticQuery_RelationsOfSymbol");
    assert_text_contains(
        relationQuery, "ZrParser_SemanticQuery_ImportOriginAt");
    assert_text_contains(
        relationQuery,
        "ZrLanguageServer_LspMetadataProvider_ResolveImportedModuleEntry");
    assert_text_contains(relationQuery, "virtualDeclarationUri");
    assert_text_contains(relationQuery, "symbol->isImport");
    assert_text_contains(relationQuery, "symbol->externalOriginUri");
    assert_text_contains(
        relationQuery, "ZR_LSP_SEMANTIC_IMPORT_ORIGIN_INVALID");
    assert_text_contains_none(relationQuery, "CollectImportBindings");
    assert_text_contains_none(relationQuery, "FindImportBindingByAlias");
    assert_text_contains_none(relationQuery, "aliasName");
    assert_text_contains_none(relationQuery, "strcmp");
    assert_text_contains_none(relationQuery, "strstr");
    assert_text_contains(
        semanticQuery,
        "ZrLanguageServer_LspSemanticRelationQuery_ResolveImportOrigin");
    assert_text_contains(
        semanticQuery,
        "ZrLanguageServer_LspSemanticRelationQuery_ResolveImportOriginAt");
    assert_text_contains(
        semanticQuery,
        "importOriginResolution == ZR_LSP_SEMANTIC_IMPORT_ORIGIN_INVALID");
    assert_text_contains(
        analyzerAnalysis, "ZrParser_SemanticCalls_PublishSource");
    assert_text_contains(
        analyzerAnalysis, "ZrParser_SemanticRelations_PublishImportOrigins");
    assert_text_contains(
        parserImportQuery, "ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN");
    assert_text_contains(
        parserImportQuery, "visible->externalOriginRange");
    assert_text_contains_none(parserImportQuery, "strcmp");
    assert_text_contains_none(parserImportQuery, "strstr");

    /* 字面量原点解析须先于 AST 回退，缺失 canonical 关系时按契约失败关闭。 */
    resolveStart = strstr(
        semanticQuery,
        "ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(");
    resolveEnd = resolveStart != NULL
                     ? strstr(resolveStart,
                              "ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_BuildHover(")
                     : NULL;
    literalResolver = resolveStart != NULL
                          ? strstr(resolveStart,
                                   "semantic_query_resolve_canonical_import_literal_target(")
                          : NULL;
    astGate = resolveStart != NULL
                  ? strstr(resolveStart, "if (analyzer->ast != ZR_NULL) {")
                  : NULL;
    if (literalResolver == NULL || astGate == NULL || literalResolver >= astGate ||
        (resolveEnd != NULL && literalResolver >= resolveEnd)) {
        printf("Canonical import literal query must run before AST import fallback\n");
        g_failures++;
    }
    moduleLiteralBranch = resolveStart != NULL
                              ? strstr(resolveStart,
                                       "if (importChainHit.memberName == ZR_NULL) {")
                              : NULL;
    memberBranch = moduleLiteralBranch != NULL
                       ? strstr(moduleLiteralBranch, "} else {")
                       : NULL;
    assert_text_section_contains(
        "canonical import module literal fail-closed branch",
        moduleLiteralBranch,
        memberBranch,
        "return ZR_FALSE;");

    free(relationQuery);
    free(semanticQuery);
    free(parserImportQuery);
    free(analyzerAnalysis);
}

/* 导入成员的引用与高亮必须先有 canonical 身份，再拼本地和跨快照结果。 */
static void test_imported_reference_consumers_require_canonical_identity(void) {
    char *semanticQuery = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c");
    const char *resolveStart;
    const char *resolveEnd;
    const char *appendReferencesStart;
    const char *appendReferencesEnd;
    const char *appendHighlightsStart;
    const char *appendHighlightsEnd;

    if (semanticQuery == NULL) {
        printf("FAIL: could not read imported reference consumer source\n");
        g_failures++;
        return;
    }

    resolveStart = strstr(
        semanticQuery,
        "ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(");
    resolveEnd = resolveStart != NULL
                     ? strstr(resolveStart,
                              "ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_BuildHover(")
                     : NULL;
    appendReferencesStart = strstr(
        semanticQuery,
        "ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendReferences(");
    appendReferencesEnd = appendReferencesStart != NULL
                              ? strstr(appendReferencesStart,
                                       "ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(")
                              : NULL;
    appendHighlightsStart = appendReferencesEnd;
    appendHighlightsEnd = appendHighlightsStart != NULL
                              ? semanticQuery + strlen(semanticQuery)
                              : NULL;

    assert_text_section_contains(
        "LspSemanticQuery_ResolveAtPosition imported canonical projection",
        resolveStart,
        resolveEnd,
        "semantic_query_try_resolve_canonical_symbol");
    assert_text_section_contains(
        "LspSemanticQuery_ResolveAtPosition imported canonical projection",
        resolveStart,
        resolveEnd,
        "ZrLanguageServer_LspExternalTargetIdentity_MatchesMember");
    assert_text_section_contains(
        "LspSemanticQuery_AppendReferences imported canonical projection",
        appendReferencesStart,
        appendReferencesEnd,
        "query->hasCanonicalSymbol");
    assert_text_section_contains(
        "LspSemanticQuery_AppendReferences imported canonical projection",
        appendReferencesStart,
        appendReferencesEnd,
        "semantic_query_imported_canonical_identity_is_available");
    assert_text_section_contains(
        "LspSemanticQuery_AppendReferences imported canonical projection",
        appendReferencesStart,
        appendReferencesEnd,
        "ZrLanguageServer_LspSemanticReferenceQuery_AppendReferences");
    assert_text_section_contains(
        "LspSemanticQuery_AppendReferences imported canonical projection",
        appendReferencesStart,
        appendReferencesEnd,
        "ZrLanguageServer_LspCrossSnapshotReferences_AppendExternal");
    assert_text_section_contains_none(
        "LspSemanticQuery_AppendReferences imported canonical projection",
        appendReferencesStart,
        appendReferencesEnd,
        "semantic_query_append_project_imported_member_references");
    assert_text_section_contains_none(
        "LspSemanticQuery_AppendReferences imported canonical projection",
        appendReferencesStart,
        appendReferencesEnd,
        "semantic_query_append_imported_member_locations_for_uri");
    assert_text_section_contains_none(
        "LspSemanticQuery_AppendReferences imported canonical projection",
        appendReferencesStart,
        appendReferencesEnd,
        "AppendMatchingLocationsForUri");
    assert_text_section_contains(
        "LspSemanticQuery_AppendDocumentHighlights imported canonical projection",
        appendHighlightsStart,
        appendHighlightsEnd,
        "query->hasCanonicalSymbol");
    assert_text_section_contains(
        "LspSemanticQuery_AppendDocumentHighlights imported canonical projection",
        appendHighlightsStart,
        appendHighlightsEnd,
        "semantic_query_imported_canonical_identity_is_available");
    assert_text_section_contains(
        "LspSemanticQuery_AppendDocumentHighlights imported canonical projection",
        appendHighlightsStart,
        appendHighlightsEnd,
        "ZrLanguageServer_LspSemanticReferenceQuery_AppendHighlights");
    assert_text_section_contains_none(
        "LspSemanticQuery_AppendDocumentHighlights imported canonical projection",
        appendHighlightsStart,
        appendHighlightsEnd,
        "semantic_query_append_imported_member_highlights");

    free(semanticQuery);
}

/* 项目层不保留按名称的旁路导航，以 parser 关系查询作为唯一语义来源。 */
static void test_dead_project_semantic_fallbacks_are_removed(void) {
    char *projectSource = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c");
    char *projectNavigation = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c");
    char *projectImports = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_imports.c");
    char *projectInternal = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_internal.h");
    char *interfaceInternal = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h");

    if (projectSource == NULL || projectNavigation == NULL || projectImports == NULL ||
        projectInternal == NULL || interfaceInternal == NULL) {
        printf("FAIL: could not read project semantic fallback sources\n");
        g_failures++;
        free(projectSource);
        free(projectNavigation);
        free(projectImports);
        free(projectInternal);
        free(interfaceInternal);
        return;
    }

    assert_text_contains_none(projectNavigation, "find_global_symbol_by_name");
    assert_text_contains_none(projectNavigation, "project_resolve_symbol_at_position");
    assert_text_contains_none(projectNavigation, "append_symbol_references_from_facts");
    assert_text_contains_none(projectNavigation, "ZrLanguageServer_Lsp_ProjectTryGetDefinition");
    assert_text_contains_none(projectNavigation, "ZrLanguageServer_Lsp_ProjectTryFindReferences");
    assert_text_contains_none(projectNavigation, "ZrLanguageServer_Lsp_ProjectTryGetDocumentHighlights");
    assert_text_contains_none(interfaceInternal, "ZrLanguageServer_Lsp_ProjectTryGetDefinition");
    assert_text_contains_none(interfaceInternal, "ZrLanguageServer_Lsp_ProjectTryFindReferences");
    assert_text_contains_none(interfaceInternal, "ZrLanguageServer_Lsp_ProjectTryGetDocumentHighlights");
    assert_text_contains_none(projectNavigation, "SZrLspProjectResolvedSymbol");
    assert_text_contains_none(
        projectImports,
        "ZrLanguageServer_LspProject_FindImportedMemberHit");
    assert_text_contains_none(
        projectInternal,
        "ZrLanguageServer_LspProject_FindImportedMemberHit");
    assert_text_contains_none(projectImports, "find_imported_member_hit_recursive");
    assert_text_contains_none(projectImports, "find_imported_member_hit_in_node_array");
    assert_text_contains_none(
        projectSource,
        "ZrLanguageServer_Lsp_ProjectEnsureProjectByProjectUri");
    assert_text_contains_none(
        interfaceInternal,
        "ZrLanguageServer_Lsp_ProjectEnsureProjectByProjectUri");

    free(projectSource);
    free(projectNavigation);
    free(projectImports);
    free(projectInternal);
    free(interfaceInternal);
}

/* rename 的位置与占位词须来自 canonical 符号，避免同名但异 ID 的编辑。 */
static void test_local_rename_consumers_require_canonical_symbol_identity(void) {
    char *interfaceSource = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c");
    char *referenceQuery = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c");
    const char *renameStart;
    const char *renameEnd;
    const char *placeholderStart;
    const char *placeholderEnd;

    if (interfaceSource == NULL || referenceQuery == NULL) {
        printf("FAIL: could not read canonical rename consumer sources\n");
        g_failures++;
        free(interfaceSource);
        free(referenceQuery);
        return;
    }

    renameStart = find_next_text(
        interfaceSource, "static TZrBool lsp_semantic_query_append_rename_locations(");
    renameEnd = renameStart != NULL
                    ? strstr(renameStart, "static SZrString *lsp_semantic_query_rename_placeholder(")
                    : NULL;
    assert_text_section_contains(
        "lsp_semantic_query_append_rename_locations",
        renameStart,
        renameEnd,
        "query->hasCanonicalSymbol");
    assert_text_section_contains(
        "lsp_semantic_query_append_rename_locations",
        renameStart,
        renameEnd,
        "ZrLanguageServer_LspSemanticReferenceQuery_AppendReferences");
    assert_text_section_contains_none(
        "lsp_semantic_query_append_rename_locations",
        renameStart,
        renameEnd,
        "ZrLanguageServer_Lsp_GetSymbolLookupRange");
    assert_text_section_contains_none(
        "lsp_semantic_query_append_rename_locations",
        renameStart,
        renameEnd,
        "query->symbol->location");

    placeholderStart = find_next_text(
        interfaceSource, "static SZrString *lsp_semantic_query_rename_placeholder(");
    placeholderEnd = placeholderStart != NULL
                         ? strstr(placeholderStart, "static SZrString *lsp_append_markdown_section(")
                         : NULL;
    assert_text_section_contains(
        "lsp_semantic_query_rename_placeholder",
        placeholderStart,
        placeholderEnd,
        "query->canonicalSymbol.displayName");
    assert_text_section_contains_none(
        "lsp_semantic_query_rename_placeholder",
        placeholderStart,
        placeholderEnd,
        "query->symbol->name");

    assert_text_contains(
        referenceQuery, "query->canonicalSymbol.symbolId == ZR_SEMANTIC_ID_INVALID");
    assert_text_contains(
        referenceQuery, "query->symbol->semanticId != query->canonicalSymbol.symbolId");
    assert_text_contains_none(referenceQuery, "return query->symbol != ZR_NULL");

    free(interfaceSource);
    free(referenceQuery);
}

/* 本地定义目标取 parser 关系和绑定的快照源，而非查询时拼接 URI 回退。 */
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

/* 实现跳转连接 parser 发布的编译契约与实现关系，防止名称猜测歧义。 */
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

/* 类型层次以语义 ID 和 parser 父子关系贯穿查询、stdio 解析与响应编码。 */
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
        hierarchyQuery, "ZrParser_Semantic_FindSymbolById");
    assert_text_contains(
        hierarchyQuery, "ZrParser_SemanticQuery_DeclarationOf");
    assert_text_contains(
        hierarchyQuery, "ZrParser_SemanticQuery_SymbolAt");
    assert_text_contains(hierarchyQuery, "item->semanticId");
    assert_text_contains(
        hierarchyQuery, "ZrLanguageServer_Lsp_GetDocumentFilePosition");
    assert_text_contains(
        hierarchyQuery, "ZrLanguageServer_SemanticAnalyzer_BindQuerySource");
    assert_text_contains_none(
        hierarchyQuery, "ZrLanguageServer_LspSemanticQuery_ResolveAtPosition");
    assert_text_contains_none(hierarchyQuery, "GetDocumentSymbols");
    assert_text_contains_none(hierarchyQuery, "referenceTracker");
    assert_text_contains_none(hierarchyQuery, "symbolTable");
    assert_text_contains_none(hierarchyQuery, "allScopes");
    assert_text_contains_none(
        hierarchyQuery, "semantic_type_hierarchy_find_symbol");
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

/* 调用层次以 parser 调用边和语义 ID 为准，包含 lambda，避免扫描名称推断边。 */
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
        hierarchyQuery, "ZrParser_SemanticQuery_SymbolAt");
    assert_text_contains(
        hierarchyQuery, "ZrParser_Semantic_FindSymbolById");
    assert_text_contains(hierarchyQuery, "item->semanticId");
    assert_text_contains(hierarchyQuery, "ZR_AST_LAMBDA_EXPRESSION");
    assert_text_contains(
        hierarchyQuery, "ZrLanguageServer_Lsp_GetDocumentFilePosition");
    assert_text_contains(
        hierarchyQuery, "ZrLanguageServer_SemanticAnalyzer_BindQuerySource");
    assert_text_contains_none(
        hierarchyQuery, "ZrLanguageServer_LspSemanticQuery_ResolveAtPosition");
    assert_text_contains_none(hierarchyQuery, "GetDocumentSymbols");
    assert_text_contains_none(hierarchyQuery, "referenceTracker");
    assert_text_contains_none(hierarchyQuery, "symbolTable");
    assert_text_contains_none(hierarchyQuery, "allScopes");
    assert_text_contains_none(
        hierarchyQuery, "semantic_call_hierarchy_find_symbol");
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

/* extern callable 装饰器由 parser 校验，LSP 仅投影 compiler 报错。 */
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

/* 声明类型显示先用 parser 的类型身份与名称接口，避免 LSP 自造同名映射。 */
static void test_declared_type_builder_uses_parser_type_identity(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic_type_prototypes.c");
    const char *probeStart;
    const char *probeEnd;
    const char *builderStart;
    const char *builderEnd;
    const char *typeNameStart;
    const char *typeNameEnd;

    if (source == NULL) {
        printf("FAIL: could not read semantic_type_prototypes.c\n");
        g_failures++;
        return;
    }

    probeStart = strstr(
        source,
        "static TZrBool semantic_type_prototypes_try_parser_conversion(");
    probeEnd = probeStart != NULL
                   ? strstr(probeStart,
                            "static void semantic_type_prototypes_apply_primitive_numeric_range(")
                   : NULL;
    assert_text_section_contains(
        "semantic_type_prototypes_try_parser_conversion",
        probeStart,
        probeEnd,
        "ZrParser_AstTypeToInferredType_Convert");

    builderStart = strstr(
        source,
        "static TZrBool semantic_type_prototypes_build_inferred_type(");
    builderEnd = builderStart != NULL
                     ? strstr(builderStart,
                              "static TZrBool semantic_type_prototypes_build_generic_argument_inferred_type(")
                     : NULL;
    assert_text_section_contains(
        "semantic_type_prototypes_build_inferred_type",
        builderStart,
        builderEnd,
        "semantic_type_prototypes_try_parser_conversion");
    typeNameStart = strstr(
            source,
            "static SZrString *semantic_type_prototypes_type_name_from_type_node(");
    typeNameEnd = typeNameStart != NULL
                      ? strstr(typeNameStart,
                               "static void semantic_type_prototypes_collect_parameter_signature(")
                      : NULL;
    assert_text_section_contains(
        "semantic_type_prototypes_type_name_from_type_node",
        typeNameStart,
        typeNameEnd,
        "ZrParser_TypeNameString_Get");
    assert_text_contains_none(
        source, "semantic_type_prototypes_base_type_from_name");

    free(source);
}

/* 光标类型查询使用 parser canonical 类型事实，不回退 AST 节点猜测。 */
static void test_semantic_analyzer_type_resolution_uses_canonical_query(void) {
    char *source = read_repo_text_file_owned(
        "zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c");
    const char *resolveStart;
    const char *resolveEnd;

    if (source == NULL) {
        printf("FAIL: could not read semantic_analyzer.c type query source\n");
        g_failures++;
        return;
    }

    resolveStart = strstr(
        source,
        "TZrBool ZrLanguageServer_SemanticAnalyzer_ResolveTypeAtPosition(");
    resolveEnd = resolveStart != NULL
                     ? strstr(
                           resolveStart + 1,
                           "// 获取悬停信息")
                     : NULL;
    assert_text_section_contains(
        "SemanticAnalyzer_ResolveTypeAtPosition canonical query",
        resolveStart,
        resolveEnd,
        "ZrParser_SemanticQuery_CanonicalTypeAt");
    assert_text_section_contains(
        "SemanticAnalyzer_ResolveTypeAtPosition canonical query",
        resolveStart,
        resolveEnd,
        "ZrParser_CanonicalType_Find");
    assert_text_section_contains(
        "SemanticAnalyzer_ResolveTypeAtPosition canonical query",
        resolveStart,
        resolveEnd,
        "semantic_find_type_record_by_id");
    assert_text_section_contains_none(
        "SemanticAnalyzer_ResolveTypeAtPosition canonical query",
        resolveStart,
        resolveEnd,
        "semantic_find_type_node_at_position");
    assert_text_section_contains_none(
        "SemanticAnalyzer_ResolveTypeAtPosition canonical query",
        resolveStart,
        resolveEnd,
        "FindExpressionNodeAtPosition");
    assert_text_section_contains_none(
        "SemanticAnalyzer_ResolveTypeAtPosition canonical query",
        resolveStart,
        resolveEnd,
        "InferExactExpressionType");
    assert_text_section_contains_none(
        "SemanticAnalyzer_ResolveTypeAtPosition canonical query",
        resolveStart,
        resolveEnd,
        "SemanticAnalyzer_GetSymbolAt");
    assert_text_section_contains_none(
        "SemanticAnalyzer_ResolveTypeAtPosition canonical query",
        resolveStart,
        resolveEnd,
        "BuildDeclaredTypeInferredType");

    free(source);
}

/* 下列 .h 是本翻译单元的静态案例片段；main 统一调度，不能单独编译运行。 */
#include "test_lsp_source_contract_duplicate_diagnostic_cases.h"
#include "test_lsp_source_contract_extern_enum_decorator_cases.h"
#include "test_lsp_source_contract_extern_struct_decorator_cases.h"
#include "test_lsp_source_contract_ffi_wrapper_decorator_cases.h"
#include "test_lsp_source_contract_extern_parameter_decorator_cases.h"
#include "test_lsp_source_contract_initializer_annotation_cases.h"
#include "test_lsp_source_contract_return_type_cases.h"
#include "test_lsp_source_contract_exact_type_diagnostic_cases.h"
#include "test_lsp_source_contract_no_local_diagnostic_api_cases.h"
#include "test_lsp_source_contract_no_local_reference_collection_cases.h"
#include "test_lsp_source_contract_inlay_declaration_cases.h"
#include "test_lsp_source_contract_completion_snapshot_cases.h"
#include "test_lsp_source_contract_canonical_completion_cases.h"
#include "test_lsp_source_contract_signature_snapshot_cases.h"
#include "test_lsp_source_contract_local_query_snapshot_cases.h"
#include "test_lsp_source_contract_code_lens_declaration_cases.h"

/* CMake 注册的源契约入口：跑完全部片段后用聚合计数给构建流水线返回状态。 */
int main(void) {
    printf("==========\n");
    printf("Language Server - LSP Source Contract Tests\n");
    printf("==========\n\n");

    test_import_chain_location_conversion_does_not_use_static_append_state();
    test_virtual_document_builder_has_no_dead_query_surface();
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
    test_project_navigation_has_no_legacy_position_conversion();
    test_project_navigation_uses_content_snapshot();
    test_project_refresh_uses_content_snapshot();
    test_metadata_provider_uses_content_snapshot();
    test_semantic_query_uses_content_snapshot();
    test_incremental_parser_parse_uses_content_snapshot();
    test_incremental_parser_content_uses_versioned_refcounted_block();
    test_editor_features_use_content_snapshot();
    test_token_metadata_hover_uses_content_snapshot();
    test_semantic_tokens_source_scan_uses_content_snapshot();
    test_semantic_tokens_use_canonical_symbol_queries();
    test_folding_ranges_uses_content_snapshot();
    test_document_links_uses_content_snapshot_for_open_documents();
    test_signature_help_code_span_uses_content_snapshot();
    test_code_action_imports_use_content_snapshot();
    test_super_navigation_uses_content_snapshot();
    test_code_actions_use_content_snapshot();
    test_hierarchy_uses_content_snapshot();
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
    test_cross_snapshot_references_use_external_identity_queries();
    test_import_chain_terminal_member_uses_external_identity();
    test_import_origin_definition_consumer_uses_parser_relations();
    test_imported_reference_consumers_require_canonical_identity();
    test_dead_project_semantic_fallbacks_are_removed();
    test_local_rename_consumers_require_canonical_symbol_identity();
    test_local_definition_consumer_uses_snapshot_source();
    test_local_implementation_consumer_uses_parser_relations();
    test_local_type_hierarchy_uses_parser_relations();
    test_local_call_hierarchy_uses_parser_edges();
    test_inlay_uses_canonical_declaration_query();
    test_completion_semantic_facts_are_snapshot_read_only();
    test_lexical_completion_uses_parser_visible_symbol_query();
    test_completion_consumer_does_not_materialize_scoped_analyzer();
    test_signature_semantic_facts_are_snapshot_read_only();
    test_local_semantic_query_is_snapshot_read_only();
    test_code_lens_uses_canonical_declaration_and_reference_queries();
    test_source_hover_uses_parser_symbol_query();
    test_public_hover_consumer_does_not_use_analyzer_hover();
    test_metadata_hover_consumer_does_not_use_analyzer_hover();
    test_receiver_completion_does_not_reinfer_ast_type();
    test_extern_callable_decorators_use_parser_diagnostic_projection();
    test_declared_type_builder_uses_parser_type_identity();
    test_semantic_analyzer_type_resolution_uses_canonical_query();
    test_extern_enum_decorators_use_parser_diagnostic_projection();
    test_extern_struct_decorators_use_parser_diagnostic_projection();
    test_ffi_wrapper_decorators_use_parser_diagnostic_projection();
    test_extern_parameter_decorators_use_parser_diagnostic_projection();
    test_duplicate_type_uses_parser_diagnostic_projection();
    test_initializer_annotation_uses_parser_diagnostic_projection();
    test_return_type_inference_uses_parser_diagnostic_projection();
    test_cannot_infer_exact_type_uses_parser_diagnostic_projection();
    test_semantic_analyzer_has_no_unstructured_diagnostic_escape_hatch();
    test_semantic_analyzer_rules_only_publish_structured_query_diagnostics();
    test_semantic_analyzer_uses_canonical_symbol_query_for_references();

    if (g_failures != 0) {
        printf("\nFAILED: %d LSP source contract test failure(s)\n", g_failures);
        return 1;
    }

    printf("PASS: Import-chain location conversion avoids static append state\n");
    printf("PASS: Virtual document builder has no dead query surface\n");
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
    printf("PASS: Project navigation avoids legacy position conversion\n");
    printf("PASS: Project navigation uses content snapshot\n");
    printf("PASS: Project refresh uses content snapshot\n");
    printf("PASS: Metadata provider uses content snapshot\n");
    printf("PASS: Semantic query uses content snapshot\n");
    printf("PASS: Incremental parser parse uses content snapshot\n");
    printf("PASS: Incremental parser content uses versioned refcounted block\n");
    printf("PASS: Editor features use content snapshot\n");
    printf("PASS: Token metadata hover uses content snapshot\n");
    printf("PASS: Semantic tokens source scan uses content snapshot\n");
    printf("PASS: Semantic tokens use canonical symbol queries\n");
    printf("PASS: Folding ranges use content snapshot\n");
    printf("PASS: Document links use content snapshot for open documents\n");
    printf("PASS: Signature help code span uses content snapshot\n");
    printf("PASS: Code action imports use content snapshot\n");
    printf("PASS: Super navigation uses content snapshot\n");
    printf("PASS: Code actions use content snapshot\n");
    printf("PASS: Hierarchy uses content snapshot\n");
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
    printf("PASS: Local rename uses canonical SymbolId and reference queries\n");
    printf("PASS: Local definition uses analyzer snapshot source identity\n");
    printf("PASS: Local implementation uses parser relation queries\n");
    printf("PASS: Local type hierarchy uses parser relation queries\n");
    printf("PASS: Local call hierarchy uses parser call-edge queries\n");
    printf("PASS: Inlay hints use canonical declaration queries\n");
    printf("PASS: Completion semantic facts are snapshot read-only\n");
    printf("PASS: Lexical completion uses parser visible-symbol queries\n");
    printf("PASS: Completion consumer does not materialize a scoped analyzer\n");
    printf("PASS: Signature semantic facts are snapshot read-only\n");
    printf("PASS: Local semantic query is snapshot read-only\n");
    printf("PASS: CodeLens uses canonical declaration and reference queries\n");
    printf("PASS: Source hover uses parser symbol query\n");
    printf("PASS: Public hover consumer avoids analyzer hover fallback\n");
    printf("PASS: Metadata hover consumer avoids analyzer hover fallback\n");
    printf("PASS: Receiver completion avoids AST type reinference\n");
    printf("PASS: Extern callable decorators use parser diagnostic projection\n");
    printf("PASS: SemanticAnalyzer ResolveTypeAtPosition uses canonical parser query\n");
    printf("PASS: Duplicate type uses parser diagnostic projection\n");
    printf("PASS: Return type inference uses parser diagnostic projection\n");
    printf("PASS: Semantic analyzer has no unstructured diagnostic escape hatch\n");
    printf("PASS: Semantic analyzer rules only publish structured query diagnostics\n");
    printf("PASS: Semantic analyzer resolves references through canonical SymbolAt\n");
    printf("\nPASSED: LSP source contract tests\n");
    return 0;
}
