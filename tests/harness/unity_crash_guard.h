#ifndef ZR_VM_TESTS_UNITY_CRASH_GUARD_H
#define ZR_VM_TESTS_UNITY_CRASH_GUARD_H

typedef struct ZrTestsUnityCrashInfo {
    int recovered;
    int signalNumber;
    int hadActiveVmState;
    int printedVmException;
} ZrTestsUnityCrashInfo;

int ZrTests_Unity_TestProtect_Begin(void);

void ZrTests_Unity_TestProtect_End(void);

void ZrTests_Unity_ExpectRecoveredCrash(void);

void ZrTests_Unity_ResetLastCrashInfo(void);

const ZrTestsUnityCrashInfo *ZrTests_Unity_GetLastCrashInfo(void);

#endif
