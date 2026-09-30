#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_STATEMENTS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_STATEMENTS_H

#include "zr_vm_parser/semantic_facts.h"

/**
 * @brief 判断语义引用事实是否归属于当前 ownership CFG 语句或清理块的转移。
 * @pre statement 与 fact 来自同一次语义分析；所引用的 AST 节点和 source 字符串在调用期间有效。参数均为借用，不转移所有权。
 * @return 事实节点与所选 AST 节点相同，或事实范围落在其同源范围内时返回 TRUE；空参数或不能归属时返回 FALSE。
 * @note 复合语句的分支体、case/default 体由独立 CFG 块处理；调用方应传入当前块保存的节点。
 */
TZrBool ZrParser_DataflowOwnership_FactInStatement(
        SZrAstNode *statement,
        const SZrSemanticReferenceFact *fact);

#endif
