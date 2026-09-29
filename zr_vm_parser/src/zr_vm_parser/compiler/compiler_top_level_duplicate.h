#ifndef ZR_VM_PARSER_COMPILER_TOP_LEVEL_DUPLICATE_H
#define ZR_VM_PARSER_COMPILER_TOP_LEVEL_DUPLICATE_H

#include "zr_vm_parser/compiler.h"

/**
 * @brief 检查新顶层类型声明是否与已登记的其他 prototype 同名。
 * @pre cs 属于当前编译单元；declaration 可为任意待处理的顶层 AST，非类型声明由本接口跳过。
 * @note 编译器在 interface signature 与后续顶层编译两个阶段调用；同一 AST 已登记只表示跨阶段复用。
 * @return 发现不同 AST 的同名声明并请求诊断时返回 true，调用者应跳过声明；非类型或无冲突返回 false。
 */
TZrBool compiler_report_duplicate_top_level_type(
        SZrCompilerState *cs,
        SZrAstNode *declaration);

#endif // ZR_VM_PARSER_COMPILER_TOP_LEVEL_DUPLICATE_H
