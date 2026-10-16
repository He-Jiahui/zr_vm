#ifndef ZR_VM_PARSER_COMPILER_EXTERN_PARAMETER_DECORATORS_H
#define ZR_VM_PARSER_COMPILER_EXTERN_PARAMETER_DECORATORS_H

#include "zr_vm_parser/compiler.h"

/** @brief 验证一个 extern 参数的完整装饰器数组，供编译器处理普通参数及 variadic 参数。
 *  @pre cs 可报告诊断，decorators 为该参数自己的数组或 NULL。
 *  @return 成功表示数组内方向互斥且每条指令形态有效。 */
TZrBool compiler_extern_validate_parameter_decorator_array(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators);

#endif
