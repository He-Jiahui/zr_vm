#ifndef ZR_LSP_VIRTUAL_DOCUMENT_IDENTITY_H
#define ZR_LSP_VIRTUAL_DOCUMENT_IDENTITY_H

#include "module/lsp_module_metadata.h"

/**
 * 虚拟 URI 的项目作用域、物理来源及 provider 代数。
 * 字符串由 VM GC 持有；解析结果不持有外部 descriptor。
 */
typedef struct SZrLspVirtualDocumentIdentity {
    SZrString *moduleName;
    SZrString *projectUri;
    SZrString *originUri;
    TZrUInt64 providerGeneration;
} SZrLspVirtualDocumentIdentity;

/** @brief 生成规范的 zr-decompiled URI，供编辑器文档与导航共享身份。 */
ZR_LANGUAGE_SERVER_API SZrString *ZrLanguageServer_LspVirtualDocumentIdentity_Create(
        SZrState *state, const SZrLspVirtualDocumentIdentity *identity);
/**
 * @brief 严格解析项目作用域 URI；只解析其声明身份，不验证项目与 provider 当前状态。
 * @note FindProject 只验证格式、代数与项目；原生插件的物理来源须由 ResolveNativeDescriptor 再验证。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspVirtualDocumentIdentity_Parse(
        SZrState *state, SZrString *uri, SZrLspVirtualDocumentIdentity *outIdentity);
/** @brief 快速识别带查询身份的声明 URI；它不是完整格式或权限校验。 */
TZrBool ZrLanguageServer_LspVirtualDocumentIdentity_IsScoped(SZrString *uri);
/** @brief 为项目原生插件建立代数绑定的虚拟 URI；内建模块沿用已有声明 URI。 */
TZrBool ZrLanguageServer_LspVirtualDocumentIdentity_ResolveNativeUri(
        SZrState *state, SZrLspContext *context, SZrLspProjectIndex *projectIndex,
        SZrString *moduleName, SZrString **outUri);
/**
 * @brief 为二进制元数据模块建立项目作用域身份，供 parser origin 关系携带。
 * @note origin 仍是物理 .zro URI，不能当作源码文本；正常 source-backed 导航继续指向物理 URI。
 */
TZrBool ZrLanguageServer_LspVirtualDocumentIdentity_ResolveBinaryUri(
        SZrState *state, SZrLspContext *context, SZrLspProjectIndex *projectIndex,
        SZrString *moduleName, SZrString **outUri);
/**
 * @brief 校验项目、代数及物理来源后借出原生插件 descriptor。
 * @note 此入口只接受原生 descriptor 插件，不用于读取二进制 .zro 的文本。
 */
TZrBool ZrLanguageServer_LspVirtualDocumentIdentity_ResolveNativeDescriptor(
        SZrState *state, SZrLspContext *context, SZrString *uri,
        SZrLspVirtualDocumentIdentity *outIdentity, SZrLspProjectIndex **outProject,
        const ZrLibModuleDescriptor **outDescriptor);
/** @brief 将 URI 绑定回当前 provider 代数下的项目索引；格式、代数或项目失效时返回空。 */
SZrLspProjectIndex *ZrLanguageServer_LspVirtualDocumentIdentity_FindProject(
        SZrLspContext *context, SZrString *uri);

#endif
