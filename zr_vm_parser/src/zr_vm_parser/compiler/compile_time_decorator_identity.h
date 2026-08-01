#ifndef ZR_VM_PARSER_COMPILE_TIME_DECORATOR_IDENTITY_H
#define ZR_VM_PARSER_COMPILE_TIME_DECORATOR_IDENTITY_H

#include "compiler_internal.h"

TZrBool ZrParser_CompileTime_EnsureDecoratorSnapshotSymbol(
        SZrCompilerState *cs,
        SZrObject *snapshot,
        SZrAstNode *declarationNode,
        SZrString *name,
        EZrSemanticSymbolKind kind,
        TZrTypeId typeId,
        TZrSymbolId *outSymbolId);

TZrBool ZrParser_CompileTime_ValidateLeafDeclarationPatch(
        SZrCompilerState *cs,
        const SZrTypeValue *targetSnapshot,
        const SZrTypeValue *patchValue,
        SZrFileRange location,
        TZrBool *outIsTypedPatch);

#endif
