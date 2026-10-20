//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/ast.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/comptime_contract.h"

#include <string.h>

static TZrBool ast_node_array_size_mul(TZrSize count, TZrSize *outBytes) {
    if (outBytes == ZR_NULL || count > ZR_MAX_SIZE / sizeof(SZrAstNode *)) {
        return ZR_FALSE;
    }
    *outBytes = count * sizeof(SZrAstNode *);
    return ZR_TRUE;
}

static TZrBool ast_node_array_size_add(
        TZrSize left, TZrSize right, TZrSize *outBytes) {
    if (outBytes == ZR_NULL || left > ZR_MAX_SIZE - right) {
        return ZR_FALSE;
    }
    *outBytes = left + right;
    return ZR_TRUE;
}

static void ast_node_array_abort_budget(
        SZrParserComptimeBudgetTransaction *transaction,
        TZrBool active) {
    if (active) {
        ZrParser_ComptimeBudget_AbortTransaction(transaction);
    }
}

static TZrBool ast_node_array_reserve_heap(
        SZrParserComptimeBudget *budget,
        TZrSize bytes,
        SZrParserComptimeBudgetTransaction *transaction) {
    if (transaction == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(transaction, 0, sizeof(*transaction));
    if (budget == ZR_NULL) {
        return ZR_TRUE;
    }
    ZrParser_ComptimeBudget_BeginTransaction(budget, transaction);
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                (TZrUInt64)bytes)) {
        ZrParser_ComptimeBudget_AbortTransaction(transaction);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

SZrAstNodeArray *ZrParser_AstNodeArray_NewBudget(
        SZrState *state,
        TZrSize initialCapacity,
        SZrParserComptimeBudget *budget) {
    SZrAstNodeArray *array;
    SZrParserComptimeBudgetTransaction transaction;
    TZrBool budgetActive = budget != ZR_NULL;
    TZrSize capacity;
    TZrSize nodeBytes;
    TZrSize heapBytes;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }
    capacity = initialCapacity != 0u
                       ? initialCapacity
                       : ZR_PARSER_INITIAL_CAPACITY_SMALL;
    if (!ast_node_array_size_mul(capacity, &nodeBytes) ||
        !ast_node_array_size_add(sizeof(*array), nodeBytes, &heapBytes) ||
        !ast_node_array_reserve_heap(budget, heapBytes, &transaction)) {
        return ZR_NULL;
    }

    array = ZrCore_Memory_RawMallocWithType(
            state->global, sizeof(*array), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (array == ZR_NULL) {
        ast_node_array_abort_budget(&transaction, budgetActive);
        return ZR_NULL;
    }
    array->nodes = ZrCore_Memory_RawMallocWithType(
            state->global, nodeBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (array->nodes == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                state->global, array, sizeof(*array),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ast_node_array_abort_budget(&transaction, budgetActive);
        return ZR_NULL;
    }
    array->count = 0u;
    array->capacity = capacity;
    if (budgetActive &&
        !ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
        ZrCore_Memory_RawFreeWithType(
                state->global, array->nodes, nodeBytes,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ZrCore_Memory_RawFreeWithType(
                state->global, array, sizeof(*array),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ast_node_array_abort_budget(&transaction, budgetActive);
        return ZR_NULL;
    }
    return array;
}

SZrAstNodeArray *ZrParser_AstNodeArray_New(
        SZrState *state, TZrSize initialCapacity) {
    return ZrParser_AstNodeArray_NewBudget(state, initialCapacity, ZR_NULL);
}

TZrBool ZrParser_AstNodeArray_TryAddBudget(
        SZrState *state,
        SZrAstNodeArray *array,
        SZrAstNode *node,
        SZrParserComptimeBudget *budget) {
    SZrParserComptimeBudgetTransaction transaction;
    TZrBool budgetActive = budget != ZR_NULL;
    SZrAstNode **newNodes;
    SZrAstNode **oldNodes;
    TZrSize required;
    TZrSize newCapacity;
    TZrSize newBytes;
    TZrSize oldBytes;

    if (state == ZR_NULL || state->global == ZR_NULL || array == ZR_NULL ||
        node == ZR_NULL || array->count > array->capacity ||
        (array->capacity != 0u && array->capacity >
                ZR_MAX_SIZE / sizeof(SZrAstNode *))) {
        return ZR_FALSE;
    }
    /* A zero-capacity list has no valid backing allocation to release. */
    if (array->capacity == 0u && array->nodes != ZR_NULL) {
        return ZR_FALSE;
    }
    if (array->count < array->capacity) {
        if (array->nodes == ZR_NULL) {
            return ZR_FALSE;
        }
        array->nodes[array->count++] = node;
        return ZR_TRUE;
    }
    if (array->count == ZR_MAX_SIZE) {
        return ZR_FALSE;
    }
    if (array->count != 0u && array->nodes == ZR_NULL) {
        return ZR_FALSE;
    }
    required = array->count + 1u;
    if (array->capacity == 0u) {
        newCapacity = ZR_PARSER_INITIAL_CAPACITY_SMALL;
    } else {
        if (array->capacity >
                ZR_MAX_SIZE / ZR_PARSER_DYNAMIC_CAPACITY_GROWTH_FACTOR) {
            return ZR_FALSE;
        }
        newCapacity = array->capacity *
                      ZR_PARSER_DYNAMIC_CAPACITY_GROWTH_FACTOR;
    }
    if (newCapacity < required) {
        newCapacity = required;
    }
    if (!ast_node_array_size_mul(newCapacity, &newBytes)) {
        return ZR_FALSE;
    }
    if (!ast_node_array_reserve_heap(budget, newBytes, &transaction)) {
        return ZR_FALSE;
    }
    newNodes = (SZrAstNode **)ZrCore_Memory_RawMallocWithType(
            state->global, newBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (newNodes == ZR_NULL) {
        ast_node_array_abort_budget(&transaction, budgetActive);
        return ZR_FALSE;
    }
    oldNodes = array->nodes;
    oldBytes = array->capacity * sizeof(SZrAstNode *);
    if (array->count != 0u) {
        memcpy(newNodes, oldNodes, array->count * sizeof(SZrAstNode *));
    }
    if (budgetActive &&
        !ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
        ZrCore_Memory_RawFreeWithType(
                state->global, newNodes, newBytes,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ast_node_array_abort_budget(&transaction, budgetActive);
        return ZR_FALSE;
    }
    newNodes[array->count] = node;
    array->nodes = newNodes;
    array->capacity = newCapacity;
    array->count++;
    if (oldNodes != ZR_NULL && oldBytes != 0u) {
        ZrCore_Memory_RawFreeWithType(
                state->global, oldNodes, oldBytes,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return ZR_TRUE;
}

void ZrParser_AstNodeArray_Add(
        SZrState *state, SZrAstNodeArray *array, SZrAstNode *node) {
    (void)ZrParser_AstNodeArray_TryAddBudget(
            state, array, node, ZR_NULL);
}

void ZrParser_AstNodeArray_Free(SZrState *state, SZrAstNodeArray *array) {
    if (array == ZR_NULL) {
        return;
    }

    if (array->nodes != ZR_NULL && array->capacity != 0u) {
        ZrCore_Memory_RawFreeWithType(
                state->global, array->nodes,
                array->capacity * sizeof(SZrAstNode *),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }

    ZrCore_Memory_RawFreeWithType(
            state->global, array, sizeof(SZrAstNodeArray),
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
}
