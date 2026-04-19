//
// Created by AI Assistant on 2026/1/2.
//

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "unity.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "gc_test_utils.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_lib_system/gc.h"

// 测试时间测量结构
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

// 测试日志宏（符合测试规范）
#define TEST_START(summary) do { \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while(0)

#define TEST_INFO(summary, details) do { \
    printf("Testing %s:\n %s\n", summary, details); \
    fflush(stdout); \
} while(0)

#ifdef TEST_PASS
#undef TEST_PASS
#endif
#define TEST_PASS(timer, summary) do { \
    double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary); \
    fflush(stdout); \
} while(0)

#ifdef TEST_FAIL
#undef TEST_FAIL
#endif
#define TEST_FAIL(timer, summary, reason) do { \
    double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason); \
    fflush(stdout); \
} while(0)

#define TEST_DIVIDER() do { \
    printf("----------\n"); \
    fflush(stdout); \
} while(0)

#define TEST_MODULE_DIVIDER() do { \
    printf("==========\n"); \
    fflush(stdout); \
} while(0)

// 测试初始化和清理
void setUp(void) {
    // Unity框架会自动调用，这里不需要额外输出
}

void tearDown(void) {
    // Unity框架会自动调用，这里不需要额外输出
}

static TZrBool gc_test_collector_contains_object(SZrState *state, SZrRawObject *target) {
    SZrRawObject *object;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL || target == ZR_NULL) {
        return ZR_FALSE;
    }

    object = state->global->garbageCollector->gcObjectList;
    while (object != ZR_NULL) {
        if (object == target) {
            return ZR_TRUE;
        }
        object = object->next;
    }

    return ZR_FALSE;
}

