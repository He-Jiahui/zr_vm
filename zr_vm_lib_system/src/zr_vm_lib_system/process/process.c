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
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
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
    /* BUG: int64 毫秒值窄化为 DWORD，最大值变成 INFINITE，更大值回绕，不能保证所请求时长。 */
    Sleep((DWORD)(milliseconds < 0 ? 0 : milliseconds));
#elif defined(__EMSCRIPTEN__)
    {
        TZrInt64 clampedMilliseconds = milliseconds < 0 ? 0 : milliseconds;
        /* BUG: 该平台同样窄化为 unsigned int，超范围等待时长回绕。 */
        emscripten_sleep((unsigned int)clampedMilliseconds);
    }
#else
    {
        struct timespec sleepTime;
        TZrInt64 clampedMilliseconds = milliseconds < 0 ? 0 : milliseconds;
        sleepTime.tv_sec = (time_t)(clampedMilliseconds / 1000);
        sleepTime.tv_nsec = (long)((clampedMilliseconds % 1000) * 1000000);
        /* BUG: POSIX nanosleep 若被信号中断会提前返回；未读取剩余时长重试。 */
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
}
