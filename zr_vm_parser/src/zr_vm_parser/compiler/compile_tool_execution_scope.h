#ifndef ZR_VM_PARSER_COMPILE_TOOL_EXECUTION_SCOPE_H
#define ZR_VM_PARSER_COMPILE_TOOL_EXECUTION_SCOPE_H

#include "compiler_internal.h"

/**
 * @brief 保存一次 compile-time 执行所需回滚的导入可见性边界。
 * @note scope 由 EnterAst/EnterFunction 初始化；mark 将嵌套调用新增的绑定与别名限制在本次执行期内，统一清理可重复调用 Leave。
 */
typedef struct SZrCompileToolExecutionScope {
    /** Enter 时的 compile-tool binding 栈长度，Leave 用它撤销本层声明。 */
    TZrSize bindingMark;
    /** Enter 时的模块别名数组长度，保留外层别名并移除本层新增项。 */
    TZrSize moduleAliasMark;
    /** 进入前活动的导入模块；Leave 恢复该模块以维持嵌套调用上下文。 */
    SZrImportedCompileTimeModule *previousModule;
    /** 防止失败回滚后或重复 Leave 再次改动编译器状态。 */
    TZrBool entered;
} SZrCompileToolExecutionScope;

/**
 * @brief 为脚本模块建立 compile-time 导入可见范围，并预声明脚本顶层 CompileTool import。
 * @pre cs 已初始化，scope 指向可覆盖的新记录；module 与 scriptAst 属于同一模块且在范围内存活。
 * @note 非脚本 AST 仍建立活动模块范围但不扫描导入；扫描失败时本函数自行 Leave 并回滚本层绑定、别名和活动模块。
 * @return 进入成功返回 true；顶层导入声明失败返回 false，且 scope 已退出。
 */
TZrBool ZrParser_CompileToolExecutionScope_EnterAst(
        SZrCompilerState *cs,
        SZrAstNode *scriptAst,
        SZrImportedCompileTimeModule *module,
        SZrCompileToolExecutionScope *scope);
/**
 * @brief 按编译期函数的 owner module 建立其执行范围，复用模块脚本的导入声明。
 * @pre function 非空时，其 ownerModule 及 AST 在本次调用范围内有效；scope 指向可覆盖的新记录。
 * @note 无 owner module 的函数以空模块范围执行，避免继承其他导入模块的活动上下文。
 * @return 返回 EnterAst 的进入结果；顶层导入失败时，scope 已由内部 Leave 回滚。
 */
TZrBool ZrParser_CompileToolExecutionScope_EnterFunction(
        SZrCompilerState *cs,
        const SZrCompileTimeFunction *function,
        SZrCompileToolExecutionScope *scope);
/**
 * @brief 离开当前 compile-time 导入范围，恢复外层绑定、别名边界与活动模块。
 * @pre scope 是同一 cs 上 EnterAst/EnterFunction 初始化的记录，且被引用模块仍存活。
 *      若 Enter 因导入声明失败，EnterAst 已先行回滚该记录。
 * @note 可重复调用；EnterAst 内部失败回滚后，调用方再执行 Leave 不会重复回滚。
 */
void ZrParser_CompileToolExecutionScope_Leave(
        SZrCompilerState *cs,
        SZrCompileToolExecutionScope *scope);

#endif
