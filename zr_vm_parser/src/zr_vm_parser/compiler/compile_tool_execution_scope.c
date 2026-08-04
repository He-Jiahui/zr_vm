#include "compile_tool_execution_scope.h"

#include "compile_time_executor_internal.h"
#include "compile_tool_binding.h"

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
