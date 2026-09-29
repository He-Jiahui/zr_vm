#ifndef ZR_VM_PARSER_SEMANTIC_QUERY_OWNERSHIP_DIAGNOSTICS_H
#define ZR_VM_PARSER_SEMANTIC_QUERY_OWNERSHIP_DIAGNOSTICS_H

#include "zr_vm_parser/semantic_query.h"

/**
 * @brief 将可定位的 ownership 错误 fact 转为查询诊断。
 * @pre context 由 semantic query 初始化，fact 属于其 ownershipFacts 且位置落在请求 scope 内。
 * @note 当前仅投影 move-after-use、borrow/loan escape、weak-after-release；其他错误由各自检查路径报告。
 */
TZrBool ZrParser_SemanticQueryOwnership_AppendDiagnostic(
        SZrSemanticContext *context,
        const SZrSemanticOwnershipFact *fact);

#endif
