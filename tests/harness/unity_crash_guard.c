#include "unity_crash_guard.h"

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime_support.h"
#include "unity_internals.h"

#if defined(_MSC_VER)
    #define ZR_TESTS_THREAD_LOCAL __declspec(thread)
#else
    #define ZR_TESTS_THREAD_LOCAL _Thread_local
#endif

#define ZR_TESTS_JUMP_BUFFER jmp_buf
#define ZR_TESTS_SETJMP(buffer) setjmp(buffer)
#define ZR_TESTS_LONGJMP(buffer, value) longjmp(buffer, value)

static ZrTestsUnityCrashInfo g_zr_tests_unity_last_crash_info = {0};
static int g_zr_tests_unity_handlers_installed = 0;
static ZR_TESTS_THREAD_LOCAL ZR_TESTS_JUMP_BUFFER g_zr_tests_unity_abort_frame;
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
    if (g_zr_tests_unity_abort_frame_active) {
        ZR_TESTS_LONGJMP(g_zr_tests_unity_abort_frame, 1);
    }
}

static void zr_tests_unity_reraise_default(int signalNumber) {
    signal(signalNumber, SIG_DFL);
    raise(signalNumber);
    abort();
}

static void zr_tests_unity_signal_handler(int signalNumber) {
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

    ZrTests_Runtime_ClearCrashState();
    Unity.CurrentTestFailed = expectedCrash ? 0 : 1;
    Unity.CurrentTestIgnored = 0;
    g_zr_tests_unity_expected_recovered_crash = 0;
}

int ZrTests_Unity_TestProtect(void) {
    int jumpStatus;

    zr_tests_unity_install_handlers_once();

    if (g_zr_tests_unity_skip_next_protect != 0) {
        g_zr_tests_unity_skip_next_protect = 0;
        zr_tests_unity_reset_pending_signal_state();
        return 0;
    }

    g_zr_tests_unity_abort_frame_active = 1;
    jumpStatus = ZR_TESTS_SETJMP(g_zr_tests_unity_abort_frame);
    if (jumpStatus == 0) {
        return 1;
    }

    if (g_zr_tests_unity_pending_signal == 0) {
        g_zr_tests_unity_abort_frame_active = 0;
        return 0;
    }

    zr_tests_unity_capture_recovered_crash((int)g_zr_tests_unity_pending_signal);
    zr_tests_unity_reset_pending_signal_state();
    g_zr_tests_unity_skip_next_protect = 1;
    return 0;
}

ZR_TESTS_NORETURN void ZrTests_Unity_TestAbort(void) {
    if (g_zr_tests_unity_abort_frame_active) {
        ZR_TESTS_LONGJMP(g_zr_tests_unity_abort_frame, 1);
    }

    abort();
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
