#include "compiler/compiler_function_parameters.h"
#include "compiler/compiler_internal.h"
#include "type_inference/type_environment_context_lookup.h"

TZrBool compiler_stack_slot_is_reference_parameter(SZrCompilerState *cs, TZrUInt32 stackSlot) {
    if (cs == ZR_NULL || cs->typeEnv == ZR_NULL || cs->semanticContext == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = cs->localVars.length; index > 0u; --index) {
        const SZrFunctionLocalVariable *local = ZrCore_Array_Get(&cs->localVars, index - 1u);
        const SZrCompilerBindingIdentity *identity;
        const SZrTypeBinding *binding;
        const SZrSemanticSymbolRecord *symbol;
        const SZrParameter *parameter;
        if (local->stackSlot != stackSlot || index > cs->localBindingIdentities.length) {
            continue;
        }
        identity = ZrCore_Array_Get(&cs->localBindingIdentities, index - 1u);
        binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(
                cs->typeEnv, local->name, identity->syntaxContext);
        symbol = binding != ZR_NULL
                ? ZrParser_Semantic_FindSymbolById(cs->semanticContext, binding->symbolId) : ZR_NULL;
        if (symbol == ZR_NULL || symbol->astNode == ZR_NULL ||
            symbol->astNode->type != ZR_AST_PARAMETER) {
            return ZR_FALSE;
        }
        parameter = &symbol->astNode->data.parameter;
        if (parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_REF_READONLY ||
            parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_SCOPED_REF_READONLY) {
            return ZR_FALSE;
        }
        return (TZrBool)(parameter->passingMode == ZR_PARAMETER_PASSING_MODE_REF ||
                         parameter->passingMode == ZR_PARAMETER_PASSING_MODE_OUT);
    }
    return ZR_FALSE;
}

typedef struct SZrCompilerParameterLocalRegistration {
    SZrSemanticIrFunction *function;
    TZrSymbolId symbolId;
    TZrTypeId typeId;
    SZrParserPlaceBase base;
    SZrFileRange sourceRange;
    TZrPlaceId placeId;
    TZrValueId valueId;
} SZrCompilerParameterLocalRegistration;

static TZrBool compiler_parameter_array_is_valid(
        const SZrArray *array, TZrSize elementSize) {
    return (TZrBool)(array->isValid && array->head != ZR_NULL &&
                     array->elementSize == elementSize &&
                     array->length <= array->capacity &&
                     array->capacity <= ZR_MAX_SIZE / elementSize);
}

static TZrBool compiler_parameter_range_equal(
        SZrFileRange left, SZrFileRange right) {
    return (TZrBool)(left.source == right.source &&
                     left.start.offset == right.start.offset &&
                     left.start.line == right.start.line &&
                     left.start.column == right.start.column &&
                     left.end.offset == right.end.offset &&
                     left.end.line == right.end.line &&
                     left.end.column == right.end.column);
}

static void compiler_parameter_add_local(SZrState *state, TZrPtr arguments) {
    SZrCompilerParameterLocalRegistration *registration = arguments;
    (void)state;
    registration->placeId = ZrParser_SemanticIr_AddLocal(
            registration->function, registration->symbolId, &registration->base,
            registration->typeId, registration->sourceRange, ZR_TRUE);
    if (registration->placeId != ZR_PLACE_ID_INVALID) {
        registration->valueId = ZrParser_SemanticIr_AddValue(
                registration->function, registration->typeId, registration->sourceRange);
    }
}

static void compiler_parameter_rollback_local(
        SZrCompilerState *cs, TZrSize placeCount, TZrSize localCount,
        TZrSize slotCount, TZrSize valueCount) {
    cs->preSemanticIrSlots.length = slotCount;
    cs->preSemanticIr.locals.length = localCount;
    cs->preSemanticIr.values.length = valueCount;
    while (cs->preSemanticIr.places.places.length > placeCount) {
        SZrParserPlace *place = ZrCore_Array_Pop(&cs->preSemanticIr.places.places);
        ZrCore_Array_Free(cs->state, &place->projections);
    }
}