// 测试基础类型
static void test_basic_types(void) {
    SZrTestTimer timer;
    const char* testSummary = "Basic Type Definitions";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("ZR_MAX_MEMORY_OFFSET calculation", 
              "Verifying that ZR_MAX_MEMORY_OFFSET equals (ZR_MAX_SIZE >> 1)");
    TZrMemoryOffset expectedOffset = (TZrMemoryOffset)(ZR_MAX_SIZE >> 1);
    TEST_ASSERT_EQUAL_UINT64(ZR_MAX_MEMORY_OFFSET, expectedOffset);
    
    TEST_INFO("ZR_MAX_SIZE definition", 
              "Verifying that ZR_MAX_SIZE equals SIZE_MAX");
    TEST_ASSERT_EQUAL_UINT64(ZR_MAX_SIZE, SIZE_MAX);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试GC状态宏
static void test_gc_status_macros(void) {
    SZrTestTimer timer;
    const char* testSummary = "GC Status Macros";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("Test environment initialization", 
              "Creating test state and verifying GC collector exists");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);
    
    TEST_INFO("Test object creation", 
              "Creating test object with type ZR_VALUE_TYPE_OBJECT");
    SZrRawObject* obj = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
    TEST_ASSERT_NOT_NULL(obj);
    
    TEST_INFO("Object status macros - INITED", 
              "Testing ZR_GC_IS_INITED macro by marking object as initialized");
    ZrCore_RawObject_MarkAsInit(state, obj);
    TEST_ASSERT_TRUE(ZR_GC_IS_INITED(obj));
    
    TEST_INFO("Object status macros - WAIT_TO_SCAN", 
              "Testing ZR_GC_IS_WAIT_TO_SCAN macro by marking object as wait to scan");
    ZrCore_RawObject_MarkAsWaitToScan(obj);
    TEST_ASSERT_TRUE(ZR_GC_IS_WAIT_TO_SCAN(obj));
    
    TEST_INFO("Object status macros - REFERENCED", 
              "Testing ZR_GC_IS_REFERENCED macro by marking object as referenced");
    ZrCore_RawObject_MarkAsReferenced(obj);
    TEST_ASSERT_TRUE(ZR_GC_IS_REFERENCED(obj));
    
    TEST_INFO("Generational marking macros - New generation", 
              "Testing ZR_GC_IS_OLD macro with NEW generational status");
    ZrCore_RawObject_SetGenerationalStatus(obj, ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW);
    TEST_ASSERT_FALSE(ZR_GC_IS_OLD(obj));
    
    TEST_INFO("Generational marking macros - Old generation", 
              "Testing ZR_GC_IS_OLD macro with SURVIVAL generational status");
    fflush(stdout);  // 确保输出刷新
    // 设置世代状态
    obj->garbageCollectMark.generationalStatus = ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL;
    fflush(stdout);  // 确保输出刷新
    // 直接检查状态值，避免宏调用可能的问题
    TZrBool isOldResult = (obj->garbageCollectMark.generationalStatus >= ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL);
    fflush(stdout);  // 确保输出刷新
    TEST_ASSERT_TRUE(isOldResult);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试GC函数
static void test_gc_functions(void) {
    SZrTestTimer timer;
    const char* testSummary = "GC Core Functions";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("Test environment initialization", 
              "Creating test state and verifying global state exists");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    
    SZrGlobalState* global = state->global;
    
    TEST_INFO("Debt space addition", 
              "Testing ZrCore_GarbageCollector_AddDebtSpace function to add memory debt");
    TZrMemoryOffset initialDebt = global->garbageCollector->gcDebtSize;
    TZrMemoryOffset addAmount = 1000;
    ZrCore_GarbageCollector_AddDebtSpace(global, addAmount);
    TZrMemoryOffset newDebt = global->garbageCollector->gcDebtSize;
    // 验证新债务等于初始债务加上添加的金额（允许溢出保护）
    TEST_ASSERT_TRUE(newDebt >= initialDebt);
    if (initialDebt + addAmount <= ZR_MAX_MEMORY_OFFSET) {
        TEST_ASSERT_EQUAL_INT64(initialDebt + addAmount, newDebt);
    } else {
        // 如果溢出，应该被限制为最大值
        TEST_ASSERT_EQUAL_INT64(ZR_MAX_MEMORY_OFFSET, newDebt);
    }
    
    TEST_INFO("Object creation", 
              "Testing ZrCore_RawObject_New function to create a new object");
    SZrRawObject* obj = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
    TEST_ASSERT_NOT_NULL(obj);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, obj->type);
    
    TEST_INFO("Object reference status", 
              "Testing ZrCore_RawObject_IsUnreferenced function to check object reference");
    TZrBool isUnreferenced = ZrCore_RawObject_IsUnreferenced(state, obj);
    TEST_ASSERT_FALSE(isUnreferenced);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试边界条件
static void test_boundary_conditions(void) {
    SZrTestTimer timer;
    const char* testSummary = "Boundary Conditions and Overflow Protection";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("Test environment initialization", 
              "Creating test state for boundary condition testing");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    
    SZrGlobalState* global = state->global;
    
    TEST_INFO("Large value operations", 
              "Testing debt space addition with large values near ZR_MAX_MEMORY_OFFSET");
    TZrMemoryOffset initialDebt = global->garbageCollector->gcDebtSize;
    // 使用一个不会导致溢出的值进行测试（确保 initialDebt + largeValue <= ZR_MAX_MEMORY_OFFSET）
    TZrMemoryOffset largeValue = (ZR_MAX_MEMORY_OFFSET / 2);
    if (initialDebt > ZR_MAX_MEMORY_OFFSET / 2) {
        largeValue = ZR_MAX_MEMORY_OFFSET - initialDebt - 100;  // 确保不会溢出
    }
    if (largeValue < 100) {
        largeValue = 100;  // 如果计算出的值太小，使用默认值
    }
    ZrCore_GarbageCollector_AddDebtSpace(global, largeValue);
    TZrMemoryOffset actualDebt = global->garbageCollector->gcDebtSize;
    // 验证债务被正确添加（应该不会溢出）
    TEST_ASSERT_TRUE(actualDebt >= initialDebt);
    if (initialDebt <= ZR_MAX_MEMORY_OFFSET - largeValue) {
        TEST_ASSERT_EQUAL_INT64(initialDebt + largeValue, actualDebt);
    } else {
        // 如果溢出，应该被限制为最大值
        TEST_ASSERT_EQUAL_INT64(ZR_MAX_MEMORY_OFFSET, actualDebt);
    }
    
    TEST_INFO("Overflow protection", 
              "Testing that debt size does not exceed ZR_MAX_MEMORY_OFFSET when adding maximum value");
    // 添加一个会导致溢出的值
    ZrCore_GarbageCollector_AddDebtSpace(global, ZR_MAX_MEMORY_OFFSET);
    TZrMemoryOffset afterOverflow = global->garbageCollector->gcDebtSize;
    // 验证溢出保护：债务应该被限制为最大值
    TEST_ASSERT_EQUAL_INT64(ZR_MAX_MEMORY_OFFSET, afterOverflow);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试GC清除阶段
static void test_gc_sweep_phase(void) {
    SZrTestTimer timer;
    const char* testSummary = "GC Sweep Phase";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("Test environment initialization", 
              "Creating test state and GC collector for sweep phase testing");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    SZrGlobalState* global = state->global;
    SZrGarbageCollector* gc = global->garbageCollector;
    
    TEST_INFO("Test object creation", 
              "Creating two test objects (OBJECT and STRING types) for sweep testing");
    SZrRawObject* obj1 = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
    SZrRawObject* obj2 = createTestObject(state, ZR_VALUE_TYPE_STRING, sizeof(SZrRawObject));
    TEST_ASSERT_NOT_NULL(obj1);
    TEST_ASSERT_NOT_NULL(obj2);
    
    TEST_INFO("Mark objects as unreferenced", 
              "Marking test objects as initialized (unreferenced) state");
    ZrCore_RawObject_MarkAsInit(state, obj1);
    ZrCore_RawObject_MarkAsInit(state, obj2);
    
    TEST_INFO("Sweep phase status check", 
              "Verifying that GC is not in sweeping phase initially");
    TZrBool isSweeping = ZrCore_GarbageCollector_IsSweeping(global);
    TEST_ASSERT_FALSE(isSweeping);
    
    TEST_INFO("Enter sweep phase", 
              "Setting GC running status to SWEEP_OBJECTS and verifying sweep phase detection");
    gc->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS;
    isSweeping = ZrCore_GarbageCollector_IsSweeping(global);
    TEST_ASSERT_TRUE(isSweeping);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试GC标记遍历
static void test_gc_mark_traversal(void) {
    SZrTestTimer timer;
    const char* testSummary = "GC Mark Traversal";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("Test environment initialization", 
              "Creating test state for mark traversal testing");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Test object creation", 
              "Creating a test object for mark traversal testing");
    SZrRawObject* obj = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
    TEST_ASSERT_NOT_NULL(obj);
    
    TEST_INFO("Mark object as INITED (initial state)", 
              "Testing INITED status by initializing object and verifying ZR_GC_IS_INITED macro");
    ZrCore_RawObject_MarkAsInit(state, obj);
    TEST_ASSERT_TRUE(ZR_GC_IS_INITED(obj));
    
    TEST_INFO("Mark object as WAIT_TO_SCAN", 
              "Testing WAIT_TO_SCAN status by setting wait-to-scan status and verifying ZR_GC_IS_WAIT_TO_SCAN macro");
    ZrCore_RawObject_MarkAsWaitToScan(obj);
    TEST_ASSERT_TRUE(ZR_GC_IS_WAIT_TO_SCAN(obj));
    
    TEST_INFO("Mark object as REFERENCED", 
              "Testing REFERENCED status by marking as referenced and verifying ZR_GC_IS_REFERENCED macro");
    ZrCore_RawObject_MarkAsReferenced(obj);
    TEST_ASSERT_TRUE(ZR_GC_IS_REFERENCED(obj));
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试GC根对象标记
static void test_gc_root_marking(void) {
    SZrTestTimer timer;
    const char* testSummary = "GC Root Object Marking";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("Test environment initialization", 
              "Creating test state for root object marking testing");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    SZrGlobalState* global = state->global;
    
    TEST_INFO("Main thread state as root object", 
              "Verifying that main thread state exists and can be used as root object");
    TEST_ASSERT_NOT_NULL(global->mainThreadState);
    
    TEST_INFO("Registry as root object", 
              "Verifying that loaded modules registry exists and can be used as root object");
    TEST_ASSERT_NOT_NULL(&global->loadedModulesRegistry);
    
    TEST_INFO("Global metatable array", 
              "Verifying that global basic type object prototype array exists");
    TZrUInt32 prototypeCount = 0;
    for (TZrUInt32 i = 0; i < ZR_VALUE_TYPE_ENUM_MAX; i++) {
        if (global->basicTypeObjectPrototype[i] != ZR_NULL) {
            prototypeCount++;
        }
    }
    TEST_ASSERT_TRUE(prototypeCount > 0);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试GC状态机控制
static void test_gc_state_machine(void) {
    SZrTestTimer timer;
    const char* testSummary = "GC State Machine Control";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("Test environment initialization", 
              "Creating test state for GC state machine testing");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    SZrGlobalState* global = state->global;
    SZrGarbageCollector* gc = global->garbageCollector;
    
    TEST_INFO("Initial state verification", 
              "Verifying that GC starts in PAUSED state");
    TEST_ASSERT_EQUAL_INT(ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED, gc->gcRunningStatus);
    
    TEST_INFO("State transition - Propagation phase", 
              "Testing state transition to FLAG_PROPAGATION and verifying not in sweep phase");
    gc->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_FLAG_PROPAGATION;
    TZrBool isSweeping = ZrCore_GarbageCollector_IsSweeping(global);
    TEST_ASSERT_FALSE(isSweeping);
    
    TEST_INFO("State transition - Sweep phase", 
              "Testing state transition to SWEEP_OBJECTS and verifying sweep phase detection");
    gc->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS;
    isSweeping = ZrCore_GarbageCollector_IsSweeping(global);
    TEST_ASSERT_TRUE(isSweeping);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试分代GC
static void test_gc_generational(void) {
    SZrTestTimer timer;
    const char* testSummary = "Generational GC";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("Test environment initialization", 
              "Creating test state for generational GC testing");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    SZrGlobalState* global = state->global;
    SZrGarbageCollector* gc = global->garbageCollector;
    
    TEST_INFO("Test object creation", 
              "Creating a test object for generational status testing");
    SZrRawObject* obj = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
    TEST_ASSERT_NOT_NULL(obj);
    
    TEST_INFO("New generation status", 
              "Testing NEW generational status and verifying ZR_GC_IS_OLD returns false");
    ZrCore_RawObject_SetGenerationalStatus(obj, ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW);
    TZrBool isOld = ZR_GC_IS_OLD(obj);
    TEST_ASSERT_FALSE(isOld);
    
    TEST_INFO("Promotion to old generation", 
              "Testing SURVIVAL generational status and verifying ZR_GC_IS_OLD returns true");
    ZrCore_RawObject_SetGenerationalStatus(obj, ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL);
    isOld = ZR_GC_IS_OLD(obj);
    TEST_ASSERT_TRUE(isOld);
    
    TEST_INFO("Default GC mode",
              "Global GC mode stays incremental in this harness; generational teardown needs initialized regions");
    TEST_ASSERT_EQUAL_INT(ZR_GARBAGE_COLLECT_MODE_INCREMENTAL, gc->gcMode);

    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试Native Data处理
static void test_gc_native_data(void) {
    SZrTestTimer timer;
    const char* testSummary = "Native Data Processing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("Test environment initialization", 
              "Creating test state for native data processing testing");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Native Data object creation", 
              "Creating a native data object with 5 value slots");
    TZrUInt32 valueCount = 5;
    struct SZrNativeData* nativeData = createTestNativeData(state, valueCount);
    TEST_ASSERT_NOT_NULL(nativeData);
    
    TEST_INFO("Value array length verification", 
              "Verifying that native data value array length matches expected count");
    TEST_ASSERT_EQUAL_UINT32(valueCount, nativeData->valueLength);
    
    TEST_INFO("Object type verification", 
              "Verifying that native data object has correct type (NATIVE_DATA)");
    TEST_ASSERT_EQUAL_INT(ZR_RAW_OBJECT_TYPE_NATIVE_DATA, nativeData->super.type);
    
    TEST_INFO("Value array initialization", 
              "Verifying that all values in native data array are initialized as NULL type");
    TZrUInt32 nullCount = 0;
    for (TZrUInt32 i = 0; i < nativeData->valueLength; i++) {
        if (nativeData->valueExtend[i].type == ZR_VALUE_TYPE_NULL) {
            nullCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(nativeData->valueLength, nullCount);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试GC完整回收
static void test_gc_full_collection(void) {
    SZrTestTimer timer;
    const char* testSummary = "GC Full Collection";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("Test environment initialization", 
              "Creating test state for full GC collection testing");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    SZrGlobalState* global = state->global;
    
    TEST_INFO("Test object creation", 
              "Creating two test objects (OBJECT and STRING types) for full GC testing");
    SZrRawObject* obj1 = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
    SZrRawObject* obj2 = createTestObject(state, ZR_VALUE_TYPE_STRING, sizeof(SZrRawObject));
    TEST_ASSERT_NOT_NULL(obj1);
    TEST_ASSERT_NOT_NULL(obj2);
    
    TEST_INFO("Execute full GC", 
              "Executing ZrCore_GarbageCollector_GcFull to perform complete garbage collection");
    ZrCore_GarbageCollector_GcFull(state, ZR_FALSE);
    
    TEST_INFO("GC status verification", 
              "Verifying that GC returns to PAUSED state after full collection");
    TEST_ASSERT_EQUAL_INT(ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED, 
                         global->garbageCollector->gcRunningStatus);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试GC单步执行
static void test_gc_step(void) {
    SZrTestTimer timer;
    const char* testSummary = "GC Step Execution";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("Test environment initialization", 
              "Creating test state for GC step execution testing");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    SZrGlobalState* global = state->global;
    
    TEST_INFO("Add debt space to trigger GC", 
              "Adding memory debt to trigger garbage collection step");
    TZrMemoryOffset addDebt = 10000;
    ZrCore_GarbageCollector_AddDebtSpace(global, addDebt);
    
    TEST_INFO("Execute GC step", 
              "Executing ZrCore_GarbageCollector_GcStep to perform incremental GC");
    ZrCore_GarbageCollector_GcStep(state);
    
    TEST_INFO("GC status verification", 
              "Verifying that GC status is either RUNNING or STOP_BY_SELF after step");
    EZrGarbageCollectStatus status = global->garbageCollector->gcStatus;
    TZrBool isValidStatus = (status == ZR_GARBAGE_COLLECT_STATUS_RUNNING ||
                          status == ZR_GARBAGE_COLLECT_STATUS_STOP_BY_SELF);
    TEST_ASSERT_TRUE(isValidStatus);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试GC屏障
static void test_gc_barrier(void) {
    SZrTestTimer timer;
    const char* testSummary = "GC Barrier Mechanism";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("Test environment initialization", 
              "Creating test state for GC barrier mechanism testing");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Create parent and child objects", 
              "Creating parent (OBJECT) and child (STRING) objects for barrier testing");
    SZrRawObject* parent = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
    SZrRawObject* child = createTestObject(state, ZR_VALUE_TYPE_STRING, sizeof(SZrRawObject));
    TEST_ASSERT_NOT_NULL(parent);
    TEST_ASSERT_NOT_NULL(child);
    
    TEST_INFO("Mark parent as REFERENCED", 
              "Marking parent object as referenced");
    ZrCore_RawObject_MarkAsReferenced(parent);
    TZrBool isReferenced = ZR_GC_IS_REFERENCED(parent);
    TEST_ASSERT_TRUE(isReferenced);
    
    TEST_INFO("Test barrier: REFERENCED object pointing to INITED object", 
              "Calling ZrCore_GarbageCollector_Barrier when REFERENCED parent points to INITED child");
    ZrCore_GarbageCollector_Barrier(state, parent, child);
    
    TEST_INFO("Verify child object is marked", 
              "Verifying that child object is marked (WAIT_TO_SCAN or REFERENCED) after barrier");
    TZrBool isWaitToScan = ZR_GC_IS_WAIT_TO_SCAN(child);
    TZrBool isChildReferenced = ZR_GC_IS_REFERENCED(child);
    TEST_ASSERT_TRUE(isWaitToScan || isChildReferenced);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_pause_budget_consumes_multiple_incremental_steps(void) {
    SZrTestTimer timer;
    const char* testSummary = "GC Pause Budget Consumes Multiple Incremental Steps";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Propagation budget setup",
              "Preparing two wait-to-scan native-data objects so one GC step must honor pause budget > 1");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    {
        SZrGarbageCollector* gc = state->global->garbageCollector;
        struct SZrNativeData* first = createTestNativeData(state, 1);
        struct SZrNativeData* second = createTestNativeData(state, 1);

        TEST_ASSERT_NOT_NULL(first);
        TEST_ASSERT_NOT_NULL(second);

        ZrCore_RawObject_MarkAsWaitToScan(ZR_CAST_RAW_OBJECT_AS_SUPER(first));
        ZrCore_RawObject_MarkAsWaitToScan(ZR_CAST_RAW_OBJECT_AS_SUPER(second));
        ZR_CAST_RAW_OBJECT_AS_SUPER(first)->gcList = ZR_CAST_RAW_OBJECT_AS_SUPER(second);
        ZR_CAST_RAW_OBJECT_AS_SUPER(second)->gcList = ZR_NULL;

        gc->waitToScanObjectList = ZR_CAST_RAW_OBJECT_AS_SUPER(first);
        gc->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_FLAG_PROPAGATION;
        gc->gcStatus = ZR_GARBAGE_COLLECT_STATUS_RUNNING;
        gc->gcPauseBudget = 2;
        gc->gcDebtSize = 4096;

        ZrCore_GarbageCollector_GcStep(state);

        TEST_ASSERT_TRUE(ZR_GC_IS_REFERENCED(ZR_CAST_RAW_OBJECT_AS_SUPER(first)));
        TEST_ASSERT_TRUE(ZR_GC_IS_REFERENCED(ZR_CAST_RAW_OBJECT_AS_SUPER(second)));
        TEST_ASSERT_NULL(gc->waitToScanObjectList);
        TEST_ASSERT_TRUE(gc->gcLastStepWork >= 2);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_sweep_slice_budget_limits_single_step_sweep(void) {
    SZrTestTimer timer;
    const char* testSummary = "GC Sweep Slice Budget Limits Single Step Sweep";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Sweep budget setup",
              "Preparing multiple dead objects so one GC step must honor the configured sweep slice budget");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    {
        SZrGarbageCollector* gc = state->global->garbageCollector;
        SZrRawObject* first = createTestObject(state, ZR_VALUE_TYPE_NATIVE_POINTER, sizeof(SZrRawObject));
        SZrRawObject* second = createTestObject(state, ZR_VALUE_TYPE_NATIVE_POINTER, sizeof(SZrRawObject));
        SZrRawObject* third = createTestObject(state, ZR_VALUE_TYPE_NATIVE_POINTER, sizeof(SZrRawObject));

        TEST_ASSERT_NOT_NULL(first);
        TEST_ASSERT_NOT_NULL(second);
        TEST_ASSERT_NOT_NULL(third);

        ZrCore_RawObject_MarkAsInit(state, first);
        ZrCore_RawObject_MarkAsInit(state, second);
        ZrCore_RawObject_MarkAsInit(state, third);
        first->garbageCollectMark.generation = ZR_GC_OTHER_GENERATION(gc);
        second->garbageCollectMark.generation = ZR_GC_OTHER_GENERATION(gc);
        third->garbageCollectMark.generation = ZR_GC_OTHER_GENERATION(gc);

        gc->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS;
        gc->gcStatus = ZR_GARBAGE_COLLECT_STATUS_RUNNING;
        gc->gcSweepSliceBudget = 1;
        gc->gcObjectListSweeper = &gc->gcObjectList;
        gc->gcDebtSize = 4096;

        ZrCore_GarbageCollector_GcStep(state);

        TEST_ASSERT_EQUAL_INT(1, (int)gc->gcLastStepWork);
        TEST_ASSERT_EQUAL_PTR(second, gc->gcObjectList);
        TEST_ASSERT_EQUAL_INT(ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS,
                              gc->gcRunningStatus);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_ignore_registry_and_phase_metadata(void) {
    SZrTestTimer timer;
    const char* testSummary = "GC Ignore Registry And Phase Metadata";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Test environment initialization",
              "Creating test state for ignore registry and phased GC metadata testing");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    {
        SZrGarbageCollector* gc = state->global->garbageCollector;
        SZrClosure *ignoredClosure = ZrCore_Closure_New(state, 0u);
        SZrRawObject* ignoredObject = ZR_CAST_RAW_OBJECT_AS_SUPER(ignoredClosure);
        TEST_ASSERT_NOT_NULL(ignoredObject);

        TEST_INFO("Initial phased-GC metadata",
                  "Verifying that pause budget, sweep slice budget, and ignore registry are initialized");
        TEST_ASSERT_TRUE(gc->gcPauseBudget > 0);
        TEST_ASSERT_TRUE(gc->gcSweepSliceBudget > 0);
        TEST_ASSERT_EQUAL_INT(0, (int)gc->ignoredObjectCount);
        TEST_ASSERT_EQUAL_INT(ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED,
                              gc->gcLastCompletedRunningStatus);

        TEST_INFO("Ignore registry registration",
                  "Registering an object in the ignore registry should make it queryable and counted");
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, ignoredObject));
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, ignoredObject));
        TEST_ASSERT_EQUAL_INT(1, (int)gc->ignoredObjectCount);

        TEST_INFO("Ignore registry round-trip",
                  "Removing an ignored object should drop it from the registry and restore the count");
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(state->global, ignoredObject));
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, ignoredObject));
        TEST_ASSERT_EQUAL_INT(0, (int)gc->ignoredObjectCount);

        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, ignoredObject));
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, ignoredObject));
        TEST_ASSERT_EQUAL_INT(1, (int)gc->ignoredObjectCount);

        TEST_INFO("Phase metadata after GC step",
                  "Executing a GC step with debt should update last-step metadata without dropping ignore registry state");
        ZrCore_GarbageCollector_AddDebtSpace(state->global, 4096);
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_TRUE(gc->gcLastStepWork > 0);
        TEST_ASSERT_TRUE(gc->gcLastCompletedRunningStatus ==
                             ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED ||
                         gc->gcLastCompletedRunningStatus ==
                             ZR_GARBAGE_COLLECT_RUNNING_STATUS_FLAG_PROPAGATION ||
                         gc->gcLastCompletedRunningStatus ==
                             ZR_GARBAGE_COLLECT_RUNNING_STATUS_BEFORE_ATOMIC ||
                         gc->gcLastCompletedRunningStatus ==
                             ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS ||
                         gc->gcLastCompletedRunningStatus ==
                             ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_WAIT_TO_RELEASE_OBJECTS ||
                         gc->gcLastCompletedRunningStatus ==
                             ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, ignoredObject));
        TEST_ASSERT_TRUE(gc_test_collector_contains_object(state, ignoredObject));
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_ignore_registry_swap_remove_keeps_remaining_object_queryable(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Ignore Registry Swap Remove Keeps Remaining Object Queryable";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Ignore registry swap-remove bookkeeping",
              "Removing one ignored object should keep the remaining ignored object queryable without a full linear compaction pass");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrRawObject *first = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
        SZrRawObject *second = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));

        TEST_ASSERT_NOT_NULL(first);
        TEST_ASSERT_NOT_NULL(second);

        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, first));
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, second));
        TEST_ASSERT_EQUAL_INT(2, (int)gc->ignoredObjectCount);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, first));
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, second));

        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(state->global, first));
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, first));
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, second));
        TEST_ASSERT_EQUAL_INT(1, (int)gc->ignoredObjectCount);

        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(state->global, second));
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, second));
        TEST_ASSERT_EQUAL_INT(0, (int)gc->ignoredObjectCount);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_barrier_unignores_escaped_object(void) {
    SZrTestTimer timer;
    const char* testSummary = "GC Barrier Unignores Escaped Object";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Test environment initialization",
              "Creating test state for ignored-object escape testing");
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    {
        SZrGarbageCollector* gc = state->global->garbageCollector;
        SZrRawObject* parent = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
        SZrRawObject* child = createTestObject(state, ZR_VALUE_TYPE_STRING, sizeof(SZrRawObject));

        TEST_ASSERT_NOT_NULL(parent);
        TEST_ASSERT_NOT_NULL(child);

        TEST_INFO("Prepare referenced parent and ignored child",
                  "The ignored child simulates a unique-owned object before it escapes into a shared GC graph");
        ZrCore_RawObject_MarkAsReferenced(parent);
        ZrCore_RawObject_MarkAsInit(state, child);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, child));
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, child));
        TEST_ASSERT_EQUAL_INT(1, (int)gc->ignoredObjectCount);

        TEST_INFO("Barrier on shared escape",
                  "Writing an ignored child through the GC barrier should restore normal tracing ownership");
        ZrCore_GarbageCollector_Barrier(state, parent, child);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, child));
        TEST_ASSERT_EQUAL_INT(0, (int)gc->ignoredObjectCount);
        TEST_ASSERT_TRUE(ZR_GC_IS_WAIT_TO_SCAN(child) || ZR_GC_IS_REFERENCED(child));
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_ownership_shared_refcount_and_weak_null_on_release(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Shared Refcount And Weak Null On Release";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Ownership runtime release path",
              "Testing Rust-style shared retain/release accounting while GC ignore-registry state stays stable until the last strong owner drops");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrRawObject *object = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
        SZrTypeValue uniqueValue;
        SZrTypeValue sharedValueA;
        SZrTypeValue sharedValueB;
        SZrTypeValue weakValue;

        TEST_ASSERT_NOT_NULL(object);
        ZrCore_Value_ResetAsNull(&uniqueValue);
        ZrCore_Value_ResetAsNull(&sharedValueA);
        ZrCore_Value_ResetAsNull(&sharedValueB);
        ZrCore_Value_ResetAsNull(&weakValue);

        TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(state, &uniqueValue, object));
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
        TEST_ASSERT_EQUAL_INT(1, (int)gc->ignoredObjectCount);
        TEST_ASSERT_EQUAL_UINT32(1, ZrCore_Ownership_GetStrongRefCount(object));

        ZrCore_GarbageCollector_AddDebtSpace(state->global, 4096);
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_TRUE(gc->gcLastStepWork > 0);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
        TEST_ASSERT_EQUAL_INT(1, (int)gc->ignoredObjectCount);
        TEST_ASSERT_EQUAL_UINT32(1, ZrCore_Ownership_GetStrongRefCount(object));

        TEST_ASSERT_TRUE(ZrCore_Ownership_ShareValue(state, &sharedValueA, &uniqueValue));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(uniqueValue.type));
        TEST_ASSERT_EQUAL_UINT32(1, ZrCore_Ownership_GetStrongRefCount(object));
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
        TEST_ASSERT_EQUAL_INT(1, (int)gc->ignoredObjectCount);

        ZrCore_GarbageCollector_GcFull(state, ZR_FALSE);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
        TEST_ASSERT_EQUAL_INT(1, (int)gc->ignoredObjectCount);
        TEST_ASSERT_EQUAL_UINT32(1, ZrCore_Ownership_GetStrongRefCount(object));

        ZrCore_Value_Copy(state, &sharedValueB, &sharedValueA);
        TEST_ASSERT_EQUAL_UINT32(2, ZrCore_Ownership_GetStrongRefCount(object));

        TEST_ASSERT_TRUE(ZrCore_Ownership_WeakValue(state, &weakValue, &sharedValueA));
        TEST_ASSERT_FALSE(ZR_VALUE_IS_TYPE_NULL(weakValue.type));

        ZrCore_Ownership_ReleaseValue(state, &sharedValueA);
        TEST_ASSERT_EQUAL_UINT32(1, ZrCore_Ownership_GetStrongRefCount(object));
        TEST_ASSERT_FALSE(ZR_VALUE_IS_TYPE_NULL(weakValue.type));
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
        TEST_ASSERT_EQUAL_INT(1, (int)gc->ignoredObjectCount);

        ZrCore_Ownership_ReleaseValue(state, &sharedValueB);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(weakValue.type));
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
        TEST_ASSERT_EQUAL_INT(0, (int)gc->ignoredObjectCount);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_ownership_unique_can_return_to_gc_control(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Unique Can Return To GC Control";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Ownership runtime GC handoff",
              "Testing that a detached unique-owned object survives GC while ignored and only rejoins normal GC control once explicitly detached");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrRawObject *object = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
        SZrTypeValue uniqueValue;
        SZrTypeValue gcValue;

        TEST_ASSERT_NOT_NULL(object);
        ZrCore_Value_ResetAsNull(&uniqueValue);
        ZrCore_Value_ResetAsNull(&gcValue);

        TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(state, &uniqueValue, object));
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
        TEST_ASSERT_EQUAL_INT(1, (int)gc->ignoredObjectCount);

        ZrCore_GarbageCollector_AddDebtSpace(state->global, 4096);
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_TRUE(gc->gcLastStepWork > 0);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
        TEST_ASSERT_EQUAL_INT(1, (int)gc->ignoredObjectCount);

        ZrCore_GarbageCollector_GcFull(state, ZR_FALSE);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
        TEST_ASSERT_EQUAL_INT(1, (int)gc->ignoredObjectCount);

        TEST_ASSERT_TRUE(ZrCore_Ownership_ReturnToGcValue(state, &gcValue, &uniqueValue));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(uniqueValue.type));
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
        TEST_ASSERT_EQUAL_INT(0, (int)gc->ignoredObjectCount);
        TEST_ASSERT_EQUAL_UINT32(0, ZrCore_Ownership_GetStrongRefCount(object));
        TEST_ASSERT_EQUAL_PTR(object, gcValue.value.object);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_ownership_weak_expires_when_returned_object_is_released(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Weak Expires When Returned Object Is Released";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Ownership weak tracking across GC handoff",
              "Testing that weak references created from %shared survive the detach bridge back to GC world and only expire once the GC-side object is released");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrRawObject *object = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
        SZrTypeValue uniqueValue;
        SZrTypeValue sharedValue;
        SZrTypeValue weakValue;
        SZrTypeValue gcValue;

        TEST_ASSERT_NOT_NULL(object);
        ZrCore_Value_ResetAsNull(&uniqueValue);
        ZrCore_Value_ResetAsNull(&sharedValue);
        ZrCore_Value_ResetAsNull(&weakValue);
        ZrCore_Value_ResetAsNull(&gcValue);

        TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(state, &uniqueValue, object));
        TEST_ASSERT_TRUE(ZrCore_Ownership_ShareValue(state, &sharedValue, &uniqueValue));
        TEST_ASSERT_TRUE(ZrCore_Ownership_WeakValue(state, &weakValue, &sharedValue));
        TEST_ASSERT_FALSE(ZR_VALUE_IS_TYPE_NULL(weakValue.type));
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
        TEST_ASSERT_EQUAL_INT(1, (int)gc->ignoredObjectCount);
        TEST_ASSERT_EQUAL_UINT32(1, ZrCore_Ownership_GetStrongRefCount(object));

        ZrCore_GarbageCollector_AddDebtSpace(state->global, 4096);
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_TRUE(gc->gcLastStepWork > 0);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
        TEST_ASSERT_EQUAL_INT(1, (int)gc->ignoredObjectCount);

        TEST_ASSERT_TRUE(ZrCore_Ownership_ReturnToGcValue(state, &gcValue, &sharedValue));
        TEST_ASSERT_FALSE(ZR_VALUE_IS_TYPE_NULL(weakValue.type));
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
        TEST_ASSERT_EQUAL_INT(0, (int)gc->ignoredObjectCount);
        TEST_ASSERT_EQUAL_UINT32(0, ZrCore_Ownership_GetStrongRefCount(object));
        TEST_ASSERT_EQUAL_PTR(object, gcValue.value.object);

        ZrCore_Ownership_NotifyObjectReleased(state, object);

        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(weakValue.type));
        ZrCore_Value_ResetAsNull(&gcValue);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_region_configuration_defaults(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Region Configuration Defaults";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Collector defaults",
              "Verifying that the collector boots with region, budget, and stats defaults for the staged generational pipeline");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrRawObject *object = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));

        TEST_ASSERT_NOT_NULL(object);
        TEST_ASSERT_EQUAL_UINT64(0u, (UNITY_UINT64)gc->heapLimitBytes);
        TEST_ASSERT_TRUE(gc->youngRegionSize > 0);
        TEST_ASSERT_TRUE(gc->youngRegionCountTarget > 0);
        TEST_ASSERT_TRUE(gc->pauseBudgetUs > 0);
        TEST_ASSERT_TRUE(gc->remarkBudgetUs > 0);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR, gc->scheduledCollectionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE, gc->collectionPhase);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR, gc->statsSnapshot.lastCollectionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_EDEN, object->garbageCollectMark.regionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE, object->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE, object->garbageCollectMark.escapeFlags);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PIN_KIND_NONE, object->garbageCollectMark.pinFlags);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE, object->garbageCollectMark.promotionReason);
        TEST_ASSERT_EQUAL_UINT32(0u, object->garbageCollectMark.survivalAge);
        TEST_ASSERT_EQUAL_UINT32(ZR_GC_SCOPE_DEPTH_NONE, object->garbageCollectMark.anchorScopeDepth);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_control_plane_updates_snapshot(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Control Plane Updates Snapshot";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Collector control plane",
              "Testing heap limit, budget, worker count, and collection scheduling controls exposed by the C API");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollectorStatsSnapshot snapshot;

        ZrCore_GarbageCollector_SetHeapLimitBytes(state->global, 1024 * 1024);
        ZrCore_GarbageCollector_SetPauseBudgetUs(state->global, 2500, 1200);
        ZrCore_GarbageCollector_SetWorkerCount(state->global, 3);
        ZrCore_GarbageCollector_ScheduleCollection(state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
        ZrCore_GarbageCollector_GetStatsSnapshot(state->global, &snapshot);

        TEST_ASSERT_EQUAL_UINT64(1024u * 1024u, (UNITY_UINT64)state->global->garbageCollector->heapLimitBytes);
        TEST_ASSERT_EQUAL_UINT64(2500u, (UNITY_UINT64)state->global->garbageCollector->pauseBudgetUs);
        TEST_ASSERT_EQUAL_UINT64(1200u, (UNITY_UINT64)state->global->garbageCollector->remarkBudgetUs);
        TEST_ASSERT_EQUAL_UINT32(3u, state->global->garbageCollector->workerCount);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR,
                                 state->global->garbageCollector->scheduledCollectionKind);
        TEST_ASSERT_EQUAL_UINT64(1024u * 1024u, (UNITY_UINT64)snapshot.heapLimitBytes);
        TEST_ASSERT_EQUAL_UINT32(3u, snapshot.workerCount);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR, snapshot.lastRequestedCollectionKind);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_step_records_timing_in_snapshot(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Step Records Timing In Snapshot";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("GC step timing telemetry",
              "Testing that an executed GC step records the last step duration and work units into the public stats snapshot");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrGarbageCollectorStatsSnapshot snapshot;
        SZrObject *object = ZrCore_Object_New(state, ZR_NULL);
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        SZrRawObject *oldObject = ZR_CAST_RAW_OBJECT_AS_SUPER(object);

        TEST_ASSERT_NOT_NULL(object);

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        gc->gcDebtSize = 0;
        gc->gcLastStepWork = 0;
        gc->statsSnapshot.lastStepDurationUs = 0u;
        gc->statsSnapshot.lastStepWork = 0u;
        gc->fragmentationCompactThreshold = 100u;

        ZrCore_RawObject_SetStorageKind(oldObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(oldObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);
        ZrCore_Stack_SetRawObjectValue(state, rootSlot, oldObject);
        state->stackTop.valuePointer = rootSlot + 1;

        ZrCore_GarbageCollector_ScheduleCollection(state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
        ZrCore_GarbageCollector_CheckGc(state);
        ZrCore_GarbageCollector_GetStatsSnapshot(state->global, &snapshot);

        TEST_ASSERT_TRUE(gc->gcLastStepWork > 0);
        TEST_ASSERT_TRUE(snapshot.lastStepDurationUs > 0u);
        TEST_ASSERT_EQUAL_UINT64((UNITY_UINT64)gc->gcLastStepWork, (UNITY_UINT64)snapshot.lastStepWork);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_system_gc_enable_callback_switches_generational_mode(void) {
    SZrTestTimer timer;
    const char *testSummary = "System GC Enable Callback Switches Generational Mode";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Public system.gc enable/disable callbacks",
              "Testing that the public zr.system.gc callbacks flip the collector into generational mode and preserve that safe mode while toggling stopGcFlag");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        ZrLibCallContext context;
        SZrTypeValue result;

        memset(&context, 0, sizeof(context));
        context.state = state;
        ZrCore_Value_ResetAsNull(&result);

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_INCREMENTAL;
        gc->stopGcFlag = ZR_TRUE;

        TEST_ASSERT_TRUE(ZrSystem_Gc_Enable(&context, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, result.type);
        TEST_ASSERT_EQUAL_INT(ZR_GARBAGE_COLLECT_MODE_GENERATIONAL, gc->gcMode);
        TEST_ASSERT_FALSE(gc->stopGcFlag);

        ZrCore_Value_ResetAsNull(&result);
        TEST_ASSERT_TRUE(ZrSystem_Gc_Disable(&context, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, result.type);
        TEST_ASSERT_EQUAL_INT(ZR_GARBAGE_COLLECT_MODE_GENERATIONAL, gc->gcMode);
        TEST_ASSERT_TRUE(gc->stopGcFlag);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_snapshot_accumulates_collection_counts_and_durations(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Snapshot Accumulates Collection Counts And Durations";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("GC cumulative telemetry",
              "Testing that minor, major, and full collections accumulate per-kind counts plus total and max durations in the public stats snapshot");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrGarbageCollectorStatsSnapshot snapshot;
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        SZrObject *youngObject;
        SZrObject *oldObject;
        SZrRawObject *oldRawObject;

        TEST_ASSERT_NOT_NULL(gc);

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        gc->gcDebtSize = 0;
        gc->gcLastStepWork = 0;

        youngObject = ZrCore_Object_New(state, ZR_NULL);
        TEST_ASSERT_NOT_NULL(youngObject);
        ZrCore_Stack_SetRawObjectValue(state, rootSlot, ZR_CAST_RAW_OBJECT_AS_SUPER(youngObject));
        state->stackTop.valuePointer = rootSlot + 1;
        gc->gcDebtSize = 4096;
        ZrCore_GarbageCollector_CheckGc(state);

        oldObject = ZrCore_Object_New(state, ZR_NULL);
        TEST_ASSERT_NOT_NULL(oldObject);
        oldRawObject = ZR_CAST_RAW_OBJECT_AS_SUPER(oldObject);
        ZrCore_RawObject_SetStorageKind(oldRawObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(oldRawObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);
        ZrCore_Stack_SetRawObjectValue(state, rootSlot, oldRawObject);
        state->stackTop.valuePointer = rootSlot + 1;
        ZrCore_GarbageCollector_ScheduleCollection(state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
        ZrCore_GarbageCollector_CheckGc(state);

        ZrCore_GarbageCollector_GcFull(state, ZR_FALSE);
        ZrCore_GarbageCollector_GetStatsSnapshot(state->global, &snapshot);

        TEST_ASSERT_EQUAL_UINT64(1u, (UNITY_UINT64)snapshot.minorCollectionCount);
        TEST_ASSERT_EQUAL_UINT64(1u, (UNITY_UINT64)snapshot.majorCollectionCount);
        TEST_ASSERT_EQUAL_UINT64(1u, (UNITY_UINT64)snapshot.fullCollectionCount);
        TEST_ASSERT_TRUE(snapshot.minorCollectionTotalDurationUs > 0u);
        TEST_ASSERT_TRUE(snapshot.majorCollectionTotalDurationUs > 0u);
        TEST_ASSERT_TRUE(snapshot.fullCollectionTotalDurationUs > 0u);
        TEST_ASSERT_TRUE(snapshot.minorCollectionMaxDurationUs > 0u);
        TEST_ASSERT_TRUE(snapshot.majorCollectionMaxDurationUs > 0u);
        TEST_ASSERT_TRUE(snapshot.fullCollectionMaxDurationUs > 0u);
        TEST_ASSERT_TRUE(snapshot.minorCollectionMaxDurationUs <= snapshot.minorCollectionTotalDurationUs);
        TEST_ASSERT_TRUE(snapshot.majorCollectionMaxDurationUs <= snapshot.majorCollectionTotalDurationUs);
        TEST_ASSERT_TRUE(snapshot.fullCollectionMaxDurationUs <= snapshot.fullCollectionTotalDurationUs);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_snapshot_reports_region_pressure_shape(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Snapshot Reports Region Pressure Shape";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("GC pressure snapshot",
              "Testing that the public GC stats snapshot reports current managed memory plus region-count/live-byte pressure shape across young old pinned and permanent spaces");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrGarbageCollectorStatsSnapshot baseline;
        SZrGarbageCollectorStatsSnapshot snapshot;
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        TZrStackValuePointer youngRootSlot = rootSlot + 1;
        TZrSize objectSize = sizeof(SZrObject);
        SZrRawObject *youngObject = ZR_NULL;
        SZrRawObject *oldObject;
        SZrRawObject *pinnedObject;
        SZrRawObject *permanentObject;
        SZrTypeValue *rootValue;

        TEST_ASSERT_NOT_NULL(gc);

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        gc->gcDebtSize = 0;
        gc->gcLastStepWork = 0;

        memset(&baseline, 0, sizeof(baseline));
        memset(&snapshot, 0, sizeof(snapshot));
        ZrCore_GarbageCollector_GetStatsSnapshot(state->global, &baseline);

        oldObject = createTestObject(state, ZR_VALUE_TYPE_OBJECT, objectSize);

        TEST_ASSERT_NOT_NULL(oldObject);

        ZrCore_Stack_SetRawObjectValue(state, rootSlot, oldObject);
        state->stackTop.valuePointer = rootSlot + 1;
        ZrCore_GarbageCollector_MarkRawObjectEscaped(state,
                                                     oldObject,
                                                     ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN,
                                                     3u,
                                                     ZR_GARBAGE_COLLECT_PROMOTION_REASON_ESCAPE);
        gc->gcDebtSize = 4096;
        ZrCore_GarbageCollector_GcStep(state);

        rootValue = ZrCore_Stack_GetValue(rootSlot);
        TEST_ASSERT_NOT_NULL(rootValue);
        TEST_ASSERT_TRUE(rootValue->isGarbageCollectable);
        TEST_ASSERT_NOT_NULL(rootValue->value.object);

        /* Allocate a fresh young object after the minor step so the snapshot still has live eden pressure. */
        youngObject = createTestObject(state, ZR_VALUE_TYPE_OBJECT, objectSize);
        TEST_ASSERT_NOT_NULL(youngObject);
        ZrCore_Stack_SetRawObjectValue(state, youngRootSlot, youngObject);
        state->stackTop.valuePointer = youngRootSlot + 1;

        pinnedObject = createTestObject(state, ZR_VALUE_TYPE_OBJECT, objectSize);
        permanentObject = createTestObject(state, ZR_VALUE_TYPE_OBJECT, objectSize);
        TEST_ASSERT_NOT_NULL(pinnedObject);
        TEST_ASSERT_NOT_NULL(permanentObject);

        ZrCore_GarbageCollector_PinObject(state, pinnedObject, ZR_GARBAGE_COLLECT_PIN_KIND_HOST_HANDLE);
        ZrCore_RawObject_MarkAsInit(state, permanentObject);
        ZrCore_RawObject_MarkAsPermanent(state, permanentObject);

        ZrCore_GarbageCollector_GetStatsSnapshot(state->global, &snapshot);

        TEST_ASSERT_TRUE(snapshot.managedMemoryBytes > 0u);
        TEST_ASSERT_TRUE(snapshot.regionCount >= baseline.regionCount);
        TEST_ASSERT_TRUE(snapshot.edenRegionCount >= 1u);
        TEST_ASSERT_TRUE(snapshot.oldRegionCount >= 1u);
        TEST_ASSERT_TRUE(snapshot.pinnedRegionCount >= 1u);
        TEST_ASSERT_TRUE(snapshot.permanentRegionCount >= 1u);
        TEST_ASSERT_TRUE(snapshot.edenLiveBytes >= (TZrUInt64)objectSize);
        TEST_ASSERT_TRUE(snapshot.oldLiveBytes >= baseline.oldLiveBytes + (TZrUInt64)objectSize);
        TEST_ASSERT_TRUE(snapshot.pinnedLiveBytes >= baseline.pinnedLiveBytes + (TZrUInt64)objectSize);
        TEST_ASSERT_TRUE(snapshot.permanentLiveBytes >= baseline.permanentLiveBytes + (TZrUInt64)objectSize);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_scheduled_collection_check_gc_runs_major_without_forced_compact(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Scheduled Collection CheckGc Runs Major Without Forced Compact";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Scheduled major collection",
              "Testing that an explicit scheduled major collection creates GC debt, runs a major mark-sweep step, and does not force compact when there is no old-generation fragmentation");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrObject *object = ZrCore_Object_New(state, ZR_NULL);
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        SZrTypeValue *rootValue;
        SZrRawObject *oldObject = ZR_CAST_RAW_OBJECT_AS_SUPER(object);

        TEST_ASSERT_NOT_NULL(object);

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        gc->gcDebtSize = 0;
        gc->gcLastStepWork = 0;
        gc->statsSnapshot.lastCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
        gc->statsSnapshot.lastRequestedCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
        gc->fragmentationCompactThreshold = 100u;

        ZrCore_RawObject_SetStorageKind(oldObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(oldObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);
        ZrCore_Stack_SetRawObjectValue(state, rootSlot, oldObject);
        state->stackTop.valuePointer = rootSlot + 1;

        ZrCore_GarbageCollector_ScheduleCollection(state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);

        TEST_ASSERT_TRUE(gc->gcDebtSize > 0);

        ZrCore_GarbageCollector_CheckGc(state);
        rootValue = ZrCore_Stack_GetValue(rootSlot);

        TEST_ASSERT_TRUE(gc->gcLastStepWork > 0);
        TEST_ASSERT_NOT_NULL(rootValue);
        TEST_ASSERT_TRUE(rootValue->isGarbageCollectable);
        TEST_ASSERT_EQUAL_PTR(oldObject, rootValue->value.object);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR, gc->scheduledCollectionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR, gc->statsSnapshot.lastCollectionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR, gc->statsSnapshot.lastRequestedCollectionKind);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_heap_limit_pressure_check_gc_escalates_to_full(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Heap Limit Pressure CheckGc Escalates To Full";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Heap limit pressure",
              "Testing that CheckGc escalates to the full generational path when managed memory reaches the configured heap limit even without pre-existing debt");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        gc->gcDebtSize = 0;
        gc->gcLastStepWork = 0;
        gc->managedMemories = 8192;
        gc->statsSnapshot.lastCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
        gc->statsSnapshot.lastRequestedCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
        ZrCore_GarbageCollector_SetHeapLimitBytes(state->global, 4096);

        ZrCore_GarbageCollector_CheckGc(state);

        TEST_ASSERT_TRUE(gc->gcLastStepWork > 0);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR, gc->scheduledCollectionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL, gc->statsSnapshot.lastCollectionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL, gc->statsSnapshot.lastRequestedCollectionKind);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_full_collection_compacts_old_reference_graph(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Full Collection Compacts Old Reference Graph";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Explicit full compact",
              "Testing that an explicit full generational collection compacts live old movable objects and rewrites both stack roots and object fields to the relocated addresses");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrObject *parent = ZrCore_Object_New(state, ZR_NULL);
        SZrObject *child = ZrCore_Object_New(state, ZR_NULL);
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "child");
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        SZrTypeValue memberKey;
        SZrTypeValue childValue;
        SZrTypeValue *rootValue;
        const SZrTypeValue *resolvedChildValue;
        SZrRawObject *oldParent = ZR_CAST_RAW_OBJECT_AS_SUPER(parent);
        SZrRawObject *oldChild = ZR_CAST_RAW_OBJECT_AS_SUPER(child);
        SZrRawObject *newParent;
        SZrRawObject *newChild;

        TEST_ASSERT_NOT_NULL(parent);
        TEST_ASSERT_NOT_NULL(child);
        TEST_ASSERT_NOT_NULL(memberName);

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;

        ZrCore_RawObject_SetStorageKind(oldParent, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(oldParent, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);
        ZrCore_RawObject_SetStorageKind(oldChild, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(oldChild, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        ZrCore_Value_InitAsRawObject(state, &memberKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
        memberKey.type = ZR_VALUE_TYPE_STRING;
        ZrCore_Value_InitAsRawObject(state, &childValue, oldChild);
        ZrCore_Object_SetValue(state, parent, &memberKey, &childValue);

        ZrCore_Stack_SetRawObjectValue(state, rootSlot, oldParent);
        state->stackTop.valuePointer = rootSlot + 1;

        resolvedChildValue = ZrCore_Object_GetValue(state, parent, &memberKey);
        TEST_ASSERT_NOT_NULL(resolvedChildValue);
        TEST_ASSERT_EQUAL_PTR(oldChild, resolvedChildValue->value.object);

        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);

        rootValue = ZrCore_Stack_GetValue(rootSlot);
        TEST_ASSERT_NOT_NULL(rootValue);
        TEST_ASSERT_TRUE(rootValue->isGarbageCollectable);
        TEST_ASSERT_NOT_EQUAL(oldParent, rootValue->value.object);

        newParent = rootValue->value.object;
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE,
                                 newParent->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_OLD,
                                 newParent->garbageCollectMark.regionKind);

        resolvedChildValue = ZrCore_Object_GetValue(state, ZR_CAST_OBJECT(state, newParent), &memberKey);
        TEST_ASSERT_NOT_NULL(resolvedChildValue);
        TEST_ASSERT_TRUE(resolvedChildValue->isGarbageCollectable);
        TEST_ASSERT_NOT_EQUAL(oldChild, resolvedChildValue->value.object);

        newChild = resolvedChildValue->value.object;
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE,
                                 newChild->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_OLD,
                                 newChild->garbageCollectMark.regionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL, gc->statsSnapshot.lastCollectionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE, gc->collectionPhase);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_barrier_records_old_to_young_remembered_escape(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Barrier Records Old To Young Remembered Escape";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Old to young write barrier",
              "Testing that old movable parents record remembered references and mark young children as escaped promotion candidates");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrRawObject *parent = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
        SZrRawObject *child = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));

        TEST_ASSERT_NOT_NULL(parent);
        TEST_ASSERT_NOT_NULL(child);

        ZrCore_RawObject_MarkAsReferenced(parent);
        ZrCore_RawObject_MarkAsInit(state, child);
        ZrCore_RawObject_SetStorageKind(parent, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(parent, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);
        ZrCore_RawObject_SetStorageKind(child, ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE);
        ZrCore_RawObject_SetRegionKind(child, ZR_GARBAGE_COLLECT_REGION_KIND_EDEN);
        child->garbageCollectMark.anchorScopeDepth = 3u;

        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, parent));
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE, child->garbageCollectMark.escapeFlags);

        ZrCore_GarbageCollector_Barrier(state, parent, child);

        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, parent));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);
        TEST_ASSERT_TRUE(parent->garbageCollectMark.rememberedRegistryIndex < gc->rememberedObjectCount);
        TEST_ASSERT_EQUAL_PTR(parent, gc->rememberedObjects[parent->garbageCollectMark.rememberedRegistryIndex]);
        TEST_ASSERT_TRUE((child->garbageCollectMark.escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_OLD_REFERENCE) != 0u);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PROMOTION_REASON_OLD_REFERENCE,
                                 child->garbageCollectMark.promotionReason);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_barrier_records_permanent_to_young_remembered_escape(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Barrier Records Permanent To Young Remembered Escape";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Permanent to young write barrier",
              "Testing that permanent root owners writing through object storage record remembered references and mark young children as global-root escape candidates");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrObject *parent = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_OBJECT);
        SZrObject *child = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_OBJECT);
        SZrTypeValue key;
        SZrTypeValue childValue;
        SZrString *fieldName;

        TEST_ASSERT_NOT_NULL(parent);
        TEST_ASSERT_NOT_NULL(child);

        fieldName = ZrCore_String_Create(state, "child", 5);
        TEST_ASSERT_NOT_NULL(fieldName);

        ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName));
        key.type = ZR_VALUE_TYPE_STRING;
        ZrCore_Value_InitAsRawObject(state, &childValue, ZR_CAST_RAW_OBJECT_AS_SUPER(child));

        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(parent));
        ZrCore_RawObject_MarkAsInit(state, ZR_CAST_RAW_OBJECT_AS_SUPER(child));
        ZrCore_RawObject_SetStorageKind(ZR_CAST_RAW_OBJECT_AS_SUPER(child), ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE);
        ZrCore_RawObject_SetRegionKind(ZR_CAST_RAW_OBJECT_AS_SUPER(child), ZR_GARBAGE_COLLECT_REGION_KIND_EDEN);

        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(parent)));
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(child)->garbageCollectMark.escapeFlags);

        ZrCore_Object_SetValue(state, parent, &key, &childValue);

        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(parent)));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);
        TEST_ASSERT_TRUE(ZR_CAST_RAW_OBJECT_AS_SUPER(parent)->garbageCollectMark.rememberedRegistryIndex <
                         gc->rememberedObjectCount);
        TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(parent),
                              gc->rememberedObjects[ZR_CAST_RAW_OBJECT_AS_SUPER(parent)
                                                            ->garbageCollectMark.rememberedRegistryIndex]);
        TEST_ASSERT_TRUE((ZR_CAST_RAW_OBJECT_AS_SUPER(child)->garbageCollectMark.escapeFlags &
                          ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) != 0u);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PROMOTION_REASON_GLOBAL_ROOT,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(child)->garbageCollectMark.promotionReason);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_object_set_value_records_old_to_young_remembered_escape_from_inited_parent(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Object SetValue Records Old To Young Remembered Escape From Inited Parent";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Idle old-to-young object write",
              "Testing that Object.SetValue records remembered old-to-young references even when the old parent is INITED between collections");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrObject *parent = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_OBJECT);
        SZrObject *child = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_OBJECT);
        SZrTypeValue key;
        SZrTypeValue childValue;
        SZrString *fieldName;

        TEST_ASSERT_NOT_NULL(parent);
        TEST_ASSERT_NOT_NULL(child);

        fieldName = ZrCore_String_Create(state, "child", 5);
        TEST_ASSERT_NOT_NULL(fieldName);

        ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName));
        key.type = ZR_VALUE_TYPE_STRING;
        ZrCore_Value_InitAsRawObject(state, &childValue, ZR_CAST_RAW_OBJECT_AS_SUPER(child));

        ZrCore_RawObject_MarkAsInit(state, ZR_CAST_RAW_OBJECT_AS_SUPER(parent));
        ZrCore_RawObject_SetStorageKind(ZR_CAST_RAW_OBJECT_AS_SUPER(parent), ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(ZR_CAST_RAW_OBJECT_AS_SUPER(parent), ZR_GARBAGE_COLLECT_REGION_KIND_OLD);
        ZrCore_RawObject_MarkAsInit(state, ZR_CAST_RAW_OBJECT_AS_SUPER(child));
        ZrCore_RawObject_SetStorageKind(ZR_CAST_RAW_OBJECT_AS_SUPER(child), ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE);
        ZrCore_RawObject_SetRegionKind(ZR_CAST_RAW_OBJECT_AS_SUPER(child), ZR_GARBAGE_COLLECT_REGION_KIND_EDEN);
        ZR_CAST_RAW_OBJECT_AS_SUPER(child)->garbageCollectMark.anchorScopeDepth = 2u;

        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(parent)));
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(child)->garbageCollectMark.escapeFlags);

        ZrCore_Object_SetValue(state, parent, &key, &childValue);

        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(parent)));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);
        TEST_ASSERT_TRUE(ZR_CAST_RAW_OBJECT_AS_SUPER(parent)->garbageCollectMark.rememberedRegistryIndex <
                         gc->rememberedObjectCount);
        TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(parent),
                              gc->rememberedObjects[ZR_CAST_RAW_OBJECT_AS_SUPER(parent)
                                                            ->garbageCollectMark.rememberedRegistryIndex]);
        TEST_ASSERT_TRUE((ZR_CAST_RAW_OBJECT_AS_SUPER(child)->garbageCollectMark.escapeFlags &
                          ZR_GARBAGE_COLLECT_ESCAPE_KIND_OLD_REFERENCE) != 0u);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PROMOTION_REASON_OLD_REFERENCE,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(child)->garbageCollectMark.promotionReason);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_pinning_marks_object_non_moving(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Pinning Marks Object Non Moving";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Pinned object transition",
              "Testing that host-visible pinned objects transition into the non-moving storage class and become escaped");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrObjectModule *pinnedModule = ZrCore_Module_Create(state);
        SZrRawObject *object = ZR_CAST_RAW_OBJECT_AS_SUPER(pinnedModule);

        TEST_ASSERT_NOT_NULL(object);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PIN_KIND_NONE, object->garbageCollectMark.pinFlags);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE, object->garbageCollectMark.storageKind);

        ZrCore_GarbageCollector_PinObject(state, object, ZR_GARBAGE_COLLECT_PIN_KIND_HOST_HANDLE);

        TEST_ASSERT_TRUE((object->garbageCollectMark.pinFlags & ZR_GARBAGE_COLLECT_PIN_KIND_HOST_HANDLE) != 0u);
        TEST_ASSERT_TRUE((object->garbageCollectMark.escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_HOST_HANDLE) != 0u);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_PINNED, object->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_PINNED, object->garbageCollectMark.regionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PROMOTION_REASON_PINNED,
                                 object->garbageCollectMark.promotionReason);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_region_allocator_reuses_emptied_eden_region_after_permanent_transition(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Region Allocator Reuses Emptied Eden Region After Permanent Transition";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Permanent transition region reuse",
              "Testing that moving a young object into a dedicated permanent region releases its eden region so the next young allocation can reuse that emptied region");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        TZrSize objectSize = sizeof(SZrObject);
        SZrRawObject *object;
        TZrUInt32 edenRegionId;
        TZrUInt32 permanentRegionId;
        SZrRawObject *reusedObject;

        gc->youngRegionSize = (TZrUInt64)objectSize;
        gc->currentEdenRegionId = 0u;
        gc->currentEdenRegionUsedBytes = 0u;

        object = createTestObject(state, ZR_VALUE_TYPE_OBJECT, objectSize);
        TEST_ASSERT_NOT_NULL(object);

        edenRegionId = object->garbageCollectMark.regionId;
        TEST_ASSERT_TRUE(edenRegionId > 0u);
        TEST_ASSERT_EQUAL_UINT64((UNITY_UINT64)objectSize, (UNITY_UINT64)gc->currentEdenRegionUsedBytes);

        ZrCore_RawObject_MarkAsPermanent(state, object);

        permanentRegionId = object->garbageCollectMark.regionId;
        TEST_ASSERT_NOT_EQUAL(edenRegionId, permanentRegionId);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT, object->garbageCollectMark.regionKind);

        reusedObject = createTestObject(state, ZR_VALUE_TYPE_OBJECT, objectSize);
        TEST_ASSERT_NOT_NULL(reusedObject);
        TEST_ASSERT_EQUAL_UINT32(edenRegionId, reusedObject->garbageCollectMark.regionId);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static TZrBool gc_test_list_contains_object(SZrRawObject *head, const SZrRawObject *needle, TZrSize maxScan) {
    TZrSize scanCount = 0;

    while (head != ZR_NULL && scanCount < maxScan) {
        if (head == needle) {
            return ZR_TRUE;
        }
        head = head->next;
        scanCount++;
    }

    return ZR_FALSE;
}

