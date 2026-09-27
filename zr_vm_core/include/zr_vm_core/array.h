//
// Created by HeJiahui on 2025/6/20.
//

#ifndef ZR_VM_CORE_LIST_H
#define ZR_VM_CORE_LIST_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/math.h"
#include "zr_vm_core/memory.h"
/** @brief 将尚未持有缓冲区的数组置为未初始化状态，供后续 Init 或安全清理使用。
 *  @pre array 尚未拥有由 Init 分配的缓冲区；传入 NULL 时不执行操作。 */
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

/** @brief 为定长元素的原生数组建立空缓冲区；零容量按一个元素分配。
 *  @pre state、state->global 和 array 有效，elementSize 非零，array 未持有旧缓冲区。
 *  @note 缓冲区归数组所有；元素内部指针及其指向的对象仍归调用方管理。 */
ZR_FORCE_INLINE void ZrCore_Array_Init(SZrState *state, SZrArray *array, TZrSize elementSize, TZrSize capacity) {
    if (capacity <= 0) {
        capacity = 1;
    }
    ZR_ASSERT(array != ZR_NULL && elementSize != 0);
    // BUG: capacity 与 elementSize 均为 size_t；乘积回绕时会分配过小的缓冲区，随后 Push 可越界写入。
    // 例如 capacity=SIZE_MAX/4+2、elementSize=4 只申请 4 字节，却记录了多个可用元素。
    // BUG: RawMallocWithType 可返回 NULL；这里仍记录正容量并标为有效，未检查结果的调用方会继续写空指针。
    array->head = ZR_CAST_UINT8_PTR(
            ZrCore_Memory_RawMallocWithType(state->global, capacity * elementSize, ZR_MEMORY_NATIVE_TYPE_ARRAY));
    array->elementSize = elementSize;
    array->length = 0;
    array->capacity = capacity;
    array->isValid = ZR_TRUE;
}

// TODO: Get 仅接受可变数组；只读调用方存在 (SZrArray *) 强转。核查是否需要只读 accessor 并迁移这些调用点。
/** @brief 借用指定元素的缓冲区地址，供调用方读取或就地修改。
 *  @pre array 已初始化且 index < length；该边界仅在调试构建中断言。
 *  @return 指向数组内部的地址；扩容、Free 后失效。 */
ZR_FORCE_INLINE TZrPtr ZrCore_Array_Get(SZrArray *array, TZrSize index) {
    ZR_ASSERT(index < array->length);
    return array->head + index * array->elementSize;
}

/** @brief 以字节复制覆盖已有元素，不释放旧元素内部指针，也不克隆新元素的指向对象。
 *  @pre array 已初始化，index < length，element 指向至少 elementSize 个可读字节且不与目标元素重叠；边界仅在调试构建中断言。 */
ZR_FORCE_INLINE void ZrCore_Array_Set(SZrArray *array, TZrSize index, TZrPtr element) {
    ZR_ASSERT(index < array->length);
    ZrCore_Memory_RawCopy(array->head + index * array->elementSize, element, array->elementSize);
}

/** @brief 移除末尾元素并借出其原缓冲区地址；不析构元素持有的对象。
 *  @pre array 已初始化且 length > 0；非空条件仅在调试构建中断言。
 *  @return 被移除元素的旧位置；下一次扩容、覆写该位置或 Free 后不得继续使用。 */
ZR_FORCE_INLINE TZrPtr ZrCore_Array_Pop(SZrArray *array) {
    ZR_ASSERT(array->length > 0);
    array->length--;
    return array->head + array->length * array->elementSize;
}

/** @brief 追加一个元素的字节副本，必要时经全局分配器扩容。
 *  @pre state、array 和缓冲区有效，length <= capacity；element 在复制期间有效，扩容时不得指向旧缓冲区。
 *  @note 只复制元素本身，不接管元素内部指针的所有权。 */
