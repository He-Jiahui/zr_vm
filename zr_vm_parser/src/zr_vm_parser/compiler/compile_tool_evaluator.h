#ifndef ZR_VM_PARSER_COMPILE_TOOL_EVALUATOR_H
#define ZR_VM_PARSER_COMPILE_TOOL_EVALUATOR_H

#include "compile_time_executor_internal.h"

/** @brief 在通用编译期求值前识别 CompileTool 的 typed 构造、声明枚举、项目特性查询和诊断调用。
 *  @pre cs、node、result、handled 有效；node 是求值器传入的 primary 或 struct-init 表达式。
 *  @note *handled=false 表示未识别，调用方继续普通求值；只有返回 true 且 *handled=true 时 result 才应视作本接口产出的值。
 *  @return 识别成功或未识别时返回 true；应阻断求值的诊断路径返回 false。
 *  BUG: typed 构造的字段名驻留失败可使下层 helper 返回 false，但此入口仅检查编译器错误标志，可能仍返回 true 且 *handled=true；struct-init 调用方会将未建立的 result 当作求值成功。 */
TZrBool ZrParser_CompileToolEvaluator_TryEvaluate(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompileTimeFrame *frame,
        SZrTypeValue *result,
        TZrBool *handled);

#endif
