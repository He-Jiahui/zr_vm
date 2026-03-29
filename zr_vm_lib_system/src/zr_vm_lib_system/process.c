//
// zr.system.process callbacks.
//

#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "zr_vm_lib_system/process.h"

#include <stdlib.h>
#include <time.h>

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
#endif

TZrBool ZrSystem_Process_SleepMilliseconds(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrInt64 milliseconds = 0;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_CallContext_ReadInt(context, 0, &milliseconds)) {
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    Sleep((DWORD)(milliseconds < 0 ? 0 : milliseconds));
#else
    {
        struct timespec sleepTime;
        TZrInt64 clampedMilliseconds = milliseconds < 0 ? 0 : milliseconds;
        sleepTime.tv_sec = (time_t)(clampedMilliseconds / 1000);
        sleepTime.tv_nsec = (long)((clampedMilliseconds % 1000) * 1000000);
        nanosleep(&sleepTime, ZR_NULL);
    }
#endif

    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrSystem_Process_Exit(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrInt64 exitCode = 0;

    ZR_UNUSED_PARAMETER(result);

    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_CallContext_ReadInt(context, 0, &exitCode)) {
        return ZR_FALSE;
    }

    exit((int)exitCode);
    return ZR_FALSE;
}
