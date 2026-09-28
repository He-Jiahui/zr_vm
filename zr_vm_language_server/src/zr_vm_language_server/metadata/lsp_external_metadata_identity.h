#ifndef ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_METADATA_IDENTITY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_METADATA_IDENTITY_H

#include "metadata/lsp_metadata_provider.h"

/**
 * 将外部身份解析为当前项目中的声明位置；URI 借用项目索引或元数据提供者。
 * 不可跨 provider 代数或项目刷新长期保存其中的 URI。
 */
typedef struct SZrLspExternalMetadataIdentityDeclaration {
    SZrString *uri;
    SZrFileRange range;
    EZrLspImportedModuleSourceKind sourceKind;
} SZrLspExternalMetadataIdentityDeclaration;

/**
 * @brief 对 parser 外部引用的 owner、token、签名做唯一匹配，供跨快照声明导航。
 * @pre provider、identity 与输出在调用期间有效；带代数的 identity 须属于当前 provider。
 * @note 只接受带源码记录及精确声明范围的目标；失败时清空输出。
 */
TZrBool ZrLanguageServer_LspExternalMetadataIdentity_ResolveDeclaration(
        SZrLspMetadataProvider *provider,
        SZrSemanticAnalyzer *analyzer,
        SZrLspProjectIndex *projectIndex,
        const SZrParserSemanticExternalReferenceQuery *identity,
        SZrLspExternalMetadataIdentityDeclaration *outDeclaration);
/**
 * @brief 在当前元数据提供者中重解析外部引用，供跨快照引用搜索验证身份。
 * @pre identity 的元数据身份完整；返回的 descriptor 指针借用当前 provider。
 */
TZrBool ZrLanguageServer_LspExternalMetadataIdentity_ResolveMember(
        SZrLspMetadataProvider *provider,
        SZrSemanticAnalyzer *analyzer,
        SZrLspProjectIndex *projectIndex,
        const SZrParserSemanticExternalReferenceQuery *identity,
        SZrLspResolvedMetadataMember *outResolved);

#endif
