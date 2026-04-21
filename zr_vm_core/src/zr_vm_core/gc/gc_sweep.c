//
// Sweep helpers for the GC.
//

#include "gc/gc_internal.h"

SZrRawObject **garbage_collector_sweep_list(SZrState *state, SZrRawObject **list, int maxCount, int *count) {
    SZrGlobalState *global = state->global;
    SZrRawObject **current = list;

    *count = 0;
    while (*current != ZR_NULL && *count < maxCount) {
        SZrRawObject *object = *current;

        if (object == ZR_NULL) {
            *current = ZR_NULL;
            break;
        }

        if ((TZrPtr)object < (TZrPtr)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND) {
            *current = ZR_NULL;
            break;
        }

        if (garbage_collector_object_is_unreferenced_fast(global->garbageCollector, object)) {
            SZrRawObject *next = object->next;
            TZrSize objectSize = garbage_collector_get_object_base_size_fast(object);

            *current = next;
            garbage_collector_free_object_sized(state, object, objectSize);
            (*count)++;

            TZrMemoryOffset objectDebt = (TZrMemoryOffset)objectSize;
            if (objectDebt <= global->garbageCollector->gcDebtSize) {
                global->garbageCollector->gcDebtSize -= objectDebt;
            } else {
                global->garbageCollector->gcDebtSize = 0;
            }
        } else {
            current = &object->next;
        }
    }

    return current;
}

void garbage_collector_enter_sweep(SZrState *state) {
    SZrGlobalState *global = state->global;

    global->garbageCollector->gcObjectListSweeper = &global->garbageCollector->gcObjectList;
    global->garbageCollector->gcGeneration = ZR_GC_OTHER_GENERATION(global->garbageCollector);
}

TZrSize garbage_collector_run_a_few_finalizers(SZrState *state, int maxCount) {
    SZrGlobalState *global = state->global;
    SZrRawObject **current = &global->garbageCollector->waitToReleaseObjectList;
    int count = 0;

    while (*current != ZR_NULL && count < maxCount) {
        SZrRawObject *object = *current;

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
