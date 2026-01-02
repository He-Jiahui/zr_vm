//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/ast.h"
#include "zr_vm_core/memory.h"

SZrAstNodeArray *ZrAstNodeArrayNew(SZrState *state, TZrSize initialCapacity) {
    SZrAstNodeArray *array = ZrMemoryRawMallocWithType(state->global, sizeof(SZrAstNodeArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (array == ZR_NULL) {
        return ZR_NULL;
    }

    initialCapacity = initialCapacity > 0 ? initialCapacity : 8;
    array->nodes = ZrMemoryRawMallocWithType(state->global, sizeof(SZrAstNode *) * initialCapacity, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (array->nodes == ZR_NULL) {
        ZrMemoryRawFreeWithType(state->global, array, sizeof(SZrAstNodeArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_NULL;
    }

    array->count = 0;
    array->capacity = initialCapacity;
    return array;
}

void ZrAstNodeArrayAdd(SZrState *state, SZrAstNodeArray *array, SZrAstNode *node) {
    if (array == ZR_NULL || node == ZR_NULL) {
        return;
    }

    if (array->count >= array->capacity) {
        TZrSize newCapacity = array->capacity * 2;
        SZrAstNode **newNodes = ZrMemoryRawMallocWithType(state->global, sizeof(SZrAstNode *) * newCapacity, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (newNodes == ZR_NULL) {
            return;  // 内存分配失败
        }

        for (TZrSize i = 0; i < array->count; i++) {
            newNodes[i] = array->nodes[i];
        }

        ZrMemoryRawFreeWithType(state->global, array->nodes, sizeof(SZrAstNode *) * array->capacity, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        array->nodes = newNodes;
        array->capacity = newCapacity;
    }

    array->nodes[array->count++] = node;
}

void ZrAstNodeArrayFree(SZrState *state, SZrAstNodeArray *array) {
    if (array == ZR_NULL) {
        return;
    }

    if (array->nodes != ZR_NULL) {
        ZrMemoryRawFreeWithType(state->global, array->nodes, sizeof(SZrAstNode *) * array->capacity, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }

    ZrMemoryRawFreeWithType(state->global, array, sizeof(SZrAstNodeArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

