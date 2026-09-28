#ifndef ZR_VM_LANGUAGE_SERVER_LSP_NATIVE_DECLARATION_PROJECTION_H
#define ZR_VM_LANGUAGE_SERVER_LSP_NATIVE_DECLARATION_PROJECTION_H

#include "lsp_virtual_documents.h"

/**
 * 只读虚拟文档中一个可导航名字的描述符身份与源码坐标。
 * declarationIdentity 和文字字段均借用原生 descriptor；range 的 URI 借用构建输入。
 */
typedef struct SZrLspVirtualRecord {
    EZrLspVirtualDeclarationKind kind;
    const void *declarationIdentity;
    const ZrLibTypeDescriptor *ownerTypeDescriptor;
    const TZrChar *ownerName;
    const TZrChar *name;
    const TZrChar *targetModuleName;
    SZrFileRange range;
} SZrLspVirtualRecord;

/**
 * @brief 同时生成原生 descriptor 的只读声明文本和可导航名字记录。
 * @pre descriptor 与 uri 在调用期间有效；可按需传入 outText、outRecords。
 * @note 文本由 VM GC 持有；outRecords 数组缓冲由调用方释放，记录内指针继续借用 descriptor。
 */
TZrBool ZrLanguageServer_LspNativeDeclarationProjection_Build(
        SZrState *state,
        const ZrLibModuleDescriptor *descriptor,
        SZrString *uri,
        SZrString **outText,
        SZrArray *outRecords);

/**
 * @brief 用描述符指针身份在当前投影中查唯一声明范围，避免同名重载误跳。
 * @pre declarationIdentity 必须指向传入 descriptor 树中的当前对象。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspNativeDeclarationProjection_Find(
        SZrState *state,
        const ZrLibModuleDescriptor *descriptor,
        SZrString *uri,
        EZrLspVirtualDeclarationKind kind,
        const void *declarationIdentity,
        SZrFileRange *outRange);

struct SZrLspResolvedMetadataMember;
/** @brief 把 provider 已解析的模块级成员映射到同一虚拟文本的声明位置。 */
TZrBool ZrLanguageServer_LspNativeDeclarationProjection_ResolveMember(
        SZrState *state,
        struct SZrLspResolvedMetadataMember *resolvedMember);

#endif