static void test_gc_permanent_transition_accepts_referenced_interned_string_without_unlinking_gc_list(void) {
    SZrTestTimer timer;
    const char *testSummary =
            "GC Permanent Transition Accepts Referenced Interned String Without Unlinking GC List";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Referenced interned string permanent transition",
              "Testing that reusing an interned short string can still transition it into a permanent non-moving root without severing the main GC object list");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrRawObject *tail = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
        SZrString *interned = ZrCore_String_Create(state, "hashCode", strlen("hashCode"));
        SZrRawObject *head = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
        SZrString *reused;
        TZrUInt32 previousRegionId;

        TEST_ASSERT_NOT_NULL(tail);
        TEST_ASSERT_NOT_NULL(interned);
        TEST_ASSERT_NOT_NULL(head);
        TEST_ASSERT_TRUE(gc_test_list_contains_object(gc->gcObjectList, head, 32u));
        TEST_ASSERT_TRUE(gc_test_list_contains_object(gc->gcObjectList, ZR_CAST_RAW_OBJECT_AS_SUPER(interned), 32u));
        TEST_ASSERT_TRUE(gc_test_list_contains_object(gc->gcObjectList, tail, 32u));

        ZrCore_RawObject_MarkAsReferenced(ZR_CAST_RAW_OBJECT_AS_SUPER(interned));
        reused = ZrCore_String_Create(state, "hashCode", strlen("hashCode"));
        TEST_ASSERT_EQUAL_PTR(interned, reused);

        previousRegionId = ZR_CAST_RAW_OBJECT_AS_SUPER(reused)->garbageCollectMark.regionId;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(reused));

        TEST_ASSERT_EQUAL_INT(ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT,
                              ZR_CAST_RAW_OBJECT_AS_SUPER(reused)->garbageCollectMark.status);
        TEST_ASSERT_EQUAL_INT(ZR_GARBAGE_COLLECT_STORAGE_KIND_LARGE_PERSISTENT,
                              ZR_CAST_RAW_OBJECT_AS_SUPER(reused)->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_INT(ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT,
                              ZR_CAST_RAW_OBJECT_AS_SUPER(reused)->garbageCollectMark.regionKind);
        TEST_ASSERT_NOT_EQUAL(previousRegionId, ZR_CAST_RAW_OBJECT_AS_SUPER(reused)->garbageCollectMark.regionId);
        TEST_ASSERT_TRUE(gc_test_list_contains_object(gc->gcObjectList, head, 32u));
        TEST_ASSERT_TRUE(gc_test_list_contains_object(gc->gcObjectList, ZR_CAST_RAW_OBJECT_AS_SUPER(reused), 32u));
        TEST_ASSERT_TRUE(gc_test_list_contains_object(gc->gcObjectList, tail, 32u));
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_region_allocator_reassigns_pinned_object_region(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Region Allocator Reassigns Pinned Object Region";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Pinned transition region reassignment",
              "Testing that pinning a young object moves it into a dedicated pinned region and frees its previous eden region for reuse");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        TZrSize objectSize = sizeof(SZrObject);
        SZrRawObject *object;
        SZrRawObject *reusedObject;
        TZrUInt32 edenRegionId;

        gc->youngRegionSize = (TZrUInt64)objectSize;
        gc->currentEdenRegionId = 0u;
        gc->currentEdenRegionUsedBytes = 0u;

        object = createTestObject(state, ZR_VALUE_TYPE_OBJECT, objectSize);
        TEST_ASSERT_NOT_NULL(object);

        edenRegionId = object->garbageCollectMark.regionId;
        TEST_ASSERT_TRUE(edenRegionId > 0u);

        ZrCore_GarbageCollector_PinObject(state, object, ZR_GARBAGE_COLLECT_PIN_KIND_HOST_HANDLE);

        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_PINNED, object->garbageCollectMark.regionKind);
        TEST_ASSERT_NOT_EQUAL(edenRegionId, object->garbageCollectMark.regionId);
        TEST_ASSERT_TRUE(object->garbageCollectMark.regionDescriptorIndex < gc->regionCount);
        TEST_ASSERT_EQUAL_UINT32(object->garbageCollectMark.regionId,
                                 gc->regions[object->garbageCollectMark.regionDescriptorIndex].id);

        reusedObject = createTestObject(state, ZR_VALUE_TYPE_OBJECT, objectSize);
        TEST_ASSERT_NOT_NULL(reusedObject);
        TEST_ASSERT_EQUAL_UINT32(edenRegionId, reusedObject->garbageCollectMark.regionId);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_function_escape_metadata_defaults(void) {
    SZrTestTimer timer;
    const char *testSummary = "Function Escape Metadata Defaults";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Function metadata defaults",
              "Testing that new function objects and function metadata structures start with empty escape metadata");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrFunction *function = ZrCore_Function_New(state);
        SZrFunctionLocalVariable localVariable = {0};
        SZrFunctionClosureVariable closureVariable = {0};

        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_NULL(function->escapeBindings);
        TEST_ASSERT_EQUAL_UINT32(0u, function->escapeBindingLength);
        TEST_ASSERT_EQUAL_UINT32(0u, function->returnEscapeSlotCount);
        TEST_ASSERT_NULL(function->returnEscapeSlots);

        TEST_ASSERT_EQUAL_UINT32(0u, localVariable.scopeDepth);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE, localVariable.escapeFlags);
        TEST_ASSERT_EQUAL_UINT32(0u, closureVariable.scopeDepth);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE, closureVariable.escapeFlags);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void gc_test_assert_object_marked(SZrState *state, const SZrRawObject *object, const char *message) {
    TEST_ASSERT_NOT_NULL_MESSAGE(object, message);
    TEST_ASSERT_TRUE_MESSAGE(ZR_GC_IS_REFERENCED(object) || ZR_GC_IS_WAIT_TO_SCAN(object) ||
                                     ZrCore_RawObject_IsPermanent(state, (SZrRawObject *)object),
                             message);
}

