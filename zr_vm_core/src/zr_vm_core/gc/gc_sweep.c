/* GC 清扫与终结阶段的私有辅助入口，由完整回收和增量切片驱动。 */

#include "gc/gc_internal.h"

/* Native destructors may inspect keys/children in their object's graph. Run
 * them before any node in that graph can be swept, with weak strings intact.
 * Detach each callback before invoking it so teardown remains exactly once. */
TZrSize garbage_collector_finalize_unreachable(SZrState *state, TZrBool minor, TZrBool shutdown) {
    TZrSize work = 0u;
    SZrRawObject *object = state->global->garbageCollector->gcObjectList;
    while (object != ZR_NULL) {
        if (object->scanMarkGcFunction != ZR_NULL &&
            (object->type == ZR_RAW_OBJECT_TYPE_NATIVE_DATA ||
             object->type == ZR_RAW_OBJECT_TYPE_OBJECT || object->type == ZR_RAW_OBJECT_TYPE_ARRAY) &&
            (shutdown ||
             ((!minor || object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE) &&
              object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED &&
              object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN &&
              object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT))) {
            FRawObjectScanMarkGc finalize = object->scanMarkGcFunction;
            object->scanMarkGcFunction = ZR_NULL;
            finalize(state, object);
            work++;
        }
        object = object->next;
    }
    return work;
}

/* 从双指针游标续扫对象链；返回待处理链节点地址供下一切片恢复。 */
SZrRawObject **garbage_collector_sweep_list(SZrState *state, SZrRawObject **list, int maxCount, int *count) {
    SZrGlobalState *global = state->global;
    SZrRawObject **current = list;

    *count = 0;
    /* Every visited node consumes the slice, including surviving objects. */
    while (*current != ZR_NULL && *count < maxCount) {
        SZrRawObject *object = *current;

        if (object == ZR_NULL) {
            *current = ZR_NULL;
            break;
        }

        /* 此低地址哨兵只用于截断可疑链，不能证明其他指针有效。 */
        if ((TZrPtr)object < (TZrPtr)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND) {
            *current = ZR_NULL;
            break;
        }
        (*count)++;

        if (garbage_collector_object_is_unreferenced_fast(global->garbageCollector, object)) {
            SZrRawObject *next = object->next;
            TZrSize objectSize = garbage_collector_get_object_base_size_fast(object);

            /* BUG: 若 TryRun 内的 GC box drop 失败或宿主扫描/布局回调抛异常，
             * 此处先摘链再释放，非局部跳转将跳过债务及 GC 暂停清理。 */
            *current = next;
            garbage_collector_free_object_sized(state, object, objectSize);

            /* 清扫债务只扣除已释放对象大小，不让当前债务变成负数。 */
            TZrMemoryOffset objectDebt = objectSize > (TZrSize)ZR_MAX_MEMORY_OFFSET
                                                ? ZR_MAX_MEMORY_OFFSET : (TZrMemoryOffset)objectSize;
            ZrCore_GarbageCollector_AddDebtSpace(global, -objectDebt);
        } else {
            /* Survivors become white in the current epoch. Leaving them black
             * makes later incremental cycles retain every previous survivor. */
            if (object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT) {
                object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
                object->garbageCollectMark.generation = global->garbageCollector->gcGeneration;
            }
            current = &object->next;
        }
    }

    return current;
}

/* 标记 sweep 游标从主对象链表头开始，后续单步驱动负责推进。 */
void garbage_collector_enter_sweep(SZrState *state) {
    SZrGlobalState *global = state->global;

    global->garbageCollector->gcObjectListSweeper = &global->garbageCollector->gcObjectList;
}

/* 处理等待终结队列并转入 released 队列；返回折算后的 GC 工作量。 */
TZrSize garbage_collector_run_a_few_finalizers(SZrState *state, int maxCount) {
    SZrGlobalState *global = state->global;
    SZrRawObject **current = &global->garbageCollector->waitToReleaseObjectList;
    int count = 0;

    while (*current != ZR_NULL && count < maxCount) {
        SZrRawObject *object = *current;

        /* TODO: 仓内尚未找到 waitToReleaseObjectList 的插入路径；若启用，
         * 必须核对回调异常后摘链对象的恢复或清理责任。 */
        *current = object->next;
        if (object->scanMarkGcFunction != ZR_NULL) {
            object->scanMarkGcFunction(state, object);
        }

        switch (object->type) {
            case ZR_RAW_OBJECT_TYPE_THREAD:
                if (global->callbacks.beforeThreadReleased != ZR_NULL) {
                    global->callbacks.beforeThreadReleased(state, ZR_CAST(SZrState *, object));
                }
                break;
            case ZR_RAW_OBJECT_TYPE_NATIVE_DATA:
            case ZR_RAW_OBJECT_TYPE_OBJECT:
            default:
                break;
        }

        object->next = global->garbageCollector->releasedObjectList;
        global->garbageCollector->releasedObjectList = object;
        ZrCore_RawObject_MarkAsReleased(object);
        count++;
    }

    return count * ZR_GC_FINALIZER_WORK_COST;
}
