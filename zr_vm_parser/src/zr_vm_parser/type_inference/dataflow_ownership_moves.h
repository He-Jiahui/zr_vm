/* 该 guard 使 ownership move 分类器的内部接口在多个翻译单元重复包含时只声明一次。 */
#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_MOVES_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_MOVES_H

#include "zr_vm_parser/semantic_facts.h"

/**
 * @brief 判断当前语句是否包含会消费指定读来源的操作。
 *
 * CFG 所有权分析仅对已纳入追踪的符号调用此分类器。它结合 AST、解析后的
 * 调用事实和形参传递模式识别初始化/赋值、按值实参、调度器消费参数及消费
 * ownership intrinsic；调用方再结合符号 qualifier 决定是否应用 unique 状态转移。
 * 本函数不发布事实，也不修改控制流状态。
 *
 * @pre context、statement 与 fact 引用的数据在本次分类期间保持有效；fact 是当前被追踪符号的读事实。任一参数为空时返回假。
 * @param context 本次语义分析持有的符号、调用及引用事实。
 * @param statement 当前 CFG 节点对应的语句。
 * @param fact 属于当前符号的读引用事实。
 * @return 若已识别出会消费该来源的语法和语义组合则为 ZR_TRUE，否则为 ZR_FALSE。
 */
TZrBool ZrParser_DataflowOwnership_StatementMovesRead(
        const SZrSemanticContext *context,
        SZrAstNode *statement,
        const SZrSemanticReferenceFact *fact);
/**
 * @brief 判断当前语句表达式是否位于要求 weak wake 的访问位置。
 *
 * 分类依赖语义上下文中发布的接收者 guard 和调用形参事实；显式 wake 包含
 * 同一读事实时不会将其报告为待唤醒。所有权数据流仅在该符号 qualifier 为
 * WEAK 时将结果用于违规判断。本接口不执行 wake，也不改变 alias 或 owner 状态。
 *
 * @pre context、statement、fact 及其引用的数据在本次分类期间保持有效；fact 是当前语句中的 weak 读事实。
 * @param context 当前语义上下文，提供 guard、已解析调用和参数模式。
 * @param statement 当前 CFG 节点对应的语句。
 * @param fact 当前需要判断的读引用事实。
 * @return 若表达式是无 guard 的 weak 接收者访问或位于借用/引用实参中则为 ZR_TRUE。
 */
TZrBool ZrParser_DataflowOwnership_StatementWeakReadRequiresWake(
        const SZrSemanticContext *context,
        SZrAstNode *statement,
        const SZrSemanticReferenceFact *fact);

#endif // ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_MOVES_H
