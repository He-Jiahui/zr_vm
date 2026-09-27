#ifndef ZR_VM_LANGUAGE_SERVER_LSP_NUMERIC_RANGE_TEXT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_NUMERIC_RANGE_TEXT_H

#include "zr_vm_parser/semantic_facts.h"

/**
 * @brief 以一致的数值区间文本服务 hover、补全和签名帮助。
 * @pre used 指向调用方缓冲区的当前写入末尾；fact 来自仍有效的语义快照。
 * @note fact 无区间时成功且不写入；缓冲区不足时返回失败，调用方不应发布半截文本。
 */
TZrBool ZrLanguageServer_LspNumericRangeText_AppendRange(
    TZrChar *buffer,
    TZrSize bufferSize,
    TZrSize *used,
    const SZrSemanticNumericFact *fact,
    TZrBool includeRangeLabel);

#endif
