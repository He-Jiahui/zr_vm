#ifndef ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_SYMBOL_DISPLAY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_SYMBOL_DISPLAY_H

#include "zr_vm_language_server/semantic_analyzer.h"
#include "zr_vm_parser/semantic_query.h"

/**
 * @brief 为项目补全、悬停和外部元数据展示已解析声明的规范类型。
 * @pre symbol 必须仍由 analyzer 对应的语义上下文持有；buffer 可写且非空。
 * @return 仅在声明事实与 symbol 的语义 ID、类型 ID 一致且格式化成功时返回真。
 */
TZrBool ZrLanguageServer_Lsp_FormatSymbolCanonicalDeclarationType(
        SZrSemanticAnalyzer *analyzer,
        SZrSymbol *symbol,
        TZrChar *buffer,
        TZrSize bufferSize);

/** 为 inlay hint 展示精确声明类型，防止仅凭 AST 位置推测不完整类型。 */
TZrBool ZrLanguageServer_Lsp_FormatCanonicalDeclarationType(
        SZrSemanticAnalyzer *analyzer,
        const SZrParserSemanticSymbolQuery *declaration,
        TZrChar *buffer,
        TZrSize bufferSize);

/** 仅为已标记 exact 的表达式事实输出类型，供接收者展示与解析使用。 */
TZrBool ZrLanguageServer_Lsp_FormatExactExpressionType(
        SZrSemanticAnalyzer *analyzer,
        const SZrAstNode *expression,
        TZrChar *buffer,
        TZrSize bufferSize);

#endif
