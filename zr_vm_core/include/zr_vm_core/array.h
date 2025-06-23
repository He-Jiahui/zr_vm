//
// Created by HeJiahui on 2025/6/20.
//

#ifndef ZR_VM_CORE_LIST_H
#define ZR_VM_CORE_LIST_H

#include "zr_vm_core/convertion.h"
#include "zr_vm_core/math.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/conf.h"
ZR_FORCE_INLINE void ZrArrayNew(SZrArray *array) {
    array->isValid = ZR_FALSE;
}

ZR_FORCE_INLINE void ZrArrayInit(SZrState *state, SZrArray *array, TZrSize elementSize, TZrSize capacity) {
    ZR_ASSERT(array != ZR_NULL && capacity != 0 && elementSize != 0);
    array->head = ZR_CAST_UINT8_PTR(ZrMemoryRawMalloc(state->global, capacity * elementSize));
    array->elementSize = elementSize;
    array->length = 0;
    array->capacity = capacity;
    array->isValid = ZR_TRUE;
}

ZR_FORCE_INLINE TZrPtr ZrArrayGet(SZrArray *array, TZrSize index) {
    ZR_ASSERT(index < array->length);
    return array->head + index * array->elementSize;
}

ZR_FORCE_INLINE void ZrArraySet(SZrArray *array, TZrSize index, TZrPtr element) {
    ZR_ASSERT(index < array->length);
    ZrMemoryRawCopy(array->head + index * array->elementSize, element, array->elementSize);
}

ZR_FORCE_INLINE TZrPtr ZrArrayPop(SZrArray *array) {
    ZR_ASSERT(array->length > 0);
    array->length--;
    return array->head + array->length * array->elementSize;
}

ZR_FORCE_INLINE void ZrArrayPush(SZrState *state, SZrArray *array, TZrPtr element) {
    ZR_ASSERT(array->head != ZR_NULL);
    SZrGlobalState *global = state->global;
    if (array->length == array->capacity) {
        TZrSize previousCapacity = array->capacity;
        // KEEP AT LEAST INCREASING 1 ELEMENT
        TZrSize toIncrease = array->capacity * ZR_MATH_MAX(ZR_ARRAY_INCREASEMENT_MULTIPLIER_PERCENT, 100) / 100 + 1;
        array->capacity = toIncrease;
        array->head = ZR_CAST_UINT8_PTR(ZrMemoryAllocate(global, array->head, previousCapacity * array->elementSize,
            array->capacity * array->elementSize));
    }
    ZrMemoryRawCopy(array->head + array->length * array->elementSize, element, array->elementSize);
    array->length++;
}

ZR_FORCE_INLINE void ZrArrayEmpty(SZrArray *array) {
    array->length = 0;
}

ZR_FORCE_INLINE void ZrArrayFree(SZrState *state, SZrArray *array) {
    ZR_ASSERT(array->head != ZR_NULL);
    SZrGlobalState *global = state->global;
    ZrMemoryRawFree(global, array->head, array->capacity * array->elementSize);
    array->isValid = ZR_FALSE;
}

ZR_FORCE_INLINE void ZrArrayAppend(SZrState *state, SZrArray *array, TZrPtr elements, TZrSize length) {
    ZR_ASSERT(array->head != ZR_NULL);
    SZrGlobalState *global = state->global;
    if (array->length + length > array->capacity) {
        TZrSize previousCapacity = array->capacity;
        // KEEP AT LEAST INCREASING 1 ELEMENT
        TZrSize toIncrease = array->capacity * ZR_MATH_MAX(ZR_ARRAY_INCREASEMENT_MULTIPLIER_PERCENT, 100) / 100 + 1;
        array->capacity = ZR_MATH_MAX(toIncrease, array->length + length);
        array->head = ZR_CAST_UINT8_PTR(ZrMemoryAllocate(global, array->head, previousCapacity * array->elementSize,
            array->capacity * array->elementSize));
    }
    ZrMemoryRawCopy(array->head + array->length * array->elementSize, elements,
                    length * array->elementSize);
    array->length += length;
}

#endif //ZR_VM_CORE_LIST_H
