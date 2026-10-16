/* 真实 Core 的构造失败回归：分配器跟踪尺寸、类别及未启动 state 的释放。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif

#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"

typedef struct Allocation {
    void *pointer;
    TZrSize size;
    TZrInt64 flag;
} Allocation;

static Allocation allocations[32768];
static size_t liveCount;
static TZrInt64 failureFlag;
static int failureEnabled;
static int failureTriggered;
static int allocatorError;
static SZrGlobalState unlaunchedGlobal;

static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t index = 0;
    void *next;
    (void)data;
    if (old != NULL) {
        for (; index < liveCount && allocations[index].pointer != old; index++) {}
        if (index == liveCount || allocations[index].size != oldSize ||
            allocations[index].flag != flag) {
            allocatorError = 1;
            return NULL;
        }
    } else if (oldSize != 0) {
        allocatorError = 1;
        return NULL;
    }
    if (newSize == 0) {
        if (old != NULL) {
            free(old);
            allocations[index] = allocations[--liveCount];
        }
        return NULL;
    }
    if (failureEnabled && flag == failureFlag) {
        failureTriggered = 1;
        return NULL;
    }
    next = realloc(old, newSize);
    if (next == NULL) {
        return NULL;
    }
    if (old == NULL) {
        if (liveCount == sizeof(allocations) / sizeof(allocations[0])) abort();
        index = liveCount++;
    }
    allocations[index].pointer = next;
    allocations[index].size = newSize;
    allocations[index].flag = flag;
    return next;
}

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "line %d: %s (live=%zu allocatorError=%d)\n", \
                __LINE__, #condition, liveCount, allocatorError); \
        return 1; \
    } \
} while (0)

static int constructor_failure(TZrInt64 flag) {
    SZrGlobalState *global;
    failureEnabled = 1;
    failureFlag = flag;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    CHECK(failureTriggered);
    CHECK(global == NULL);
    CHECK(liveCount == 0 && !allocatorError);
    return 0;
}

static int unlaunched_state(void) {
    SZrState *state;
    unlaunchedGlobal.allocator = tracked_allocator;
    state = ZrCore_State_New(&unlaunchedGlobal);
    CHECK(state != NULL && state->stackBase.valuePointer == NULL);
    ZrCore_State_Free(&unlaunchedGlobal, state);
    CHECK(liveCount == 0 && !allocatorError);
    return 0;
}

static int worker_stack_oom_retry(void) {
    SZrGlobalState *global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    SZrState *state;
    size_t baseline;
    CHECK(global != NULL && !allocatorError);
    baseline = liveCount;
    state = ZrCore_State_New(global);
    CHECK(state != NULL && state->stackBase.valuePointer == NULL);
    failureEnabled = 1;
    failureFlag = ZR_MEMORY_NATIVE_TYPE_STACK;
    CHECK(!ZrCore_State_MutatorLaunch(state));
    CHECK(failureTriggered && state->stackBase.valuePointer == NULL &&
          state->stackTop.valuePointer == NULL && state->stackTail.valuePointer == NULL &&
          state->callInfoList == NULL);
    CHECK(liveCount == baseline + 1 && !allocatorError);
    failureEnabled = 0;
    CHECK(ZrCore_State_MutatorLaunch(state));
    CHECK(state->stackBase.valuePointer != NULL && state->callInfoList != NULL);
    ZrCore_State_MutatorExit(state);
    ZrCore_State_Free(global, state);
    CHECK(liveCount == baseline && !allocatorError);
    ZrCore_GlobalState_Free(global);
    CHECK(liveCount == 0 && !allocatorError);
    return 0;
}

static int stack_size_overflow(void) {
    SZrState *state;
    TZrStackPointer stack;
    size_t baseline;
    unlaunchedGlobal.allocator = tracked_allocator;
    state = ZrCore_State_New(&unlaunchedGlobal);
    CHECK(state != NULL);
    baseline = liveCount;
    stack.valuePointer = NULL;
    CHECK(ZrCore_Stack_Construct(
                  state, &stack, SIZE_MAX / sizeof(SZrTypeValueOnStack) + 1) == NULL);
    CHECK(stack.valuePointer == NULL && liveCount == baseline && !allocatorError);
    ZrCore_State_Free(&unlaunchedGlobal, state);
    CHECK(liveCount == 0 && !allocatorError);
    return 0;
}

int main(int argc, char **argv) {
    int result;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) return 2;
    if (strcmp(argv[1], "main_state_oom") == 0) {
        result = constructor_failure(ZR_MEMORY_NATIVE_TYPE_STATE);
    } else if (strcmp(argv[1], "import_array_oom") == 0) {
        result = constructor_failure(ZR_MEMORY_NATIVE_TYPE_ARRAY);
    } else if (strcmp(argv[1], "main_stack_oom") == 0) {
        result = constructor_failure(ZR_MEMORY_NATIVE_TYPE_STACK);
    } else if (strcmp(argv[1], "unlaunched_state") == 0) {
        result = unlaunched_state();
    } else if (strcmp(argv[1], "worker_stack_oom_retry") == 0) {
        result = worker_stack_oom_retry();
    } else if (strcmp(argv[1], "stack_size_overflow") == 0) {
        result = stack_size_overflow();
    } else {
        return 2;
    }
    while (liveCount > 0) free(allocations[--liveCount].pointer);
    if (result == 0) printf("PASS %s\n", argv[1]);
    return result;
}
