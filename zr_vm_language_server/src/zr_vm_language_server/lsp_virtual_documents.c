#include "lsp_virtual_documents.h"

#include "metadata/lsp_metadata_provider.h"
#include "metadata/lsp_native_declaration_projection.h"
#include "metadata/lsp_virtual_document_identity.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_library/native_registry.h"

/* 旧式 URI 与带项目身份的 URI 共用 scheme；来源授权须在描述符解析阶段完成。 */
#define ZR_LSP_VIRTUAL_URI_PREFIX "zr-decompiled:/"
/* 只用于临时投影记录；调用方不得持有数组元素地址越过释放点。 */
#define ZR_LSP_VIRTUAL_RECORD_INITIAL_CAPACITY 32U

/* 描述符路径只在本次调用期间读取 URI 文本；返回值借用 VM 字符串存储。 */
static const TZrChar *virtual_documents_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

/* 渲染页使用内部一基字节坐标；模块链接点击会用这个范围还原记录身份。 */
static TZrBool virtual_documents_range_contains_file_position(SZrFileRange range, SZrFilePosition position) {
    /* BUG: 投影的 end 是标识符之后的独占位置；这里允许等于 end，分隔符光标会命中上一声明。 */
    TZrInt32 line = position.line;
    TZrInt32 column = position.column;

    return (range.start.line < line || (range.start.line == line && range.start.column <= column)) &&
           (line < range.end.line || (line == range.end.line && column <= range.end.column));
}

/* 兼容未渲染的物理插件 URI：紧凑记录沿用一基列，输入仍是 LSP 零基光标。 */
static TZrBool virtual_documents_range_contains_position(SZrFileRange range, SZrLspPosition position) {
    /* BUG: 兼容记录也把独占 end 当命中点，物理 URI 光标落在成员后仍可能误选。 */
    TZrInt32 line = position.line + 1;
    TZrInt32 column = position.character + 1;

    return (range.start.line < line || (range.start.line == line && range.start.column <= column)) &&
           (line < range.end.line || (line == range.end.line && column <= range.end.column));
}

/* 有虚拟文本时先做 UTF-16 到字节列投影；只有兼容路径才直接比较光标列。 */
static TZrBool virtual_documents_range_contains_lsp_position(SZrFileRange range,
                                                             SZrLspPosition position,
                                                             const TZrChar *content,
                                                             TZrSize contentLength) {
    SZrFilePosition filePosition;

    if (content == ZR_NULL) {
        return virtual_documents_range_contains_position(range, position);
    }

    filePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position, content, contentLength);
    return virtual_documents_range_contains_file_position(range, filePosition);
}

/* 仅做前缀路由，具体模块、项目来源与代数交由 ResolveDescriptorForUri 核对。 */
static TZrBool virtual_documents_uri_is_virtual_declaration(SZrString *uri) {
    const TZrChar *text = virtual_documents_string_text(uri);
    TZrSize prefixLength = strlen(ZR_LSP_VIRTUAL_URI_PREFIX);

    return text != ZR_NULL && strncmp(text, ZR_LSP_VIRTUAL_URI_PREFIX, prefixLength) == 0;
}

