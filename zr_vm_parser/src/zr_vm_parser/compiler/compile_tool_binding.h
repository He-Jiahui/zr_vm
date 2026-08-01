#ifndef ZR_VM_PARSER_COMPILE_TOOL_BINDING_H
#define ZR_VM_PARSER_COMPILE_TOOL_BINDING_H

#include "compiler_internal.h"

ZR_PARSER_API void ZrParser_CompileToolBinding_Reset(SZrCompilerState *cs);
TZrSize ZrParser_CompileToolBinding_Mark(const SZrCompilerState *cs);
void ZrParser_CompileToolBinding_Restore(SZrCompilerState *cs, TZrSize mark);
ZR_PARSER_API TZrBool ZrParser_CompileToolBinding_DeclareProvider(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider);
ZR_PARSER_API TZrBool ZrParser_CompileToolBinding_DeclareProviderWithContentHash(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider,
        const TZrChar *providerContentHash);
ZR_PARSER_API TZrBool ZrParser_CompileToolBinding_DeclareResolvedProvider(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider,
        const SZrParserCompileToolResolvedArtifact *resolvedArtifact);
TZrBool ZrParser_CompileToolBinding_DeclareShadow(SZrCompilerState *cs, SZrString *name);
const SZrCompileToolBinding *ZrParser_CompileToolBinding_Resolve(
        const SZrCompilerState *cs,
        SZrString *name);

#endif
