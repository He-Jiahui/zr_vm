//
// zr.system.gc callbacks.
//

#include "zr_vm_lib_system/gc.h"

#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"

TZrBool ZrSystem_Gc_Start(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL || context->state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    context->state->global->garbageCollector->stopGcFlag = ZR_FALSE;
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrSystem_Gc_Stop(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL || context->state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    context->state->global->garbageCollector->stopGcFlag = ZR_TRUE;
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrSystem_Gc_Step(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_GarbageCollector_GcStep(context->state);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrSystem_Gc_Collect(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_GarbageCollector_GcFull(context->state, ZR_TRUE);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}