/* 历史插件文件 URI 没有可渲染声明页；成员范围是兼容导航所需的紧凑占位坐标。 */
static TZrBool virtual_documents_record_compact_type_members(SZrState *state,
                                                             const ZrLibModuleDescriptor *descriptor,
                                                             SZrString *uri,
                                                             SZrArray *outRecords) {
    if (state == ZR_NULL || descriptor == ZR_NULL || uri == ZR_NULL || outRecords == ZR_NULL) {
        return ZR_FALSE;
    }

    /* BUG: 插件成员名超过固定的八列间隔时相邻占位范围会重叠；
     * FindDeclarationAtPosition 首个命中即返回，点击后一成员的起始列可能误选前一成员。 */
    ZrCore_Array_Init(state, outRecords, sizeof(SZrLspVirtualRecord), ZR_LSP_VIRTUAL_RECORD_INITIAL_CAPACITY);
    for (TZrSize typeIndex = 0; typeIndex < descriptor->typeCount; typeIndex++) {
        const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[typeIndex];
        TZrInt32 fileLine = (TZrInt32)(typeIndex + 2);

        if (typeDescriptor == ZR_NULL || typeDescriptor->name == ZR_NULL) {
            continue;
        }

        for (TZrSize fieldIndex = 0; fieldIndex < typeDescriptor->fieldCount; fieldIndex++) {
            const ZrLibFieldDescriptor *fieldDescriptor = &typeDescriptor->fields[fieldIndex];
            SZrLspVirtualRecord record;
            TZrSize nameLength;
            TZrInt32 startColumn;
            TZrInt32 endColumn;

            if (fieldDescriptor == ZR_NULL || fieldDescriptor->name == ZR_NULL) {
                continue;
            }

            memset(&record, 0, sizeof(record));
            nameLength = strlen(fieldDescriptor->name);
            startColumn = (TZrInt32)(fieldIndex * 8 + 1);
            endColumn = startColumn + (TZrInt32)(nameLength > 0 ? nameLength : 1);
            record.kind = ZR_LSP_VIRTUAL_DECLARATION_FIELD;
            record.declarationIdentity = fieldDescriptor;
            record.ownerTypeDescriptor = typeDescriptor;
            record.ownerName = typeDescriptor->name;
            record.name = fieldDescriptor->name;
            record.range = ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, fileLine, startColumn),
                                                     ZrParser_FilePosition_Create(0, fileLine, endColumn),
                                                     uri);
            ZrCore_Array_Push(state, outRecords, &record);
        }

        for (TZrSize methodIndex = 0; methodIndex < typeDescriptor->methodCount; methodIndex++) {
            const ZrLibMethodDescriptor *methodDescriptor = &typeDescriptor->methods[methodIndex];
            SZrLspVirtualRecord record;
            TZrSize nameLength;
            TZrInt32 startColumn;
            TZrInt32 endColumn;

            if (methodDescriptor == ZR_NULL || methodDescriptor->name == ZR_NULL) {
                continue;
            }

            memset(&record, 0, sizeof(record));
            nameLength = strlen(methodDescriptor->name);
            startColumn = (TZrInt32)(methodIndex * 8 + 129);
            endColumn = startColumn + (TZrInt32)(nameLength > 0 ? nameLength : 1);
            record.kind = ZR_LSP_VIRTUAL_DECLARATION_METHOD;
            record.declarationIdentity = methodDescriptor;
            record.ownerTypeDescriptor = typeDescriptor;
            record.ownerName = typeDescriptor->name;
            record.name = methodDescriptor->name;
            record.range = ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, fileLine, startColumn),
                                                     ZrParser_FilePosition_Create(0, fileLine, endColumn),
                                                     uri);
            ZrCore_Array_Push(state, outRecords, &record);
        }
    }

    return ZR_TRUE;
}

/* 虚拟页的文本和坐标由同一投影生成；物理插件文件保留紧凑记录兼容旧查询入口。 */
static TZrBool virtual_documents_collect_records(SZrState *state,
                                                 const ZrLibModuleDescriptor *descriptor,
                                                 SZrString *uri,
                                                 SZrArray *outRecords) {
    if (state == ZR_NULL || descriptor == ZR_NULL || uri == ZR_NULL || outRecords == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!virtual_documents_uri_is_virtual_declaration(uri)) {
        return virtual_documents_record_compact_type_members(state, descriptor, uri, outRecords);
    }

    ZrCore_Array_Construct(outRecords);
    return ZrLanguageServer_LspNativeDeclarationProjection_Build(state, descriptor, uri, ZR_NULL, outRecords);
}

/* 供普通文档分析、诊断和文档链接快速分流；解析阶段还要做身份检查。 */
TZrBool ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(SZrString *uri) {
    return virtual_documents_uri_is_virtual_declaration(uri);
}

