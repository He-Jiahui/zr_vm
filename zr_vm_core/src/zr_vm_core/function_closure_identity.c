#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"

static void function_clear_closure_capture_identity_outputs(
        const SZrFunctionTypedTypeRef **outType,
        TZrUInt32 *outSymbolId,
        TZrUInt32 *outTypeId,
        SZrFunctionSourceRange *outDeclarationRange) {
    if (outType != ZR_NULL) {
        *outType = ZR_NULL;
    }
    if (outSymbolId != ZR_NULL) {
        *outSymbolId = 0u;
    }
    if (outTypeId != ZR_NULL) {
        *outTypeId = 0u;
    }
    if (outDeclarationRange != ZR_NULL) {
        ZrCore_Memory_RawSet(outDeclarationRange, 0, sizeof(*outDeclarationRange));
    }
}

TZrBool ZrCore_Function_GetClosureCaptureIdentity(
        const SZrFunction *function,
        TZrUInt32 captureIndex,
        const SZrFunctionTypedTypeRef **outType,
        TZrUInt32 *outSymbolId,
        TZrUInt32 *outTypeId,
        SZrFunctionSourceRange *outDeclarationRange) {
    const SZrFunctionTypedClosureBinding *match = ZR_NULL;

    function_clear_closure_capture_identity_outputs(outType,
                                                    outSymbolId,
                                                    outTypeId,
                                                    outDeclarationRange);
    if (function == ZR_NULL ||
        function->closureValueList == ZR_NULL ||
        captureIndex >= function->closureValueLength ||
        function->typedClosureBindings == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->typedClosureBindingLength; index++) {
        const SZrFunctionTypedClosureBinding *candidate = &function->typedClosureBindings[index];

        if (candidate->captureIndex != captureIndex) {
            continue;
        }
        if (match != ZR_NULL) {
            function_clear_closure_capture_identity_outputs(outType,
                                                            outSymbolId,
                                                            outTypeId,
                                                            outDeclarationRange);
            return ZR_FALSE;
        }
        match = candidate;
    }

    if (match == ZR_NULL ||
        match->symbolId == 0u ||
        match->typeId == 0u ||
        match->declarationEndLine < match->declarationStartLine) {
        return ZR_FALSE;
    }
    {
        const SZrFunctionClosureVariable *legacyCapture = &function->closureValueList[captureIndex];

        if (legacyCapture->symbolId != 0u &&
            (legacyCapture->symbolId != match->symbolId ||
             legacyCapture->typeId != match->typeId ||
             legacyCapture->declarationStartLine != match->declarationStartLine ||
             legacyCapture->declarationStartColumn != match->declarationStartColumn ||
             legacyCapture->declarationEndLine != match->declarationEndLine ||
             legacyCapture->declarationEndColumn != match->declarationEndColumn)) {
            return ZR_FALSE;
        }
    }

    if (outType != ZR_NULL) {
        *outType = &match->type;
    }
    if (outSymbolId != ZR_NULL) {
        *outSymbolId = match->symbolId;
    }
    if (outTypeId != ZR_NULL) {
        *outTypeId = match->typeId;
    }
    if (outDeclarationRange != ZR_NULL) {
        outDeclarationRange->startLine = match->declarationStartLine;
        outDeclarationRange->startColumn = match->declarationStartColumn;
        outDeclarationRange->endLine = match->declarationEndLine;
        outDeclarationRange->endColumn = match->declarationEndColumn;
    }
    return ZR_TRUE;
}
