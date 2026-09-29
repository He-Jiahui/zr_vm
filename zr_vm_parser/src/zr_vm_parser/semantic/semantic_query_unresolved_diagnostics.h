#ifndef ZR_VM_PARSER_SEMANTIC_QUERY_UNRESOLVED_DIAGNOSTICS_H
#define ZR_VM_PARSER_SEMANTIC_QUERY_UNRESOLVED_DIAGNOSTICS_H

#include "zr_vm_parser/semantic_query.h"

/**
 * @brief 将未解析引用事实投影为语言服务/编译器共用的结构化查询诊断。
 * @pre context 的 state 与 queryDiagnostics 已由语义上下文初始化；fact 来自其 referenceFacts。
 * @note 只处理真实 unresolved 的可报告引用；同位置 resolved fact、canonical 外部目标和合成引用会被忽略。
 * @return 是否产生并追加了一条诊断；false 也表示该 fact 不属于此投影器。
 */
TZrBool ZrParser_SemanticQueryUnresolved_AppendDiagnostic(
        SZrSemanticContext *context,
        const SZrSemanticReferenceFact *fact);

#endif
