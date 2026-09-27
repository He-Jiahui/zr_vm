#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_TOKEN_CANONICAL_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_TOKEN_CANONICAL_H

#include "zr_vm_language_server/semantic_analyzer.h"
#include "zr_vm_parser/semantic_query.h"

/* 负值只在分类阶段表示缺乏可投影的精确事实，不进入 LSP 的无符号五元组。 */
#define ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN ((TZrInt32)-1)
/* 与 stdio 公布的首个 token modifier 位保持一致。 */
#define ZR_LSP_SEMANTIC_TOKEN_MODIFIER_DECLARATION ((TZrUInt32)1U)

/** @brief 稳定的内部类型序号；顺序须与公开 legend 的名称数组完全一致。 */
typedef enum EZrLspSemanticTokenType {
    ZR_LSP_SEMANTIC_TOKEN_NAMESPACE = 0,
    ZR_LSP_SEMANTIC_TOKEN_CLASS = 1,
    ZR_LSP_SEMANTIC_TOKEN_STRUCT = 2,
    ZR_LSP_SEMANTIC_TOKEN_INTERFACE = 3,
    ZR_LSP_SEMANTIC_TOKEN_ENUM = 4,
    ZR_LSP_SEMANTIC_TOKEN_FUNCTION = 5,
    ZR_LSP_SEMANTIC_TOKEN_METHOD = 6,
    ZR_LSP_SEMANTIC_TOKEN_PROPERTY = 7,
    ZR_LSP_SEMANTIC_TOKEN_VARIABLE = 8,
    ZR_LSP_SEMANTIC_TOKEN_PARAMETER = 9,
    ZR_LSP_SEMANTIC_TOKEN_KEYWORD = 10,
    ZR_LSP_SEMANTIC_TOKEN_DECORATOR = 11,
    ZR_LSP_SEMANTIC_TOKEN_META_METHOD = 12
} EZrLspSemanticTokenType;

/**
 * @brief 将 parser 的已解析符号投影为客户端可理解的 token 类型。
 * @note 声明节点保留 class/struct/interface 等区别；外部目标只在身份元数据完整时参与分类。
 * @return 类型序号，或表示不可确定的 ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN。
 */
TZrInt32 ZrLanguageServer_LspSemanticToken_TypeFromCanonicalSymbol(
        const SZrParserSemanticSymbolQuery *symbol);

/**
 * @brief 为源码扫描出的标识符查询精确语义身份，避免按名字猜测成员类型。
 * @pre startOffset/length 必须对应当前 analyzer 快照及 uri 的同一份文档内容。
 * @note outModifiers 可为 NULL；当前仅导出 declaration 位，查询失败时清零。
 */
TZrInt32 ZrLanguageServer_LspSemanticToken_ResolveCanonical(
        SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        TZrSize startOffset,
        TZrSize length,
        TZrUInt32 line,
        TZrUInt32 character,
        TZrUInt32 *outModifiers);

#endif
