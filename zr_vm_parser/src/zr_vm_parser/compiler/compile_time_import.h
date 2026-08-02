#ifndef ZR_VM_PARSER_COMPILE_TIME_IMPORT_H
#define ZR_VM_PARSER_COMPILE_TIME_IMPORT_H

#include "compiler_internal.h"

ZR_PARSER_API SZrImportedCompileTimeModule *
ZrParser_CompileTimeImport_LoadSourceModule(
        SZrCompilerState *cs,
        SZrString *moduleName,
        const TZrByte *sourceBytes,
        TZrSize sourceByteCount,
        TZrBool canonicalizeImports);
ZR_PARSER_API TZrBool ZrParser_CompileTimeImport_RegisterModuleAlias(
        SZrCompilerState *cs,
        SZrString *aliasName,
        SZrImportedCompileTimeModule *module,
        SZrFileRange location,
        TZrBool exposeUnqualifiedFunctions);

#endif
