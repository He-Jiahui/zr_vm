/* ZrLibrary_Project_Run 在项目模块对象无法创建时的契约：入口不得执行，入口函数的根必须归还。
 * 这里只覆盖 ZrCore_Module_Create 返回空的路径，即 RawObject_New 因 GC 域不接收而拒绝模块对象；
 * 对象存储耗尽由 ZrCore_Memory_GcMalloc 抛出 MEMORY_ERROR，不会返回空，不在本文件范围内。 */
#include <stdio.h>
#include <string.h>

#include "unity.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser.h"

/* 与 global->compileSource 同形；包装器借此调用 parser 的真实编译入口。 */
typedef SZrFunction *(*FProjectRunCompileSource)(SZrState *state,
                                                 const TZrChar *source,
                                                 TZrSize sourceLength,
                                                 SZrString *sourceName);

/* 一次注入的全部状态。全局存储保证包装分配器的 userData 活到 tearDown 归还 VM 之后。 */
typedef struct ProjectRunModuleFailure {
    /* 安装包装器前的宿主分配器及其参数；未命中注入条件的请求原样转发。 */
    FZrAllocator upstreamAllocator;
    TZrPtr upstreamAllocationArguments;
    /* 被注入的主线程；只在模块对象存储存活期间摘除它的 GC 域。 */
    SZrState *state;
    /* 摘除期间保存的域；非空表示域尚未恢复，tearDown 必须先恢复再销毁 VM。 */
    struct SZrGcDomain *detachedDomain;
    /* 被拒绝的模块对象存储；RawObject_New 归还它时恢复域。 */
    TZrPtr rejectedModuleStorage;
    /* 一次性开关：命中模块对象申请后立即清除，后续对象照常分配。 */
    TZrBool rejectNextModuleObject;
    TZrUInt32 rejectedModuleObjectCount;
    TZrUInt32 returnedModuleStorageCount;

    FProjectRunCompileSource compileSource;
    /* 入口编译成功后才打开注入，避免编译期的对象申请被误拒。 */
    TZrBool rejectModuleAfterCompile;
    /* 用例自己再持有一个根，使 Run 归还 host root 后仍可安全读取入口函数。 */
    TZrBool leaseEntryAfterCompile;
    /* 模拟不带 host root 的编译结果，迫使 Run 自己把入口函数登记为 ignored。 */
    TZrBool dropHostRootAfterCompile;
    TZrUInt32 compileCount;
    /* 借用指针：只在用例持根或 GC 停止期间读取。 */
    SZrFunction *entryFunction;
    SZrGcRootLease entryLease;
} ProjectRunModuleFailure;

/* TryRun 回调与用例之间的交接；结果槽由用例栈借出。 */
typedef struct ProjectRunRequest {
    SZrTypeValue *result;
    EZrThreadStatus status;
} ProjectRunRequest;

static ProjectRunModuleFailure g_failure;
static SZrGlobalState *g_global;
static SZrState *g_state;
static TZrBool g_savedStopGcFlag;

/* 拒绝一次模块对象：存储照常取得，但在 RawObject_New 把对象交给 GC 域之前摘除域。
 * RawObject_New 随即归还该存储并返回空，本包装器在归还时恢复域，使之后的分配全部正常。 */
static TZrPtr project_run_module_allocator(TZrPtr userData,
                                           TZrPtr pointer,
                                           TZrSize originalSize,
                                           TZrSize newSize,
                                           TZrInt64 flag) {
    ProjectRunModuleFailure *failure = (ProjectRunModuleFailure *)userData;
    TZrPtr result;

    if (failure->detachedDomain != ZR_NULL && pointer != ZR_NULL &&
        pointer == failure->rejectedModuleStorage && newSize == 0u) {
        failure->state->gcDomain = failure->detachedDomain;
        failure->detachedDomain = ZR_NULL;
        failure->rejectedModuleStorage = ZR_NULL;
        failure->returnedModuleStorageCount++;
    }

    result = failure->upstreamAllocator(failure->upstreamAllocationArguments, pointer, originalSize, newSize, flag);

    if (failure->rejectNextModuleObject && pointer == ZR_NULL && result != ZR_NULL &&
        flag == ZR_MEMORY_NATIVE_TYPE_OBJECT && newSize == sizeof(SZrObjectModule)) {
        failure->rejectNextModuleObject = ZR_FALSE;
        failure->rejectedModuleStorage = result;
        failure->detachedDomain = failure->state->gcDomain;
        failure->state->gcDomain = ZR_NULL;
        failure->rejectedModuleObjectCount++;
    }
    return result;
}

