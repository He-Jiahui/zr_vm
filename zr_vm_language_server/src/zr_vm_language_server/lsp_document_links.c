#include "lsp_editor_features_internal.h"
#include "lsp_virtual_documents.h"
#include "metadata/lsp_virtual_document_identity.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* GetDefinition 的位置数组只借用其中 URI；本层释放位置外壳，GC 字符串继续由请求状态持有。 */
static void lsp_document_links_free_locations(SZrState *state, SZrArray *locations) {
    if (state == ZR_NULL || locations == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *locationPtr, sizeof(SZrLspLocation));
        }
    }
    ZrCore_Array_Free(state, locations);
}

/* 将短/长 VM 字符串统一交给 URI 与 .zrp 文本扫描；返回值不转移所有权。 */
static const TZrChar *lsp_document_links_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

/* 字面导入链接的轻量候选门：先排除标识符成员和非调用形式，
 * 后续仍必须用 offset_is_code 核实不处于注释或字符串。 */
static TZrBool lsp_document_links_import_call_at(const TZrChar *content,
                                                 TZrSize contentLength,
                                                 TZrSize offset) {
    TZrSize scan;

    if (content == ZR_NULL || offset + strlen("import") > contentLength ||
        memcmp(content + offset, "import", strlen("import")) != 0) {
        return ZR_FALSE;
    }
    if ((offset > 0 &&
         (isalnum((unsigned char)content[offset - 1]) || content[offset - 1] == '_' || content[offset - 1] == '.')) ||
        (offset + strlen("import") < contentLength &&
         (isalnum((unsigned char)content[offset + strlen("import")]) ||
          content[offset + strlen("import")] == '_'))) {
        return ZR_FALSE;
    }

    scan = offset + strlen("import");
    while (scan < contentLength && isspace((unsigned char)content[scan])) {
        scan++;
    }
    return scan < contentLength && content[scan] == '(';
}

/* 为 stdio/WASM 的初次 documentLink 响应构造完整目标；
 * 释放函数只回收 link 外壳和数组，target/tooltip 为请求期间的 GC 字符串。 */
static TZrBool lsp_document_links_append(SZrState *state,
                                         SZrArray *result,
                                         SZrLspRange range,
                                         SZrString *target) {
    SZrLspDocumentLink *link;

    if (state == ZR_NULL || result == ZR_NULL || target == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDocumentLink *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    link = (SZrLspDocumentLink *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDocumentLink));
    if (link == ZR_NULL) {
        return ZR_FALSE;
    }
    link->range = range;
    link->target = target;
    link->tooltip = lsp_editor_create_string(state, "Open Zr import target", strlen("Open Zr import target"));
    ZrCore_Array_Push(state, result, &link);
    return ZR_TRUE;
}

/* 仅对 zr.* 导入尝试原生虚拟声明回退；普通导入须靠语义定义查询。 */
static TZrBool lsp_document_links_import_is_native(const TZrChar *content,
                                                   TZrSize valueStart,
                                                   TZrSize valueEnd) {
    return content != ZR_NULL &&
           valueEnd >= valueStart + 3 &&
           memcmp(content + valueStart, "zr.", strlen("zr.")) == 0;
}

/* 语义定义不可用时，按当前 URI 的项目作用域把 zr.* 模块映射到声明 URI；
 * 未解析的导入不产出不可点击的占位链接。 */
static TZrBool lsp_document_links_append_native_import(SZrState *state,
                                                       SZrLspContext *context,
                                                       SZrString *uri,
                                                       SZrArray *result,
                                                       const TZrChar *content,
                                                       TZrSize valueStart,
                                                       TZrSize valueEnd,
                                                       SZrLspRange range) {
    TZrSize valueLength;
    SZrString *target = ZR_NULL;
    SZrString *moduleName;

    if (state == ZR_NULL || result == ZR_NULL ||
        !lsp_document_links_import_is_native(content, valueStart, valueEnd)) {
        return ZR_TRUE;
    }

    valueLength = valueEnd - valueStart;
    moduleName = ZrCore_String_Create(state, (TZrNativeString)(content + valueStart), valueLength);
    if (!ZrLanguageServer_LspVirtualDocumentIdentity_ResolveNativeUri(state, context,
                ZrLanguageServer_LspProject_FindProjectForUri(context, uri), moduleName, &target)) {
        return ZR_TRUE;
    }
    return lsp_document_links_append(state, result, range, target);
}