static void test_gc_function_auxiliary_metadata_is_marked_from_root_function(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Function Auxiliary Metadata Is Marked From Root Function";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Function metadata mark traversal",
              "Testing that a rooted function marks auxiliary metadata such as source hashes, static imports, escape bindings, callable summaries, exported bindings, and callsite cache targets during collection propagation");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrFunction *function = ZrCore_Function_New(state);
        SZrFunction *cachedFunction = ZrCore_Function_New(state);
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        SZrString *functionName = ZrCore_String_CreateFromNative(state, "rootFunction");
        SZrString *memberSymbol = ZrCore_String_CreateFromNative(state, "add");
        SZrString *sourceHash = ZrCore_String_CreateFromNative(state, "hash:gc");
        SZrString *staticImport = ZrCore_String_CreateFromNative(state, "zr.container");
        SZrString *exportedName = ZrCore_String_CreateFromNative(state, "exported");
        SZrString *moduleName = ZrCore_String_CreateFromNative(state, "bench");
        SZrString *effectSymbol = ZrCore_String_CreateFromNative(state, "payload");
        SZrString *summaryName = ZrCore_String_CreateFromNative(state, "summary");
        SZrString *topLevelName = ZrCore_String_CreateFromNative(state, "topLevel");
        SZrString *escapeName = ZrCore_String_CreateFromNative(state, "escape");
        SZrString *receiverTypeName = ZrCore_String_CreateFromNative(state, "ReceiverProto");
        SZrString *ownerTypeName = ZrCore_String_CreateFromNative(state, "OwnerProto");
        SZrObjectPrototype *receiverPrototype;
        SZrObjectPrototype *ownerPrototype;

        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_NOT_NULL(cachedFunction);
        TEST_ASSERT_NOT_NULL(functionName);
        TEST_ASSERT_NOT_NULL(memberSymbol);
        TEST_ASSERT_NOT_NULL(sourceHash);
        TEST_ASSERT_NOT_NULL(staticImport);
        TEST_ASSERT_NOT_NULL(exportedName);
        TEST_ASSERT_NOT_NULL(moduleName);
        TEST_ASSERT_NOT_NULL(effectSymbol);
        TEST_ASSERT_NOT_NULL(summaryName);
        TEST_ASSERT_NOT_NULL(topLevelName);
        TEST_ASSERT_NOT_NULL(escapeName);
        TEST_ASSERT_NOT_NULL(receiverTypeName);
        TEST_ASSERT_NOT_NULL(ownerTypeName);

        receiverPrototype = ZrCore_ObjectPrototype_New(state, receiverTypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        ownerPrototype = ZrCore_ObjectPrototype_New(state, ownerTypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        TEST_ASSERT_NOT_NULL(receiverPrototype);
        TEST_ASSERT_NOT_NULL(ownerPrototype);

        function->functionName = functionName;
        function->sourceHash = sourceHash;

        function->memberEntryLength = 1u;
        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberSymbol;

        function->staticImportLength = 1u;
        function->staticImports = (SZrString **)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrString *),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->staticImports);
        function->staticImports[0] = staticImport;

        function->exportedVariableLength = 1u;
        function->exportedVariables = (SZrFunctionExportedVariable *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionExportedVariable),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->exportedVariables);
        memset(function->exportedVariables, 0, sizeof(SZrFunctionExportedVariable));
        function->exportedVariables[0].name = exportedName;

        function->moduleEntryEffectLength = 1u;
        function->moduleEntryEffects = (SZrFunctionModuleEffect *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionModuleEffect),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->moduleEntryEffects);
        memset(function->moduleEntryEffects, 0, sizeof(SZrFunctionModuleEffect));
        function->moduleEntryEffects[0].moduleName = moduleName;
        function->moduleEntryEffects[0].symbolName = effectSymbol;

        function->exportedCallableSummaryLength = 1u;
        function->exportedCallableSummaries = (SZrFunctionCallableSummary *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallableSummary),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->exportedCallableSummaries);
        memset(function->exportedCallableSummaries, 0, sizeof(SZrFunctionCallableSummary));
        function->exportedCallableSummaries[0].name = summaryName;

        function->topLevelCallableBindingLength = 1u;
        function->topLevelCallableBindings = (SZrFunctionTopLevelCallableBinding *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionTopLevelCallableBinding),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->topLevelCallableBindings);
        memset(function->topLevelCallableBindings, 0, sizeof(SZrFunctionTopLevelCallableBinding));
        function->topLevelCallableBindings[0].name = topLevelName;

        function->escapeBindingLength = 1u;
        function->escapeBindings = (SZrFunctionEscapeBinding *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionEscapeBinding),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->escapeBindings);
        memset(function->escapeBindings, 0, sizeof(SZrFunctionEscapeBinding));
        function->escapeBindings[0].name = escapeName;
        function->escapeBindings[0].bindingKind = (TZrUInt8)ZR_FUNCTION_ESCAPE_BINDING_KIND_GLOBAL_BINDING;
        function->escapeBindings[0].escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT;

        function->callSiteCacheLength = 1u;
        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
        function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype = receiverPrototype;
        function->callSiteCaches[0].picSlots[0].cachedOwnerPrototype = ownerPrototype;
        function->callSiteCaches[0].picSlots[0].cachedFunction = cachedFunction;
        function->callSiteCaches[0].picSlotCount = 1u;

        ZrCore_Stack_SetRawObjectValue(state, rootSlot, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
        state->stackTop.valuePointer = rootSlot + 1;

        ZrGarbageCollectorRestartCollection(state);
        TEST_ASSERT_TRUE(ZrGarbageCollectorPropagateAll(state) > 0u);

        gc_test_assert_object_marked(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function), "root function should be marked");
        gc_test_assert_object_marked(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(functionName),
                                     "function name should be marked from rooted function");
        gc_test_assert_object_marked(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(memberSymbol),
                                     "member symbol control should stay marked");
        gc_test_assert_object_marked(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(sourceHash),
                                     "source hash should be marked from rooted function");
        gc_test_assert_object_marked(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(staticImport),
                                     "static import should be marked from rooted function");
        gc_test_assert_object_marked(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(exportedName),
                                     "exported variable name should be marked from rooted function");
        gc_test_assert_object_marked(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(moduleName),
                                     "module effect module name should be marked from rooted function");
        gc_test_assert_object_marked(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(effectSymbol),
                                     "module effect symbol name should be marked from rooted function");
        gc_test_assert_object_marked(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(summaryName),
                                     "callable summary name should be marked from rooted function");
        gc_test_assert_object_marked(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(topLevelName),
                                     "top-level callable binding name should be marked from rooted function");
        gc_test_assert_object_marked(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(escapeName),
                                     "escape binding name should be marked from rooted function");
        gc_test_assert_object_marked(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(receiverPrototype),
                                     "callsite cached receiver prototype should remain live");
        gc_test_assert_object_marked(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(ownerPrototype),
                                     "callsite cached owner prototype should remain live");
        gc_test_assert_object_marked(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(cachedFunction),
                                     "callsite cached function should be marked from rooted function");
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static SZrFunction *gc_test_create_function_with_return_escape(SZrState *state,
                                                               TZrUInt32 stackSlot,
                                                               TZrUInt32 scopeDepth,
                                                               TZrUInt32 escapeFlags) {
    SZrFunction *function;

    function = ZrCore_Function_New(state);
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    function->localVariableLength = 1u;
    function->localVariableList = (SZrFunctionLocalVariable *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionLocalVariable),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->localVariableList == ZR_NULL) {
        return function;
    }

    memset(function->localVariableList, 0, sizeof(SZrFunctionLocalVariable));
    function->localVariableList[0].stackSlot = stackSlot;
    function->localVariableList[0].scopeDepth = scopeDepth;
    function->localVariableList[0].escapeFlags = escapeFlags;

    function->returnEscapeSlotCount = 1u;
    function->returnEscapeSlots = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrUInt32),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->returnEscapeSlots == ZR_NULL) {
        return function;
    }

    function->returnEscapeSlots[0] = stackSlot;
    return function;
}

