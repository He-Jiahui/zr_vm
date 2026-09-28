#ifndef ZR_VM_LANGUAGE_SERVER_LSP_WORKSPACE_EDIT_SNAPSHOT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_WORKSPACE_EDIT_SNAPSHOT_H

#include "zr_vm_language_server/lsp_interface.h"

#ifndef ZR_LSP_WORKSPACE_EDIT_DOCUMENT_SNAPSHOT_DEFINED
#define ZR_LSP_WORKSPACE_EDIT_DOCUMENT_SNAPSHOT_DEFINED
/** 工作区编辑生成时的文档一致性令牌；URI 为借用引用。
 *  打开文档还需版本与内容代数；磁盘文档靠内容摘要；语义身份阻止依赖变化后复用旧编辑。 */
typedef struct SZrLspWorkspaceEditDocumentSnapshot {
    SZrString *uri;
    TZrUInt64 contentHash;
    TZrSize contentLength;
    TZrSize version;
    TZrSize contentGeneration;
    TZrBool isOpenDocument;
    SZrLspSemanticSnapshotIdentity semanticIdentity;
    TZrBool hasSemanticIdentity;
} SZrLspWorkspaceEditDocumentSnapshot;
#endif

/** 来源文件重命名沿用同一一致性令牌，避免第二套版本校验规则。 */
typedef SZrLspWorkspaceEditDocumentSnapshot
        SZrLspSourceRenameDocumentSnapshot;

/** @brief 在生成 code action 或重命名编辑前记录单个文档的内容与语义身份。
 *  @pre uri 在令牌完成验证和序列化前保持有效；失败时不得使用输出。
 *  @note 关闭文档必须可读磁盘文件，且磁盘内容与已缓存的关闭文档一致。 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_LspWorkspaceEdit_CaptureDocumentSnapshot(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspWorkspaceEditDocumentSnapshot *outDocumentSnapshot);
/** @brief 在发送或解析既有编辑前重新读取目标文档，拒绝内容或依赖已变化的令牌。 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshot(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspWorkspaceEditDocumentSnapshot *documentSnapshot);
/** @brief 将位置列表按 URI 去重并为每个编辑目标捕获令牌。
 *  @pre outDocumentSnapshots 为空；调用方负责释放数组缓冲，失败时也需释放已追加元素。 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_LspWorkspaceEdit_CaptureDocumentSnapshots(
        SZrState *state,
        SZrLspContext *context,
        const SZrArray *locations,
        SZrArray *outDocumentSnapshots);
/** @brief 逐一复核多文档编辑计划；任何一个目标变化就拒绝整组编辑。 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshots(
        SZrState *state,
        SZrLspContext *context,
        const SZrArray *documentSnapshots);
/** @brief 从编辑计划借用指定 URI 的令牌；返回指针随数组扩容或释放失效。 */
ZR_LANGUAGE_SERVER_API const SZrLspWorkspaceEditDocumentSnapshot *
ZrLanguageServer_LspWorkspaceEdit_FindDocumentSnapshot(
        const SZrArray *documentSnapshots,
        SZrString *uri);

#endif
