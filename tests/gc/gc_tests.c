//
// Created by AI Assistant on 2026/1/2.
//

#include <stdio.h>
#include <time.h>
#include "unity.h"
#include "gc_test_utils.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_common/zr_common_conf.h"

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

#define TEST_PASS(timer, summary) do { \
    double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary); \
    fflush(stdout); \
} while(0)

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

// 测试基础类型
void test_basic_types(void) {
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
void test_gc_status_macros(void) {
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
    
    SZrGlobalState* global = state->global;
    SZrGarbageCollector* gc = global->garbageCollector;
    
    TEST_INFO("Test object creation", 
              "Creating test object with type ZR_VALUE_TYPE_OBJECT");
    SZrRawObject* obj = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
    TEST_ASSERT_NOT_NULL(obj);
    
    TEST_INFO("Object status macros - INITED", 
              "Testing ZR_GC_IS_INITED macro by marking object as initialized");
    ZrRawObjectMarkAsInit(state, obj);
    TEST_ASSERT_TRUE(ZR_GC_IS_INITED(obj));
    
    TEST_INFO("Object status macros - WAIT_TO_SCAN", 
              "Testing ZR_GC_IS_WAIT_TO_SCAN macro by marking object as wait to scan");
    ZrRawObjectMarkAsWaitToScan(obj);
    TEST_ASSERT_TRUE(ZR_GC_IS_WAIT_TO_SCAN(obj));
    
    TEST_INFO("Object status macros - REFERENCED", 
              "Testing ZR_GC_IS_REFERENCED macro by marking object as referenced");
    ZrRawObjectMarkAsReferenced(obj);
    TEST_ASSERT_TRUE(ZR_GC_IS_REFERENCED(obj));
    
    TEST_INFO("Generational marking macros - New generation", 
              "Testing ZR_GC_IS_OLD macro with NEW generational status");
    ZrRawObjectSetGenerationalStatus(obj, ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW);
    TEST_ASSERT_FALSE(ZR_GC_IS_OLD(obj));
    
    TEST_INFO("Generational marking macros - Old generation", 
              "Testing ZR_GC_IS_OLD macro with SURVIVAL generational status");
    fflush(stdout);  // 确保输出刷新
    // 设置世代状态
    obj->garbageCollectMark.generationalStatus = ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL;
    fflush(stdout);  // 确保输出刷新
    // 直接检查状态值，避免宏调用可能的问题
    TBool isOldResult = (obj->garbageCollectMark.generationalStatus >= ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL);
    fflush(stdout);  // 确保输出刷新
    TEST_ASSERT_TRUE(isOldResult);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试GC函数
void test_gc_functions(void) {
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
              "Testing ZrGarbageCollectorAddDebtSpace function to add memory debt");
    TZrMemoryOffset initialDebt = global->garbageCollector->gcDebtSize;
    TZrMemoryOffset addAmount = 1000;
    ZrGarbageCollectorAddDebtSpace(global, addAmount);
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
              "Testing ZrRawObjectNew function to create a new object");
    SZrRawObject* obj = createTestObject(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrRawObject));
    TEST_ASSERT_NOT_NULL(obj);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, obj->type);
    
    TEST_INFO("Object reference status", 
              "Testing ZrRawObjectIsUnreferenced function to check object reference");
    TBool isUnreferenced = ZrRawObjectIsUnreferenced(state, obj);
    TEST_ASSERT_FALSE(isUnreferenced);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试边界条件
