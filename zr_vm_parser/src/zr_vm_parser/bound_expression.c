#include "zr_vm_parser/bound_expression.h"

#include "zr_vm_parser/semantic.h"
#include "zr_vm_core/memory.h"

#include <string.h>

static void bound_value_construct_reset(SZrState *state, SZrBoundValueConstruct *bound) {
    if (state == ZR_NULL || bound == ZR_NULL) {
        return;
    }
    if (bound->arguments.isValid) {
        ZrCore_Array_Free(state, &bound->arguments);
    }
    memset(bound, 0, sizeof(*bound));
    bound->kind = ZR_BOUND_EXPRESSION_INVALID;
    bound->typeId = ZR_SEMANTIC_ID_INVALID;
    bound->constructorId = ZR_SEMANTIC_ID_INVALID;
    bound->resultTypeId = ZR_SEMANTIC_ID_INVALID;
    ZrCore_Array_Init(
            state,
            &bound->arguments,
            sizeof(SZrBoundValueConstructArgument),
            ZR_PARSER_INITIAL_CAPACITY_TINY);
}

void ZrParser_BoundValueConstruct_Init(
        SZrState *state,
        SZrBoundValueConstruct *bound) {
    if (state == ZR_NULL || bound == ZR_NULL) {
        return;
    }
    memset(bound, 0, sizeof(*bound));
    bound_value_construct_reset(state, bound);
}

void ZrParser_BoundValueConstruct_Free(
        SZrState *state,
        SZrBoundValueConstruct *bound) {
    if (state == ZR_NULL || bound == ZR_NULL) {
        return;
    }
    if (bound->arguments.isValid) {
        ZrCore_Array_Free(state, &bound->arguments);
    }
    memset(bound, 0, sizeof(*bound));
}

EZrValueConstructorResolution ZrParser_BoundValueConstruct_Bind(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        const SZrBoundValueConstructArgumentInput *arguments,
        TZrSize argumentCount,
        SZrFileRange sourceRange,
        SZrBoundValueConstruct *outBound) {
    SZrCanonicalValueArgument *canonicalArguments = ZR_NULL;
    TZrUInt32 *parameterIndices = ZR_NULL;
    TZrSymbolId constructorId = ZR_SEMANTIC_ID_INVALID;
    EZrValueConstructorResolution resolution;
    TZrSize index;

    if (context == ZR_NULL || outBound == ZR_NULL ||
        (argumentCount > 0U && arguments == ZR_NULL)) {
        return ZR_VALUE_CONSTRUCTOR_INVALID_ARGUMENTS;
    }
    bound_value_construct_reset(context->state, outBound);
    if (argumentCount > 0U) {
        canonicalArguments = (SZrCanonicalValueArgument *)ZrCore_Memory_RawMallocWithType(
                context->state->global,
                sizeof(SZrCanonicalValueArgument) * argumentCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        parameterIndices = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(
                context->state->global,
                sizeof(TZrUInt32) * argumentCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (canonicalArguments == ZR_NULL || parameterIndices == ZR_NULL) {
            if (canonicalArguments != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(
                        context->state->global,
                        canonicalArguments,
                        sizeof(SZrCanonicalValueArgument) * argumentCount,
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            if (parameterIndices != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(
                        context->state->global,
                        parameterIndices,
                        sizeof(TZrUInt32) * argumentCount,
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            return ZR_VALUE_CONSTRUCTOR_INVALID_ARGUMENTS;
        }
        for (index = 0U; index < argumentCount; index++) {
            canonicalArguments[index].typeId = arguments[index].typeId;
            canonicalArguments[index].name = arguments[index].name;
            canonicalArguments[index].callSiteMarker = arguments[index].callSiteMarker;
        }
    }
    resolution = ZrParser_CanonicalType_ResolveValueConstructorContract(
            context,
            typeId,
            canonicalArguments,
            argumentCount,
            &constructorId,
            parameterIndices);
    if (resolution == ZR_VALUE_CONSTRUCTOR_RESOLVED) {
        for (index = 0U; index < argumentCount; index++) {
            SZrBoundValueConstructArgument argument;
            argument.typeId = arguments[index].typeId;
            argument.name = arguments[index].name;
            argument.callSiteMarker = arguments[index].callSiteMarker;
            argument.sourceIndex = (TZrUInt32)index;
            argument.parameterIndex = parameterIndices[index];
            argument.sourceRange = arguments[index].sourceRange;
            ZrCore_Array_Push(context->state, &outBound->arguments, &argument);
        }
        outBound->kind = ZR_BOUND_EXPRESSION_VALUE_CONSTRUCT;
        outBound->typeId = typeId;
        outBound->constructorId = constructorId;
        outBound->resultTypeId = typeId;
        outBound->sourceRange = sourceRange;
    }
    if (canonicalArguments != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                context->state->global,
                canonicalArguments,
                sizeof(SZrCanonicalValueArgument) * argumentCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (parameterIndices != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                context->state->global,
                parameterIndices,
                sizeof(TZrUInt32) * argumentCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return resolution;
}