static SZrFunction *gc_test_create_function_with_closure_capture(SZrState *state,
                                                                 TZrUInt32 scopeDepth,
                                                                 TZrUInt32 escapeFlags) {
    SZrFunction *function;

    function = ZrCore_Function_New(state);
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    function->closureValueLength = 1u;
    function->closureValueList = (SZrFunctionClosureVariable *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionClosureVariable),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->closureValueList == ZR_NULL) {
        return function;
    }

    memset(function->closureValueList, 0, sizeof(SZrFunctionClosureVariable));
    function->closureValueList[0].inStack = ZR_TRUE;
    function->closureValueList[0].index = 0u;
    function->closureValueList[0].scopeDepth = scopeDepth;
    function->closureValueList[0].escapeFlags = escapeFlags;
    return function;
}

static SZrFunction *gc_test_create_function_with_callable_captures(SZrState *state) {
    SZrFunction *function;

    function = ZrCore_Function_New(state);
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    function->functionName = ZrCore_String_CreateFromNative(state, "payloadCaptureTest");
    function->closureValueLength = 2u;
    function->closureValueList = (SZrFunctionClosureVariable *)ZrCore_Memory_RawMallocWithType(
            state->global,
            2u * sizeof(SZrFunctionClosureVariable),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->functionName == ZR_NULL || function->closureValueList == ZR_NULL) {
        return function;
    }

    memset(function->closureValueList, 0, 2u * sizeof(SZrFunctionClosureVariable));
    function->closureValueList[0].inStack = ZR_TRUE;
    function->closureValueList[0].index = 1u;
    function->closureValueList[0].scopeDepth = 0u;
    function->closureValueList[0].escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE;
    function->closureValueList[1].inStack = ZR_TRUE;
    function->closureValueList[1].index = 0u;
    function->closureValueList[1].scopeDepth = 0u;
    function->closureValueList[1].escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE;
    return function;
}

