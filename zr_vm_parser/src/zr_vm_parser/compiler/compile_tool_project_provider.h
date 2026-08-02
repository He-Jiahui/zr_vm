#ifndef ZR_VM_PARSER_COMPILE_TOOL_PROJECT_PROVIDER_H
#define ZR_VM_PARSER_COMPILE_TOOL_PROJECT_PROVIDER_H

#include "compiler_internal.h"

typedef struct SZrCompileToolProjectProvider SZrCompileToolProjectProvider;

TZrBool ZrParser_CompileToolProjectProvider_Declare(
        SZrCompilerState *cs,
        SZrString *aliasName,
        const TZrChar *rawSpecifier,
        SZrFileRange location);
void ZrParser_CompileToolProjectProvider_FreeAll(SZrCompilerState *cs);

#endif