/* 真实编译入口的包装：记录入口函数，并按用例要求调整它的根，再打开模块注入。 */
static SZrFunction *project_run_compile_entry(SZrState *state,
                                              const TZrChar *source,
                                              TZrSize sourceLength,
                                              SZrString *sourceName) {
    SZrFunction *function = g_failure.compileSource(state, source, sourceLength, sourceName);

    g_failure.compileCount++;
    if (function == ZR_NULL || !g_failure.rejectModuleAfterCompile) {
        return function;
    }

    g_failure.entryFunction = function;
    if (g_failure.leaseEntryAfterCompile) {
        (void)ZrCore_Gc_RootLeaseAcquire(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function), &g_failure.entryLease);
    }
    if (g_failure.dropHostRootAfterCompile) {
        ZrCore_Function_ReleaseHostRoot(state, function);
    }
    g_failure.rejectNextModuleObject = ZR_TRUE;
    return function;
}

/* 每例独立的项目 VM；包装器在未打开注入时只做转发。 */
void setUp(void) {
    char projectPath[512];

    memset(&g_failure, 0, sizeof(g_failure));
    g_global = ZR_NULL;
    g_state = ZR_NULL;

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/fixtures/projects/hello_world/hello_world.zrp",
             ZR_VM_TESTS_SOURCE_DIR);
    g_global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(g_global);
    g_savedStopGcFlag = g_global->garbageCollector->stopGcFlag;
    g_state = g_global->mainThreadState;
    TEST_ASSERT_NOT_NULL(g_state);

    ZrParser_ToGlobalState_Register(g_state);
    TEST_ASSERT_NOT_NULL(g_global->compileSource);
    g_failure.state = g_state;
    g_failure.compileSource = g_global->compileSource;
    ZrCore_GlobalState_SetCompileSource(g_global, project_run_compile_entry);

    g_failure.upstreamAllocator = g_global->upstreamAllocator;
    g_failure.upstreamAllocationArguments = g_global->upstreamAllocationArguments;
    g_global->upstreamAllocator = project_run_module_allocator;
    g_global->upstreamAllocationArguments = &g_failure;
}

/* 断言失败会长跳转到这里：先撤销注入并恢复域和分配器，再销毁 VM。 */
void tearDown(void) {
    if (g_global == ZR_NULL) {
        return;
    }

    g_failure.rejectNextModuleObject = ZR_FALSE;
    if (g_failure.detachedDomain != ZR_NULL) {
        g_failure.state->gcDomain = g_failure.detachedDomain;
        g_failure.detachedDomain = ZR_NULL;
    }
    ZrCore_Gc_RootLeaseRelease(g_global, &g_failure.entryLease);
    g_global->garbageCollector->stopGcFlag = g_savedStopGcFlag;
    if (g_failure.upstreamAllocator != ZR_NULL) {
        g_global->upstreamAllocator = g_failure.upstreamAllocator;
        g_global->upstreamAllocationArguments = g_failure.upstreamAllocationArguments;
    }
    ZrLibrary_CommonState_CommonGlobalState_Free(g_global);
    g_global = ZR_NULL;
    g_state = ZR_NULL;
}

static void project_run_body(SZrState *state, TZrPtr arguments) {
    ProjectRunRequest *request = (ProjectRunRequest *)arguments;

    request->status = ZrLibrary_Project_Run(state, request->result);
}

/* 与现有工程运行用例一样在异常边界内调用；Run 应当返回状态，而不是越过公共入口抛出。 */
static EZrThreadStatus run_project(SZrTypeValue *result) {
    ProjectRunRequest request;
    EZrThreadStatus boundaryStatus;

    ZrCore_Value_ResetAsNull(result);
    request.result = result;
    request.status = ZR_THREAD_STATUS_INVALID;
    boundaryStatus = ZrCore_Exception_TryRun(g_state, project_run_body, &request);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_THREAD_STATUS_FINE,
                                  boundaryStatus,
                                  "ZrLibrary_Project_Run must return its status instead of throwing");
    return request.status;
}

static void assert_entry_result(const SZrTypeValue *result) {
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result->type);
    TEST_ASSERT_NOT_NULL(result->value.object);
    TEST_ASSERT_EQUAL_STRING("hello world",
                             ZrCore_String_GetNativeString(ZR_CAST_STRING(g_state, result->value.object)));
}

/* 模块对象恰好被拒绝一次且域已恢复；否则后面的失败断言不能归因于模块创建。 */
static void assert_module_object_was_rejected_once(void) {
    TEST_ASSERT_EQUAL_UINT32(1u, g_failure.rejectedModuleObjectCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_failure.returnedModuleStorageCount);
    TEST_ASSERT_NULL(g_failure.detachedDomain);
    TEST_ASSERT_FALSE(g_failure.rejectNextModuleObject);
}