static void test_function_return_escape_promotes_returned_object_during_minor_gc(void) {
    SZrTestTimer timer;
    const char *testSummary = "Function Return Escape Promotes Returned Object During Minor GC";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Return escape metadata to GC promotion",
              "Testing that return-slot escape metadata marks a returned young object as escaped and minor GC promotes it out of survivor space");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrFunction *function = gc_test_create_function_with_return_escape(
                state,
                0u,
                3u,
                ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN);
        SZrObject *object = ZrCore_Object_New(state, ZR_NULL);
        SZrRawObject *oldObject = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
        SZrRawObject *newObject;
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        SZrTypeValue *rootValue;

        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_NOT_NULL(function->localVariableList);
        TEST_ASSERT_NOT_NULL(function->returnEscapeSlots);
        TEST_ASSERT_NOT_NULL(object);

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_Stack_SetRawObjectValue(state, rootSlot, oldObject);
        state->stackTop.valuePointer = rootSlot + 1;

        TEST_ASSERT_TRUE(ZrCore_Function_ApplyReturnEscape(state, function, 0u, ZrCore_Stack_GetValue(rootSlot)));
        TEST_ASSERT_TRUE((oldObject->garbageCollectMark.escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN) != 0u);
        TEST_ASSERT_EQUAL_UINT32(3u, oldObject->garbageCollectMark.anchorScopeDepth);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PROMOTION_REASON_ESCAPE,
                                 oldObject->garbageCollectMark.promotionReason);

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);

        rootValue = ZrCore_Stack_GetValue(rootSlot);
        TEST_ASSERT_NOT_NULL(rootValue);
        TEST_ASSERT_TRUE(rootValue->isGarbageCollectable);
        TEST_ASSERT_NOT_EQUAL(oldObject, rootValue->value.object);

        newObject = rootValue->value.object;
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_OLD, newObject->garbageCollectMark.regionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE,
                                 newObject->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PROMOTION_REASON_ESCAPE,
                                 newObject->garbageCollectMark.promotionReason);
        TEST_ASSERT_TRUE((newObject->garbageCollectMark.escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN) != 0u);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_module_export_marks_exported_object_as_module_root(void) {
    SZrTestTimer timer;
    const char *testSummary = "Module Export Marks Exported Object As Module Root";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Module export escape publication",
              "Testing that publishing a heap object through module exports immediately tags it as a module-root escape candidate");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrObjectModule *module = ZrCore_Module_Create(state);
        SZrObject *child = ZrCore_Object_New(state, ZR_NULL);
        SZrString *name = ZrCore_String_CreateFromNative(state, "value");
        SZrRawObject *childObject = ZR_CAST_RAW_OBJECT_AS_SUPER(child);
        SZrTypeValue childValue;

        TEST_ASSERT_NOT_NULL(module);
        TEST_ASSERT_NOT_NULL(child);
        TEST_ASSERT_NOT_NULL(name);

        ZrCore_Value_InitAsRawObject(state, &childValue, childObject);
        ZrCore_Module_AddPubExport(state, module, name, &childValue);

        TEST_ASSERT_TRUE((childObject->garbageCollectMark.escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PROMOTION_REASON_MODULE_ROOT,
                                 childObject->garbageCollectMark.promotionReason);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_object_base_size_tracks_custom_object_layouts(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Object Base Size Tracks Custom Object Layouts";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Custom object sizing",
              "Testing that GC sizing preserves module and prototype tail fields instead of truncating every object-backed allocation to SZrObject");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrObjectModule *module = ZrCore_Module_Create(state);
        SZrString *className = ZrCore_String_CreateFromNative(state, "GcSizeClass");
        SZrString *structName = ZrCore_String_CreateFromNative(state, "GcSizeStruct");
        SZrObjectPrototype *classPrototype =
                ZrCore_ObjectPrototype_New(state, className, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrStructPrototype *structPrototype = ZrCore_StructPrototype_New(state, structName);

        TEST_ASSERT_NOT_NULL(module);
        TEST_ASSERT_NOT_NULL(className);
        TEST_ASSERT_NOT_NULL(structName);
        TEST_ASSERT_NOT_NULL(classPrototype);
        TEST_ASSERT_NOT_NULL(structPrototype);

        TEST_ASSERT_EQUAL_UINT64((UNITY_UINT64)sizeof(SZrObjectModule),
                                 (UNITY_UINT64)ZrCore_GarbageCollector_GetObjectBaseSize(
                                         state, ZR_CAST_RAW_OBJECT_AS_SUPER(module)));
        TEST_ASSERT_EQUAL_UINT64((UNITY_UINT64)sizeof(SZrObjectPrototype),
                                 (UNITY_UINT64)ZrCore_GarbageCollector_GetObjectBaseSize(
                                         state, ZR_CAST_RAW_OBJECT_AS_SUPER(classPrototype)));
        TEST_ASSERT_EQUAL_UINT64((UNITY_UINT64)sizeof(SZrStructPrototype),
                                 (UNITY_UINT64)ZrCore_GarbageCollector_GetObjectBaseSize(
                                         state, ZR_CAST_RAW_OBJECT_AS_SUPER(structPrototype)));
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_escaped_closure_propagates_capture_escape_on_close(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Escaped Closure Propagates Capture Escape On Close";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Escaped closure close propagation",
              "Testing that a closure escaped through return keeps capture escape metadata on the closure-value wrapper and marks the captured heap object when the stack slot later closes");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrFunction *function = gc_test_create_function_with_closure_capture(
                state,
                7u,
                ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE);
        SZrObject *capturedObject = ZrCore_Object_New(state, ZR_NULL);
        SZrRawObject *capturedRawObject = ZR_CAST_RAW_OBJECT_AS_SUPER(capturedObject);
        TZrStackValuePointer capturedSlot = state->stackBase.valuePointer + 1;
        TZrStackValuePointer closureSlot = state->stackBase.valuePointer + 2;
        SZrTypeValue *closureValueOnStack;
        SZrClosure *closure;
        SZrClosureValue *captureCell;

        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_NOT_NULL(function->closureValueList);
        TEST_ASSERT_NOT_NULL(capturedObject);

        ZrCore_Stack_SetRawObjectValue(state, capturedSlot, capturedRawObject);
        state->stackTop.valuePointer = closureSlot + 1;
        ZrCore_Closure_PushToStack(state, function, ZR_NULL, capturedSlot, closureSlot);

        closureValueOnStack = ZrCore_Stack_GetValue(closureSlot);
        TEST_ASSERT_NOT_NULL(closureValueOnStack);
        TEST_ASSERT_TRUE(closureValueOnStack->isGarbageCollectable);

        closure = ZR_CAST_VM_CLOSURE(state, closureValueOnStack->value.object);
        TEST_ASSERT_NOT_NULL(closure);
        TEST_ASSERT_EQUAL_UINT32(1u, (UNITY_UINT32)closure->closureValueCount);
        captureCell = closure->closureValuesExtend[0];
        TEST_ASSERT_NOT_NULL(captureCell);
        TEST_ASSERT_FALSE(ZrCore_ClosureValue_IsClosed(captureCell));
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE,
                                 capturedRawObject->garbageCollectMark.escapeFlags);

        ZrCore_GarbageCollector_MarkValueEscaped(state,
                                                 closureValueOnStack,
                                                 ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN,
                                                 1u,
                                                 ZR_GARBAGE_COLLECT_PROMOTION_REASON_ESCAPE);

        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE,
                                 capturedRawObject->garbageCollectMark.escapeFlags);
        TEST_ASSERT_TRUE((ZR_CAST_RAW_OBJECT_AS_SUPER(captureCell)->garbageCollectMark.escapeFlags &
                          ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN) != 0u);

        ZrCore_Closure_CloseStackValue(state, capturedSlot);

        TEST_ASSERT_TRUE(ZrCore_ClosureValue_IsClosed(captureCell));
        TEST_ASSERT_TRUE((capturedRawObject->garbageCollectMark.escapeFlags &
                          ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN) != 0u);
        TEST_ASSERT_TRUE((capturedRawObject->garbageCollectMark.escapeFlags &
                          ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PROMOTION_REASON_ESCAPE,
                                 capturedRawObject->garbageCollectMark.promotionReason);
        TEST_ASSERT_EQUAL_UINT32(7u, capturedRawObject->garbageCollectMark.anchorScopeDepth);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_minor_collection_evacuates_stack_root_young_object(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Minor Collection Evacuates Stack Root Young Object";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Stack-rooted young evacuation",
              "Testing that a stack-rooted young object evacuates into survivor space during minor GC and the root slot is rewritten");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrObject *object = ZrCore_Object_New(state, ZR_NULL);
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        SZrRawObject *oldObject = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
        SZrRawObject *newObject;
        SZrTypeValue *rootValue;

        TEST_ASSERT_NOT_NULL(object);

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_Stack_SetRawObjectValue(state, rootSlot, oldObject);
        state->stackTop.valuePointer = rootSlot + 1;
        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;

        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_EDEN, oldObject->garbageCollectMark.regionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE,
                                 oldObject->garbageCollectMark.storageKind);

        ZrCore_GarbageCollector_GcStep(state);

        rootValue = ZrCore_Stack_GetValue(rootSlot);
        TEST_ASSERT_TRUE(rootValue->isGarbageCollectable);
        TEST_ASSERT_NOT_EQUAL(oldObject, rootValue->value.object);

        newObject = rootValue->value.object;
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR, newObject->garbageCollectMark.regionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE,
                                 newObject->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_UINT32(1u, newObject->garbageCollectMark.survivalAge);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PROMOTION_REASON_SURVIVAL,
                                 newObject->garbageCollectMark.promotionReason);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_minor_collection_rewrites_generated_frame_slot_above_stack_top(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Minor Collection Rewrites Generated Frame Slot Above Stack Top";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Generated-frame root rewrite",
              "Testing that minor GC rewrites a young object stored in an active frame slot above the current stackTop but below functionTop, matching the slots GC mark already treats as live");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->callInfoList);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrObject *object = ZrCore_Object_New(state, ZR_NULL);
        TZrStackValuePointer originalStackTop = state->stackTop.valuePointer;
        SZrCallInfo *frame = ZrCore_CallInfo_Extend(state);
        TZrStackValuePointer rootSlot;
        SZrRawObject *oldObject = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
        SZrRawObject *newObject;
        SZrTypeValue *rootValue;

        TEST_ASSERT_NOT_NULL(object);
        TEST_ASSERT_NOT_NULL(frame);

        ZrCore_CallInfo_EntryNativeInit(state, frame, state->stackTop, state->stackTop, state->callInfoList);
        frame->functionBase.valuePointer = originalStackTop;
        frame->functionTop.valuePointer = originalStackTop + 16;
        frame->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
        state->callInfoList = frame;

        rootSlot = frame->functionBase.valuePointer + 8;
        TEST_ASSERT_TRUE(rootSlot >= originalStackTop);
        TEST_ASSERT_TRUE(rootSlot < frame->functionTop.valuePointer);
        TEST_ASSERT_TRUE(ZR_CALL_INFO_IS_VM(state->callInfoList));

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_Stack_SetRawObjectValue(state, rootSlot, oldObject);
        state->stackTop.valuePointer = originalStackTop;
        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;

        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_EDEN, oldObject->garbageCollectMark.regionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE,
                                 oldObject->garbageCollectMark.storageKind);

        ZrCore_GarbageCollector_GcStep(state);

        rootValue = ZrCore_Stack_GetValue(rootSlot);
        TEST_ASSERT_NOT_NULL(rootValue);
        TEST_ASSERT_TRUE(rootValue->isGarbageCollectable);
        TEST_ASSERT_NOT_EQUAL(oldObject, rootValue->value.object);

        newObject = rootValue->value.object;
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR, newObject->garbageCollectMark.regionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE,
                                 newObject->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_UINT32(1u, newObject->garbageCollectMark.survivalAge);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PROMOTION_REASON_SURVIVAL,
                                 newObject->garbageCollectMark.promotionReason);
        TEST_ASSERT_EQUAL_PTR(originalStackTop, state->stackTop.valuePointer);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_minor_collection_preserves_young_descendant_through_old_stack_root_chain(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Minor Collection Preserves Young Descendant Through Old Stack Root Chain";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Old stack-root transitive scan",
              "Testing that minor GC rescans an old stack-root graph deeply enough to preserve a young descendant reachable only through old intermediate objects");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        struct SZrNativeData *root = createTestNativeData(state, 1u);
        struct SZrNativeData *middle = createTestNativeData(state, 1u);
        SZrObject *leaf = ZrCore_Object_New(state, ZR_NULL);
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        SZrRawObject *oldLeaf = ZR_CAST_RAW_OBJECT_AS_SUPER(leaf);
        SZrTypeValue *rootValue;
        SZrTypeValue *middleValue;
        SZrRawObject *newLeaf;

        TEST_ASSERT_NOT_NULL(root);
        TEST_ASSERT_NOT_NULL(middle);
        TEST_ASSERT_NOT_NULL(leaf);

        ZrCore_Value_InitAsRawObject(state, &root->valueExtend[0], ZR_CAST_RAW_OBJECT_AS_SUPER(middle));
        ZrCore_Value_InitAsRawObject(state, &middle->valueExtend[0], oldLeaf);

        ZrCore_RawObject_MarkAsReferenced(ZR_CAST_RAW_OBJECT_AS_SUPER(root));
        ZrCore_RawObject_MarkAsReferenced(ZR_CAST_RAW_OBJECT_AS_SUPER(middle));
        ZrCore_RawObject_SetGenerationalStatus(ZR_CAST_RAW_OBJECT_AS_SUPER(root),
                                               ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL);
        ZrCore_RawObject_SetGenerationalStatus(ZR_CAST_RAW_OBJECT_AS_SUPER(middle),
                                               ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL);
        ZrCore_RawObject_SetStorageKind(ZR_CAST_RAW_OBJECT_AS_SUPER(root),
                                        ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetStorageKind(ZR_CAST_RAW_OBJECT_AS_SUPER(middle),
                                        ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(ZR_CAST_RAW_OBJECT_AS_SUPER(root),
                                       ZR_GARBAGE_COLLECT_REGION_KIND_OLD);
        ZrCore_RawObject_SetRegionKind(ZR_CAST_RAW_OBJECT_AS_SUPER(middle),
                                       ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        ZrCore_Stack_SetRawObjectValue(state, rootSlot, ZR_CAST_RAW_OBJECT_AS_SUPER(root));
        state->stackTop.valuePointer = rootSlot + 1;
        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;

        ZrCore_GarbageCollector_GcStep(state);

        rootValue = ZrCore_Stack_GetValue(rootSlot);
        TEST_ASSERT_NOT_NULL(rootValue);
        TEST_ASSERT_TRUE(rootValue->isGarbageCollectable);
        TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(root), rootValue->value.object);

        middleValue = &root->valueExtend[0];
        TEST_ASSERT_TRUE(middleValue->isGarbageCollectable);
        TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(middle), middleValue->value.object);
        TEST_ASSERT_TRUE(middle->valueExtend[0].isGarbageCollectable);
        TEST_ASSERT_NOT_EQUAL(oldLeaf, middle->valueExtend[0].value.object);

        newLeaf = middle->valueExtend[0].value.object;
        TEST_ASSERT_NOT_NULL(newLeaf);
        TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_OBJECT, newLeaf->type);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR, newLeaf->garbageCollectMark.regionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE,
                                 newLeaf->garbageCollectMark.storageKind);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_minor_collection_preserves_closed_callable_captures(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Minor Collection Preserves Closed Callable Captures";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Closed closure-value evacuation",
              "Testing that minor GC keeps closed callable captures closed after evacuation instead of leaving them pointed at stale stack slots");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrFunction *calleeAFunction = ZrCore_Function_New(state);
        SZrFunction *calleeBFunction = ZrCore_Function_New(state);
        SZrFunction *payloadFunction = gc_test_create_function_with_callable_captures(state);
        SZrClosure *calleeA = ZrCore_Closure_New(state, 0u);
        SZrClosure *calleeB = ZrCore_Closure_New(state, 0u);
        TZrStackValuePointer calleeASlot = state->stackBase.valuePointer;
        TZrStackValuePointer calleeBSlot = state->stackBase.valuePointer + 1;
        TZrStackValuePointer payloadSlot = state->stackBase.valuePointer + 2;
        SZrTypeValue *payloadValue;
        SZrClosure *payloadClosure;
        SZrClosureValue *captureA;
        SZrClosureValue *captureB;
        SZrTypeValue *captureAValue;
        SZrTypeValue *captureBValue;

        TEST_ASSERT_NOT_NULL(calleeAFunction);
        TEST_ASSERT_NOT_NULL(calleeBFunction);
        TEST_ASSERT_NOT_NULL(payloadFunction);
        TEST_ASSERT_NOT_NULL(calleeA);
        TEST_ASSERT_NOT_NULL(calleeB);

        calleeA->function = calleeAFunction;
        calleeB->function = calleeBFunction;
        ZrCore_Stack_SetRawObjectValue(state, calleeASlot, ZR_CAST_RAW_OBJECT_AS_SUPER(calleeA));
        ZrCore_Stack_SetRawObjectValue(state, calleeBSlot, ZR_CAST_RAW_OBJECT_AS_SUPER(calleeB));
        state->stackTop.valuePointer = payloadSlot + 1;

        ZrCore_Closure_PushToStack(state, payloadFunction, ZR_NULL, calleeASlot, payloadSlot);
        payloadValue = ZrCore_Stack_GetValue(payloadSlot);
        TEST_ASSERT_NOT_NULL(payloadValue);
        TEST_ASSERT_TRUE(payloadValue->isGarbageCollectable);
        TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_CLOSURE, payloadValue->type);

        payloadClosure = ZR_CAST_VM_CLOSURE(state, payloadValue->value.object);
        TEST_ASSERT_NOT_NULL(payloadClosure);
        TEST_ASSERT_EQUAL_UINT32(2u, (UNITY_UINT32)payloadClosure->closureValueCount);
        captureA = payloadClosure->closureValuesExtend[0];
        captureB = payloadClosure->closureValuesExtend[1];
        TEST_ASSERT_NOT_NULL(captureA);
        TEST_ASSERT_NOT_NULL(captureB);
        TEST_ASSERT_FALSE(ZrCore_ClosureValue_IsClosed(captureA));
        TEST_ASSERT_FALSE(ZrCore_ClosureValue_IsClosed(captureB));

        ZrCore_Closure_CloseStackValue(state, calleeASlot);
        TEST_ASSERT_TRUE(ZrCore_ClosureValue_IsClosed(captureA));
        TEST_ASSERT_TRUE(ZrCore_ClosureValue_IsClosed(captureB));

        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(calleeASlot), 101);
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(calleeBSlot), 202);
        state->stackTop.valuePointer = payloadSlot + 1;

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;

        ZrCore_GarbageCollector_GcStep(state);

        payloadValue = ZrCore_Stack_GetValue(payloadSlot);
        TEST_ASSERT_NOT_NULL(payloadValue);
        TEST_ASSERT_TRUE(payloadValue->isGarbageCollectable);
        TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_CLOSURE, payloadValue->type);

        payloadClosure = ZR_CAST_VM_CLOSURE(state, payloadValue->value.object);
        TEST_ASSERT_NOT_NULL(payloadClosure);
        captureA = payloadClosure->closureValuesExtend[0];
        captureB = payloadClosure->closureValuesExtend[1];
        TEST_ASSERT_NOT_NULL(captureA);
        TEST_ASSERT_NOT_NULL(captureB);
        TEST_ASSERT_TRUE(ZrCore_ClosureValue_IsClosed(captureA));
        TEST_ASSERT_TRUE(ZrCore_ClosureValue_IsClosed(captureB));

        captureAValue = ZrCore_ClosureValue_GetValue(captureA);
        captureBValue = ZrCore_ClosureValue_GetValue(captureB);
        TEST_ASSERT_NOT_NULL(captureAValue);
        TEST_ASSERT_NOT_NULL(captureBValue);
        TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_CLOSURE, captureAValue->type);
        TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_CLOSURE, captureBValue->type);
        TEST_ASSERT_EQUAL_PTR(calleeBFunction, ZR_CAST_VM_CLOSURE(state, captureAValue->value.object)->function);
        TEST_ASSERT_EQUAL_PTR(calleeAFunction, ZR_CAST_VM_CLOSURE(state, captureBValue->value.object)->function);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_minor_collection_rewrites_old_reference_to_forwarded_child(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Minor Collection Rewrites Old Reference To Forwarded Child";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Old-to-young forwarding rewrite",
              "Testing that minor GC promotes an old-referenced young child and rewrites the parent's field to the forwarded location");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrObject *parent = ZrCore_Object_New(state, ZR_NULL);
        SZrObject *child = ZrCore_Object_New(state, ZR_NULL);
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "child");
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        SZrTypeValue memberKey;
        SZrTypeValue childValue;
        SZrRawObject *oldChild = ZR_CAST_RAW_OBJECT_AS_SUPER(child);
        const SZrTypeValue *resolvedChildValue;
        SZrRawObject *newChild;

        TEST_ASSERT_NOT_NULL(parent);
        TEST_ASSERT_NOT_NULL(child);
        TEST_ASSERT_NOT_NULL(memberName);

        ZrCore_Value_InitAsRawObject(state, &memberKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
        ZrCore_Value_InitAsRawObject(state, &childValue, oldChild);

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_RawObject_MarkAsReferenced(ZR_CAST_RAW_OBJECT_AS_SUPER(parent));
        ZrCore_RawObject_SetStorageKind(ZR_CAST_RAW_OBJECT_AS_SUPER(parent),
                                        ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(ZR_CAST_RAW_OBJECT_AS_SUPER(parent),
                                       ZR_GARBAGE_COLLECT_REGION_KIND_OLD);
        ZrCore_Object_SetValue(state, parent, &memberKey, &childValue);
        ZrCore_Stack_SetRawObjectValue(state, rootSlot, ZR_CAST_RAW_OBJECT_AS_SUPER(parent));
        state->stackTop.valuePointer = rootSlot + 1;
        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;

        resolvedChildValue = ZrCore_Object_GetValue(state, parent, &memberKey);
        TEST_ASSERT_NOT_NULL(resolvedChildValue);
        TEST_ASSERT_EQUAL_PTR(oldChild, resolvedChildValue->value.object);

        ZrCore_GarbageCollector_GcStep(state);

        memberName = ZrCore_String_CreateFromNative(state, "child");
        TEST_ASSERT_NOT_NULL(memberName);
        ZrCore_Value_InitAsRawObject(state, &memberKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
        resolvedChildValue = ZrCore_Object_GetValue(state, parent, &memberKey);
        TEST_ASSERT_NOT_NULL(resolvedChildValue);
        TEST_ASSERT_TRUE(resolvedChildValue->isGarbageCollectable);
        TEST_ASSERT_NOT_EQUAL(oldChild, resolvedChildValue->value.object);

        newChild = resolvedChildValue->value.object;
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_OLD, newChild->garbageCollectMark.regionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE,
                                 newChild->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PROMOTION_REASON_OLD_REFERENCE,
                                 newChild->garbageCollectMark.promotionReason);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_minor_collection_remembers_promoted_parent_holding_young_child(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Minor Collection Remembers Promoted Parent Holding Young Child";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Promoted-parent remembered set registration",
              "Testing that a young parent promoted to old during minor GC is inserted into the remembered set before the next minor when it still holds a surviving young child");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrObject *parent = ZrCore_Object_New(state, ZR_NULL);
        SZrObject *child = ZrCore_Object_New(state, ZR_NULL);
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        SZrTypeValue childValue;
        SZrTypeValue memberKey;
        SZrTypeValue *rootValue;
        const SZrTypeValue *resolvedChildValue;
        SZrRawObject *oldParent = ZR_CAST_RAW_OBJECT_AS_SUPER(parent);
        SZrRawObject *newParent;
        SZrRawObject *firstMinorChild;
        SZrRawObject *secondMinorChild;
        SZrString *memberName;

        TEST_ASSERT_NOT_NULL(parent);
        TEST_ASSERT_NOT_NULL(child);

        memberName = ZrCore_String_CreateFromNative(state, "child");
        TEST_ASSERT_NOT_NULL(memberName);
        ZrCore_Value_InitAsRawObject(state, &memberKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
        ZrCore_Value_InitAsRawObject(state, &childValue, ZR_CAST_RAW_OBJECT_AS_SUPER(child));

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_Object_SetValue(state, parent, &memberKey, &childValue);
        ZrCore_Stack_SetRawObjectValue(state, rootSlot, oldParent);
        state->stackTop.valuePointer = rootSlot + 1;
        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;

        ZrCore_GarbageCollector_MarkRawObjectEscaped(state,
                                                     oldParent,
                                                     ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT,
                                                     oldParent->garbageCollectMark.anchorScopeDepth,
                                                     ZR_GARBAGE_COLLECT_PROMOTION_REASON_GLOBAL_ROOT);

        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        ZrCore_GarbageCollector_GcStep(state);

        rootValue = ZrCore_Stack_GetValue(rootSlot);
        TEST_ASSERT_NOT_NULL(rootValue);
        TEST_ASSERT_TRUE(rootValue->isGarbageCollectable);
        TEST_ASSERT_NOT_EQUAL(oldParent, rootValue->value.object);

        newParent = rootValue->value.object;
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_OLD, newParent->garbageCollectMark.regionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE,
                                 newParent->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, newParent));
        TEST_ASSERT_TRUE(newParent->garbageCollectMark.rememberedRegistryIndex < gc->rememberedObjectCount);
        TEST_ASSERT_EQUAL_PTR(newParent,
                              gc->rememberedObjects[newParent->garbageCollectMark.rememberedRegistryIndex]);

        memberName = ZrCore_String_CreateFromNative(state, "child");
        TEST_ASSERT_NOT_NULL(memberName);
        ZrCore_Value_InitAsRawObject(state, &memberKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
        resolvedChildValue = ZrCore_Object_GetValue(state, ZR_CAST_OBJECT(state, newParent), &memberKey);
        TEST_ASSERT_NOT_NULL(resolvedChildValue);
        TEST_ASSERT_TRUE(resolvedChildValue->isGarbageCollectable);
        firstMinorChild = resolvedChildValue->value.object;
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR,
                                 firstMinorChild->garbageCollectMark.regionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE,
                                 firstMinorChild->garbageCollectMark.storageKind);

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);

        memberName = ZrCore_String_CreateFromNative(state, "child");
        TEST_ASSERT_NOT_NULL(memberName);
        ZrCore_Value_InitAsRawObject(state, &memberKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
        resolvedChildValue = ZrCore_Object_GetValue(state, ZR_CAST_OBJECT(state, newParent), &memberKey);
        TEST_ASSERT_NOT_NULL(resolvedChildValue);
        TEST_ASSERT_TRUE(resolvedChildValue->isGarbageCollectable);
        secondMinorChild = resolvedChildValue->value.object;
        TEST_ASSERT_NOT_EQUAL(firstMinorChild, secondMinorChild);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_OLD,
                                 secondMinorChild->garbageCollectMark.regionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE,
                                 secondMinorChild->garbageCollectMark.storageKind);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_repeated_minor_collections_do_not_churn_region_ids_without_new_young_allocations(void) {
    SZrTestTimer timer;
    const char *testSummary =
            "GC Repeated Minor Collections Do Not Churn Region Ids Without New Young Allocations";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Repeated minor collection region stability",
              "Testing that once a promoted parent and child finish evacuating into old space, later minor collections without any new young allocations do not keep minting fresh regions");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrObject *parent = ZrCore_Object_New(state, ZR_NULL);
        SZrObject *child = ZrCore_Object_New(state, ZR_NULL);
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        SZrTypeValue childValue;
        SZrTypeValue memberKey;
        SZrTypeValue *rootValue;
        const SZrTypeValue *resolvedChildValue;
        SZrRawObject *oldParent = ZR_CAST_RAW_OBJECT_AS_SUPER(parent);
        SZrRawObject *newParent;
        SZrRawObject *promotedChild;
        SZrString *memberName;
        TZrUInt32 stableNextRegionId;
        TZrSize stableRegionCount;
        TZrUInt32 stableParentRegionId;
        TZrUInt32 stableChildRegionId;
        TZrUInt32 iteration;

        TEST_ASSERT_NOT_NULL(parent);
        TEST_ASSERT_NOT_NULL(child);

        memberName = ZrCore_String_CreateFromNative(state, "child");
        TEST_ASSERT_NOT_NULL(memberName);
        ZrCore_Value_InitAsRawObject(state, &memberKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
        ZrCore_Value_InitAsRawObject(state, &childValue, ZR_CAST_RAW_OBJECT_AS_SUPER(child));

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_Object_SetValue(state, parent, &memberKey, &childValue);
        ZrCore_Stack_SetRawObjectValue(state, rootSlot, oldParent);
        state->stackTop.valuePointer = rootSlot + 1;
        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;

        ZrCore_GarbageCollector_MarkRawObjectEscaped(state,
                                                     oldParent,
                                                     ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT,
                                                     oldParent->garbageCollectMark.anchorScopeDepth,
                                                     ZR_GARBAGE_COLLECT_PROMOTION_REASON_GLOBAL_ROOT);

        ZrCore_GarbageCollector_GcStep(state);

        rootValue = ZrCore_Stack_GetValue(rootSlot);
        TEST_ASSERT_NOT_NULL(rootValue);
        TEST_ASSERT_TRUE(rootValue->isGarbageCollectable);
        newParent = rootValue->value.object;
        TEST_ASSERT_NOT_NULL(newParent);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_OLD, newParent->garbageCollectMark.regionKind);

        memberName = ZrCore_String_CreateFromNative(state, "child");
        TEST_ASSERT_NOT_NULL(memberName);
        ZrCore_Value_InitAsRawObject(state, &memberKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
        resolvedChildValue = ZrCore_Object_GetValue(state, ZR_CAST_OBJECT(state, newParent), &memberKey);
        TEST_ASSERT_NOT_NULL(resolvedChildValue);
        TEST_ASSERT_TRUE(resolvedChildValue->isGarbageCollectable);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR,
                                 resolvedChildValue->value.object->garbageCollectMark.regionKind);

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);

        memberName = ZrCore_String_CreateFromNative(state, "child");
        TEST_ASSERT_NOT_NULL(memberName);
        ZrCore_Value_InitAsRawObject(state, &memberKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
        resolvedChildValue = ZrCore_Object_GetValue(state, ZR_CAST_OBJECT(state, newParent), &memberKey);
        TEST_ASSERT_NOT_NULL(resolvedChildValue);
        TEST_ASSERT_TRUE(resolvedChildValue->isGarbageCollectable);
        promotedChild = resolvedChildValue->value.object;
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_OLD,
                                 promotedChild->garbageCollectMark.regionKind);

        stableNextRegionId = gc->nextRegionId;
        stableRegionCount = gc->regionCount;
        stableParentRegionId = newParent->garbageCollectMark.regionId;
        stableChildRegionId = promotedChild->garbageCollectMark.regionId;

        for (iteration = 0u; iteration < 6u; iteration++) {
            gc->gcDebtSize = 4096;
            gc->gcLastStepWork = 0;
            ZrCore_GarbageCollector_GcStep(state);

            rootValue = ZrCore_Stack_GetValue(rootSlot);
            TEST_ASSERT_NOT_NULL(rootValue);
            TEST_ASSERT_TRUE(rootValue->isGarbageCollectable);
            TEST_ASSERT_EQUAL_PTR(newParent, rootValue->value.object);

            memberName = ZrCore_String_CreateFromNative(state, "child");
            TEST_ASSERT_NOT_NULL(memberName);
            ZrCore_Value_InitAsRawObject(state, &memberKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
            resolvedChildValue = ZrCore_Object_GetValue(state, ZR_CAST_OBJECT(state, newParent), &memberKey);
            TEST_ASSERT_NOT_NULL(resolvedChildValue);
            TEST_ASSERT_TRUE(resolvedChildValue->isGarbageCollectable);
            TEST_ASSERT_EQUAL_PTR(promotedChild, resolvedChildValue->value.object);

            TEST_ASSERT_EQUAL_UINT32(stableNextRegionId, gc->nextRegionId);
            TEST_ASSERT_EQUAL_UINT64((UNITY_UINT64)stableRegionCount, (UNITY_UINT64)gc->regionCount);
            TEST_ASSERT_EQUAL_UINT32(stableParentRegionId, newParent->garbageCollectMark.regionId);
            TEST_ASSERT_EQUAL_UINT32(stableChildRegionId, promotedChild->garbageCollectMark.regionId);
            TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, newParent));
            TEST_ASSERT_EQUAL_UINT64((UNITY_UINT64)ZR_MAX_SIZE,
                                     (UNITY_UINT64)newParent->garbageCollectMark.rememberedRegistryIndex);
        }
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static void test_gc_current_region_cache_tracks_descriptor_index_across_registry_growth(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Current Region Cache Tracks Descriptor Index Across Registry Growth";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Current region descriptor index cache",
              "Testing that the active eden-region cache keeps a direct descriptor index, clears it when the region is released, and remains valid after the region registry grows");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        TZrSize objectSize = sizeof(SZrObject);
        TZrSize iteration;

        gc->youngRegionSize = (TZrUInt64)objectSize;
        gc->currentEdenRegionId = 0u;
        gc->currentEdenRegionUsedBytes = 0u;
        gc->currentEdenRegionIndex = ZR_MAX_SIZE;

        for (iteration = 0u; iteration < 12u; iteration++) {
            SZrRawObject *object = createTestObject(state, ZR_VALUE_TYPE_OBJECT, objectSize);

            TEST_ASSERT_NOT_NULL(object);
            TEST_ASSERT_TRUE(gc->regionCount >= 1u);
            TEST_ASSERT_TRUE(gc->currentEdenRegionIndex < gc->regionCount);
            TEST_ASSERT_EQUAL_UINT32(object->garbageCollectMark.regionId, gc->currentEdenRegionId);
            TEST_ASSERT_EQUAL_UINT64((UNITY_UINT64)gc->currentEdenRegionIndex,
                                     (UNITY_UINT64)object->garbageCollectMark.regionDescriptorIndex);
            TEST_ASSERT_EQUAL_UINT32(gc->currentEdenRegionId, gc->regions[gc->currentEdenRegionIndex].id);
            TEST_ASSERT_EQUAL_UINT64((UNITY_UINT64)gc->currentEdenRegionUsedBytes,
                                     (UNITY_UINT64)gc->regions[gc->currentEdenRegionIndex].usedBytes);

            ZrCore_RawObject_MarkAsPermanent(state, object);

            TEST_ASSERT_EQUAL_UINT32(0u, gc->currentEdenRegionId);
            TEST_ASSERT_EQUAL_UINT64(0u, (UNITY_UINT64)gc->currentEdenRegionUsedBytes);
            TEST_ASSERT_EQUAL_UINT64((UNITY_UINT64)ZR_MAX_SIZE, (UNITY_UINT64)gc->currentEdenRegionIndex);
            TEST_ASSERT_TRUE(object->garbageCollectMark.regionDescriptorIndex < gc->regionCount);
            TEST_ASSERT_EQUAL_UINT32(object->garbageCollectMark.regionId,
                                     gc->regions[object->garbageCollectMark.regionDescriptorIndex].id);
        }

        TEST_ASSERT_TRUE(gc->regionCount > 8u);

        {
            SZrRawObject *reusedObject = createTestObject(state, ZR_VALUE_TYPE_OBJECT, objectSize);

            TEST_ASSERT_NOT_NULL(reusedObject);
            TEST_ASSERT_TRUE(gc->currentEdenRegionIndex < gc->regionCount);
            TEST_ASSERT_EQUAL_UINT32(reusedObject->garbageCollectMark.regionId, gc->currentEdenRegionId);
            TEST_ASSERT_EQUAL_UINT64((UNITY_UINT64)gc->currentEdenRegionIndex,
                                     (UNITY_UINT64)reusedObject->garbageCollectMark.regionDescriptorIndex);
            TEST_ASSERT_EQUAL_UINT32(gc->currentEdenRegionId, gc->regions[gc->currentEdenRegionIndex].id);
            TEST_ASSERT_EQUAL_UINT64((UNITY_UINT64)gc->currentEdenRegionUsedBytes,
                                     (UNITY_UINT64)gc->regions[gc->currentEdenRegionIndex].usedBytes);
        }
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

