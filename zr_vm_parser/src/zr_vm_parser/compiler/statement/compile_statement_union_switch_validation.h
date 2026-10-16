#ifndef ZR_VM_PARSER_COMPILE_STATEMENT_UNION_SWITCH_VALIDATION_H
#define ZR_VM_PARSER_COMPILE_STATEMENT_UNION_SWITCH_VALIDATION_H

#include "zr_vm_parser/compiler.h"

/**
 * @brief 在 union switch 各 case 分支降低前，拒绝重复覆盖同一 variant 的 case。
 * @pre unionDeclaration 和 switchUnionTypeName 必须对应已推断的 switch subject；subject 表达式值可已完成 lowering。
 * @note 本函数只报告错误到 cs->hasError，不回滚 subject 指令；调用方应在错误后停止 case 比较、payload 绑定和分支编译。入参 AST 与名称均为借用引用，不会被保留。
 */
void compile_switch_validate_union_duplicate_cases(SZrCompilerState *cs,
                                                   SZrSwitchExpression *switchExpression,
                                                   SZrAstNode *unionDeclaration,
                                                   SZrString *switchUnionTypeName);

/**
 * @brief 计算 union switch 的显式 variant 覆盖状态，供调用方写入 AST 并让 CFG 判断 default 可达性。
 * @pre 同一 switch 的重复覆盖校验已通过；unionDeclaration 与 switchUnionTypeName 对应 subject 类型。
 * @return 所有有效声明 variant 均有匹配 case 时为 true；遗漏 variant 时始终为 false。遗漏 variant 且没有 default 时会向 cs 报非穷尽错误；遗漏 variant 但存在 default 时保留运行时兜底，不把 AST 标为穷尽。
 * @note location 仅用于非穷尽诊断；入参 AST 与名称均为借用引用，本函数不保留它们。
 */
TZrBool compile_switch_validate_union_exhaustiveness(SZrCompilerState *cs,
                                                     SZrSwitchExpression *switchExpression,
                                                     SZrAstNode *unionDeclaration,
                                                     SZrString *switchUnionTypeName,
                                                     SZrFileRange location);

#endif