EZrCompilerParameterIdentityResult compiler_register_parameter_semantic_identity(
        SZrCompilerState *cs,
        SZrString *name,
        SZrSyntaxContext syntaxContext,
        TZrUInt32 stackSlot,
        SZrFileRange sourceRange) {
    const SZrTypeBinding *binding;
    const SZrSemanticSymbolRecord *symbol;
    const SZrCanonicalTypeNode *type = ZR_NULL;
    const SZrCompilerSemanticIrSlot *existingSlot = ZR_NULL;
    const SZrParserPlace *existingPlace = ZR_NULL;
    const SZrSemanticIrValue *entryValue;
    SZrCompilerSemanticIrSlot slot;
    SZrCompilerParameterLocalRegistration registration;
    TZrSize matchingSlots = 0U;
    TZrSize matchingLocals = 0U;
    TZrSize placeCount;
    TZrSize localCount;
    TZrSize slotCount;
    TZrSize valueCount;
    TZrBool equalContext;
    TZrBool isScalar;
    TZrTypeId storageTypeId;

    if (cs == ZR_NULL || cs->hasError || cs->state == ZR_NULL ||
        cs->state->global == ZR_NULL || cs->semanticContext == ZR_NULL ||
        cs->typeEnv == ZR_NULL || cs->typeEnv->semanticContext != cs->semanticContext ||
        name == ZR_NULL || stackSlot == ZR_PARSER_SLOT_NONE ||
        !cs->preSemanticIrInitialized ||
        cs->preSemanticIr.state != cs->state ||
        cs->preSemanticIr.places.state != cs->state ||
        !compiler_parameter_array_is_valid(
                &cs->typeEnv->variableTypes, sizeof(SZrTypeBinding)) ||
        !compiler_parameter_array_is_valid(
                &cs->localVars, sizeof(SZrFunctionLocalVariable)) ||
        !compiler_parameter_array_is_valid(
                &cs->localBindingIdentities, sizeof(SZrCompilerBindingIdentity)) ||
        cs->localBindingIdentities.length != cs->localVars.length ||
        !compiler_parameter_array_is_valid(
                &cs->preSemanticIr.instructions, sizeof(SZrSemanticIrInstruction)) ||
        !compiler_parameter_array_is_valid(
                &cs->preSemanticIr.values, sizeof(SZrSemanticIrValue)) ||
        !compiler_parameter_array_is_valid(
                &cs->preSemanticIr.places.places, sizeof(SZrParserPlace)) ||
        !compiler_parameter_array_is_valid(
                &cs->preSemanticIr.locals, sizeof(SZrSemanticIrLocal)) ||
        !compiler_parameter_array_is_valid(
                &cs->preSemanticIrSlots, sizeof(SZrCompilerSemanticIrSlot)) ||
        !type_environment_find_current_variable_binding(
                cs->typeEnv, name, syntaxContext, &binding) || binding == ZR_NULL ||
        binding->symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
    }

    for (TZrSize index = 0U; index < cs->localVars.length; index++) {
        const SZrFunctionLocalVariable *local = ZrCore_Array_Get(&cs->localVars, index);
        if (local->stackSlot == stackSlot) {
            const SZrCompilerBindingIdentity *identity =
                    ZrCore_Array_Get(&cs->localBindingIdentities, index);
            if (matchingSlots != 0U || local->name == ZR_NULL ||
                !ZrCore_String_Equal(local->name, name) ||
                !type_environment_context_equal(identity->syntaxContext, syntaxContext,
                        ZR_SYNTAX_NAMESPACE_LOCAL, &equalContext) || !equalContext) {
                return ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
            }
            matchingSlots++;
        }
    }
    if (matchingSlots != 1U) {
        return ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
    }

    symbol = ZrParser_Semantic_FindSymbolById(cs->semanticContext, binding->symbolId);
    if (symbol == ZR_NULL || symbol->id != binding->symbolId ||
        symbol->name == ZR_NULL || !ZrCore_String_Equal(symbol->name, name) ||
        symbol->typeId != binding->typeId ||
        (symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_PARAMETER &&
         symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_VARIABLE) ||
        !type_environment_context_equal(symbol->syntaxContext, binding->syntaxContext,
                ZR_SYNTAX_NAMESPACE_LOCAL, &equalContext) || !equalContext ||
        (binding->hasDeclarationRange &&
         !compiler_parameter_range_equal(binding->declarationRange, sourceRange))) {
        return ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
    }
    storageTypeId = binding->typeId;
    if (storageTypeId != ZR_SEMANTIC_ID_INVALID &&
        compiler_stack_slot_is_reference_parameter(cs, stackSlot)) {
        storageTypeId = ZrParser_CanonicalType_InternRef(
                cs->semanticContext, storageTypeId, ZR_CANONICAL_REF_WRITABLE);
        if (storageTypeId == ZR_SEMANTIC_ID_INVALID) {
            return ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
        }
    }
    if (storageTypeId != ZR_SEMANTIC_ID_INVALID) {
        type = ZrParser_CanonicalType_Find(cs->semanticContext, storageTypeId);
        if (type == ZR_NULL || type->id != storageTypeId) {
            return ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
        }
    }
    isScalar = (TZrBool)(type != ZR_NULL && type->kind == ZR_CANONICAL_TYPE_PRIMITIVE);

    for (TZrSize index = 0U; index < cs->preSemanticIrSlots.length; index++) {
        const SZrCompilerSemanticIrSlot *candidate =
                ZrCore_Array_Get(&cs->preSemanticIrSlots, index);
        if (candidate->stackSlot == stackSlot) {
            if (existingSlot != ZR_NULL || type == ZR_NULL ||
                candidate->typeId != storageTypeId ||
                candidate->symbolId != binding->symbolId ||
                candidate->placeId == ZR_PLACE_ID_INVALID) {
                return ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
            }
            existingSlot = candidate;
        } else if (candidate->symbolId == binding->symbolId) {
            return ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
        }
    }
    if (existingSlot != ZR_NULL) {
        existingPlace = ZrParser_PlaceGraph_Get(
                &cs->preSemanticIr.places, existingSlot->placeId);
        entryValue = ZrParser_SemanticIr_Value(
                &cs->preSemanticIr, existingSlot->valueId);
        if (existingPlace == ZR_NULL || existingPlace->id != existingSlot->placeId ||
            existingPlace->parentId != ZR_PLACE_ID_INVALID ||
            existingPlace->base.kind != ZR_PARSER_PLACE_BASE_PARAMETER ||
            existingPlace->base.identity != stackSlot ||
            existingPlace->typeId != storageTypeId ||
            existingPlace->projections.length != 0U ||
            !compiler_parameter_range_equal(existingPlace->sourceRange, sourceRange) ||
            entryValue == ZR_NULL || entryValue->id != existingSlot->valueId ||
            entryValue->typeId != storageTypeId ||
            entryValue->definitionInstructionId != ZR_SEMANTIC_INSTRUCTION_ID_INVALID ||
            !compiler_parameter_range_equal(entryValue->sourceRange, sourceRange)) {
            return ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
        }
    }
    for (TZrSize index = 0U; index < cs->preSemanticIr.places.places.length; index++) {
        const SZrParserPlace *place =
                ZrCore_Array_Get(&cs->preSemanticIr.places.places, index);
        if (place->parentId == ZR_PLACE_ID_INVALID &&
            place->base.identity == stackSlot &&
            (place->base.kind == ZR_PARSER_PLACE_BASE_PARAMETER ||
             place->base.kind == ZR_PARSER_PLACE_BASE_LOCAL ||
             place->base.kind == ZR_PARSER_PLACE_BASE_THIS) &&
            place != existingPlace) {
            return ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
        }
    }
    for (TZrSize index = 0U; index < cs->preSemanticIr.locals.length; index++) {
        const SZrSemanticIrLocal *local = ZrCore_Array_Get(&cs->preSemanticIr.locals, index);
        if (local->symbolId == binding->symbolId ||
            (existingSlot != ZR_NULL && local->placeId == existingSlot->placeId)) {
            if (existingSlot == ZR_NULL || local->symbolId != binding->symbolId ||
                local->typeId != storageTypeId || local->placeId != existingSlot->placeId ||
                !local->isParameter || local->isScalar != isScalar || matchingLocals != 0U) {
                return ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
            }
            matchingLocals++;
        }
    }
    if (existingSlot != ZR_NULL) {
        return matchingLocals == 1U ? ZR_COMPILER_PARAMETER_IDENTITY_REGISTERED
                                    : ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
    }
    if (type == ZR_NULL) {
        return ZR_COMPILER_PARAMETER_IDENTITY_UNAVAILABLE;
    }

    placeCount = cs->preSemanticIr.places.places.length;
    localCount = cs->preSemanticIr.locals.length;
    slotCount = cs->preSemanticIrSlots.length;
    valueCount = cs->preSemanticIr.values.length;
    if (placeCount >= UINT32_MAX || valueCount >= UINT32_MAX ||
        localCount == ZR_MAX_SIZE || slotCount == ZR_MAX_SIZE ||
        !ZrCore_Array_TryReserve(cs->state, &cs->preSemanticIr.places.places, placeCount + 1U) ||
        !ZrCore_Array_TryReserve(cs->state, &cs->preSemanticIr.locals, localCount + 1U) ||
        !ZrCore_Array_TryReserve(cs->state, &cs->preSemanticIrSlots, slotCount + 1U) ||
        !ZrCore_Array_TryReserve(cs->state, &cs->preSemanticIr.values, valueCount + 1U)) {
        return ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
    }

    memset(&registration, 0, sizeof(registration));
    registration.function = &cs->preSemanticIr;
    registration.symbolId = binding->symbolId;
    registration.typeId = storageTypeId;
    registration.base.kind = ZR_PARSER_PLACE_BASE_PARAMETER;
    registration.base.identity = stackSlot;
    registration.sourceRange = sourceRange;
    /* AddLocal owns its projection buffer only after publishing the place.
     * Reserving the other arrays keeps its remaining allocation before publication. */
    if (ZrCore_Exception_TryRun(cs->state, compiler_parameter_add_local, &registration) !=
                ZR_THREAD_STATUS_FINE || registration.placeId == ZR_PLACE_ID_INVALID ||
        registration.valueId != (TZrValueId)(valueCount + 1U) ||
        cs->preSemanticIr.places.places.length != placeCount + 1U ||
        cs->preSemanticIr.locals.length != localCount + 1U ||
        cs->preSemanticIr.values.length != valueCount + 1U) {
        compiler_parameter_rollback_local(cs, placeCount, localCount, slotCount, valueCount);
        return ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
    }
    entryValue = ZrParser_SemanticIr_Value(&cs->preSemanticIr, registration.valueId);
    if (entryValue == ZR_NULL || entryValue->id != registration.valueId ||
        entryValue->typeId != registration.typeId ||
        entryValue->definitionInstructionId != ZR_SEMANTIC_INSTRUCTION_ID_INVALID ||
        !compiler_parameter_range_equal(entryValue->sourceRange, sourceRange) ||
        !ZrParser_SemanticIr_SetLocalScalar(
                &cs->preSemanticIr, registration.placeId, isScalar)) {
        compiler_parameter_rollback_local(cs, placeCount, localCount, slotCount, valueCount);
        return ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
    }

    memset(&slot, 0, sizeof(slot));
    slot.stackSlot = stackSlot;
    slot.typeId = registration.typeId;
    slot.symbolId = registration.symbolId;
    slot.placeId = registration.placeId;
    slot.valueId = registration.valueId;
    if (!ZrCore_Array_TryPush(cs->state, &cs->preSemanticIrSlots, &slot)) {
        compiler_parameter_rollback_local(cs, placeCount, localCount, slotCount, valueCount);
        return ZR_COMPILER_PARAMETER_IDENTITY_ERROR;
    }
    cs->preSemanticIrValidated = ZR_FALSE;
    return ZR_COMPILER_PARAMETER_IDENTITY_REGISTERED;
}
