#include "compiler_typed_export_generics.h"

static void compiler_typed_export_collected_generic_parameters_free(
        SZrState *state,
        SZrArray *genericParameters) {
    if (state == ZR_NULL || genericParameters == ZR_NULL ||
        !genericParameters->isValid || genericParameters->head == ZR_NULL ||
        genericParameters->capacity == 0 || genericParameters->elementSize == 0) {
        return;
    }

    for (TZrSize index = 0; index < genericParameters->length; index++) {
        SZrTypeGenericParameterInfo *parameter =
                (SZrTypeGenericParameterInfo *)ZrCore_Array_Get(genericParameters, index);
        if (parameter != ZR_NULL && parameter->constraintTypeNames.isValid &&
            parameter->constraintTypeNames.head != ZR_NULL &&
            parameter->constraintTypeNames.capacity > 0 &&
            parameter->constraintTypeNames.elementSize > 0) {
            ZrCore_Array_Free(state, &parameter->constraintTypeNames);
        }
    }

    ZrCore_Array_Free(state, genericParameters);
}

void compiler_typed_export_generic_contract_free(
        SZrState *state,
        SZrFunctionTypedExportSymbol *symbol) {
    if (state == ZR_NULL || state->global == ZR_NULL || symbol == ZR_NULL ||
        symbol->genericParameters == ZR_NULL || symbol->genericParameterCount == 0) {
        return;
    }

    for (TZrUInt32 index = 0; index < symbol->genericParameterCount; index++) {
        SZrFunctionTypedGenericParameter *parameter = &symbol->genericParameters[index];
        if (parameter->constraintTypeNames != ZR_NULL && parameter->constraintTypeCount > 0) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          parameter->constraintTypeNames,
                                          sizeof(SZrString *) * parameter->constraintTypeCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
    }

    ZrCore_Memory_RawFreeWithType(state->global,
                                  symbol->genericParameters,
                                  sizeof(SZrFunctionTypedGenericParameter) *
                                          symbol->genericParameterCount,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    symbol->genericParameters = ZR_NULL;
    symbol->genericParameterCount = 0;
}

TZrBool compiler_typed_export_generic_contract_copy_from_infos(
        SZrCompilerState *cs,
        const SZrArray *genericParameters,
        SZrFunctionTypedExportSymbol *outSymbol) {
    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL ||
        outSymbol == ZR_NULL) {
        return ZR_FALSE;
    }
    if (genericParameters == ZR_NULL || !genericParameters->isValid ||
        genericParameters->length == 0) {
        return ZR_TRUE;
    }
    if (genericParameters->length > (TZrSize)0xFFFFFFFFu) {
        return ZR_FALSE;
    }

    outSymbol->genericParameterCount = (TZrUInt32)genericParameters->length;
    outSymbol->genericParameters =
            (SZrFunctionTypedGenericParameter *)ZrCore_Memory_RawMallocWithType(
                    cs->state->global,
                    sizeof(SZrFunctionTypedGenericParameter) * outSymbol->genericParameterCount,
                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (outSymbol->genericParameters == ZR_NULL) {
        outSymbol->genericParameterCount = 0;
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(outSymbol->genericParameters,
                         0,
                         sizeof(SZrFunctionTypedGenericParameter) *
                                 outSymbol->genericParameterCount);

    for (TZrUInt32 index = 0; index < outSymbol->genericParameterCount; index++) {
        const SZrTypeGenericParameterInfo *source =
                (const SZrTypeGenericParameterInfo *)ZrCore_Array_Get(
                        (SZrArray *)genericParameters, index);
        SZrFunctionTypedGenericParameter *destination = &outSymbol->genericParameters[index];

        if (source == ZR_NULL) {
            compiler_typed_export_generic_contract_free(cs->state, outSymbol);
            return ZR_FALSE;
        }

        destination->name = source->name;
        destination->genericKind = (TZrUInt8)source->genericKind;
        destination->variance = (TZrUInt8)source->variance;
        destination->requiresClass = source->requiresClass ? 1u : 0u;
        destination->requiresStruct = source->requiresStruct ? 1u : 0u;
        destination->requiresNew = source->requiresNew ? 1u : 0u;
        destination->requiresOwner = source->requiresOwner ? 1u : 0u;
        destination->requiredOwnershipQualifier = (TZrUInt32)source->requiredOwnershipQualifier;

        if (!source->constraintTypeNames.isValid ||
            source->constraintTypeNames.length == 0) {
            continue;
        }
        if (source->constraintTypeNames.length > (TZrSize)0xFFFFFFFFu) {
            compiler_typed_export_generic_contract_free(cs->state, outSymbol);
            return ZR_FALSE;
        }

        destination->constraintTypeCount = (TZrUInt32)source->constraintTypeNames.length;
        destination->constraintTypeNames =
                (SZrString **)ZrCore_Memory_RawMallocWithType(
                        cs->state->global,
                        sizeof(SZrString *) * destination->constraintTypeCount,
                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (destination->constraintTypeNames == ZR_NULL) {
            compiler_typed_export_generic_contract_free(cs->state, outSymbol);
            return ZR_FALSE;
        }
        for (TZrUInt32 constraintIndex = 0;
             constraintIndex < destination->constraintTypeCount;
             constraintIndex++) {
            SZrString **constraint = (SZrString **)ZrCore_Array_Get(
                    (SZrArray *)&source->constraintTypeNames, constraintIndex);
            destination->constraintTypeNames[constraintIndex] =
                    constraint != ZR_NULL ? *constraint : ZR_NULL;
        }
    }

    return ZR_TRUE;
}

TZrBool compiler_typed_export_generic_contract_copy_from_declaration(
        SZrCompilerState *cs,
        const SZrGenericDeclaration *genericDeclaration,
        SZrFunctionTypedExportSymbol *outSymbol) {
    SZrArray genericParameters;
    TZrBool result;

    if (cs == ZR_NULL || outSymbol == ZR_NULL) {
        return ZR_FALSE;
    }
    if (genericDeclaration == ZR_NULL || genericDeclaration->params == ZR_NULL ||
        genericDeclaration->params->count == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Construct(&genericParameters);
    compiler_collect_generic_parameter_info(
            cs, &genericParameters, (SZrGenericDeclaration *)genericDeclaration);
    result = compiler_typed_export_generic_contract_copy_from_infos(
            cs, &genericParameters, outSymbol);
    compiler_typed_export_collected_generic_parameters_free(cs->state, &genericParameters);
    return result;
}
