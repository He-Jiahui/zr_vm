#ifndef ZR_VM_LANGUAGE_SERVER_LSP_DIAGNOSTIC_STORE_H
#define ZR_VM_LANGUAGE_SERVER_LSP_DIAGNOSTIC_STORE_H

#include "zr_vm_language_server/lsp_interface.h"

/**
 * @brief 各传输层分配诊断结果 ID 缓冲区时共用的容量。
 * @note BuildResultId 会检查实际写入长度；使用更短的缓冲区可能返回失败。
 */
#define ZR_LSP_DIAGNOSTIC_RESULT_ID_MAX 128U

/**
 * @brief 根据语义快照身份及结构化诊断内容构造推送／拉取共用的结果 ID。
 * @pre diagnostics 是有效的 SZrLspDiagnostic* 数组，state、context 和 uri 属于同一会话。
 * @return 成功写入完整且以零结尾的 ID 时为真；上下文、文档身份或缓冲区不足时为假。
 * @note 只借用 diagnostics；调用方仍负责释放诊断及数组。stdio 推送去重和拉取 unchanged
 *       比较，以及 WASM 诊断报告，都依赖此 ID 指向相同的文档与诊断快照。
 * TODO: 实现将诊断及其子项排序后再计算哈希，而 stdio 序列化保留原顺序；
 *       需核对客户端是否将 items 顺序视为结果内容，并以重排诊断的 previousResultId 测试确认。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspDiagnosticStore_BuildResultId(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        const SZrArray *diagnostics,
        TZrChar *buffer,
        TZrSize bufferLength);

#endif // ZR_VM_LANGUAGE_SERVER_LSP_DIAGNOSTIC_STORE_H
