#ifndef ZR_VM_PARSER_COMPILE_TIME_BINDING_METADATA_H
#define ZR_VM_PARSER_COMPILE_TIME_BINDING_METADATA_H

#include "compiler_internal.h"

typedef struct SZrCompileTimeBindingSourceVariable {
    SZrString *name;
    SZrAstNode *value;
    SZrFunctionCompileTimeVariableInfo *info;
    TZrBool isResolving;
    TZrBool isResolved;
} SZrCompileTimeBindingSourceVariable;

typedef struct SZrCompileTimeBindingResolver {
    SZrState *state;
    TZrPtr userData;
    SZrCompileTimeBindingSourceVariable *(*findVariable)(TZrPtr userData, SZrString *name);
    SZrCompileTimeFunction *(*findFunction)(TZrPtr userData, SZrString *name);
    SZrCompileTimeDecoratorClass *(*findDecoratorClass)(TZrPtr userData, SZrString *name);
} SZrCompileTimeBindingResolver;

TZrBool ZrParser_CompileTimeBinding_ResolveAll(SZrCompileTimeBindingResolver *resolver,
                                               SZrCompileTimeBindingSourceVariable *variables,
                                               TZrSize variableCount);

TZrBool ZrParser_CompileTimeBinding_BuildStaticMemberPath(SZrState *state,
                                                          const SZrAstNodeArray *members,
                                                          TZrSize startIndex,
                                                          TZrSize endIndex,
                                                          SZrString **outPath);

const SZrFunctionCompileTimePathBinding *ZrParser_CompileTimeBinding_FindPath(
        const SZrFunctionCompileTimeVariableInfo *info,
        SZrString *path);

#endif
