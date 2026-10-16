//
// Created by HeJiahui on 2025/6/20.
//

#ifndef ZR_VM_CORE_LIST_H
#define ZR_VM_CORE_LIST_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/exception.h"
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
 * @pre state、state->global 和 array 有效，elementSize 非零，array 未持有旧缓冲区。
 * @return 成功为 true；尺寸不可表示或 OOM 为 false，array 保持 Construct 空状态。
 * @note 缓冲区归数组所有；元素内部资源归调用方管理。不触发 GC 或异常传播，适合启动和宿主构造路径。 */
ZR_FORCE_INLINE TZrBool ZrCore_Array_TryInit(SZrState *state, SZrArray *array, TZrSize elementSize, TZrSize capacity) {
    TZrBytePtr head;
    ZR_ASSERT(array != ZR_NULL && elementSize != 0);
    ZrCore_Array_Construct(array);
    if (capacity == 0) {
        capacity = 1;
    }
    if (capacity > ZR_MAX_SIZE / elementSize) {
        return ZR_FALSE;
    }
    head = ZR_CAST_UINT8_PTR(ZrCore_Memory_RawMallocWithType(
            state->global, capacity * elementSize, ZR_MEMORY_NATIVE_TYPE_ARRAY));
    if (head == ZR_NULL) {
        return ZR_FALSE;
    }
    array->head = head;
    array->elementSize = elementSize;
    array->capacity = capacity;
    array->isValid = ZR_TRUE;
    return ZR_TRUE;
}

/** @brief 初始化数组；失败时传播 MEMORY_ERROR，要求调用方已建立异常恢复或 panic 策略。
 * @note 需要返回失败并清理局部资源的构造函数使用 TryInit；其他前置条件与 TryInit 一致。 */
