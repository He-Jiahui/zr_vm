#ifndef ZR_VM_PARSER_COMPILE_TOOL_EXECUTION_SCOPE_H
#define ZR_VM_PARSER_COMPILE_TOOL_EXECUTION_SCOPE_H

#include "compiler/compiler_internal.h"

/* Caller arrays remain untouched while the callee uses independent lexical tables. */
typedef struct SZrCompileToolExecutionScope {
    SZrArray previousBindings;
    SZrArray previousModuleAliases;
    SZrImportedCompileTimeModule *previousModule;
    TZrBool entered;
} SZrCompileToolExecutionScope;

/* Pure shadow declarations shared with BuildFacts; neither helper executes ASTs. */
TZrBool ZrParser_CompileToolExecutionScope_DeclareFunctionShadow(
        SZrCompilerState *cs,
        SZrAstNode *node);
TZrBool ZrParser_CompileToolExecutionScope_DeclarePatternShadows(
        SZrCompilerState *cs,
        SZrAstNode *pattern);

/* Install script lexical facts in fresh arrays; failed entry restores the caller. */
TZrBool ZrParser_CompileToolExecutionScope_EnterAst(
        SZrCompilerState *cs,
        SZrAstNode *scriptAst,
        SZrImportedCompileTimeModule *module,
        SZrCompileToolExecutionScope *scope);
/* Root functions must belong to the current compiler or its real provider parents. */
TZrBool ZrParser_CompileToolExecutionScope_EnterFunction(
        SZrCompilerState *cs,
        const SZrCompileTimeFunction *function,
        SZrCompileToolExecutionScope *scope);
/* Free only the scratch arrays and restore both complete caller headers; idempotent. */
void ZrParser_CompileToolExecutionScope_Leave(
        SZrCompilerState *cs,
        SZrCompileToolExecutionScope *scope);

#endif
