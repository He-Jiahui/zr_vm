#ifndef ZR_VM_TESTS_UNITY_CRASH_GUARD_H
#define ZR_VM_TESTS_UNITY_CRASH_GUARD_H

/** @brief 最近一次恢复信号及 VM 诊断是否可用，由 guard 持有。 */
typedef struct ZrTestsUnityCrashInfo {
    int recovered;
    int signalNumber;
    int hadActiveVmState;
    int printedVmException;
} ZrTestsUnityCrashInfo;

/** @brief 在 Unity AbortFrame 的 setjmp 前安装保护；仅在测试边界调用。 */
int ZrTests_Unity_TestProtect_Begin(void);

/** @brief 在正常或 longjmp 返回后结算信号并修复 Unity 测试状态。 */
void ZrTests_Unity_TestProtect_End(void);

/** @brief 将紧随其后的恢复信号视为当前负向测试的预期结果。 */
void ZrTests_Unity_ExpectRecoveredCrash(void);

/** @brief 开始新的崩溃恢复断言前清空进程级诊断快照。 */
void ZrTests_Unity_ResetLastCrashInfo(void);

/** @brief 借用 guard 持有的最近崩溃快照，后续恢复会覆盖它。 */
const ZrTestsUnityCrashInfo *ZrTests_Unity_GetLastCrashInfo(void);

#endif
