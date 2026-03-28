//
// Created by HeJiahui on 2025/6/20.
//

#ifndef ZR_VM_CORE_LIST_H
#define ZR_VM_CORE_LIST_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/math.h"
#include "zr_vm_core/memory.h"
ZR_FORCE_INLINE void ZrCore_Array_Construct(SZrArray *array) {
    if (array == ZR_NULL) {
        return;
    }

    array->head = ZR_NULL;
    array->elementSize = 0;
    array->length = 0;
    array->capacity = 0;
    array->isValid = ZR_FALSE;
}

ZR_FORCE_INLINE void ZrCore_Array_Init(SZrState *state, SZrArray *array, TZrSize elementSize, TZrSize capacity) {
    if (capacity <= 0) {
        capacity = 1;
    }
    ZR_ASSERT(array != ZR_NULL && elementSize != 0);
    array->head = ZR_CAST_UINT8_PTR(
            ZrCore_Memory_RawMallocWithType(state->global, capacity * elementSize, ZR_MEMORY_NATIVE_TYPE_ARRAY));
    array->elementSize = elementSize;
    array->length = 0;
    array->capacity = capacity;
    array->isValid = ZR_TRUE;
}

ZR_FORCE_INLINE TZrPtr ZrCore_Array_Get(SZrArray *array, TZrSize index) {
    ZR_ASSERT(index < array->length);
    return array->head + index * array->elementSize;
}

ZR_FORCE_INLINE void ZrCore_Array_Set(SZrArray *array, TZrSize index, TZrPtr element) {
    ZR_ASSERT(index < array->length);
    ZrCore_Memory_RawCopy(array->head + index * array->elementSize, element, array->elementSize);
}

ZR_FORCE_INLINE TZrPtr ZrCore_Array_Pop(SZrArray *array) {
    ZR_ASSERT(array->length > 0);
    array->length--;
    return array->head + array->length * array->elementSize;
}

ZR_FORCE_INLINE void ZrCore_Array_Push(SZrState *state, SZrArray *array, TZrPtr element) {
    ZR_ASSERT(array->head != ZR_NULL);
    SZrGlobalState *global = state->global;
    if (array->length == array->capacity) {
        TZrSize previousCapacity = array->capacity;
        // KEEP AT LEAST INCREASING 1 ELEMENT
        TZrSize toIncrease = array->capacity * ZR_MATH_MAX(ZR_ARRAY_INCREASEMENT_MULTIPLIER_PERCENT, 100) / 100 + 1;
        array->capacity = toIncrease;
        array->head =
                ZR_CAST_UINT8_PTR(ZrCore_Memory_Allocate(global, array->head, previousCapacity * array->elementSize,
                                                   array->capacity * array->elementSize, ZR_MEMORY_NATIVE_TYPE_ARRAY));
    }
    ZrCore_Memory_RawCopy(array->head + array->length * array->elementSize, element, array->elementSize);
    array->length++;
}

ZR_FORCE_INLINE void ZrCore_Array_Empty(SZrArray *array) { array->length = 0; }

ZR_FORCE_INLINE void ZrCore_Array_Free(SZrState *state, SZrArray *array) {
    if (array == ZR_NULL || !array->isValid || array->head == ZR_NULL) {
        return;
    }
    SZrGlobalState *global = state->global;
    ZrCore_Memory_RawFreeWithType(global, array->head, array->capacity * array->elementSize, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    array->head = ZR_NULL;
    array->elementSize = 0;
    array->length = 0;
    array->capacity = 0;
    array->isValid = ZR_FALSE;
}

ZR_FORCE_INLINE void ZrCore_Array_Append(SZrState *state, SZrArray *array, TZrPtr elements, TZrSize length) {
    ZR_ASSERT(array->head != ZR_NULL);
    SZrGlobalState *global = state->global;
    if (array->length + length > array->capacity) {
        TZrSize previousCapacity = array->capacity;
        // KEEP AT LEAST INCREASING 1 ELEMENT
        TZrSize toIncrease = array->capacity * ZR_MATH_MAX(ZR_ARRAY_INCREASEMENT_MULTIPLIER_PERCENT, 100) / 100 + 1;
        array->capacity = ZR_MATH_MAX(toIncrease, array->length + length);
        array->head =
                ZR_CAST_UINT8_PTR(ZrCore_Memory_Allocate(global, array->head, previousCapacity * array->elementSize,
                                                   array->capacity * array->elementSize, ZR_MEMORY_NATIVE_TYPE_ARRAY));
    }
    ZrCore_Memory_RawCopy(array->head + array->length * array->elementSize, elements, length * array->elementSize);
    array->length += length;
}

#endif // ZR_VM_CORE_LIST_H
