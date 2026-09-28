#ifndef ZR_VM_LANGUAGE_SERVER_LSP_PROPERTY_CODE_ACTIONS_H
#define ZR_VM_LANGUAGE_SERVER_LSP_PROPERTY_CODE_ACTIONS_H

#include "interface/lsp_interface_internal.h"

/** @brief 在请求位置的 property 契约上追加安全的 refactor.rewrite 动作。
 * @note 只借用当前分析器与 content；追加结果由 GetCodeActions 的调用方统一释放。
 *       无唯一 owner、无适用改写或无语义快照时成功返回且不追加动作。
 */
TZrBool ZrLanguageServer_LspPropertyCodeActions_Append(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        const TZrChar *content,
        TZrSize contentLength,
        SZrLspRange requestedRange,
        SZrArray *result);

#endif