/* .zrp 的路径字段仅在项目描述文件中解释，普通 Zr 源文件不走该投影。 */
static TZrBool lsp_document_links_uri_ends_with(const TZrChar *uriText, const TZrChar *suffix) {
    TZrSize uriLength;
    TZrSize suffixLength;

    if (uriText == ZR_NULL || suffix == ZR_NULL) {
        return ZR_FALSE;
    }

    uriLength = strlen(uriText);
    suffixLength = strlen(suffix);
    return uriLength >= suffixLength &&
           memcmp(uriText + uriLength - suffixLength, suffix, suffixLength) == 0;
}

/* 保留原文值的字节区间，使编辑器高亮与打开的 .zrp 文本一致。
 * BUG: 此扫描不判断 JSON 对象层级；根对象前的嵌套同名字段会先命中。
 * 例如 metadata.source=shadow、根 source=src 时返回 shadow 的链接；
 * 项目加载器 project.c 只读根字段，需解析根键并保留原文范围。 */
static TZrBool lsp_document_links_find_json_string_value(const TZrChar *content,
                                                        TZrSize contentLength,
                                                        const TZrChar *key,
                                                        TZrSize *outValueStart,
                                                        TZrSize *outValueEnd) {
    TZrSize keyLength;
    TZrSize cursor = 0;

    if (content == ZR_NULL || key == ZR_NULL || outValueStart == ZR_NULL || outValueEnd == ZR_NULL) {
        return ZR_FALSE;
    }

    keyLength = strlen(key);
    while (cursor + keyLength + 2 < contentLength) {
        TZrSize keyStart;
        TZrSize quoteOffset;
        TZrSize closeOffset;

        if (content[cursor] != '"') {
            cursor++;
            continue;
        }

        keyStart = cursor + 1;
        if (keyStart + keyLength >= contentLength ||
            memcmp(content + keyStart, key, keyLength) != 0 ||
            content[keyStart + keyLength] != '"') {
            cursor++;
            continue;
        }

        quoteOffset = keyStart + keyLength + 1;
        while (quoteOffset < contentLength && content[quoteOffset] != ':') {
            quoteOffset++;
        }
        if (quoteOffset >= contentLength) {
            return ZR_FALSE;
        }
        quoteOffset++;
        while (quoteOffset < contentLength &&
               (content[quoteOffset] == ' ' || content[quoteOffset] == '\t' ||
                content[quoteOffset] == '\r' || content[quoteOffset] == '\n')) {
            quoteOffset++;
        }
        if (quoteOffset >= contentLength || content[quoteOffset] != '"') {
            cursor = quoteOffset;
            continue;
        }

        closeOffset = quoteOffset + 1;
        while (closeOffset < contentLength) {
            if (content[closeOffset] == '"' && (closeOffset == quoteOffset + 1 || content[closeOffset - 1] != '\\')) {
                *outValueStart = quoteOffset + 1;
                *outValueEnd = closeOffset;
                return ZR_TRUE;
            }
            closeOffset++;
        }
        return ZR_FALSE;
    }

    return ZR_FALSE;
}

/* 路径文本并入 file URI 前做 URL 转义；此处输入目前是 JSON 原文字节，
 * 并非 cJSON 解码后的路径，相关 JSON 转义失配在 append_zrp_links 中标出。 */
