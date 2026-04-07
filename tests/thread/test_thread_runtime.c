#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "test_support.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"
#include "zr_vm_lib_thread/module.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/compiler.h"

static SZrState *create_thread_test_state(void) {
    SZrState *state = ZrTests_State_Create(ZR_NULL);

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return state;
    }

    ZrParser_ToGlobalState_Register(state);
    if (!ZrCore_TaskRuntime_RegisterBuiltins(state->global) || !ZrVmThread_Register(state->global)) {
        ZrTests_State_Destroy(state);
        return ZR_NULL;
    }

    return state;
}

static SZrState *create_thread_test_state_with_project_flags(TZrBool supportMultithread, TZrBool autoCoroutine) {
    static const char *kProjectTemplate =
            "{\n"
            "  \"name\": \"thread_runtime_project\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"supportMultithread\": %s,\n"
            "  \"autoCoroutine\": %s\n"
            "}";
    char json[256];
    SZrState *state = create_thread_test_state();
    SZrLibrary_Project *project;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return state;
    }

    snprintf(json,
             sizeof(json),
             kProjectTemplate,
             supportMultithread ? "true" : "false",
             autoCoroutine ? "true" : "false");
    project = ZrLibrary_Project_New(state,
                                    (TZrNativeString)json,
                                    (TZrNativeString)"tests/fixtures/projects/hello_world/hello_world.zrp");
    if (project == ZR_NULL) {
        ZrTests_State_Destroy(state);
        return ZR_NULL;
    }

    state->global->userData = project;
    return state;
}

static void destroy_thread_test_state(SZrState *state) {
    if (state == ZR_NULL) {
        return;
    }

    if (state->global != ZR_NULL && state->global->userData != ZR_NULL) {
        ZrLibrary_Project_Free(state, (SZrLibrary_Project *)state->global->userData);
        state->global->userData = ZR_NULL;
    }

    ZrTests_State_Destroy(state);
}

static SZrFunction *compile_thread_source(SZrState *state, const char *source, const char *name) {
    SZrString *sourceName;

    if (state == ZR_NULL || source == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)name, strlen(name));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static const ZrLibTypeDescriptor *find_type_descriptor(const ZrLibModuleDescriptor *descriptor, const char *typeName) {
    TZrSize index;

    if (descriptor == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < descriptor->typeCount; index++) {
        const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[index];
        if (typeDescriptor->name != ZR_NULL && strcmp(typeDescriptor->name, typeName) == 0) {
            return typeDescriptor;
        }
    }

    return ZR_NULL;
}

static const ZrLibMethodDescriptor *find_method_descriptor(const ZrLibTypeDescriptor *descriptor, const char *methodName) {
    TZrSize index;

    if (descriptor == ZR_NULL || methodName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < descriptor->methodCount; index++) {
        const ZrLibMethodDescriptor *methodDescriptor = &descriptor->methods[index];
        if (methodDescriptor->name != ZR_NULL && strcmp(methodDescriptor->name, methodName) == 0) {
            return methodDescriptor;
        }
    }

    return ZR_NULL;
}

