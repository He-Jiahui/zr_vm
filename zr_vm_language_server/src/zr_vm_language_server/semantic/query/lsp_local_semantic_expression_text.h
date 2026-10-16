#ifndef ZR_VM_LANGUAGE_SERVER_LSP_LOCAL_SEMANTIC_EXPRESSION_TEXT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_LOCAL_SEMANTIC_EXPRESSION_TEXT_H

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_parser/semantic_facts.h"

/**
 * @brief 将 parser 的表达式 fact 转成可附加到 hover 的文字片段。
 * @pre used 是 buffer 内已有内容长度；fact 在其语义快照存活期间可读。
 * @note 容量不足返回失败，调用方不应发布部分格式化内容。
 */
TZrBool ZrLanguageServer_LspLocalSemanticExpressionText_AppendHover(
    TZrChar *buffer,
    TZrSize bufferSize,
    TZrSize *used,
    const SZrSemanticExpressionFact *fact);

/**
 * @brief 显示所有权 intrinsic 的源事实，用于解释编辑器中对应操作的契约。
 * @pre fact 如非空，须来自本次 hover 仍有效的语义快照；buffer/used 遵循 AppendHover 的容量约定。
 * @note 未识别的 intrinsic 操作不会生成文字。
 */
TZrBool ZrLanguageServer_LspLocalSemanticExpressionText_AppendOwnershipIntrinsicHover(
    TZrChar *buffer,
    TZrSize bufferSize,
    TZrSize *used,
    const SZrOwnershipIntrinsicFact *fact);

#endif
