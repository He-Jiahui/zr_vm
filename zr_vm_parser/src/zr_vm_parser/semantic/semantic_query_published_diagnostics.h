#ifndef ZR_VM_PARSER_SEMANTIC_QUERY_PUBLISHED_DIAGNOSTICS_H
#define ZR_VM_PARSER_SEMANTIC_QUERY_PUBLISHED_DIAGNOSTICS_H

#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"

/**
 * @brief 复制已发布诊断事实供当前查询 scope 返回。
 * @pre fact 的 diagnostic 由 semantic facts 持有，context 的 state 与查询诊断数组已初始化。
 * @note 成功后查询结果拥有副本；重复投影由 materializer 的重建生命周期负责，不在此处去重。
 */
TZrBool ZrParser_SemanticQueryPublished_AppendDiagnostic(
        SZrSemanticContext *context,
        const SZrSemanticDiagnosticFact *fact);

#endif
