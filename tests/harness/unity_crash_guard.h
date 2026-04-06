#ifndef ZR_VM_TESTS_UNITY_CRASH_GUARD_H
#define ZR_VM_TESTS_UNITY_CRASH_GUARD_H

#if defined(_MSC_VER)
    #define ZR_TESTS_NORETURN __declspec(noreturn)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define ZR_TESTS_NORETURN _Noreturn
#elif defined(__GNUC__) || defined(__clang__)
    #define ZR_TESTS_NORETURN __attribute__((noreturn))
#else
    #define ZR_TESTS_NORETURN
#endif

typedef struct ZrTestsUnityCrashInfo {
    int recovered;
    int signalNumber;
    int hadActiveVmState;
    int printedVmException;
} ZrTestsUnityCrashInfo;

int ZrTests_Unity_TestProtect(void);

ZR_TESTS_NORETURN void ZrTests_Unity_TestAbort(void);

void ZrTests_Unity_ExpectRecoveredCrash(void);

void ZrTests_Unity_ResetLastCrashInfo(void);

const ZrTestsUnityCrashInfo *ZrTests_Unity_GetLastCrashInfo(void);

#endif