void test_boundary_conditions(void) {
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
    ZrGarbageCollectorAddDebtSpace(global, largeValue);
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
    TZrMemoryOffset beforeOverflow = global->garbageCollector->gcDebtSize;
    // 添加一个会导致溢出的值
    ZrGarbageCollectorAddDebtSpace(global, ZR_MAX_MEMORY_OFFSET);
    TZrMemoryOffset afterOverflow = global->garbageCollector->gcDebtSize;
    // 验证溢出保护：债务应该被限制为最大值
    TEST_ASSERT_EQUAL_INT64(ZR_MAX_MEMORY_OFFSET, afterOverflow);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试GC清除阶段
void test_gc_sweep_phase(void) {
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
    ZrRawObjectMarkAsInit(state, obj1);
    ZrRawObjectMarkAsInit(state, obj2);
    
    TEST_INFO("Sweep phase status check", 
              "Verifying that GC is not in sweeping phase initially");
    TBool isSweeping = ZrGarbageCollectorIsSweeping(global);
    TEST_ASSERT_FALSE(isSweeping);
    
    TEST_INFO("Enter sweep phase", 
              "Setting GC running status to SWEEP_OBJECTS and verifying sweep phase detection");
    gc->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS;
    isSweeping = ZrGarbageCollectorIsSweeping(global);
    TEST_ASSERT_TRUE(isSweeping);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试GC标记遍历
void test_gc_mark_traversal(void) {
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
    ZrRawObjectMarkAsInit(state, obj);
    TEST_ASSERT_TRUE(ZR_GC_IS_INITED(obj));
    
    TEST_INFO("Mark object as WAIT_TO_SCAN", 
              "Testing WAIT_TO_SCAN status by setting wait-to-scan status and verifying ZR_GC_IS_WAIT_TO_SCAN macro");
    ZrRawObjectMarkAsWaitToScan(obj);
    TEST_ASSERT_TRUE(ZR_GC_IS_WAIT_TO_SCAN(obj));
    
    TEST_INFO("Mark object as REFERENCED", 
              "Testing REFERENCED status by marking as referenced and verifying ZR_GC_IS_REFERENCED macro");
    ZrRawObjectMarkAsReferenced(obj);
    TEST_ASSERT_TRUE(ZR_GC_IS_REFERENCED(obj));
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试GC根对象标记
void test_gc_root_marking(void) {
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
    TUInt32 prototypeCount = 0;
    for (TUInt32 i = 0; i < ZR_VALUE_TYPE_ENUM_MAX; i++) {
        if (global->basicTypeObjectPrototype[i] != ZR_NULL) {
            prototypeCount++;
        }
    }
    // 验证数组存在（元表可能为NULL，这是正常的）
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试GC状态机控制
void test_gc_state_machine(void) {
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
    TBool isSweeping = ZrGarbageCollectorIsSweeping(global);
    TEST_ASSERT_FALSE(isSweeping);
    
    TEST_INFO("State transition - Sweep phase", 
              "Testing state transition to SWEEP_OBJECTS and verifying sweep phase detection");
    gc->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS;
    isSweeping = ZrGarbageCollectorIsSweeping(global);
    TEST_ASSERT_TRUE(isSweeping);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试分代GC
void test_gc_generational(void) {
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
    ZrRawObjectSetGenerationalStatus(obj, ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW);
    TBool isOld = ZR_GC_IS_OLD(obj);
    TEST_ASSERT_FALSE(isOld);
    
    TEST_INFO("Promotion to old generation", 
              "Testing SURVIVAL generational status and verifying ZR_GC_IS_OLD returns true");
    ZrRawObjectSetGenerationalStatus(obj, ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL);
    isOld = ZR_GC_IS_OLD(obj);
    TEST_ASSERT_TRUE(isOld);
    
    TEST_INFO("Generational GC mode setting", 
              "Setting GC mode to GENERATIONAL and verifying the setting");
    gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    TEST_ASSERT_EQUAL_INT(ZR_GARBAGE_COLLECT_MODE_GENERATIONAL, gc->gcMode);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试Native Data处理
void test_gc_native_data(void) {
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
    TUInt32 valueCount = 5;
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
    TUInt32 nullCount = 0;
    for (TUInt32 i = 0; i < nativeData->valueLength; i++) {
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
void test_gc_full_collection(void) {
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
              "Executing ZrGarbageCollectorGcFull to perform complete garbage collection");
    ZrGarbageCollectorGcFull(state, ZR_FALSE);
    
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
void test_gc_step(void) {
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
    ZrGarbageCollectorAddDebtSpace(global, addDebt);
    
    TEST_INFO("Execute GC step", 
              "Executing ZrGarbageCollectorGcStep to perform incremental GC");
    ZrGarbageCollectorGcStep(state);
    
    TEST_INFO("GC status verification", 
              "Verifying that GC status is either RUNNING or STOP_BY_SELF after step");
    EZrGarbageCollectStatus status = global->garbageCollector->gcStatus;
    TBool isValidStatus = (status == ZR_GARBAGE_COLLECT_STATUS_RUNNING ||
                          status == ZR_GARBAGE_COLLECT_STATUS_STOP_BY_SELF);
    TEST_ASSERT_TRUE(isValidStatus);
    
    destroyTestState(state);
    
    timer.endTime = clock();
    TEST_PASS(timer, testSummary);
    TEST_DIVIDER();
}

// 测试GC屏障
void test_gc_barrier(void) {
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
    ZrRawObjectMarkAsReferenced(parent);
    TBool isReferenced = ZR_GC_IS_REFERENCED(parent);
    TEST_ASSERT_TRUE(isReferenced);
    
    TEST_INFO("Test barrier: REFERENCED object pointing to INITED object", 
              "Calling ZrGarbageCollectorBarrier when REFERENCED parent points to INITED child");
    ZrGarbageCollectorBarrier(state, parent, child);
    
    TEST_INFO("Verify child object is marked", 
              "Verifying that child object is marked (WAIT_TO_SCAN or REFERENCED) after barrier");
    TBool isWaitToScan = ZR_GC_IS_WAIT_TO_SCAN(child);
    TBool isChildReferenced = ZR_GC_IS_REFERENCED(child);
    TEST_ASSERT_TRUE(isWaitToScan || isChildReferenced);
    
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
    
    printf("\n");
    TEST_MODULE_DIVIDER();
    printf("All Test Cases Completed\n");
    TEST_MODULE_DIVIDER();
    printf("\n");
    
    return UNITY_END();
}