ZR_FORCE_INLINE void ZrCore_Array_Init(SZrState *state, SZrArray *array, TZrSize elementSize, TZrSize capacity) {
    if (!ZrCore_Array_TryInit(state, array, elementSize, capacity)) {
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
    }
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

/** @brief 为 Push/Append 的完整目标长度预留容量，成功后才发布新 head/capacity。
 * @pre array 已初始化且 length <= capacity；required 是经过字节上界检查的总元素数。
 * @return 成功为 true，OOM 为 false；失败时旧块、容量和长度不变，不引入 GC 安全点。
 * 增长比例分解为整数和余数计算，避免 capacity * percent 在 32/64 位上回绕。 */
static ZR_FORCE_INLINE TZrBool zr_core_array_try_reserve(SZrState *state, SZrArray *array, TZrSize required) {
    TZrSize maximum = ZR_MAX_SIZE / array->elementSize;
    TZrSize percent = ZR_MATH_MAX(ZR_ARRAY_INCREASEMENT_MULTIPLIER_PERCENT, 100);
    TZrSize whole = percent / 100;
    TZrSize fraction = percent % 100;
    TZrSize extra;
    TZrSize capacity;
    TZrBytePtr head;
    if (required <= array->capacity) {
        return ZR_TRUE;
    }
    if (array->capacity > (maximum - 1) / whole) {
        capacity = maximum;
    } else {
        capacity = array->capacity * whole;
        extra = (array->capacity / 100) * fraction + (array->capacity % 100) * fraction / 100;
        capacity = extra >= maximum - capacity ? maximum : capacity + extra + 1;
    }
    if (capacity < required) {
        capacity = required;
    }
    head = ZR_CAST_UINT8_PTR(ZrCore_Memory_Allocate(
            state->global, array->head, array->capacity * array->elementSize,
            capacity * array->elementSize, ZR_MEMORY_NATIVE_TYPE_ARRAY));
    if (head == ZR_NULL) {
        return ZR_FALSE;
    }
    array->head = head;
    array->capacity = capacity;
    return ZR_TRUE;
}

/** @brief 预留总元素容量，不改变 length 或已有元素，不缩小现有缓冲区。
 * @pre array 已初始化且 length <= capacity；成功扩容后旧借用地址失效。
 * @return 成功为 true；字节尺寸不可表示或 OOM 为 false，旧数组保持不变。
 * @note 不触发 GC 或异常；供需要先预留、再转移其他资源所有权的事务使用。 */
ZR_FORCE_INLINE TZrBool ZrCore_Array_TryReserve(SZrState *state, SZrArray *array, TZrSize requiredCapacity) {
    ZR_ASSERT(array->head != ZR_NULL && array->elementSize != 0 && array->length <= array->capacity);
    if (requiredCapacity > ZR_MAX_SIZE / array->elementSize) {
        return ZR_FALSE;
    }
    return zr_core_array_try_reserve(state, array, requiredCapacity);
}

/** @brief 追加一个元素的字节副本，必要时经全局分配器扩容。
 * @pre state、array 和缓冲区有效，length <= capacity；element 在复制期间有效，扩容时不得指向旧缓冲区。
 * @return 成功为 true；溢出或 OOM 为 false，旧数组保持不变，不抛异常。
 * @note 只复制元素本身，不接管元素内部指针。 */
ZR_FORCE_INLINE TZrBool ZrCore_Array_TryPush(SZrState *state, SZrArray *array, TZrPtr element) {
    ZR_ASSERT(array->head != ZR_NULL && array->elementSize != 0 && array->length <= array->capacity);
    if (array->length == ZR_MAX_SIZE / array->elementSize) {
        return ZR_FALSE;
    }
    if (!zr_core_array_try_reserve(state, array, array->length + 1)) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawCopy(array->head + array->length * array->elementSize, element, array->elementSize);
    array->length++;
    return ZR_TRUE;
}

/** @brief 追加一个元素；失败传播 MEMORY_ERROR，其他契约与 TryPush 一致。 */
ZR_FORCE_INLINE void ZrCore_Array_Push(SZrState *state, SZrArray *array, TZrPtr element) {
    if (!ZrCore_Array_TryPush(state, array, element)) {
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
    }
}

/** @brief 清空逻辑元素区以复用缓冲区；不释放缓冲区或元素内部持有的对象。
 *  @pre array 指向已初始化的数组；旧元素的外部资源由调用方先行处理。 */
ZR_FORCE_INLINE void ZrCore_Array_Empty(SZrArray *array) { array->length = 0; }

/** @brief 释放数组持有的原生缓冲区并清零数组状态；不遍历析构元素。
 *  @pre state 使用分配该缓冲区时的全局分配器；元素内部资源已由调用方处理。 */
ZR_FORCE_INLINE void ZrCore_Array_Free(SZrState *state, SZrArray *array) {
    if (array == ZR_NULL) {
        return;
    }
    if (array->isValid && array->head != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global, array->head,
                array->capacity * array->elementSize, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    ZrCore_Array_Construct(array);
}

/** @brief 从独立源缓冲区追加多个元素的字节副本，必要时扩容目标。
 * @pre state、array 和缓冲区有效，length <= capacity；非零 length 时 elements 可读且不借自本数组。
 * @return 成功为 true；不可表示的总长度或 OOM 为 false，旧数组保持不变，不抛异常。
 * @note 不克隆或释放元素内部指针。零追加不访问 elements。扩容时自借用源地址会失效，因此不支持自追加。 */
ZR_FORCE_INLINE TZrBool ZrCore_Array_TryAppend(SZrState *state, SZrArray *array, TZrPtr elements, TZrSize length) {
    ZR_ASSERT(array->head != ZR_NULL && array->elementSize != 0 && array->length <= array->capacity);
    if (length == 0) {
        return ZR_TRUE;
    }
    if (length > ZR_MAX_SIZE / array->elementSize - array->length) {
        return ZR_FALSE;
    }
    if (!zr_core_array_try_reserve(state, array, array->length + length)) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawCopy(array->head + array->length * array->elementSize, elements, length * array->elementSize);
    array->length += length;
    return ZR_TRUE;
}

/** @brief 追加多个元素；失败传播 MEMORY_ERROR，其他契约与 TryAppend 一致。 */
ZR_FORCE_INLINE void ZrCore_Array_Append(SZrState *state, SZrArray *array, TZrPtr elements, TZrSize length) {
    if (!ZrCore_Array_TryAppend(state, array, elements, length)) {
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
    }
}

#endif // ZR_VM_CORE_LIST_H