/* 入口没有执行：结果保持空值，线程状态与返回值都报告失败，并留下可供宿主输出的异常。 */
static void assert_project_run_stopped(EZrThreadStatus status, const SZrTypeValue *result) {
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, status);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, g_state->threadStatus);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, result->type);
    TEST_ASSERT_TRUE(g_state->hasCurrentException);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, g_state->currentExceptionStatus);
}

/* 对照：包装器未打开注入时，同一夹具照常创建模块并返回入口结果。 */
static void test_project_run_executes_entry_when_project_module_is_created(void) {
    SZrTypeValue result;

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, run_project(&result));
    assert_entry_result(&result);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, g_state->threadStatus);
    TEST_ASSERT_EQUAL_UINT32(1u, g_failure.compileCount);
    TEST_ASSERT_EQUAL_UINT32(0u, g_failure.rejectedModuleObjectCount);
}

/* 模块创建失败时不得执行入口；编译结果自带的 host root 必须归还，之后同一 VM 可以重试。 */
static void test_project_run_stops_and_releases_host_root_when_project_module_is_rejected(void) {
    SZrTypeValue result;
    EZrThreadStatus status;

    g_failure.rejectModuleAfterCompile = ZR_TRUE;
    g_failure.leaseEntryAfterCompile = ZR_TRUE;
    status = run_project(&result);

    TEST_ASSERT_EQUAL_UINT32(1u, g_failure.compileCount);
    assert_module_object_was_rejected_once();
    assert_project_run_stopped(status, &result);

    /* 只剩用例自己的根：Run 已归还 host root，也没有留下显式 ignore。 */
    TEST_ASSERT_NOT_NULL(g_failure.entryFunction);
    TEST_ASSERT_NOT_NULL(g_failure.entryLease.object);
    TEST_ASSERT_NULL(g_failure.entryFunction->hostRootOwner);
    TEST_ASSERT_FALSE(g_failure.entryFunction->super.garbageCollectMark.explicitIgnoredRoot);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)g_failure.entryFunction->super.garbageCollectMark.temporaryRootRefCount);
    ZrCore_Gc_RootLeaseRelease(g_global, &g_failure.entryLease);
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(
            g_global, ZR_CAST_RAW_OBJECT_AS_SUPER(g_failure.entryFunction)));
    g_failure.entryFunction = ZR_NULL;

    /* 宿主按 CLI 的做法清掉失败状态后重试；失败路径没有留下阻碍再次运行的状态。 */
    g_failure.rejectModuleAfterCompile = ZR_FALSE;
    g_failure.leaseEntryAfterCompile = ZR_FALSE;
    ZrCore_State_ResetThread(g_state, status);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, run_project(&result));
    assert_entry_result(&result);
    TEST_ASSERT_EQUAL_UINT32(2u, g_failure.compileCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_failure.rejectedModuleObjectCount);
}

/* 编译结果不带 host root 时由 Run 自己登记 ignore；模块创建失败必须把这次登记撤销。 */
static void test_project_run_unignores_entry_function_when_project_module_is_rejected(void) {
    SZrTypeValue result;
    EZrThreadStatus status;

    /* 失败返回后入口函数不再有任何根；停掉 GC 才能在返回后读取它的标记。 */
    g_global->garbageCollector->stopGcFlag = ZR_TRUE;
    g_failure.rejectModuleAfterCompile = ZR_TRUE;
    g_failure.dropHostRootAfterCompile = ZR_TRUE;
    status = run_project(&result);

    TEST_ASSERT_EQUAL_UINT32(1u, g_failure.compileCount);
    assert_module_object_was_rejected_once();
    assert_project_run_stopped(status, &result);

    TEST_ASSERT_NOT_NULL(g_failure.entryFunction);
    TEST_ASSERT_NULL(g_failure.entryFunction->hostRootOwner);
    TEST_ASSERT_FALSE(g_failure.entryFunction->super.garbageCollectMark.explicitIgnoredRoot);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)g_failure.entryFunction->super.garbageCollectMark.temporaryRootRefCount);
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(
            g_global, ZR_CAST_RAW_OBJECT_AS_SUPER(g_failure.entryFunction)));
    g_failure.entryFunction = ZR_NULL;
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_project_run_executes_entry_when_project_module_is_created);
    RUN_TEST(test_project_run_stops_and_releases_host_root_when_project_module_is_rejected);
    RUN_TEST(test_project_run_unignores_entry_function_when_project_module_is_rejected);

    return UNITY_END();
}