/* 内建模块仍使用稳定的旧式 URI；项目插件改由带来源和代数的 identity 路径生成。 */
SZrString *ZrLanguageServer_LspVirtualDocuments_CreateDeclarationUri(SZrState *state, const TZrChar *moduleName) {
    TZrChar buffer[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (state == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    /* BUG: 这里不检验截断；超长模块名返回看似有效的错误 URI，导航请求再也找不到原 descriptor。 */
    snprintf(buffer, sizeof(buffer), "%s%s.zr", ZR_LSP_VIRTUAL_URI_PREFIX, moduleName);
    return ZrCore_String_Create(state, buffer, strlen(buffer));
}

/* 旧式 URI 是查找键，不是项目插件的授权凭据；scope 查询由身份模块另行解析。 */
TZrBool ZrLanguageServer_LspVirtualDocuments_ParseDeclarationUri(SZrString *uri,
                                                                 TZrChar *moduleNameBuffer,
                                                                 TZrSize bufferSize) {
    const TZrChar *text = virtual_documents_string_text(uri);
    TZrSize prefixLength = strlen(ZR_LSP_VIRTUAL_URI_PREFIX);
    TZrSize length;

    if (moduleNameBuffer != ZR_NULL && bufferSize > 0) {
        moduleNameBuffer[0] = '\0';
    }
    if (text == ZR_NULL || moduleNameBuffer == ZR_NULL || bufferSize == 0 ||
        strncmp(text, ZR_LSP_VIRTUAL_URI_PREFIX, prefixLength) != 0) {
        return ZR_FALSE;
    }

    text += prefixLength;
    while (*text == '/') {
        text++;
    }

    length = strlen(text);
    if (length >= 3 && strcmp(text + length - 3, ".zr") == 0) {
        length -= 3;
    }
    if (length == 0 || length + 1 > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(moduleNameBuffer, text, length);
    moduleNameBuffer[length] = '\0';
    return ZR_TRUE;
}

/* 文档请求和项目导航均在读取时重新解析来源，避免缓存插件 descriptor 跨重载代数。 */
TZrBool ZrLanguageServer_LspVirtualDocuments_ResolveDescriptorForUri(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrLspProjectIndex *projectIndex,
                                                                     SZrString *uri,
                                                                     const ZrLibModuleDescriptor **outDescriptor,
                                                                     EZrLspImportedModuleSourceKind *outSourceKind,
                                                                     TZrChar *moduleNameBuffer,
                                                                     TZrSize bufferSize) {
    SZrString *moduleNameString;
    SZrLspResolvedImportedModule resolved;
    TZrBool parsedVirtualUri = ZR_FALSE;

    if (outDescriptor != ZR_NULL) {
        *outDescriptor = ZR_NULL;
    }
    if (outSourceKind != ZR_NULL) {
        *outSourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED;
    }
    if (state == ZR_NULL || uri == ZR_NULL || moduleNameBuffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    /* 带作用域的 URI 只能回到原项目和当前代数；不能退回同名的全局注册项。 */
    if (ZrLanguageServer_LspVirtualDocumentIdentity_IsScoped(uri)) {
        SZrLspVirtualDocumentIdentity identity;
        SZrLspProjectIndex *owner = ZrLanguageServer_LspVirtualDocumentIdentity_FindProject(context, uri);
        if (owner == ZR_NULL || (projectIndex != ZR_NULL && projectIndex != owner) ||
            !ZrLanguageServer_LspVirtualDocumentIdentity_ResolveNativeDescriptor(
                    state, context, uri, &identity, ZR_NULL, outDescriptor) ||
            strlen(ZrCore_String_GetNativeString(identity.moduleName)) >= bufferSize) {
            if (outDescriptor != ZR_NULL) {
                *outDescriptor = ZR_NULL;
            }
            return ZR_FALSE;
        }
        strcpy(moduleNameBuffer, ZrCore_String_GetNativeString(identity.moduleName));
        if (outSourceKind != ZR_NULL) {
            *outSourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN;
        }
        return ZR_TRUE;
    }

    parsedVirtualUri = ZrLanguageServer_LspVirtualDocuments_ParseDeclarationUri(uri, moduleNameBuffer, bufferSize);
    /* 物理 file URI 必须从注册表匹配插件来源；旧式虚拟 URI 只能通向内建模块。 */
    if (!parsedVirtualUri) {
        TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
        ZrLibRegisteredModuleInfo moduleInfo;

        memset(&moduleInfo, 0, sizeof(moduleInfo));
        if (state->global == ZR_NULL ||
            !ZrLanguageServer_Lsp_FileUriToNativePath(uri, nativePath, sizeof(nativePath)) ||
            !ZrLibrary_NativeRegistry_GetModuleInfoBySourcePath(state->global, nativePath, &moduleInfo) ||
            moduleInfo.moduleName == ZR_NULL ||
            moduleInfo.moduleName[0] == '\0' ||
            !(moduleInfo.registrationKind == ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_DESCRIPTOR_PLUGIN ||
              moduleInfo.isDescriptorPlugin)) {
            return ZR_FALSE;
        }

        if (strlen(moduleInfo.moduleName) + 1 > bufferSize) {
            return ZR_FALSE;
        }

        memcpy(moduleNameBuffer, moduleInfo.moduleName, strlen(moduleInfo.moduleName) + 1);
    }

    moduleNameString = ZrCore_String_Create(state, moduleNameBuffer, strlen(moduleNameBuffer));
    if (moduleNameString != ZR_NULL && projectIndex != ZR_NULL &&
        ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(state,
                                                                 ZR_NULL,
                                                                 projectIndex,
                                                                 moduleNameString,
                                                                 &resolved) &&
        resolved.nativeDescriptor != ZR_NULL) {
        if (parsedVirtualUri && resolved.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN) {
            return ZR_FALSE;
        }
        if (outDescriptor != ZR_NULL) {
            *outDescriptor = resolved.nativeDescriptor;
        }
        if (outSourceKind != ZR_NULL) {
            *outSourceKind = resolved.sourceKind;
        }
        return ZR_TRUE;
    }

    if (outDescriptor != ZR_NULL) {
        EZrLspImportedModuleSourceKind sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED;
        *outDescriptor = ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleDescriptor(state,
                                                                                          moduleNameBuffer,
                                                                                          &sourceKind);
        if (parsedVirtualUri && sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN) {
            *outDescriptor = ZR_NULL;
            return ZR_FALSE;
        }
        if (outSourceKind != ZR_NULL) {
            *outSourceKind = sourceKind;
        }
    }
    return outDescriptor != ZR_NULL && *outDescriptor != ZR_NULL;
}

/* 文档内容和 FindDeclarationAtPosition 的范围都借助同一原生声明投影。 */
TZrBool ZrLanguageServer_LspVirtualDocuments_RenderDeclarationText(SZrState *state,
                                                                   const ZrLibModuleDescriptor *descriptor,
                                                                   SZrString *uri,
                                                                   SZrString **outText) {
    return ZrLanguageServer_LspNativeDeclarationProjection_Build(state, descriptor, uri, outText, ZR_NULL);
}

/* 用零宽首行范围表示整个模块，供缺少精确声明位置的项目摘要导航。 */
SZrFileRange ZrLanguageServer_LspVirtualDocuments_ModuleEntryRange(SZrString *uri) {
    SZrFilePosition start = ZrParser_FilePosition_Create(0, 1, 1);
    return ZrParser_FileRange_Create(start, start, uri);
}

/* 描述符指针而非同名字符串决定成员身份；重载和同名字段须保持各自范围。 */
TZrBool ZrLanguageServer_LspVirtualDocuments_FindTypeMemberDeclaration(SZrState *state,
                                                                       const ZrLibModuleDescriptor *descriptor,
                                                                       SZrString *uri,
                                                                       const void *declarationIdentity,
                                                                       TZrInt32 memberKind,
                                                                       SZrFileRange *outRange) {
    SZrArray records;
    TZrBool found = ZR_FALSE;
    const SZrLspVirtualRecord *match = ZR_NULL;
    EZrLspVirtualDeclarationKind kind;

    if (outRange != ZR_NULL) {
        memset(outRange, 0, sizeof(*outRange));
    }
    if (state == ZR_NULL || descriptor == ZR_NULL || uri == ZR_NULL ||
        declarationIdentity == ZR_NULL || outRange == ZR_NULL ||
        (memberKind != ZR_LSP_METADATA_MEMBER_FIELD && memberKind != ZR_LSP_METADATA_MEMBER_METHOD)) {
        return ZR_FALSE;
    }
    kind = memberKind == ZR_LSP_METADATA_MEMBER_FIELD
            ? ZR_LSP_VIRTUAL_DECLARATION_FIELD : ZR_LSP_VIRTUAL_DECLARATION_METHOD;
    if (virtual_documents_uri_is_virtual_declaration(uri)) {
        return ZrLanguageServer_LspNativeDeclarationProjection_Find(
                state, descriptor, uri, kind, declarationIdentity, outRange);
    }

    if (!virtual_documents_collect_records(state, descriptor, uri, &records)) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < records.length; index++) {
        SZrLspVirtualRecord *record = (SZrLspVirtualRecord *)ZrCore_Array_Get(&records, index);
        if (record == ZR_NULL || record->kind != kind ||
            record->declarationIdentity != declarationIdentity) {
            continue;
        }
        if (match != ZR_NULL) {
            goto cleanup;
        }
        match = record;
    }
    if (match != ZR_NULL) {
        *outRange = match->range;
        found = ZR_TRUE;
    }

cleanup:
    ZrCore_Array_Free(state, &records);
    return found;
}

/* 该匹配喂给定义导航和元数据查询；记录数组释放后只保留 descriptor/URI 的借用字段。 */
TZrBool ZrLanguageServer_LspVirtualDocuments_FindDeclarationAtPosition(SZrState *state,
                                                                       const ZrLibModuleDescriptor *descriptor,
                                                                       SZrString *uri,
                                                                       SZrLspPosition position,
                                                                       SZrLspVirtualDeclarationMatch *outMatch) {
    SZrArray records;
    SZrString *renderedText = ZR_NULL;
    const TZrChar *content = ZR_NULL;
    TZrSize contentLength = 0;

    if (outMatch != ZR_NULL) {
        memset(outMatch, 0, sizeof(*outMatch));
    }
    if (state == ZR_NULL || descriptor == ZR_NULL || uri == ZR_NULL || outMatch == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&records);
    /* 获取与客户端文档一致的文本，再把 UTF-16 光标换回投影记录的字节坐标。 */
    if (virtual_documents_uri_is_virtual_declaration(uri)) {
        if (!ZrLanguageServer_LspNativeDeclarationProjection_Build(state, descriptor, uri, &renderedText, &records)) {
            return ZR_FALSE;
        }
        content = virtual_documents_string_text(renderedText);
        contentLength = content != ZR_NULL ? strlen(content) : 0;
    } else if (!virtual_documents_collect_records(state, descriptor, uri, &records)) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < records.length; index++) {
        SZrLspVirtualRecord *record = (SZrLspVirtualRecord *)ZrCore_Array_Get(&records, index);

        if (record != ZR_NULL &&
            virtual_documents_range_contains_lsp_position(record->range, position, content, contentLength)) {
            outMatch->kind = record->kind;
            outMatch->descriptor = descriptor;
            outMatch->declarationIdentity = record->declarationIdentity;
            outMatch->ownerTypeDescriptor = record->ownerTypeDescriptor;
            outMatch->moduleName = descriptor->moduleName;
            outMatch->ownerName = record->ownerName;
            outMatch->name = record->name;
            outMatch->targetModuleName = record->targetModuleName;
            outMatch->range = record->range;
            ZrCore_Array_Free(state, &records);
            return ZR_TRUE;
        }
    }

    ZrCore_Array_Free(state, &records);
    return ZR_FALSE;
}