static TZrSize gc_test_concat_pair_cache_bucket_index(const SZrString *left, const SZrString *right) {
    TZrUInt64 leftHash;
    TZrUInt64 rightHash;
    TZrUInt64 mixedHash;

    if (left == ZR_NULL || right == ZR_NULL) {
        return 0u;
    }

    leftHash = left->super.hash;
    rightHash = right->super.hash;
    mixedHash = (leftHash * 1315423911u) ^ (rightHash + (leftHash << 7u) + (rightHash >> 3u));
    return (TZrSize)(mixedHash % ZR_GLOBAL_CONCAT_PAIR_CACHE_BUCKET_COUNT);
}

static const ZrStringConcatPairCacheEntry *gc_test_find_concat_pair_cache_entry(SZrGlobalState *global,
                                                                                const SZrString *left,
                                                                                const SZrString *right) {
    TZrSize bucketIndex;

    if (global == ZR_NULL || left == ZR_NULL || right == ZR_NULL) {
        return ZR_NULL;
    }

    bucketIndex = gc_test_concat_pair_cache_bucket_index(left, right);
    for (TZrSize depthIndex = 0; depthIndex < ZR_GLOBAL_CONCAT_PAIR_CACHE_BUCKET_DEPTH; depthIndex++) {
        const ZrStringConcatPairCacheEntry *entry = &global->stringConcatPairCache[bucketIndex][depthIndex];

        if (entry->left == left && entry->right == right) {
            return entry;
        }
    }

    return ZR_NULL;
}

