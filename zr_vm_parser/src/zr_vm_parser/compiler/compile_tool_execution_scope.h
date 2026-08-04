#ifndef ZR_VM_PARSER_COMPILE_TOOL_EXECUTION_SCOPE_H
#define ZR_VM_PARSER_COMPILE_TOOL_EXECUTION_SCOPE_H

#include "compiler_internal.h"

typedef struct SZrCompileToolExecutionScope {
    TZrSize bindingMark;
    TZrSize moduleAliasMark;
    SZrImportedCompileTimeModule *previousModule;
    TZrBool entered;
} SZrCompileToolExecutionScope;

TZrBool ZrParser_CompileToolExecutionScope_EnterAst(
        SZrCompilerState *cs,
        SZrAstNode *scriptAst,
        SZrImportedCompileTimeModule *module,
        SZrCompileToolExecutionScope *scope);
TZrBool ZrParser_CompileToolExecutionScope_EnterFunction(
        SZrCompilerState *cs,
        const SZrCompileTimeFunction *function,
        SZrCompileToolExecutionScope *scope);
void ZrParser_CompileToolExecutionScope_Leave(
        SZrCompilerState *cs,
        SZrCompileToolExecutionScope *scope);

#endif