static TZrBool generic_parameter_has_constraint(const ZrLibGenericParameterDescriptor *parameter,
                                                const char *constraintName) {
    TZrSize index;

    if (parameter == ZR_NULL || constraintName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < parameter->constraintTypeCount; index++) {
        const char *currentConstraint = parameter->constraintTypeNames[index];
        if (currentConstraint != ZR_NULL && strcmp(currentConstraint, constraintName) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void test_zr_thread_registers_public_shapes_without_legacy_mutex_or_atomic(void) {
    SZrState *state = create_thread_test_state();
    const ZrLibModuleDescriptor *threadDescriptor;

    TEST_ASSERT_NOT_NULL(state);
    threadDescriptor = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.thread");
    TEST_ASSERT_NOT_NULL(threadDescriptor);
    TEST_ASSERT_NOT_NULL(find_type_descriptor(threadDescriptor, "Scheduler"));
    TEST_ASSERT_NOT_NULL(find_type_descriptor(threadDescriptor, "Thread"));
    TEST_ASSERT_NOT_NULL(find_type_descriptor(threadDescriptor, "Channel"));
    TEST_ASSERT_NOT_NULL(find_type_descriptor(threadDescriptor, "Shared"));
    TEST_ASSERT_NOT_NULL(find_type_descriptor(threadDescriptor, "Transfer"));
    TEST_ASSERT_NOT_NULL(find_type_descriptor(threadDescriptor, "WeakShared"));
    TEST_ASSERT_NOT_NULL(find_type_descriptor(threadDescriptor, "Send"));
    TEST_ASSERT_NOT_NULL(find_type_descriptor(threadDescriptor, "Sync"));
    TEST_ASSERT_NOT_NULL(find_type_descriptor(threadDescriptor, "UniqueMutex"));
    TEST_ASSERT_NOT_NULL(find_type_descriptor(threadDescriptor, "SharedMutex"));
    TEST_ASSERT_NOT_NULL(find_type_descriptor(threadDescriptor, "Lock"));
    TEST_ASSERT_NOT_NULL(find_type_descriptor(threadDescriptor, "SharedLock"));
    TEST_ASSERT_NULL(find_type_descriptor(threadDescriptor, "Mutex"));
    TEST_ASSERT_NULL(find_type_descriptor(threadDescriptor, "AtomicBool"));
    TEST_ASSERT_NULL(find_type_descriptor(threadDescriptor, "AtomicInt"));
    TEST_ASSERT_NULL(find_type_descriptor(threadDescriptor, "AtomicUInt"));

    ZrTests_State_Destroy(state);
}

static void test_zr_thread_descriptors_express_send_sync_contracts(void) {
    SZrState *state = create_thread_test_state();
    const ZrLibModuleDescriptor *threadDescriptor;
    const ZrLibTypeDescriptor *channelDescriptor;
    const ZrLibTypeDescriptor *transferDescriptor;
    const ZrLibTypeDescriptor *sharedDescriptor;
    const ZrLibTypeDescriptor *weakSharedDescriptor;
    const ZrLibTypeDescriptor *uniqueMutexDescriptor;
    const ZrLibTypeDescriptor *sharedMutexDescriptor;
    const ZrLibTypeDescriptor *lockDescriptor;
    const ZrLibTypeDescriptor *sharedLockDescriptor;
    const ZrLibTypeDescriptor *threadTypeDescriptor;
    const ZrLibTypeDescriptor *schedulerDescriptor;
    const ZrLibMethodDescriptor *threadStartDescriptor;
    const ZrLibMethodDescriptor *schedulerStartDescriptor;

    TEST_ASSERT_NOT_NULL(state);
    threadDescriptor = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.thread");
    TEST_ASSERT_NOT_NULL(threadDescriptor);

    channelDescriptor = find_type_descriptor(threadDescriptor, "Channel");
    transferDescriptor = find_type_descriptor(threadDescriptor, "Transfer");
    sharedDescriptor = find_type_descriptor(threadDescriptor, "Shared");
    weakSharedDescriptor = find_type_descriptor(threadDescriptor, "WeakShared");
    uniqueMutexDescriptor = find_type_descriptor(threadDescriptor, "UniqueMutex");
    sharedMutexDescriptor = find_type_descriptor(threadDescriptor, "SharedMutex");
    lockDescriptor = find_type_descriptor(threadDescriptor, "Lock");
    sharedLockDescriptor = find_type_descriptor(threadDescriptor, "SharedLock");
    threadTypeDescriptor = find_type_descriptor(threadDescriptor, "Thread");
    schedulerDescriptor = find_type_descriptor(threadDescriptor, "Scheduler");

    TEST_ASSERT_NOT_NULL(channelDescriptor);
    TEST_ASSERT_NOT_NULL(transferDescriptor);
    TEST_ASSERT_NOT_NULL(sharedDescriptor);
    TEST_ASSERT_NOT_NULL(weakSharedDescriptor);
    TEST_ASSERT_NOT_NULL(uniqueMutexDescriptor);
    TEST_ASSERT_NOT_NULL(sharedMutexDescriptor);
    TEST_ASSERT_NOT_NULL(lockDescriptor);
    TEST_ASSERT_NOT_NULL(sharedLockDescriptor);
    TEST_ASSERT_NOT_NULL(threadTypeDescriptor);
    TEST_ASSERT_NOT_NULL(schedulerDescriptor);

    TEST_ASSERT_EQUAL_UINT64(1, channelDescriptor->genericParameterCount);
    TEST_ASSERT_TRUE(generic_parameter_has_constraint(&channelDescriptor->genericParameters[0], "zr.thread.Send"));
    TEST_ASSERT_FALSE(generic_parameter_has_constraint(&channelDescriptor->genericParameters[0], "zr.thread.Sync"));

    TEST_ASSERT_EQUAL_UINT64(1, transferDescriptor->genericParameterCount);
    TEST_ASSERT_TRUE(generic_parameter_has_constraint(&transferDescriptor->genericParameters[0], "zr.thread.Send"));
    TEST_ASSERT_FALSE(generic_parameter_has_constraint(&transferDescriptor->genericParameters[0], "zr.thread.Sync"));

    TEST_ASSERT_EQUAL_UINT64(1, sharedDescriptor->genericParameterCount);
    TEST_ASSERT_TRUE(generic_parameter_has_constraint(&sharedDescriptor->genericParameters[0], "zr.thread.Send"));
    TEST_ASSERT_TRUE(generic_parameter_has_constraint(&sharedDescriptor->genericParameters[0], "zr.thread.Sync"));

    TEST_ASSERT_EQUAL_UINT64(1, weakSharedDescriptor->genericParameterCount);
    TEST_ASSERT_TRUE(generic_parameter_has_constraint(&weakSharedDescriptor->genericParameters[0], "zr.thread.Send"));
    TEST_ASSERT_TRUE(generic_parameter_has_constraint(&weakSharedDescriptor->genericParameters[0], "zr.thread.Sync"));

    TEST_ASSERT_EQUAL_UINT64(1, uniqueMutexDescriptor->genericParameterCount);
    TEST_ASSERT_TRUE(generic_parameter_has_constraint(&uniqueMutexDescriptor->genericParameters[0], "zr.thread.Send"));
    TEST_ASSERT_FALSE(generic_parameter_has_constraint(&uniqueMutexDescriptor->genericParameters[0], "zr.thread.Sync"));

    TEST_ASSERT_EQUAL_UINT64(1, sharedMutexDescriptor->genericParameterCount);
    TEST_ASSERT_TRUE(generic_parameter_has_constraint(&sharedMutexDescriptor->genericParameters[0], "zr.thread.Send"));
    TEST_ASSERT_TRUE(generic_parameter_has_constraint(&sharedMutexDescriptor->genericParameters[0], "zr.thread.Sync"));

    TEST_ASSERT_EQUAL_UINT64(1, lockDescriptor->genericParameterCount);
    TEST_ASSERT_EQUAL_UINT64(0, lockDescriptor->implementsTypeCount);
    TEST_ASSERT_EQUAL_UINT64(1, sharedLockDescriptor->genericParameterCount);
    TEST_ASSERT_EQUAL_UINT64(0, sharedLockDescriptor->implementsTypeCount);

    threadStartDescriptor = find_method_descriptor(threadTypeDescriptor, "start");
    schedulerStartDescriptor = find_method_descriptor(schedulerDescriptor, "start");
    TEST_ASSERT_NOT_NULL(threadStartDescriptor);
    TEST_ASSERT_NOT_NULL(schedulerStartDescriptor);

    TEST_ASSERT_EQUAL_UINT64(1, threadStartDescriptor->parameterCount);
    TEST_ASSERT_EQUAL_UINT64(1, threadStartDescriptor->genericParameterCount);
    TEST_ASSERT_EQUAL_STRING("zr.task.TaskRunner<T>", threadStartDescriptor->parameters[0].typeName);
    TEST_ASSERT_EQUAL_STRING("zr.task.Task<T>", threadStartDescriptor->returnTypeName);
    TEST_ASSERT_TRUE(generic_parameter_has_constraint(&threadStartDescriptor->genericParameters[0], "zr.thread.Send"));

    TEST_ASSERT_EQUAL_UINT64(1, schedulerStartDescriptor->parameterCount);
    TEST_ASSERT_EQUAL_UINT64(1, schedulerStartDescriptor->genericParameterCount);
    TEST_ASSERT_EQUAL_STRING("zr.task.TaskRunner<T>", schedulerStartDescriptor->parameters[0].typeName);
    TEST_ASSERT_EQUAL_STRING("zr.task.Task<T>", schedulerStartDescriptor->returnTypeName);
    TEST_ASSERT_TRUE(generic_parameter_has_constraint(&schedulerStartDescriptor->genericParameters[0], "zr.thread.Send"));

    ZrTests_State_Destroy(state);
}

static void test_spawn_thread_requires_support_multithread(void) {
    static const char *source =
            "var thread = %import(\"zr.thread\");\n"
            "var worker = thread.spawnThread();\n"
            "return 1;\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_FALSE, ZR_TRUE);
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_source(state, source, "thread_spawn_gate_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(ZrTests_Function_Execute(state, function, &result));

    destroy_thread_test_state(state);
}

static void test_thread_start_and_await_execute_runner_result(void) {
    static const char *source =
            "var thread = %import(\"zr.thread\");\n"
            "%async addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "%async run(): int {\n"
            "    var worker = thread.spawnThread();\n"
            "    var task = worker.start(addOne(4));\n"
            "    return %await task;\n"
            "}\n"
            "return %await run().start();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_source(state, source, "thread_start_await_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);

    destroy_thread_test_state(state);
}

static void test_async_runner_creation_still_works_with_thread_import_present(void) {
    static const char *source =
            "var thread = %import(\"zr.thread\");\n"
            "%async addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "%async run(): int {\n"
            "    var task = addOne(4).start();\n"
            "    return %await task;\n"
            "}\n"
            "return %await run().start();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_source(state, source, "thread_import_local_runner_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);

    destroy_thread_test_state(state);
}

static void test_thread_start_with_precomputed_runner_execute_runner_result(void) {
    static const char *source =
            "var thread = %import(\"zr.thread\");\n"
            "%async addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "%async run(): int {\n"
            "    var worker = thread.spawnThread();\n"
            "    var runner = addOne(4);\n"
            "    var task = worker.start(runner);\n"
            "    return %await task;\n"
            "}\n"
            "return %await run().start();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_source(state, source, "thread_start_precomputed_runner_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);

    destroy_thread_test_state(state);
}

static void test_channel_transports_value_back_from_worker_isolate(void) {
    static const char *source =
            "var thread = %import(\"zr.thread\");\n"
            "%async run(): int {\n"
            "    var worker = thread.spawnThread();\n"
            "    var channel = new thread.Channel<int>();\n"
            "    %async sendBack(): int {\n"
            "        channel.send(41);\n"
            "        return 1;\n"
            "    }\n"
            "    var task = worker.start(sendBack());\n"
            "    if (%await task != 1) { return 0; }\n"
            "    return channel.recv();\n"
            "}\n"
            "return %await run().start();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_source(state, source, "thread_channel_transport_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(41, result);

    destroy_thread_test_state(state);
}

static void test_transfer_moves_value_into_worker_isolate_and_invalidates_source(void) {
    static const char *source =
            "var thread = %import(\"zr.thread\");\n"
            "%async run(): int {\n"
            "    var worker = thread.spawnThread();\n"
            "    var transfer = new thread.Transfer<int>(41);\n"
            "    %async consume(): int {\n"
            "        var moved = transfer.take();\n"
            "        if (transfer.isTaken() != true) { return 0; }\n"
            "        return moved + 1;\n"
            "    }\n"
            "    var task = worker.start(consume());\n"
            "    if (%await task != 42) { return 0; }\n"
            "    if (transfer.isTaken() != true) { return 0; }\n"
            "    if (transfer.take() != null) { return 0; }\n"
            "    return 1;\n"
            "}\n"
            "return %await run().start();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_source(state, source, "thread_transfer_move_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    destroy_thread_test_state(state);
}

static void test_transfer_rejects_non_send_thread_handle_payload(void) {
    static const char *source =
            "var thread = %import(\"zr.thread\");\n"
            "var worker = thread.spawnThread();\n"
            "var transfer = new thread.Transfer(worker);\n"
            "return 0;\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_source(state, source, "thread_transfer_non_send_rejected_test.zr");
    TEST_ASSERT_NULL(function);

    destroy_thread_test_state(state);
}

static void test_shared_handle_capture_roundtrips_across_worker_isolate(void) {
    static const char *source =
            "var thread = %import(\"zr.thread\");\n"
            "%async run(): int {\n"
            "    var worker = thread.spawnThread();\n"
            "    var shared = new thread.Shared<int>(41);\n"
            "    %async bump(): int {\n"
            "        var current = shared.load();\n"
            "        shared.store(current + 1);\n"
            "        return current;\n"
            "    }\n"
            "    var task = worker.start(bump());\n"
            "    if (%await task != 41) { return 0; }\n"
            "    return shared.load();\n"
            "}\n"
            "return %await run().start();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_source(state, source, "thread_shared_capture_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);

    destroy_thread_test_state(state);
}

static void test_shared_rejects_non_sync_thread_handle_payload(void) {
    static const char *source =
            "var thread = %import(\"zr.thread\");\n"
            "var worker = thread.spawnThread();\n"
            "var shared = new thread.Shared(worker);\n"
            "return 0;\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_source(state, source, "thread_shared_non_sync_rejected_test.zr");
    TEST_ASSERT_NULL(function);

    destroy_thread_test_state(state);
}

static void test_weak_shared_handle_capture_upgrades_across_worker_isolate(void) {
    static const char *source =
            "var thread = %import(\"zr.thread\");\n"
            "%async run(): int {\n"
            "    var worker = thread.spawnThread();\n"
            "    var shared = new thread.Shared<int>(7);\n"
            "    var weak = shared.downgrade();\n"
            "    %async readWeak(): int {\n"
            "        var upgraded = weak.upgrade();\n"
            "        if (upgraded == null) { return 0; }\n"
            "        return upgraded.load();\n"
            "    }\n"
            "    return %await worker.start(readWeak());\n"
            "}\n"
            "return %await run().start();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_source(state, source, "thread_weak_shared_capture_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);

    destroy_thread_test_state(state);
}

static void test_unique_mutex_lock_guard_updates_value(void) {
    static const char *source =
            "var thread = %import(\"zr.thread\");\n"
            "var mutex = new thread.UniqueMutex<int>(41);\n"
            "var lock = mutex.lock();\n"
            "if (lock.load() != 41) { return 0; }\n"
            "lock.store(42);\n"
            "lock.unlock();\n"
            "var verify = mutex.lock();\n"
            "if (verify.load() != 42) { return 0; }\n"
            "verify.unlock();\n"
            "return 1;\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_source(state, source, "thread_unique_mutex_lock_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    destroy_thread_test_state(state);
}

static void test_shared_mutex_read_and_write_guards_observe_updates(void) {
    static const char *source =
            "var thread = %import(\"zr.thread\");\n"
            "var mutex = new thread.SharedMutex<int>(7);\n"
            "var readOne = mutex.read();\n"
            "if (readOne.load() != 7) { return 0; }\n"
            "readOne.unlock();\n"
            "var write = mutex.write();\n"
            "write.store(12);\n"
            "write.unlock();\n"
            "var readTwo = mutex.read();\n"
            "if (readTwo.load() != 12) { return 0; }\n"
            "readTwo.unlock();\n"
            "return 1;\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_source(state, source, "thread_shared_mutex_read_write_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    destroy_thread_test_state(state);
}

static void test_lock_guard_rejects_transfer_storage(void) {
    static const char *source =
            "var thread = %import(\"zr.thread\");\n"
            "var mutex = new thread.UniqueMutex<int>(1);\n"
            "var lock = mutex.lock();\n"
            "var moved = new thread.Transfer(lock);\n"
            "return 0;\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_source(state, source, "thread_lock_guard_transfer_rejected_test.zr");
    TEST_ASSERT_NULL(function);

    destroy_thread_test_state(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zr_thread_registers_public_shapes_without_legacy_mutex_or_atomic);
    RUN_TEST(test_zr_thread_descriptors_express_send_sync_contracts);
    RUN_TEST(test_spawn_thread_requires_support_multithread);
    RUN_TEST(test_async_runner_creation_still_works_with_thread_import_present);
    RUN_TEST(test_thread_start_with_precomputed_runner_execute_runner_result);
    RUN_TEST(test_thread_start_and_await_execute_runner_result);
    RUN_TEST(test_channel_transports_value_back_from_worker_isolate);
    RUN_TEST(test_transfer_moves_value_into_worker_isolate_and_invalidates_source);
    RUN_TEST(test_transfer_rejects_non_send_thread_handle_payload);
    RUN_TEST(test_shared_handle_capture_roundtrips_across_worker_isolate);
    RUN_TEST(test_shared_rejects_non_sync_thread_handle_payload);
    RUN_TEST(test_weak_shared_handle_capture_upgrades_across_worker_isolate);
    RUN_TEST(test_unique_mutex_lock_guard_updates_value);
    RUN_TEST(test_shared_mutex_read_and_write_guards_observe_updates);
    RUN_TEST(test_lock_guard_rejects_transfer_storage);
    return UNITY_END();
}
