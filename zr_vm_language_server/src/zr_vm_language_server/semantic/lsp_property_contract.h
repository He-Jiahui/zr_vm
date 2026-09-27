#ifndef ZR_VM_LANGUAGE_SERVER_LSP_PROPERTY_CONTRACT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_PROPERTY_CONTRACT_H

#include "zr_vm_language_server/semantic_analyzer.h"
#include "zr_vm_parser/semantic_query.h"

/**
 * @brief 将 parser 的 property 契约映射进 LSP 旧符号表，供索引和导航统一使用。
 * @pre ownerTypeNode、propertyNode 与 analyzer 的 semanticContext 来自同一 AST 快照。
 * @note outSymbol 借用符号表中的对象，调用方不单独释放。
 */
TZrBool ZrLanguageServer_LspPropertyContract_RegisterSourceSymbol(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ownerTypeNode,
        SZrAstNode *propertyNode,
        SZrSymbol **outSymbol);

/** @brief 从源符号重新核对 canonical 类型后，为 hover 展示 property 契约。 */
SZrString *ZrLanguageServer_LspPropertyContract_FormatSignature(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        const SZrSymbol *symbol);

/** @brief 共用源和外部元数据的 property 签名呈现；要求调用方提供已验证类型文本。 */
SZrString *ZrLanguageServer_LspPropertyContract_FormatQuery(
        SZrState *state,
        SZrString *name,
        const TZrChar *typeText,
        const SZrParserSemanticPropertyQuery *query);

/** @brief 按 parser property 身份找回 LSP 符号；返回指针仍归 analyzer 符号表所有。 */
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrLanguageServer_LspPropertyContract_FindSourceSymbolAt(
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position);

#endif // ZR_VM_LANGUAGE_SERVER_LSP_PROPERTY_CONTRACT_H
