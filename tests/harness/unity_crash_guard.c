#include "unity_crash_guard.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime_support.h"
#include "unity_internals.h"

#define ZR_TESTS_THREAD_LOCAL ZR_THREAD_LOCAL

/* 最近诊断供后续 Unity 用例读取；信号与保护帧状态独立按线程保存。 */
static ZrTestsUnityCrashInfo g_zr_tests_unity_last_crash_info = {0};
static int g_zr_tests_unity_handlers_installed = 0;
static ZR_TESTS_THREAD_LOCAL volatile sig_atomic_t g_zr_tests_unity_abort_frame_active = 0;
static ZR_TESTS_THREAD_LOCAL volatile sig_atomic_t g_zr_tests_unity_pending_signal = 0;
static ZR_TESTS_THREAD_LOCAL int g_zr_tests_unity_skip_next_protect = 0;
static ZR_TESTS_THREAD_LOCAL int g_zr_tests_unity_expected_recovered_crash = 0;

static const char *zr_tests_unity_signal_name(int signalNumber) {
    switch (signalNumber) {
        case SIGABRT:
            return "SIGABRT";
        case SIGILL:
            return "SIGILL";
        case SIGFPE:
            return "SIGFPE";
        case SIGSEGV:
            return "SIGSEGV";
#ifdef SIGBUS
        case SIGBUS:
            return "SIGBUS";
#endif
        default:
            return "UNKNOWN";
    }
}

static void zr_tests_unity_reset_pending_signal_state(void) {
    g_zr_tests_unity_pending_signal = 0;
    g_zr_tests_unity_abort_frame_active = 0;
}

static void zr_tests_unity_interrupt_current_test(int signalNumber) {
    g_zr_tests_unity_pending_signal = signalNumber;
    /* 只跳到当前 Unity AbortFrame；无保护帧时交回默认信号处理。 */
    if (g_zr_tests_unity_abort_frame_active) {
        longjmp(Unity.AbortFrame, 1);
    }
}

static void zr_tests_unity_reraise_default(int signalNumber) {
    signal(signalNumber, SIG_DFL);
    raise(signalNumber);
    abort();
}

static void zr_tests_unity_signal_handler(int signalNumber) {
    /* TODO: Begin 先标记保护帧活跃，runner 随后才 setjmp；此间的异步信号可能跳向未建帧。
     * 现有用例只经 VM panic hook 恢复，需真实信号用例核查该窗口及库调用中断恢复。 */
    zr_tests_unity_interrupt_current_test(signalNumber);
    if (g_zr_tests_unity_abort_frame_active) {
        return;
    }
    zr_tests_unity_reraise_default(signalNumber);
}

static void zr_tests_unity_vm_panic_hook(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    zr_tests_unity_interrupt_current_test(SIGABRT);
}

static void zr_tests_unity_install_handlers_once(void) {
    if (g_zr_tests_unity_handlers_installed) {
        return;
    }

    /* TODO: 安装标记及进程级 signal handler 未同步；目前调用入口为
     * 单线程测试 runner，需在线程并发启动保护帧时核对注册竞态。 */
    ZrTests_Runtime_SetFatalCrashHook(zr_tests_unity_vm_panic_hook);
    signal(SIGABRT, zr_tests_unity_signal_handler);
    signal(SIGILL, zr_tests_unity_signal_handler);
    signal(SIGFPE, zr_tests_unity_signal_handler);
    signal(SIGSEGV, zr_tests_unity_signal_handler);
#ifdef SIGBUS
    signal(SIGBUS, zr_tests_unity_signal_handler);
#endif
    g_zr_tests_unity_handlers_installed = 1;
}

static void zr_tests_unity_capture_recovered_crash(int signalNumber) {
    TZrBool printedVmException = ZR_FALSE;
    int expectedCrash = g_zr_tests_unity_expected_recovered_crash;

    memset(&g_zr_tests_unity_last_crash_info, 0, sizeof(g_zr_tests_unity_last_crash_info));
    g_zr_tests_unity_last_crash_info.recovered = 1;
    g_zr_tests_unity_last_crash_info.signalNumber = signalNumber;

    fprintf(stderr,
            "[zr-tests] recovered fatal signal %s (%d) while running test %s.\n",
            zr_tests_unity_signal_name(signalNumber),
            signalNumber,
            Unity.CurrentTestName != NULL ? Unity.CurrentTestName : "<unknown>");

    if (ZrTests_Runtime_ReportCrashState(stderr, &printedVmException)) {
        g_zr_tests_unity_last_crash_info.hadActiveVmState = 1;
        g_zr_tests_unity_last_crash_info.printedVmException = printedVmException ? 1 : 0;
    } else {
        fputs("[zr-tests] no active zr vm state was recorded for this crash.\n", stderr);
        fflush(stderr);
    }

    /* longjmp 绕过执行函数的 scope End，这里统一丢弃悬空 VM 指针。 */
    ZrTests_Runtime_ClearCrashState();
    Unity.CurrentTestFailed = expectedCrash ? 0 : 1;
    Unity.CurrentTestIgnored = 0;
    g_zr_tests_unity_expected_recovered_crash = 0;
}

int ZrTests_Unity_TestProtect_Begin(void) {
    zr_tests_unity_install_handlers_once();

    if (g_zr_tests_unity_skip_next_protect != 0) {
        g_zr_tests_unity_skip_next_protect = 0;
        zr_tests_unity_reset_pending_signal_state();
        return 0;
    }

    g_zr_tests_unity_abort_frame_active = 1;
    return 1;
}

void ZrTests_Unity_TestProtect_End(void) {
    if (g_zr_tests_unity_pending_signal == 0) {
        g_zr_tests_unity_abort_frame_active = 0;
        return;
    }

    zr_tests_unity_capture_recovered_crash((int)g_zr_tests_unity_pending_signal);
    zr_tests_unity_reset_pending_signal_state();
    /* runner 的下一次 Begin 返回 0，跳过本次 tearDown 及其保护；崩溃状态已由 ClearCrashState 清理。 */
    g_zr_tests_unity_skip_next_protect = 1;
}

void ZrTests_Unity_ExpectRecoveredCrash(void) {
    g_zr_tests_unity_expected_recovered_crash = 1;
}

void ZrTests_Unity_ResetLastCrashInfo(void) {
    memset(&g_zr_tests_unity_last_crash_info, 0, sizeof(g_zr_tests_unity_last_crash_info));
    g_zr_tests_unity_expected_recovered_crash = 0;
}

const ZrTestsUnityCrashInfo *ZrTests_Unity_GetLastCrashInfo(void) {
    return &g_zr_tests_unity_last_crash_info;
}
