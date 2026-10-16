#ifndef ZR_VM_PARSER_COMPILER_EXTERN_DECORATOR_DIAGNOSTICS_H
#define ZR_VM_PARSER_COMPILER_EXTERN_DECORATOR_DIAGNOSTICS_H

#include "zr_vm_parser/compiler.h"

/**
 * @brief 将 extern/FFI 装饰器规则失败统一发布为 `invalid_decorator` 编译诊断。
 * @pre cs、cs->state、decoratorNode 及 message/cause/suggestion 均有效；调用方应把该节点定位为当前失败装饰器。
 * @return 始终返回 false，供装饰器校验器立即停止并向声明编译阶段传播失败。
 * @note 成功构造时错误位置取 decoratorNode->location，并交由 compiler state 持有结构化诊断；构造失败会退回普通编译错误，仍然拒绝声明。
 */
TZrBool compiler_extern_report_invalid_decorator(
        SZrCompilerState *cs,
        SZrAstNode *decoratorNode,
        const TZrChar *message,
        const TZrChar *cause,
        const TZrChar *suggestion);

#endif
