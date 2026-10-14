/**
 * @file process.c
 * @brief `zr.system.process` 的宿主进程回调实现。
 * @details 两个公开回调由 process_registry.c 中的静态函数描述符间接注册，并借用本次调用的
 *          context/result。Sleep 在 Windows/POSIX 阻塞调用线程；Emscripten 配置异步转换后会让出
 *          event loop。参数错误通过 VM 异常上报，具体语义见各函数注释。
 * @see zr_vm_lib_system/include/zr_vm_lib_system/process.h, process_registry.c
 */

#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
/** @brief 在 Linux 系统头包含前启用 POSIX.1-2008 声明，以公开 nanosleep；保留构建已指定的值。 */
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

/**
 * ## Summary
 * 实现 `process.h` 的原生 sleep 回调：读取第一个数值参数，在平台等待后向结果槽写入 VM null。
 * Windows/POSIX 等待阻塞调用线程；Emscripten 的伪同步等待会让出事件循环。
 *
 * ## Parameters
 * @param[in] context 仅本次调用有效的借用上下文；从位置 0 读取整数接口接受的数值，注册描述符
 *                    要求恰好一个显式参数；注册分派器在进入回调前检查固定元数。
 * @param[out] result 分派器提供的借用结果槽；仅在平台等待调用返回后写入 null，回调不保留该指针。
 *
 * ## Returns
 * @return context/result 为空时返回 ZR_FALSE。注册分派先验证恰好一个实参，失败在调用前抛元数异常；
 *         回调内 ReadInt 对非数值或直接 C 调用缺参抛 VM 参数异常。有效输入经平台等待后返回 ZR_TRUE，
 *         即使 POSIX `nanosleep` 报错或提前结束也不会由此回调报告失败。
 *
 * ## Constraints and lifecycle
 * 直接调用者也必须提供非空 context/result 和可接受的第一个数值参数；缺参或错类型走 VM 参数异常。
 * 负毫秒钳制为零。Windows/POSIX 调用阻塞 OS 线程；注册描述符的 `dispatchFlags` 为零，分派器在
 * 等待期间保持 GC-aware native 状态，其他 mutator 申请全局 stop-the-world 时可能被此阻塞线程延迟。
 * TODO: 核对 Windows/POSIX 长等待期间是否应支持让出 GC 域。Emscripten 只有 Asyncify/JSPI 配置下
 * 才能使伪同步 `emscripten_sleep` 暂停并续行；等待期间浏览器 event loop 会运行，事件可重入编译代码。
 * TODO: 在本仓实际 Emscripten 构建/测试入口核对所用配置、重入安全，以及挂起恢复时借用的 context/result
 * 是否仍有效；当前 checkout 未声明该配置。回调不释放借用指针，仅额外保存整数标量。
 *
 * ## References
 * 公开契约见 `process.h`；回调指针来自 `process_registry.c`，参数读取和执行通过原生分派器完成。
 */
TZrBool ZrSystem_Process_SleepMilliseconds(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrInt64 milliseconds = 0;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_CallContext_ReadInt(context, 0, &milliseconds)) {
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    /* BUG: DWORD 窄化按 2^32 取模；0xFFFFFFFF 变成 INFINITE，0x100000000 变成零，未兑现输入时长。 */
    Sleep((DWORD)(milliseconds < 0 ? 0 : milliseconds));
#elif defined(__EMSCRIPTEN__)
    {
        TZrInt64 clampedMilliseconds = milliseconds < 0 ? 0 : milliseconds;
        /* BUG: unsigned int 窄化按 2^32 取模；0x100000000 变成零，未兑现输入时长。 */
        /* TODO: 对使用此分支的实际 toolchain 确认 Asyncify/JSPI 配置；本 checkout 的构建脚本未声明该前提。 */
        emscripten_sleep((unsigned int)clampedMilliseconds);
    }
#else
    {
        struct timespec sleepTime;
        TZrInt64 clampedMilliseconds = milliseconds < 0 ? 0 : milliseconds;
        /* TODO: 核对支持平台的 time_t 宽度；长毫秒值转为较窄 time_t 前没有可表示范围检查。 */
        sleepTime.tv_sec = (time_t)(clampedMilliseconds / 1000);
        sleepTime.tv_nsec = (long)((clampedMilliseconds % 1000) * 1000000);
        /* BUG: nanosleep 的 EINTR/其他错误未检查，剩余时长未重试；下方仍写 null 并报告成功。 */
        nanosleep(&sleepTime, ZR_NULL);
    }
#endif

    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 实现 `process.h` 的进程退出回调：读取第一个数值参数并交给 C 运行库 `exit`，从而终止宿主进程。
 *
 * ## Parameters
 * @param[in] context 仅本次调用有效的借用上下文；从位置 0 读取 TZrInt64，注册描述符要求一个
 *                    显式参数，正常分派器会先检查元数。
 * @param[in] result 原生分派器提供的结果槽；实现为兼容回调签名而忽略它，不读取、不写入。
 *
 * ## Returns
 * @return context 为空时返回 ZR_FALSE。注册数量错误先在分派器抛元数异常，回调不会执行；ReadInt 对非数值
 *         或直接 C 调用缺参抛 VM 参数异常。读取可接受数值后执行 `exit`，宿主进程终止且本函数不返回。
 *
 * ## Constraints and lifecycle
 * context/result 为分派器在本次回调期间借用的指针，回调不持有或释放 VM 对象；result 完全未使用。
 * 注册分派先验证数量；ReadInt 对非数值或绕过分派器的直接 C 调用缺参走 noreturn 异常，不是普通 bool 失败。
 * 成功读取后，`exit` 终止宿主进程，不会返回到分派器清理/返回阶段。TODO: 确认 `exitCode` 从
 * `TZrInt64` 转为 `int` 的范围契约；当前没有范围检查。
 *
 * ## References
 * 公开契约见 `process.h`；回调由 `process_registry.c` 注册，参数读取由原生分派器提供。
 */
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
