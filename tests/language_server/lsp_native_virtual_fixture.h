#ifndef ZR_TEST_LSP_NATIVE_VIRTUAL_FIXTURE_H
#define ZR_TEST_LSP_NATIVE_VIRTUAL_FIXTURE_H

#include "../../zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_virtual_document_identity.h"

/** @brief 按打开的项目和 provider 代次重建原生插件声明的预期虚拟 URI。
 *  @pre context、mainUri、originUri 有效，mainUri 已归属已索引项目；
 *       调用方随后用返回值比对定义/引用的项目作用域目标。
 *  @return 项目缺失或 URI 创建失败时为空；结果字符串由 VM GC 管理。 */
static SZrString *test_native_virtual_document_uri(SZrState *state, SZrLspContext *context,
                                                  SZrString *mainUri, SZrString *originUri) {
    SZrLspProjectIndex *project = test_find_project_for_uri(context, mainUri);
    SZrLspVirtualDocumentIdentity identity = {0};
    if (project == ZR_NULL) return ZR_NULL;
    identity.moduleName = ZrCore_String_Create(state, "zr.pluginprobe", 14U);
    identity.projectUri = project->projectFileUri;
    identity.originUri = originUri;
    /* 代次属于当前快照；旧注册表中同名插件不得污染新项目的定位结果。 */
    identity.providerGeneration = context->semanticSnapshotProviderGeneration;
    return ZrLanguageServer_LspVirtualDocumentIdentity_Create(state, &identity);
}

#endif