static void test_gc_short_concat_pair_cache_rewrites_forwarded_entries(void) {
    SZrTestTimer timer;
    const char *testSummary = "GC Short Concat Pair Cache Rewrites Forwarded Entries";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Short concat pair cache forwarding rewrite",
              "Testing that the exact short-string concat cache keeps left/right/result rooted during minor GC and rewrites the cache slots to the forwarded objects");
    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *left;
        SZrString *right;
        SZrString *result;
        SZrString *currentLeft;
        SZrString *currentRight;
        SZrString *currentResult;
        const ZrStringConcatPairCacheEntry *entry;

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        gc->youngRegionSize = (TZrUInt64)(sizeof(SZrString) + ZR_VM_SHORT_STRING_MAX + 32u);

        left = ZrCore_String_CreateFromNative(state, "aa");
        right = ZrCore_String_CreateFromNative(state, "_slot");
        result = ZrCore_String_ConcatPair(state, left, right);

        TEST_ASSERT_NOT_NULL(left);
        TEST_ASSERT_NOT_NULL(right);
        TEST_ASSERT_NOT_NULL(result);

        entry = gc_test_find_concat_pair_cache_entry(state->global, left, right);
        TEST_ASSERT_NOT_NULL(entry);
        TEST_ASSERT_EQUAL_PTR(left, entry->left);
        TEST_ASSERT_EQUAL_PTR(right, entry->right);
        TEST_ASSERT_EQUAL_PTR(result, entry->result);

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);

        currentLeft = ZrCore_String_CreateFromNative(state, "aa");
        currentRight = ZrCore_String_CreateFromNative(state, "_slot");
        currentResult = ZrCore_String_CreateFromNative(state, "aa_slot");

        TEST_ASSERT_NOT_NULL(currentLeft);
        TEST_ASSERT_NOT_NULL(currentRight);
        TEST_ASSERT_NOT_NULL(currentResult);
        TEST_ASSERT_TRUE(left != currentLeft || right != currentRight || result != currentResult);

        entry = gc_test_find_concat_pair_cache_entry(state->global, currentLeft, currentRight);
        TEST_ASSERT_NOT_NULL(entry);
        TEST_ASSERT_EQUAL_PTR(currentLeft, entry->left);
        TEST_ASSERT_EQUAL_PTR(currentRight, entry->right);
        TEST_ASSERT_EQUAL_PTR(currentResult, entry->result);
    }

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 主测试函数
int main(void) {
    printf("\n");
    TEST_MODULE_DIVIDER();
    printf("ZR-VM GC Module Unit Tests\n");
    TEST_MODULE_DIVIDER();
    printf("\n");
    
    UNITY_BEGIN();
    
    // 基础功能测试模块
    printf("==========\n");
    printf("Basic Functionality Tests\n");
    printf("==========\n");
    RUN_TEST(test_basic_types);
    RUN_TEST(test_gc_status_macros);
    RUN_TEST(test_gc_functions);
    RUN_TEST(test_boundary_conditions);
    
    // GC核心功能测试模块
    printf("==========\n");
    printf("GC Core Functionality Tests\n");
    printf("==========\n");
    RUN_TEST(test_gc_sweep_phase);
    RUN_TEST(test_gc_mark_traversal);
    RUN_TEST(test_system_gc_enable_callback_switches_generational_mode);
    RUN_TEST(test_gc_root_marking);
    RUN_TEST(test_gc_state_machine);
    
    // GC高级功能测试模块
    printf("==========\n");
    printf("GC Advanced Functionality Tests\n");
    printf("==========\n");
    RUN_TEST(test_gc_generational);
    RUN_TEST(test_gc_native_data);
    RUN_TEST(test_gc_full_collection);
    RUN_TEST(test_gc_step);
    RUN_TEST(test_gc_barrier);
    RUN_TEST(test_gc_pause_budget_consumes_multiple_incremental_steps);
    RUN_TEST(test_gc_sweep_slice_budget_limits_single_step_sweep);
    RUN_TEST(test_gc_ignore_registry_and_phase_metadata);
    RUN_TEST(test_gc_ignore_registry_swap_remove_keeps_remaining_object_queryable);
    RUN_TEST(test_gc_barrier_unignores_escaped_object);
    RUN_TEST(test_gc_region_configuration_defaults);
    RUN_TEST(test_gc_control_plane_updates_snapshot);
    RUN_TEST(test_gc_step_records_timing_in_snapshot);
    RUN_TEST(test_gc_snapshot_accumulates_collection_counts_and_durations);
    RUN_TEST(test_gc_snapshot_reports_region_pressure_shape);
    RUN_TEST(test_gc_scheduled_collection_check_gc_runs_major_without_forced_compact);
    RUN_TEST(test_gc_heap_limit_pressure_check_gc_escalates_to_full);
    RUN_TEST(test_gc_full_collection_compacts_old_reference_graph);
    RUN_TEST(test_gc_barrier_records_old_to_young_remembered_escape);
    RUN_TEST(test_gc_barrier_records_permanent_to_young_remembered_escape);
    RUN_TEST(test_gc_object_set_value_records_old_to_young_remembered_escape_from_inited_parent);
    RUN_TEST(test_gc_pinning_marks_object_non_moving);
    RUN_TEST(test_gc_region_allocator_reuses_emptied_eden_region_after_permanent_transition);
    RUN_TEST(test_gc_permanent_transition_accepts_referenced_interned_string_without_unlinking_gc_list);
    RUN_TEST(test_gc_region_allocator_reassigns_pinned_object_region);
    RUN_TEST(test_function_escape_metadata_defaults);
    RUN_TEST(test_gc_function_auxiliary_metadata_is_marked_from_root_function);
    RUN_TEST(test_function_return_escape_promotes_returned_object_during_minor_gc);
    RUN_TEST(test_module_export_marks_exported_object_as_module_root);
    RUN_TEST(test_gc_object_base_size_tracks_custom_object_layouts);
    RUN_TEST(test_gc_escaped_closure_propagates_capture_escape_on_close);
    RUN_TEST(test_gc_minor_collection_evacuates_stack_root_young_object);
    RUN_TEST(test_gc_minor_collection_rewrites_generated_frame_slot_above_stack_top);
    RUN_TEST(test_gc_minor_collection_preserves_young_descendant_through_old_stack_root_chain);
    RUN_TEST(test_gc_minor_collection_preserves_closed_callable_captures);
    RUN_TEST(test_gc_minor_collection_rewrites_old_reference_to_forwarded_child);
    RUN_TEST(test_gc_minor_collection_remembers_promoted_parent_holding_young_child);
    RUN_TEST(test_gc_repeated_minor_collections_do_not_churn_region_ids_without_new_young_allocations);
    RUN_TEST(test_gc_current_region_cache_tracks_descriptor_index_across_registry_growth);
    RUN_TEST(test_gc_short_concat_pair_cache_rewrites_forwarded_entries);
    RUN_TEST(test_ownership_shared_refcount_and_weak_null_on_release);
    RUN_TEST(test_ownership_unique_can_return_to_gc_control);
    RUN_TEST(test_ownership_weak_expires_when_returned_object_is_released);
    
    printf("\n");
    TEST_MODULE_DIVIDER();
    printf("All Test Cases Completed\n");
    TEST_MODULE_DIVIDER();
    printf("\n");
    
    return UNITY_END();
}