ZR_FORCE_INLINE void ZrCore_Array_Push(SZrState *state, SZrArray *array, TZrPtr element) {
    ZR_ASSERT(array->head != ZR_NULL);
    SZrGlobalState *global = state->global;
    if (array->length == array->capacity) {
        TZrSize previousCapacity = array->capacity;
        // KEEP AT LEAST INCREASING 1 ELEMENT
        // BUG: 增长乘法与字节数乘法均未检查 size_t 溢出；32 位容量 21474837、元素大小 1 时新容量回绕为 2。
        // 原缓冲可有 21474837 字节，扩容后复制却仍按旧长度定位，造成越界写入。
        TZrSize toIncrease = array->capacity * ZR_MATH_MAX(ZR_ARRAY_INCREASEMENT_MULTIPLIER_PERCENT, 100) / 100 + 1;
        // BUG: Allocate 可返回 NULL；先改容量再覆盖 head 会遗失原缓冲区，随后 RawCopy 向空指针写入。
        array->capacity = toIncrease;
        array->head =
                ZR_CAST_UINT8_PTR(ZrCore_Memory_Allocate(global, array->head, previousCapacity * array->elementSize,
                                                   array->capacity * array->elementSize, ZR_MEMORY_NATIVE_TYPE_ARRAY));
    }
    ZrCore_Memory_RawCopy(array->head + array->length * array->elementSize, element, array->elementSize);
    array->length++;
}

/** @brief 清空逻辑元素区以复用缓冲区；不释放缓冲区或元素内部持有的对象。
 *  @pre array 指向已初始化的数组；旧元素的外部资源由调用方先行处理。 */
ZR_FORCE_INLINE void ZrCore_Array_Empty(SZrArray *array) { array->length = 0; }

/** @brief 释放数组持有的原生缓冲区并清零数组状态；不遍历析构元素。
 *  @pre state 使用分配该缓冲区时的全局分配器；元素内部资源已由调用方处理。 */
ZR_FORCE_INLINE void ZrCore_Array_Free(SZrState *state, SZrArray *array) {
    // BUG: Init 分配失败后仍置 isValid，且 head 为 NULL；此处直接返回使无缓冲区的有效状态无法复位。
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

/** @brief 从独立源缓冲区追加多个元素的字节副本，必要时扩容目标。
 *  @pre state、array 和缓冲区有效，length <= capacity；elements 在复制期间可读且不与目标写入区重叠。
 *  @note 不克隆或释放元素内部指针；扩容时从本数组借出的源地址会失效。 */
ZR_FORCE_INLINE void ZrCore_Array_Append(SZrState *state, SZrArray *array, TZrPtr elements, TZrSize length) {
    ZR_ASSERT(array->head != ZR_NULL);
    SZrGlobalState *global = state->global;
    // TODO: 长度相加及增长、字节数计算未检查溢出；现行两处生产调用从已分配的独立源区复制。
    // 核查输入上界与不完整 SZrArray 是否允许传入，再判定有效调用能否触发回绕及应如何拒绝。
    if (array->length + length > array->capacity) {
        TZrSize previousCapacity = array->capacity;
        // KEEP AT LEAST INCREASING 1 ELEMENT
        TZrSize toIncrease = array->capacity * ZR_MATH_MAX(ZR_ARRAY_INCREASEMENT_MULTIPLIER_PERCENT, 100) / 100 + 1;
        // BUG: Allocate 失败可返回 NULL；先更新容量并覆盖 head 会遗失原缓冲区，非零长度复制随即写空指针。
        array->capacity = ZR_MATH_MAX(toIncrease, array->length + length);
        array->head =
                ZR_CAST_UINT8_PTR(ZrCore_Memory_Allocate(global, array->head, previousCapacity * array->elementSize,
                                                   array->capacity * array->elementSize, ZR_MEMORY_NATIVE_TYPE_ARRAY));
    }
    // TODO: 两个现行生产调用都从独立快照复制；核查公开 API 是否承诺支持从自身缓冲区追加。
    // 若要求支持，扩容后的旧 elements 指针与 memcpy 重叠规则都需要另行处理。
    ZrCore_Memory_RawCopy(array->head + array->length * array->elementSize, elements, length * array->elementSize);
    array->length += length;
}

#endif // ZR_VM_CORE_LIST_H
