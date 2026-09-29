#include "compile_tool_execution_scope.h"

#include "compile_time_executor_internal.h"
#include "compile_tool_binding.h"

/**
 * @brief 建立脚本级 CompileTool 导入上下文，并让声明后的执行路径共享这些绑定。
 * @note 先保存外层 marks 和活动模块，再安装本模块；导入预声明失败统一经 Leave 回滚，供所有调用者使用同一失败出口。
 */
TZrBool ZrParser_CompileToolExecutionScope_EnterAst(
        SZrCompilerState *cs,
        SZrAstNode *scriptAst,
        SZrImportedCompileTimeModule *module,
        SZrCompileToolExecutionScope *scope) {
    SZrAstNodeArray *statements;

    if (cs == ZR_NULL || scope == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(scope, 0, sizeof(*scope));
    scope->bindingMark = ZrParser_CompileToolBinding_Mark(cs);
    scope->moduleAliasMark = cs->importedCompileTimeModuleAliases.length;
    scope->previousModule = cs->activeImportedCompileTimeModule;
    cs->activeImportedCompileTimeModule = module;
    scope->entered = ZR_TRUE;

    /* 函数体仅在确有模块脚本时扫描顶层导入；无脚本时仍需切换/恢复活动模块。 */
    if (scriptAst == ZR_NULL || scriptAst->type != ZR_AST_SCRIPT) {
        return ZR_TRUE;
    }
    statements = scriptAst->data.script.statements;
    for (TZrSize index = 0U;
         statements != ZR_NULL && index < statements->count;
         index++) {
        SZrAstNode *statement = statements->nodes[index];
        if (!compiler_is_compile_tool_import_declaration(
                    cs->state, statement)) {
            continue;
        }
        if (!ZrParser_CompileToolExecution_DeclareImport(cs, statement)) {
            ZrParser_CompileToolExecutionScope_Leave(cs, scope);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/** @brief 从函数所属模块派生导入范围，使跨模块调用按函数所有者解析 CompileTool 名称。 */
TZrBool ZrParser_CompileToolExecutionScope_EnterFunction(
        SZrCompilerState *cs,
        const SZrCompileTimeFunction *function,
        SZrCompileToolExecutionScope *scope) {
    SZrImportedCompileTimeModule *module =
            function != ZR_NULL ? function->ownerModule : ZR_NULL;
    return ZrParser_CompileToolExecutionScope_EnterAst(
            cs,
            module != ZR_NULL ? module->scriptAst : ZR_NULL,
            module,
            scope);
}

/**
 * @brief 关闭嵌套导入范围；撤销本层声明后恢复外层活动模块。
 * @note 先恢复绑定与别名可见长度，再切回外层模块，避免后续执行观察到已退出范围的名称。
 */
void ZrParser_CompileToolExecutionScope_Leave(
        SZrCompilerState *cs,
        SZrCompileToolExecutionScope *scope) {
    if (cs == ZR_NULL || scope == ZR_NULL || !scope->entered) {
        return;
    }
    ZrParser_CompileToolBinding_Restore(cs, scope->bindingMark);
    if (scope->moduleAliasMark <= cs->importedCompileTimeModuleAliases.length) {
        cs->importedCompileTimeModuleAliases.length = scope->moduleAliasMark;
    }
    cs->activeImportedCompileTimeModule = scope->previousModule;
    scope->entered = ZR_FALSE;
}
