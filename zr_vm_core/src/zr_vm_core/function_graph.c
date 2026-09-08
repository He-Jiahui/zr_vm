#include "zr_vm_core/function.h"

#include <stdint.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/function_identity.h"

typedef struct SZrFunctionGraphIndexResolver {
    struct SZrState *state;
    const SZrFunction **visitedFunctions;
    TZrUInt32 visitedFunctionCount;
    TZrUInt32 visitedFunctionCapacity;
    TZrUInt32 targetFlatIndex;
    const SZrFunction *resolvedFunction;
    TZrBool failed;
} SZrFunctionGraphIndexResolver;

/* Keep identity compatible with the AOT function-table flattener. */
static TZrBool function_graph_functions_equivalent(
        const SZrFunction *left,
        const SZrFunction *right) {
    return ZrCore_Function_HasSameDefinition(left, right);
}

static TZrBool function_graph_index_resolver_has_visited(
        const SZrFunctionGraphIndexResolver *resolver,
        const SZrFunction *function) {
    for (TZrUInt32 index = 0u; index < resolver->visitedFunctionCount; ++index) {
        if (resolver->visitedFunctions[index] == function ||
            function_graph_functions_equivalent(resolver->visitedFunctions[index], function)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool function_graph_index_resolver_reserve(
        SZrFunctionGraphIndexResolver *resolver) {
    const SZrFunction **newVisitedFunctions;
    TZrUInt32 newCapacity;
    TZrSize newByteSize;

    if (resolver->visitedFunctionCount < resolver->visitedFunctionCapacity) {
        return ZR_TRUE;
    }
    newCapacity = resolver->visitedFunctionCapacity == 0u
                          ? 16u
                          : (resolver->visitedFunctionCapacity <= UINT32_MAX / 2u
                                     ? resolver->visitedFunctionCapacity * 2u
                                     : UINT32_MAX);
    if (newCapacity <= resolver->visitedFunctionCapacity) {
        return ZR_FALSE;
    }
#if SIZE_MAX < UINT32_MAX
    if (newCapacity > SIZE_MAX / sizeof(*resolver->visitedFunctions)) {
        return ZR_FALSE;
    }
#endif
    newByteSize = (TZrSize)newCapacity * sizeof(*resolver->visitedFunctions);
    newVisitedFunctions = (const SZrFunction **)ZrCore_Memory_RawMallocWithType(
            resolver->state->global, newByteSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newVisitedFunctions == ZR_NULL) {
        return ZR_FALSE;
    }
    if (resolver->visitedFunctions != ZR_NULL) {
        TZrSize oldByteSize = (TZrSize)resolver->visitedFunctionCapacity *
                              sizeof(*resolver->visitedFunctions);
        ZrCore_Memory_RawCopy(
                (TZrPtr)newVisitedFunctions,
                (TZrPtr)resolver->visitedFunctions,
                (TZrSize)resolver->visitedFunctionCount *
                        sizeof(*resolver->visitedFunctions));
        ZrCore_Memory_RawFreeWithType(
                resolver->state->global,
                (TZrPtr)resolver->visitedFunctions,
                oldByteSize,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    resolver->visitedFunctions = newVisitedFunctions;
    resolver->visitedFunctionCapacity = newCapacity;
    return ZR_TRUE;
}

static TZrBool function_graph_index_resolver_visit(
        SZrFunctionGraphIndexResolver *resolver,
        const SZrFunction *function) {
    if (resolver->failed || function == ZR_NULL ||
        function_graph_index_resolver_has_visited(resolver, function)) {
        return ZR_FALSE;
    }
    if (resolver->visitedFunctionCount == resolver->targetFlatIndex) {
        resolver->resolvedFunction = function;
        return ZR_TRUE;
    }
    if (!function_graph_index_resolver_reserve(resolver)) {
        resolver->failed = ZR_TRUE;
        return ZR_FALSE;
    }
    resolver->visitedFunctions[resolver->visitedFunctionCount++] = function;

    for (TZrUInt32 constantIndex = 0u;
         constantIndex < function->constantValueLength;
         ++constantIndex) {
        const SZrFunction *constantFunction = ZrCore_Closure_GetMetadataFunctionFromValue(
                resolver->state, &function->constantValueList[constantIndex]);
        if (function_graph_index_resolver_visit(resolver, constantFunction)) {
            return ZR_TRUE;
        }
    }
    for (TZrUInt32 childIndex = 0u;
         childIndex < function->childFunctionLength;
         ++childIndex) {
        if (function_graph_index_resolver_visit(
                    resolver, &function->childFunctionList[childIndex])) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

SZrFunction *ZrCore_Function_ResolveGraphFunctionByFlatIndex(
        struct SZrState *state,
        SZrFunction *rootFunction,
        TZrUInt32 flatIndex) {
    SZrFunctionGraphIndexResolver resolver = {0};

    if (state == ZR_NULL || state->global == ZR_NULL || rootFunction == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Function_RebindConstantFunctionValuesToChildren(rootFunction);
    resolver.state = state;
    resolver.targetFlatIndex = flatIndex;
    function_graph_index_resolver_visit(&resolver, rootFunction);
    if (resolver.visitedFunctions != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                state->global,
                (TZrPtr)resolver.visitedFunctions,
                (TZrSize)resolver.visitedFunctionCapacity *
                        sizeof(*resolver.visitedFunctions),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    return (SZrFunction *)(TZrPtr)resolver.resolvedFunction;
}
