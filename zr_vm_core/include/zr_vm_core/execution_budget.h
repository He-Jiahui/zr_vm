#ifndef ZR_VM_CORE_EXECUTION_BUDGET_H
#define ZR_VM_CORE_EXECUTION_BUDGET_H

#include "zr_vm_core/conf.h"

struct SZrState;
struct SZrGlobalState;
struct SZrCallInfo;

typedef struct SZrExecutionCancelToken SZrExecutionCancelToken;

typedef enum EZrExecutionTermination {
    ZR_EXECUTION_TERMINATION_NONE = 0,
    ZR_EXECUTION_TERMINATION_INSTRUCTION_LIMIT = 1,
    ZR_EXECUTION_TERMINATION_DEADLINE = 2,
    ZR_EXECUTION_TERMINATION_CANCELLED = 3,
    ZR_EXECUTION_TERMINATION_HEAP_LIMIT = 4,
    ZR_EXECUTION_TERMINATION_NATIVE_CALL_LIMIT = 5,
    ZR_EXECUTION_TERMINATION_GC_TIME_LIMIT = 6
} EZrExecutionTermination;

typedef struct SZrExecutionBudget {
    TZrUInt64 maxInstructions;
    TZrUInt64 deadlineMicros;
    const SZrExecutionCancelToken *cancelToken;
    TZrBool hasInstructionLimit;
    TZrBool hasDeadline;
    TZrUInt64 executedInstructions;
    EZrExecutionTermination termination;
    TZrUInt64 maxHeapBytes;
    TZrUInt64 maxNativeCalls;
    TZrUInt64 maxGcMicros;
    TZrBool hasHeapLimit;
    TZrBool hasNativeCallLimit;
    TZrBool hasGcTimeLimit;
    TZrUInt64 peakHeapBytes;
    TZrUInt64 nativeCalls;
    TZrUInt64 gcMicros;
    TZrUInt64 gcStartedMicros;
    TZrUInt32 gcDepth;
    struct SZrCallInfo *countedNativeFrame;
} SZrExecutionBudget;

/* Tokens are one-shot. Only Cancel/IsCancelled may run concurrently with a call. */
ZR_CORE_API SZrExecutionCancelToken *ZrCore_ExecutionCancelToken_New(void);
ZR_CORE_API void ZrCore_ExecutionCancelToken_Cancel(SZrExecutionCancelToken *token);
ZR_CORE_API TZrBool ZrCore_ExecutionCancelToken_IsCancelled(const SZrExecutionCancelToken *token);
ZR_CORE_API void ZrCore_ExecutionCancelToken_Free(SZrExecutionCancelToken *token);

/* Monotonic microseconds, independent of wall-clock adjustments. */
ZR_CORE_API TZrUInt64 ZrCore_ExecutionBudget_NowMicros(void);

/* Admission occurs before each dispatched bytecode instruction. Native polls do
 * not consume instructions. Termination is sticky until the host detaches the
 * scope, and returns normally so foreign native callback stacks can unwind. */
ZR_CORE_API TZrBool ZrCore_ExecutionBudget_Poll(struct SZrState *state, TZrBool consumeInstruction);
ZR_CORE_API void ZrCore_ExecutionBudget_UnwindVmFrames(struct SZrState *state);
ZR_CORE_API TZrBool ZrCore_ExecutionBudget_NativeEnter(struct SZrState *state, TZrBool throughBinding);
ZR_CORE_API void ZrCore_ExecutionBudget_GcBegin(struct SZrState *state);
ZR_CORE_API void ZrCore_ExecutionBudget_GcEnd(struct SZrState *state);

/* Global allocator requested-byte accounting includes transient allocations and
 * excludes allocations performed outside this VM allocator. Heap excess is
 * sticky at the next dispatch/native/GC boundary; allocation itself still
 * succeeds, so a single allocation or cooperative native call may overshoot. */
ZR_CORE_API TZrPtr ZrCore_ExecutionBudget_Allocate(TZrPtr userData, TZrPtr pointer,
        TZrSize originalSize, TZrSize newSize, TZrInt64 flag);
ZR_CORE_API TZrUInt64 ZrCore_ExecutionBudget_BeginMemory(struct SZrGlobalState *global);
ZR_CORE_API TZrUInt64 ZrCore_ExecutionBudget_MemoryPeak(const struct SZrGlobalState *global);

#endif
