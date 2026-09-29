#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SYNTAX_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SYNTAX_H

#include "zr_vm_parser/type_inference.h"

/**
 * @brief 取出表达式语句中的表达式，其余语句原样返回，便于循环赋值扫描统一处理 AST。
 * @param statement AST 语句；允许为 NULL。
 * @return 输入语句或其内部表达式；输入或表达式语句的子表达式为空时返回 NULL。返回值借用 AST。
 */
SZrAstNode *ZrParser_TypeInferenceLoopAssignment_StatementExpression(SZrAstNode *statement);

/**
 * @brief 判断节点是否为不带值的 break 语句。
 * @param statement 待判定 AST 节点。
 * @return 仅当节点是 break 且没有值表达式时为 true；continue 与带值 break 均为 false。
 */
TZrBool ZrParser_TypeInferenceLoopAssignment_StatementIsPlainBreak(SZrAstNode *statement);

/**
 * @brief 按 AST 结构启发式识别无值 break；结果仅作 step 优化线索，不是通用控制流证明。
 * @param statement 待分析语句或块。
 * @return 命中直接 break、块内可识别 break 或双分支 break if 的结构模式时为 true；否则为 false。该结果仅用于选择是否尝试 step 优化，实际 join/提交须等后续计划收集成功。
 */
TZrBool ZrParser_TypeInferenceLoopAssignment_StatementGuaranteesPlainBreak(SZrAstNode *statement);

/**
 * @brief 拆出简单标识符赋值的目标名和右值，供循环赋值形状筛选及重放共用。
 * @param assignmentNode 候选赋值 AST 节点。
 * @param[out] outName 成功时指向 AST 借用的标识符名；调用方不得释放或延长其生命周期。
 * @param[out] outRight 成功时指向 AST 借用的右值节点；调用方不得释放或延长其生命周期。
 * @return 仅接受运算符为 `=`、左侧为具名标识符且右值非空的节点；失败时非空输出均置为 NULL。
 */
TZrBool ZrParser_TypeInferenceLoopAssignment_AssignmentParts(SZrAstNode *assignmentNode,
                                                             SZrString **outName,
                                                             SZrAstNode **outRight);

#endif
