#include "compile_time_executor_internal.h"

static TZrBool compile_time_execute_late_check_node(
        SZrCompilerState *cs,
        SZrAstNode *node);

/* 脚本与块共用顺序遍历，第一条失败即停止，避免失败后仍执行后续编译期副作用。 */
static TZrBool compile_time_execute_late_check_array(
        SZrCompilerState *cs,
        SZrAstNodeArray *nodes) {
    for (TZrSize index = 0; nodes != ZR_NULL && index < nodes->count; index++) {
        if (!compile_time_execute_late_check_node(cs, nodes->nodes[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* 只进入已选中的编译期声明；普通运行时代码和 build-facts 已登记的函数不在收尾阶段重执行。 */
static TZrBool compile_time_execute_late_check_node(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrCompileTimeDeclaration *declaration;
    SZrAstNode *selectedBranch;

    if (cs == ZR_NULL || node == ZR_NULL) {
        return cs != ZR_NULL;
    }
    if (node->type == ZR_AST_SCRIPT) {
        return compile_time_execute_late_check_array(
                cs, node->data.script.statements);
    }
    if (node->type == ZR_AST_BLOCK) {
        return compile_time_execute_late_check_array(
                cs, node->data.block.body);
    }
    if (node->type != ZR_AST_COMPILE_TIME_DECLARATION) {
        return ZR_TRUE;
    }

    declaration = &node->data.compileTimeDeclaration;
    /* build facts 阶段已经判定条件并保存分支；这里不能重新求值纯值谓词。 */
    if (declaration->isConditionalPruning) {
        selectedBranch = declaration->selectedBranch;
        return selectedBranch == ZR_NULL ||
               compile_time_execute_late_check_node(cs, selectedBranch);
    }
    if (declaration->declarationType == ZR_COMPILE_TIME_FUNCTION) {
        return ZR_TRUE;
    }
    return ZrParser_CompileTimeDeclaration_Execute(cs, node) &&
           !cs->hasCompileTimeError && !cs->hasError && !cs->hasFatalError;
}

/* 编译器在布局完成后切换到 LATE_CHECK，再让诊断指令读取最终的类型与布局事实。 */
TZrBool ZrParser_CompileTime_ExecuteLateChecksInCompilerState(
        SZrCompilerState *cs,
        SZrAstNode *ast) {
    if (cs == ZR_NULL || ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        cs->compilePhase != ZR_PARSER_COMPILE_PHASE_LATE_CHECK) {
        return ZR_FALSE;
    }
    return compile_time_execute_late_check_node(cs, ast);
}
