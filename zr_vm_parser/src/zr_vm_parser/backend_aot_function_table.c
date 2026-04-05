#include "backend_aot_function_table.h"

#include "backend_aot_internal.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"

static const SZrFunction *backend_aot_function_from_constant_value(SZrState *state, const SZrTypeValue *value) {
    return ZrCore_Closure_GetMetadataFunctionFromValue(state, value);
}

static TZrUInt32 backend_aot_count_function_graph_capacity(SZrState *state, const SZrFunction *function) {
    TZrUInt32 childIndex;
    TZrUInt32 constantIndex;
    TZrUInt32 count = ZR_AOT_COUNT_NONE;

    if (state == ZR_NULL || function == ZR_NULL) {
        return ZR_AOT_COUNT_NONE;
    }

    count = ZR_AOT_FUNCTION_TREE_ROOT_INDEX + 1U;
    for (constantIndex = 0; constantIndex < function->constantValueLength; constantIndex++) {
        const SZrFunction *constantFunction =
                backend_aot_function_from_constant_value(state, &function->constantValueList[constantIndex]);
        if (constantFunction != ZR_NULL) {
            count += backend_aot_count_function_graph_capacity(state, constantFunction);
        }
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        count += backend_aot_count_function_graph_capacity(state, &function->childFunctionList[childIndex]);
    }

    return count;
}

static TZrBool backend_aot_functions_equivalent(const SZrFunction *left, const SZrFunction *right) {
    TZrBool sameFunctionName;

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    sameFunctionName = left->functionName == right->functionName ||
                       (left->functionName == ZR_NULL && right->functionName == ZR_NULL) ||
                       (left->functionName != ZR_NULL && right->functionName != ZR_NULL &&
                        ZrCore_String_Equal(left->functionName, right->functionName));

    return sameFunctionName &&
           left->parameterCount == right->parameterCount &&
           left->instructionsLength == right->instructionsLength &&
           left->lineInSourceStart == right->lineInSourceStart &&
           left->lineInSourceEnd == right->lineInSourceEnd;
}

static TZrBool backend_aot_function_table_contains(const SZrAotFunctionEntry *entries,
                                                   TZrUInt32 count,
                                                   const SZrFunction *function) {
    TZrUInt32 index;

    if (entries == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < count; index++) {
        if (entries[index].function == function ||
            backend_aot_functions_equivalent(entries[index].function, function)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void backend_aot_flatten_function_graph(SZrState *state,
                                               const SZrFunction *function,
                                               SZrAotFunctionEntry *entries,
                                               TZrUInt32 capacity,
                                               TZrUInt32 *ioIndex) {
    TZrUInt32 childIndex;
    TZrUInt32 constantIndex;

    if (state == ZR_NULL || function == ZR_NULL || entries == ZR_NULL || ioIndex == ZR_NULL) {
        return;
    }

    if (backend_aot_function_table_contains(entries, *ioIndex, function)) {
        return;
    }
    if (*ioIndex >= capacity) {
        return;
    }

    entries[*ioIndex].function = function;
    entries[*ioIndex].flatIndex = *ioIndex;
    (*ioIndex)++;

    for (constantIndex = 0; constantIndex < function->constantValueLength; constantIndex++) {
        const SZrFunction *constantFunction =
                backend_aot_function_from_constant_value(state, &function->constantValueList[constantIndex]);
        if (constantFunction != ZR_NULL) {
            backend_aot_flatten_function_graph(state, constantFunction, entries, capacity, ioIndex);
        }
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        backend_aot_flatten_function_graph(state, &function->childFunctionList[childIndex], entries, capacity, ioIndex);
    }
}

TZrBool backend_aot_build_function_table(SZrState *state,
                                         const SZrFunction *function,
                                         SZrAotFunctionTable *outTable) {
    TZrUInt32 capacity;
    TZrUInt32 writeIndex = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || outTable == ZR_NULL) {
        return ZR_FALSE;
    }

    outTable->entries = ZR_NULL;
    outTable->count = ZR_AOT_COUNT_NONE;
    outTable->capacity = ZR_AOT_COUNT_NONE;

    ZrCore_Function_RebindConstantFunctionValuesToChildren((SZrFunction *)function);
    capacity = backend_aot_count_function_graph_capacity(state, function);
    if (capacity == 0) {
        return ZR_TRUE;
    }

    outTable->entries = (SZrAotFunctionEntry *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrAotFunctionEntry) * capacity,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (outTable->entries == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outTable->entries, 0, sizeof(SZrAotFunctionEntry) * capacity);
    backend_aot_flatten_function_graph(state, function, outTable->entries, capacity, &writeIndex);
    outTable->count = writeIndex;
    outTable->capacity = capacity;
    return ZR_TRUE;
}

static TZrUInt32 backend_aot_find_function_index(const SZrAotFunctionTable *table, const SZrFunction *function) {
    TZrUInt32 index;

    if (table == ZR_NULL || table->entries == ZR_NULL || function == ZR_NULL) {
        return ZR_AOT_INVALID_FUNCTION_INDEX;
    }

    for (index = 0; index < table->count; index++) {
        if (table->entries[index].function == function ||
            backend_aot_functions_equivalent(table->entries[index].function, function)) {
            return table->entries[index].flatIndex;
        }
    }

    return ZR_AOT_INVALID_FUNCTION_INDEX;
}

void backend_aot_release_function_table(SZrState *state, SZrAotFunctionTable *table) {
    if (state == ZR_NULL || state->global == ZR_NULL || table == ZR_NULL) {
        return;
    }

    if (table->entries != ZR_NULL && table->capacity > 0) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      table->entries,
                                      sizeof(SZrAotFunctionEntry) * table->capacity,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    table->entries = ZR_NULL;
    table->count = ZR_AOT_COUNT_NONE;
    table->capacity = ZR_AOT_COUNT_NONE;
}

TZrBool backend_aot_resolve_callable_constant_function_index(const SZrAotFunctionTable *table,
                                                             SZrState *state,
                                                             const SZrFunction *function,
                                                             TZrInt32 constantIndex,
                                                             TZrUInt32 *outFunctionIndex) {
    const SZrTypeValue *constantValue;
    const SZrFunction *constantFunction;
    TZrUInt32 functionIndex;

    if (outFunctionIndex != ZR_NULL) {
        *outFunctionIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
    }

    if (table == ZR_NULL || state == ZR_NULL || function == ZR_NULL || function->constantValueList == ZR_NULL ||
        constantIndex < 0 || (TZrUInt32)constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }

    constantValue = &function->constantValueList[(TZrUInt32)constantIndex];
    constantFunction = backend_aot_function_from_constant_value(state, constantValue);
    if (constantFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    functionIndex = backend_aot_find_function_index(table, constantFunction);
    if (functionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return ZR_FALSE;
    }

    if (outFunctionIndex != ZR_NULL) {
        *outFunctionIndex = functionIndex;
    }
    return ZR_TRUE;
}
