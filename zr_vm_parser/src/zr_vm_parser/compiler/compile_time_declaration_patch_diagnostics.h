#ifndef ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_DIAGNOSTICS_H
#define ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_DIAGNOSTICS_H

#include "compiler_internal.h"

ZR_PARSER_API TZrBool ZrParser_CompileTime_ProcessPatchDiagnostics(
        SZrCompilerState *cs,
        const SZrTypeValue *diagnosticsValue,
        TZrSymbolId patchTargetSymbolId,
        SZrFileRange location,
        TZrBool *hasErrorDiagnostic);

#endif // ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_DIAGNOSTICS_H