static TZrBool lsp_document_links_uri_append_escaped(TZrChar *buffer,
                                                     TZrSize bufferSize,
                                                     TZrSize *offset,
                                                     const TZrChar *text,
                                                     TZrSize textLength) {
    static const TZrChar hex[] = "0123456789ABCDEF";

    if (buffer == ZR_NULL || offset == ZR_NULL || text == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < textLength; index++) {
        TZrChar ch = text[index] == '\\' ? '/' : text[index];

        if ((ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '/' || ch == '.' || ch == '_' || ch == '-' || ch == '~' || ch == ':') {
            if (*offset + 1 >= bufferSize) {
                return ZR_FALSE;
            }
            buffer[(*offset)++] = ch;
        } else {
            if (*offset + 3 >= bufferSize) {
                return ZR_FALSE;
            }
            buffer[(*offset)++] = '%';
            buffer[(*offset)++] = hex[((unsigned char)ch >> 4) & 0x0F];
            buffer[(*offset)++] = hex[(unsigned char)ch & 0x0F];
        }
    }

    buffer[*offset] = '\0';
    return ZR_TRUE;
}

/* 相对 .zrp 路径以描述文件目录为根，已给出的 file URI 直接沿用；
 * 原生绝对路径与项目加载器的差异在下方标出。 */
static TZrBool lsp_document_links_build_relative_uri(TZrChar *buffer,
                                                     TZrSize bufferSize,
                                                     const TZrChar *baseUri,
                                                     const TZrChar *pathText,
                                                     TZrSize pathLength) {
    TZrSize baseLength;
    TZrSize baseDirLength = 0;
    TZrSize offset;

    if (buffer == ZR_NULL || bufferSize == 0 || baseUri == ZR_NULL || pathText == ZR_NULL || pathLength == 0) {
        return ZR_FALSE;
    }

    if (pathLength >= 7 && memcmp(pathText, "file://", 7) == 0) {
        if (pathLength >= bufferSize) {
            return ZR_FALSE;
        }
        memcpy(buffer, pathText, pathLength);
        buffer[pathLength] = '\0';
        return ZR_TRUE;
    }

    /* BUG: File_PathJoin 对绝对 source 路径会忽略项目目录；此处只认 file://。
     * /tmp/src 会被拼成 file:///项目目录//tmp/src，source/entry 链接指向错误位置；
     * 需按项目加载器的原生路径规则解析后再生成 URI。 */
    baseLength = strlen(baseUri);
    for (TZrSize index = 0; index < baseLength; index++) {
        if (baseUri[index] == '/') {
            baseDirLength = index + 1;
        }
    }

    if (baseDirLength == 0 || baseDirLength >= bufferSize) {
        return ZR_FALSE;
    }
    memcpy(buffer, baseUri, baseDirLength);
    offset = baseDirLength;
    return lsp_document_links_uri_append_escaped(buffer, bufferSize, &offset, pathText, pathLength);
}

/* 把项目字段的原文范围与目标 URI 成对加入结果；过长路径按无链接处理。 */
static TZrBool lsp_document_links_append_zrp_path(SZrState *state,
                                                  SZrArray *result,
                                                  const TZrChar *content,
                                                  TZrSize contentLength,
                                                  const TZrChar *baseUri,
                                                  TZrSize valueStart,
                                                  TZrSize valueEnd,
                                                  const TZrChar *targetText,
                                                  TZrSize targetLength) {
    TZrChar targetBuffer[ZR_VM_PATH_LENGTH_MAX];
    SZrLspRange range;
    SZrString *target;

    if (!lsp_document_links_build_relative_uri(targetBuffer,
                                               sizeof(targetBuffer),
                                               baseUri,
                                               targetText,
                                               targetLength)) {
        return ZR_TRUE;
    }

    range = lsp_editor_range_from_offsets(content, contentLength, valueStart, valueEnd);
    target = lsp_editor_create_string(state, targetBuffer, strlen(targetBuffer));
    return lsp_document_links_append(state, result, range, target);
}

/* .zrp 的 source/binary/dependency/local/entry 字段直接服务编辑器路径跳转；
 * 相对 entry 文本以 source 为根并补 .zr，不经项目加载器规范化。 */
static TZrBool lsp_document_links_append_zrp_links(SZrState *state,
                                                   SZrArray *result,
                                                   SZrString *uri,
                                                   const TZrChar *content,
                                                   TZrSize contentLength) {
    const TZrChar *uriText = lsp_document_links_string_text(uri);
    TZrSize sourceStart = 0;
    TZrSize sourceEnd = 0;
    TZrSize binaryStart = 0;
    TZrSize binaryEnd = 0;
    TZrSize entryStart = 0;
    TZrSize entryEnd = 0;
    TZrSize dependencyStart = 0;
    TZrSize dependencyEnd = 0;
    TZrSize localStart = 0;
    TZrSize localEnd = 0;
    TZrBool hasSource;

    if (state == ZR_NULL || result == ZR_NULL || uriText == ZR_NULL ||
        !lsp_document_links_uri_ends_with(uriText, ".zrp")) {
        return ZR_TRUE;
    }

    /* BUG: 项目加载器 project.c 用 cJSON 解码字段，此处却直接使用 JSON 原文字节。
     * 合法值如 "src\u0020dir" 会被投影为 src/u0020dir 而非 src%20dir，
     * 点击 source 或 entry 会打开错误 URI；需解码值并保留原文高亮范围。 */
    hasSource =
        lsp_document_links_find_json_string_value(content, contentLength, "source", &sourceStart, &sourceEnd);
    if (hasSource &&
        !lsp_document_links_append_zrp_path(state,
                                            result,
                                            content,
                                            contentLength,
                                            uriText,
                                            sourceStart,
                                            sourceEnd,
                                            content + sourceStart,
                                            sourceEnd - sourceStart)) {
        return ZR_FALSE;
    }

    if (lsp_document_links_find_json_string_value(content, contentLength, "binary", &binaryStart, &binaryEnd) &&
        !lsp_document_links_append_zrp_path(state,
                                            result,
                                            content,
                                            contentLength,
                                            uriText,
                                            binaryStart,
                                            binaryEnd,
                                            content + binaryStart,
                                            binaryEnd - binaryStart)) {
        return ZR_FALSE;
    }

    if (lsp_document_links_find_json_string_value(content,
                                                  contentLength,
                                                  "dependency",
                                                  &dependencyStart,
                                                  &dependencyEnd) &&
        !lsp_document_links_append_zrp_path(state,
                                            result,
                                            content,
                                            contentLength,
                                            uriText,
                                            dependencyStart,
                                            dependencyEnd,
                                            content + dependencyStart,
                                            dependencyEnd - dependencyStart)) {
        return ZR_FALSE;
    }

    if (lsp_document_links_find_json_string_value(content, contentLength, "local", &localStart, &localEnd) &&
        !lsp_document_links_append_zrp_path(state,
                                            result,
                                            content,
                                            contentLength,
                                            uriText,
                                            localStart,
                                            localEnd,
                                            content + localStart,
                                            localEnd - localStart)) {
        return ZR_FALSE;
    }

    if (lsp_document_links_find_json_string_value(content, contentLength, "entry", &entryStart, &entryEnd)) {
        TZrChar entryPath[ZR_VM_PATH_LENGTH_MAX];
        TZrSize offset = 0;

        if (hasSource) {
            TZrSize sourceLength = sourceEnd - sourceStart;
            if (sourceLength + 1 < sizeof(entryPath)) {
                memcpy(entryPath, content + sourceStart, sourceLength);
                offset = sourceLength;
                if (offset > 0 && entryPath[offset - 1] != '/' && entryPath[offset - 1] != '\\') {
                    entryPath[offset++] = '/';
                }
            }
        }
        if (offset + (entryEnd - entryStart) + 4 < sizeof(entryPath)) {
            memcpy(entryPath + offset, content + entryStart, entryEnd - entryStart);
            offset += entryEnd - entryStart;
            if (offset < 3 ||
                entryPath[offset - 3] != '.' ||
                entryPath[offset - 2] != 'z' ||
                entryPath[offset - 1] != 'r') {
                entryPath[offset++] = '.';
                entryPath[offset++] = 'z';
                entryPath[offset++] = 'r';
            }
            entryPath[offset] = '\0';
            if (!lsp_document_links_append_zrp_path(state,
                                                    result,
                                                    content,
                                                    contentLength,
                                                    uriText,
                                                    entryStart,
                                                    entryEnd,
                                                    entryPath,
                                                    offset)) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

/* 原生虚拟声明文档的 pub module 行与 GetDefinition 共享投影；
 * 链接目标从定义查询取得，以维持项目专属 URI 身份。 */
static TZrBool lsp_document_links_append_virtual_module_links(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrArray *result,
                                                              SZrString *uri,
                                                              const TZrChar *content,
                                                              TZrSize contentLength) {
    static const TZrChar prefix[] = "pub module ";
    TZrSize cursor = 0;

    if (state == ZR_NULL || result == ZR_NULL || uri == ZR_NULL ||
        content == ZR_NULL || !ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(uri)) {
        return ZR_TRUE;
    }

    while (cursor < contentLength) {
        TZrSize lineStart = cursor;
        TZrSize lineEnd = cursor;
        TZrSize trimStart;

        while (lineEnd < contentLength && content[lineEnd] != '\n') {
            lineEnd++;
        }

        trimStart = lineStart;
        while (trimStart < lineEnd && (content[trimStart] == ' ' || content[trimStart] == '\t')) {
            trimStart++;
        }

        if (lineEnd - trimStart > strlen(prefix) &&
            memcmp(content + trimStart, prefix, strlen(prefix)) == 0) {
            TZrSize nameStart = trimStart + strlen(prefix);
            TZrSize nameEnd = nameStart;
            SZrLspRange range;
            SZrArray definitions = {0};

            while (nameEnd < lineEnd &&
                   content[nameEnd] != ':' &&
                   content[nameEnd] != ' ' &&
                   content[nameEnd] != '\t') {
                nameEnd++;
            }

            if (nameStart < nameEnd) {
                range = lsp_editor_range_from_offsets(content, contentLength, nameStart, nameEnd);
                if (ZrLanguageServer_Lsp_GetDefinition(state, context, uri, range.start, &definitions)) {
                    for (TZrSize index = 0U; index < definitions.length; index++) {
                        SZrLspLocation **location = (SZrLspLocation **)ZrCore_Array_Get(&definitions, index);
                        if (location != ZR_NULL && *location != ZR_NULL &&
                            !lsp_document_links_append(state, result, range, (*location)->uri)) {
                            lsp_document_links_free_locations(state, &definitions);
                            return ZR_FALSE;
                        }
                    }
                }
                lsp_document_links_free_locations(state, &definitions);
            }
        }

        cursor = lineEnd < contentLength ? lineEnd + 1 : lineEnd;
    }

    return ZR_TRUE;
}

/* 无可用快照的普通 file URI 可从磁盘取文本；返回的原生缓冲区由 GetDocumentLinks
 * 在处理完成后按 contentLength+1 释放，不与打开文档的快照混用。 */
static TZrChar *lsp_document_links_read_uri_from_disk(SZrState *state,
                                                      SZrString *uri,
                                                      TZrSize *outLength) {
    TZrChar path[ZR_LIBRARY_MAX_PATH_LENGTH];
    FILE *file;
    long fileSize;
    TZrChar *buffer;
    size_t readLength;

    if (state == ZR_NULL || uri == ZR_NULL || outLength == ZR_NULL ||
        !ZrLanguageServer_Lsp_FileUriToNativePath(uri, path, sizeof(path))) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }
    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (TZrChar *)ZrCore_Memory_RawMalloc(state->global, (TZrSize)fileSize + 1);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    readLength = fileSize > 0 ? fread(buffer, 1, (size_t)fileSize, file) : 0;
    fclose(file);
    if (readLength != (size_t)fileSize) {
        ZrCore_Memory_RawFree(state->global, buffer, (TZrSize)fileSize + 1);
        return ZR_NULL;
    }

    buffer[readLength] = '\0';
    *outLength = (TZrSize)readLength;
    return buffer;
}

/* stdio/WASM 的 documentLink 入口：优先使用打开文档的稳定快照，
 * 虚拟声明从投影生成文本，无可用快照的普通 file URI 才从磁盘读取。
 * 调用者无论成功与否都须释放可能部分追加的结果。 */
TZrBool ZrLanguageServer_Lsp_GetDocumentLinks(SZrState *state,
                                              SZrLspContext *context,
                                              SZrString *uri,
                                              SZrArray *result) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    SZrString *virtualDocumentText = ZR_NULL;
    TZrChar *diskContent = ZR_NULL;
    const TZrChar *content;
    TZrSize contentLength = 0;
    TZrSize cursor = 0;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDocumentLink *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = lsp_editor_get_file_version(context, uri);
    if (fileVersion == ZR_NULL) {
        if (ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(uri)) {
            if (!ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, uri, &virtualDocumentText) ||
                virtualDocumentText == ZR_NULL) {
                return ZR_FALSE;
            }
            content = lsp_document_links_string_text(virtualDocumentText);
            contentLength = content != ZR_NULL ? strlen(content) : 0;
            return lsp_document_links_append_virtual_module_links(state, context, result, uri, content, contentLength);
        }
    }

    if (ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        content = snapshot.content;
        contentLength = snapshot.contentLength;
    } else {
        if (ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(uri)) {
            if (!ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, uri, &virtualDocumentText) ||
                virtualDocumentText == ZR_NULL) {
                return ZR_FALSE;
            }
            content = lsp_document_links_string_text(virtualDocumentText);
            contentLength = content != ZR_NULL ? strlen(content) : 0;
            return lsp_document_links_append_virtual_module_links(state, context, result, uri, content, contentLength);
        }
        diskContent = lsp_document_links_read_uri_from_disk(state, uri, &contentLength);
        if (diskContent == ZR_NULL) {
            return ZR_FALSE;
        }
        content = diskContent;
    }

    if (!lsp_document_links_append_zrp_links(state, result, uri, content, contentLength)) {
        goto cleanup;
    }
    if (!lsp_document_links_append_virtual_module_links(state, context, result, uri, content, contentLength)) {
        goto cleanup;
    }

    while (cursor + strlen("import") < contentLength) {
        TZrSize matchOffset;
        TZrSize quoteOffset;
        TZrSize closeOffset;
        SZrArray definitions = {0};
        SZrLspRange linkRange;
        SZrLspPosition queryPosition;
        TZrBool appendedDefinition = ZR_FALSE;

        while (cursor + strlen("import") <= contentLength &&
               !lsp_document_links_import_call_at(content, contentLength, cursor)) {
            cursor++;
        }
        if (cursor + strlen("import") > contentLength) {
            break;
        }
        matchOffset = cursor;
        if (!lsp_editor_offset_is_code(content, contentLength, matchOffset)) {
            cursor = matchOffset + strlen("import");
            continue;
        }
        /* BUG: 这里只找 import 后任意后续双引号，不限定为调用的首个字面参数。
         * `import(x); let name="zr.math";` 可把 name 字符串误投影成原生导入链接；
         * 应由解析后的调用参数范围而非跨语句文本搜索决定候选。 */
        quoteOffset = matchOffset;
        while (quoteOffset < contentLength && content[quoteOffset] != '"') {
            quoteOffset++;
        }
        if (quoteOffset >= contentLength) {
            cursor = matchOffset + strlen("import");
            continue;
        }
        closeOffset = quoteOffset + 1;
        while (closeOffset < contentLength && content[closeOffset] != '"') {
            closeOffset++;
        }
        if (closeOffset >= contentLength) {
            cursor = quoteOffset + 1;
            continue;
        }

        linkRange = lsp_editor_range_from_offsets(content, contentLength, quoteOffset + 1, closeOffset);
        queryPosition = linkRange.start;
        if (ZrLanguageServer_Lsp_GetDefinition(state, context, uri, queryPosition, &definitions) &&
            definitions.length > 0) {
            SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(&definitions, 0);
            if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL && (*locationPtr)->uri != ZR_NULL) {
                if (!lsp_document_links_append(state, result, linkRange, (*locationPtr)->uri)) {
                    lsp_document_links_free_locations(state, &definitions);
                    goto cleanup;
                }
                appendedDefinition = ZR_TRUE;
            }
        }
        lsp_document_links_free_locations(state, &definitions);
        if (!appendedDefinition &&
            !lsp_document_links_append_native_import(state,
                                                     context,
                                                     uri,
                                                     result,
                                                     content,
                                                     quoteOffset + 1,
                                                     closeOffset,
                                                     linkRange)) {
            goto cleanup;
        }
        cursor = closeOffset + 1;
    }

    success = ZR_TRUE;

cleanup:
    if (diskContent != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, diskContent, contentLength + 1);
    }
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return success;
}
