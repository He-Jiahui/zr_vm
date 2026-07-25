#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "test_support.h"
#include "zr_vm_common/zr_contract_conf.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"
#include "zr_vm_lib_thread/module.h"
#include "zr_vm_lib_thread/runtime.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/compiler.h"
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"
#include "../../zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h"

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

static TZrBool thread_test_shutdown_isolated_schedulers_callback(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    TZrBool shutDown;

    if (context == ZR_NULL || context->state == ZR_NULL || context->state->global == ZR_NULL ||
        result == ZR_NULL || !ZrLib_CallContext_CheckArity(context, 0u, 0u)) {
        return ZR_FALSE;
    }
    shutDown = ZrVmThread_Runtime_ShutdownIsolatedSchedulers(context->state->global);
    ZrCore_Value_InitAsInt(context->state, result, shutDown ? 1 : 0);
    return ZR_TRUE;
}

static const ZrLibFunctionDescriptor kThreadTestHostFunctions[] = {
        {
                .name = "shutdownIsolatedSchedulers",
                .minArgumentCount = 0u,
                .maxArgumentCount = 0u,
                .callback = thread_test_shutdown_isolated_schedulers_callback,
                .returnTypeName = "int",
                .documentation = "Test-only host shutdown hook.",
        },
};

static const ZrLibModuleDescriptor kThreadTestHostModule = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "thread.test.host",
        .functions = kThreadTestHostFunctions,
        .functionCount = ZR_ARRAY_COUNT(kThreadTestHostFunctions),
        .documentation = "Thread runtime test host hooks.",
        .moduleVersion = "1.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
};

static TZrBool register_thread_test_host_module(SZrState *state) {
    return state != ZR_NULL && state->global != ZR_NULL &&
           ZrLibrary_NativeRegistry_RegisterModule(state->global, &kThreadTestHostModule);
}

static SZrAstNode *parse_thread_source_ast(SZrState *state, const char *source, const char *name) {
    SZrString *sourceName;

    if (state == ZR_NULL || source == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)name, strlen(name));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Parse(state, source, strlen(source), sourceName);
}

static SZrCompilerState *create_thread_test_compiler_state(SZrState *state) {
    SZrCompilerState *cs;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    cs = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));
    if (cs == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_CompilerState_Init(cs, state);
    return cs;
}

static void destroy_thread_test_compiler_state(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    if (cs->topLevelFunction != ZR_NULL && cs->topLevelFunction != cs->currentFunction) {
        ZrCore_Function_Free(cs->state, cs->topLevelFunction);
        cs->topLevelFunction = ZR_NULL;
    }

    if (cs->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(cs->state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
    }

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

static void init_thread_generic_type(SZrState *state,
                                     SZrInferredType *type,
                                     const char *typeNameText,
                                     SZrInferredType *argumentType) {
    SZrString *typeName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(type);
    TEST_ASSERT_NOT_NULL(typeNameText);
    TEST_ASSERT_NOT_NULL(argumentType);

    typeName = ZrCore_String_Create(state, (TZrNativeString)typeNameText, strlen(typeNameText));
    TEST_ASSERT_NOT_NULL(typeName);

    ZrParser_InferredType_InitFull(state, type, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
    ZrCore_Array_Init(state, &type->elementTypes, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(state, &type->elementTypes, argumentType);
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

static void test_zr_thread_registers_canonical_scheduler_without_legacy_wrappers(void) {
    SZrState *state = create_thread_test_state();
    const ZrLibModuleDescriptor *threadDescriptor;

    TEST_ASSERT_NOT_NULL(state);
    threadDescriptor = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.thread");
    TEST_ASSERT_NOT_NULL(threadDescriptor);
    TEST_ASSERT_NOT_NULL(find_type_descriptor(threadDescriptor, "ThreadScheduler"));
    TEST_ASSERT_NULL(find_type_descriptor(threadDescriptor, "Scheduler"));
    TEST_ASSERT_NULL(find_type_descriptor(threadDescriptor, "Thread"));
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
    const ZrLibTypeDescriptor *threadSchedulerDescriptor;
    const ZrLibMethodDescriptor *scheduleDescriptor;

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
    threadSchedulerDescriptor = find_type_descriptor(threadDescriptor, "ThreadScheduler");

    TEST_ASSERT_NOT_NULL(channelDescriptor);
    TEST_ASSERT_NOT_NULL(transferDescriptor);
    TEST_ASSERT_NOT_NULL(sharedDescriptor);
    TEST_ASSERT_NOT_NULL(weakSharedDescriptor);
    TEST_ASSERT_NOT_NULL(uniqueMutexDescriptor);
    TEST_ASSERT_NOT_NULL(sharedMutexDescriptor);
    TEST_ASSERT_NOT_NULL(lockDescriptor);
    TEST_ASSERT_NOT_NULL(sharedLockDescriptor);
    TEST_ASSERT_NOT_NULL(threadSchedulerDescriptor);
    TEST_ASSERT_NULL(find_type_descriptor(threadDescriptor, "Thread"));
    TEST_ASSERT_NULL(find_type_descriptor(threadDescriptor, "Scheduler"));

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

    scheduleDescriptor = find_method_descriptor(threadSchedulerDescriptor, "schedule");
    TEST_ASSERT_NOT_NULL(scheduleDescriptor);
    TEST_ASSERT_NULL(find_method_descriptor(threadSchedulerDescriptor, "start"));
    TEST_ASSERT_NULL(find_method_descriptor(threadSchedulerDescriptor, "pump"));
    TEST_ASSERT_NULL(find_method_descriptor(threadSchedulerDescriptor, "step"));
    TEST_ASSERT_EQUAL_UINT64(1, scheduleDescriptor->parameterCount);
    TEST_ASSERT_EQUAL_UINT64(1, scheduleDescriptor->genericParameterCount);
    TEST_ASSERT_EQUAL_STRING("zr.task.Job<T>", scheduleDescriptor->parameters[0].typeName);
    TEST_ASSERT_EQUAL_STRING("zr.task.Task<T>", scheduleDescriptor->returnTypeName);
    TEST_ASSERT_TRUE(generic_parameter_has_constraint(&scheduleDescriptor->genericParameters[0], "zr.thread.Send"));

    ZrTests_State_Destroy(state);
}

static void test_zr_thread_descriptor_publishes_canonical_thread_scheduler_contract(void) {
    const ZrLibModuleDescriptor *descriptor = ZrVmThread_GetModuleDescriptor();
    const ZrLibTypeDescriptor *threadScheduler;
    const ZrLibTypeDescriptor *send;
    const ZrLibTypeDescriptor *sync;
    const ZrLibMethodDescriptor *schedule;

    TEST_ASSERT_NOT_NULL(descriptor);
    threadScheduler = find_type_descriptor(descriptor, "ThreadScheduler");
    send = find_type_descriptor(descriptor, "Send");
    sync = find_type_descriptor(descriptor, "Sync");
    TEST_ASSERT_NOT_NULL(threadScheduler);
    TEST_ASSERT_NOT_NULL(send);
    TEST_ASSERT_NOT_NULL(sync);
    TEST_ASSERT_TRUE((threadScheduler->protocolMask & ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_TASK_SCHEDULER)) != 0U);
    TEST_ASSERT_TRUE((threadScheduler->protocolMask & ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_THREAD_SCHEDULER)) != 0U);
    TEST_ASSERT_TRUE((send->protocolMask & ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_THREAD_SEND)) != 0U);
    TEST_ASSERT_TRUE((sync->protocolMask & ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_THREAD_SYNC)) != 0U);

    schedule = find_method_descriptor(threadScheduler, "schedule");
    TEST_ASSERT_NOT_NULL(schedule);
    TEST_ASSERT_EQUAL_UINT64(1, schedule->parameterCount);
    TEST_ASSERT_EQUAL_STRING("zr.task.Job<T>", schedule->parameters[0].typeName);
    TEST_ASSERT_EQUAL_STRING("zr.task.Task<T>", schedule->returnTypeName);
    TEST_ASSERT_EQUAL_UINT32(ZR_MEMBER_CONTRACT_ROLE_TASK_SCHEDULER_SCHEDULE,
                             schedule->contractRole);
}

static void test_thread_markers_reject_isolate_alias_ownership_qualifiers(void) {
    SZrState *state = create_thread_test_state();
    SZrCompilerState *cs = create_thread_test_compiler_state(state);
    SZrString *sendName;
    SZrString *syncName;
    SZrInferredType primitiveType;
    SZrInferredType uniqueType;
    SZrInferredType borrowedType;
    SZrInferredType sharedType;
    SZrInferredType weakType;
    SZrInferredType loanedType;
    SZrInferredType arrayType;
    SZrInferredType elementType;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(cs);

    sendName = ZrCore_String_Create(state, (TZrNativeString)"zr.thread.Send", strlen("zr.thread.Send"));
    syncName = ZrCore_String_Create(state, (TZrNativeString)"zr.thread.Sync", strlen("zr.thread.Sync"));
    TEST_ASSERT_NOT_NULL(sendName);
    TEST_ASSERT_NOT_NULL(syncName);

    ZrParser_InferredType_Init(state, &primitiveType, ZR_VALUE_TYPE_INT64);
    ZrParser_InferredType_Init(state, &uniqueType, ZR_VALUE_TYPE_INT64);
    ZrParser_InferredType_Init(state, &borrowedType, ZR_VALUE_TYPE_INT64);
    ZrParser_InferredType_Init(state, &sharedType, ZR_VALUE_TYPE_INT64);
    ZrParser_InferredType_Init(state, &weakType, ZR_VALUE_TYPE_INT64);
    ZrParser_InferredType_Init(state, &loanedType, ZR_VALUE_TYPE_INT64);
    ZrParser_InferredType_Init(state, &arrayType, ZR_VALUE_TYPE_ARRAY);
    ZrParser_InferredType_Init(state, &elementType, ZR_VALUE_TYPE_INT64);

    uniqueType.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_UNIQUE;
    borrowedType.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_BORROWED;
    sharedType.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_SHARED;
    weakType.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_WEAK;
    loanedType.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_LOANED;
    elementType.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_BORROWED;

    ZrCore_Array_Init(state, &arrayType.elementTypes, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(state, &arrayType.elementTypes, &elementType);

    TEST_ASSERT_TRUE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &primitiveType, sendName));
    TEST_ASSERT_TRUE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &primitiveType, syncName));
    TEST_ASSERT_TRUE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &uniqueType, sendName));
    TEST_ASSERT_TRUE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &uniqueType, syncName));

    TEST_ASSERT_FALSE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &borrowedType, sendName));
    TEST_ASSERT_FALSE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &borrowedType, syncName));
    TEST_ASSERT_FALSE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &sharedType, sendName));
    TEST_ASSERT_FALSE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &sharedType, syncName));
    TEST_ASSERT_FALSE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &weakType, sendName));
    TEST_ASSERT_FALSE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &weakType, syncName));
    TEST_ASSERT_FALSE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &loanedType, sendName));
    TEST_ASSERT_FALSE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &loanedType, syncName));
    TEST_ASSERT_FALSE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &arrayType, sendName));
    TEST_ASSERT_FALSE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &arrayType, syncName));

    ZrParser_InferredType_Free(state, &arrayType);
    ZrParser_InferredType_Free(state, &loanedType);
    ZrParser_InferredType_Free(state, &weakType);
    ZrParser_InferredType_Free(state, &sharedType);
    ZrParser_InferredType_Free(state, &borrowedType);
    ZrParser_InferredType_Free(state, &uniqueType);
    ZrParser_InferredType_Free(state, &primitiveType);
    destroy_thread_test_compiler_state(cs);
    destroy_thread_test_state(state);
}

static void test_thread_markers_reject_nested_isolate_alias_generic_arguments(void) {
    SZrState *state = create_thread_test_state();
    SZrCompilerState *cs = create_thread_test_compiler_state(state);
    SZrString *threadModuleName;
    SZrString *sendName;
    SZrString *syncName;
    SZrInferredType primitiveElement;
    SZrInferredType borrowedElement;
    SZrInferredType loanedElement;
    SZrInferredType transferPrimitiveType;
    SZrInferredType sharedPrimitiveType;
    SZrInferredType transferBorrowedType;
    SZrInferredType sharedLoanedType;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(cs);

    threadModuleName = ZrCore_String_Create(state, (TZrNativeString)"zr.thread", strlen("zr.thread"));
    sendName = ZrCore_String_Create(state, (TZrNativeString)"zr.thread.Send", strlen("zr.thread.Send"));
    syncName = ZrCore_String_Create(state, (TZrNativeString)"zr.thread.Sync", strlen("zr.thread.Sync"));
    TEST_ASSERT_NOT_NULL(threadModuleName);
    TEST_ASSERT_NOT_NULL(sendName);
    TEST_ASSERT_NOT_NULL(syncName);
    TEST_ASSERT_TRUE(ensure_native_module_compile_info(cs, threadModuleName));

    ZrParser_InferredType_Init(state, &primitiveElement, ZR_VALUE_TYPE_INT64);
    ZrParser_InferredType_Init(state, &borrowedElement, ZR_VALUE_TYPE_INT64);
    ZrParser_InferredType_Init(state, &loanedElement, ZR_VALUE_TYPE_INT64);
    borrowedElement.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_BORROWED;
    loanedElement.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_LOANED;

    init_thread_generic_type(state, &transferPrimitiveType, "Transfer<int>", &primitiveElement);
    init_thread_generic_type(state, &sharedPrimitiveType, "Shared<int>", &primitiveElement);
    init_thread_generic_type(state, &transferBorrowedType, "Transfer<int>", &borrowedElement);
    init_thread_generic_type(state, &sharedLoanedType, "Shared<int>", &loanedElement);

    TEST_ASSERT_TRUE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &transferPrimitiveType, sendName));
    TEST_ASSERT_TRUE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &sharedPrimitiveType, sendName));
    TEST_ASSERT_TRUE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &sharedPrimitiveType, syncName));

    TEST_ASSERT_FALSE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &transferBorrowedType, sendName));
    TEST_ASSERT_FALSE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &sharedLoanedType, sendName));
    TEST_ASSERT_FALSE(ZrParser_InferredType_SatisfiesNamedConstraint(cs, &sharedLoanedType, syncName));

    ZrParser_InferredType_Free(state, &sharedLoanedType);
    ZrParser_InferredType_Free(state, &transferBorrowedType);
    ZrParser_InferredType_Free(state, &sharedPrimitiveType);
    ZrParser_InferredType_Free(state, &transferPrimitiveType);
    ZrParser_InferredType_Free(state, &loanedElement);
    ZrParser_InferredType_Free(state, &borrowedElement);
    ZrParser_InferredType_Free(state, &primitiveElement);
    destroy_thread_test_compiler_state(cs);
    destroy_thread_test_state(state);
}

static void test_lock_guard_is_rejected_after_await_boundary(void) {
    static const char *source =
            "var thread = %import(\"zr.thread\");\n"
            "async fn pause(): Task<int> {\n"
            "    return 1;\n"
            "}\n"
            "async fn invalid(): Task<int> {\n"
            "    var mutex = new thread.UniqueMutex<int>(1);\n"
            "    var lock = mutex.lock();\n"
            "    var task = pause();\n"
            "    await task;\n"
            "    return lock.load();\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_thread_test_state();
    SZrCompilerState *cs = create_thread_test_compiler_state(state);
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(cs);

    ast = parse_thread_source_ast(state, source, "thread_lock_guard_after_await_effect_test.zr");
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE(compiler_validate_task_effects(cs, ast));
    TEST_ASSERT_TRUE(cs->hasError);
    TEST_ASSERT_NOT_NULL(cs->errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Affine guard"));
    TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "await boundary"));

    ZrParser_Ast_Free(state, ast);
    destroy_thread_test_compiler_state(cs);
    destroy_thread_test_state(state);
}

static void test_thread_scheduler_requires_support_multithread(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var thread = %import(\"zr.thread\");\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var job = init task.Job<int>(() => { return 1; });\n"
            "return scheduler.schedule<int>(job).result();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_FALSE, ZR_TRUE);
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_source(state, source, "thread_scheduler_gate_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(ZrTests_Function_Execute(state, function, &result));

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

static void test_thread_worker_state_attaches_to_caller_gc_domain(void) {
    SZrState *caller = create_thread_test_state();
    SZrState *worker;
    SZrGcDomainIdentity callerDomain;
    SZrGcDomainIdentity workerDomain;
    SZrGcDomainMutatorSnapshot beforeLeave;
    SZrGcDomainMutatorSnapshot afterLeave;

    TEST_ASSERT_NOT_NULL(caller);
    worker = ZrCore_State_New(caller->global);
    TEST_ASSERT_NOT_NULL(worker);

    callerDomain = ZrCore_GcDomain_GetIdentity(caller);
    workerDomain = ZrCore_GcDomain_GetIdentity(worker);
    TEST_ASSERT_TRUE(ZrCore_GcDomain_IdentityEquals(callerDomain, workerDomain));
    TEST_ASSERT_TRUE(ZrCore_State_MutatorLaunch(worker));
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, ZrCore_GcDomain_GetMutatorId(worker));

    ZrCore_GcDomain_GetMutatorSnapshot(caller, &beforeLeave);
    TEST_ASSERT_EQUAL_UINT32(2u, beforeLeave.registeredMutatorCount);
    TEST_ASSERT_EQUAL_UINT32(1u, beforeLeave.runningMutatorCount);

    ZrCore_State_MutatorExit(worker);
    ZrCore_GcDomain_GetMutatorSnapshot(caller, &afterLeave);
    TEST_ASSERT_EQUAL_UINT32(2u, afterLeave.registeredMutatorCount);
    TEST_ASSERT_EQUAL_UINT32(0u, afterLeave.runningMutatorCount);

    ZrCore_State_Free(caller->global, worker);
    destroy_thread_test_state(caller);
}

static void test_thread_scheduler_constructs_with_worker_count_and_consumes_job(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var thread = %import(\"zr.thread\");\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var job = init task.Job<int>(() => { return 7; });\n"
            "var completion = scheduler.schedule<int>(job);\n"
            "return completion.result();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_source(state, source, "thread_scheduler_job_contract_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);

    destroy_thread_test_state(state);
}

static void test_thread_scheduler_isolated_domain_completes_caller_task(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var thread = %import(\"zr.thread\");\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var job = init task.Job<int>(() => { return 29; });\n"
            "var completion = scheduler.schedule<int>(job);\n"
            "return completion.result();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    SZrGcDomainIdentity callerDomain;
    SZrGcDomainIdentity workerDomain;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    callerDomain = ZrCore_GcDomain_GetIdentity(state);
    TEST_ASSERT_TRUE(ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
            state->global,
            ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN));
    function = compile_thread_source(state, source, "thread_scheduler_isolated_domain_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(29, result);
    TEST_ASSERT_TRUE(ZrVmThread_Runtime_GetLastSchedulerWorkerDomain(
            state->global,
            &workerDomain));
    TEST_ASSERT_FALSE(ZrCore_GcDomain_IdentityEquals(callerDomain, workerDomain));

    destroy_thread_test_state(state);
}

static void test_thread_scheduler_isolated_domain_transfers_scalar_capture(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var thread = %import(\"zr.thread\");\n"
            "var captured = 17;\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var job = init task.Job<int>(() => { return captured + 1; });\n"
            "var completion = scheduler.schedule<int>(job);\n"
            "return completion.result();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
            state->global,
            ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN));
    function = compile_thread_source(state, source, "thread_scheduler_isolated_scalar_capture.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(18, result);

    destroy_thread_test_state(state);
}

static void test_thread_scheduler_isolated_domain_clones_string_capture(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var thread = %import(\"zr.thread\");\n"
            "var captured = \"isolated clone\";\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var job = init task.Job<int>(() => {\n"
            "    if (captured == \"isolated clone\") { return 37; }\n"
            "    return 0;\n"
            "});\n"
            "var completion = scheduler.schedule<int>(job);\n"
            "return completion.result();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
            state->global,
            ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN));
    function = compile_thread_source(state, source, "thread_scheduler_isolated_string_clone.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(37, result);

    destroy_thread_test_state(state);
}

static void test_thread_scheduler_isolated_domain_clones_string_result(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var thread = %import(\"zr.thread\");\n"
            "var captured = \"isolated result\";\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var job = init task.Job<string>(() => { return captured; });\n"
            "var completion = scheduler.schedule<string>(job);\n"
            "var result = completion.result();\n"
            "if (result == \"isolated result\") { return 43; }\n"
            "return 0;\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
            state->global,
            ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN));
    function = compile_thread_source(state, source, "thread_scheduler_isolated_string_result.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(43, result);

    destroy_thread_test_state(state);
}

static void test_thread_scheduler_isolated_domain_settles_multiple_requests(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var thread = %import(\"zr.thread\");\n"
            "var firstValue = 6;\n"
            "var secondValue = 8;\n"
            "var scheduler = new thread.ThreadScheduler(2);\n"
            "var firstJob = init task.Job<int>(() => { return firstValue; });\n"
            "var secondJob = init task.Job<int>(() => { return secondValue; });\n"
            "var first = scheduler.schedule<int>(firstJob);\n"
            "var second = scheduler.schedule<int>(secondJob);\n"
            "return first.result() * 10 + second.result();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
            state->global,
            ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN));
    function = compile_thread_source(state, source, "thread_scheduler_isolated_multiple_requests.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(68, result);

    destroy_thread_test_state(state);
}

static void test_thread_scheduler_isolated_domain_drains_shared_provider_queue(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var thread = %import(\"zr.thread\");\n"
            "var firstValue = 1;\n"
            "var secondValue = 2;\n"
            "var thirdValue = 3;\n"
            "var fourthValue = 4;\n"
            "var scheduler = new thread.ThreadScheduler(2);\n"
            "var firstJob = init task.Job<int>(() => { return firstValue; });\n"
            "var secondJob = init task.Job<int>(() => { return secondValue; });\n"
            "var thirdJob = init task.Job<int>(() => { return thirdValue; });\n"
            "var fourthJob = init task.Job<int>(() => { return fourthValue; });\n"
            "var first = scheduler.schedule<int>(firstJob);\n"
            "var second = scheduler.schedule<int>(secondJob);\n"
            "var third = scheduler.schedule<int>(thirdJob);\n"
            "var fourth = scheduler.schedule<int>(fourthJob);\n"
            "return first.result() * 1000 + second.result() * 100 + "
            "third.result() * 10 + fourth.result();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
            state->global,
            ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN));
    function = compile_thread_source(
            state, source, "thread_scheduler_isolated_shared_provider_queue.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1234, result);

    destroy_thread_test_state(state);
}

static void test_thread_scheduler_isolated_domain_quota_faults_prepared_task(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var thread = %import(\"zr.thread\");\n"
            "var captured = \"quota must reject this clone\";\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var job = init task.Job<int>(() => {\n"
            "    if (captured == \"quota must reject this clone\") { return 1; }\n"
            "    return 0;\n"
            "});\n"
            "var completion = scheduler.schedule<int>(job);\n"
            "return completion.result();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
            state->global,
            ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN));
    TEST_ASSERT_TRUE(ZrVmThread_Runtime_SetIsolatedTransferQuota(state->global, 1u, 1u, 1u));
    function = compile_thread_source(state, source, "thread_scheduler_isolated_quota_fault.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));

    destroy_thread_test_state(state);
}

static void test_thread_scheduler_isolated_domain_faults_forbidden_resource_capture(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var thread = %import(\"zr.thread\");\n"
            "resource class Counter {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "  pub const fn read(): int { return this.value; }\n"
            "}\n"
            "var captured: Unique<Counter> = own Counter(47);\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var job = init task.Job<int>(() => { return captured.read(); });\n"
            "var completion = scheduler.schedule<int>(job);\n"
            "return completion.result();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
            state->global,
            ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN));
    function = compile_thread_source(
            state, source, "thread_scheduler_isolated_forbidden_resource_capture.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));

    destroy_thread_test_state(state);
}

static void test_thread_scheduler_isolated_domain_shutdown_faults_later_submission(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var thread = %import(\"zr.thread\");\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var job = init task.Job<int>(() => { return 5; });\n"
            "var completion = scheduler.schedule<int>(job);\n"
            "return completion.result();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
            state->global,
            ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN));
    TEST_ASSERT_TRUE(ZrVmThread_Runtime_ShutdownIsolatedSchedulers(state->global));
    function = compile_thread_source(state, source, "thread_scheduler_isolated_shutdown_fault.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));

    destroy_thread_test_state(state);
}

static void test_thread_scheduler_isolated_domain_shutdown_faults_queued_submission(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var thread = %import(\"zr.thread\");\n"
            "var host = %import(\"thread.test.host\");\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var firstJob = init task.Job<int>(() => { return 3; });\n"
            "var secondJob = init task.Job<int>(() => { return 5; });\n"
            "var first = scheduler.schedule<int>(firstJob);\n"
            "var second = scheduler.schedule<int>(secondJob);\n"
            "var shutDown = host.shutdownIsolatedSchedulers();\n"
            "if (shutDown != 1) { return -1; }\n"
            "var firstValue = first.result();\n"
            "return firstValue + second.result();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
            state->global,
            ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN));
    TEST_ASSERT_TRUE(register_thread_test_host_module(state));
    function = compile_thread_source(state, source, "thread_scheduler_isolated_shutdown_queued.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));

    destroy_thread_test_state(state);
}

static void test_thread_scheduler_drains_one_worker_queue_in_submission_order(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var thread = %import(\"zr.thread\");\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var firstJob = init task.Job<int>(() => { return 3; });\n"
            "var secondJob = init task.Job<int>(() => { return 5; });\n"
            "var first = scheduler.schedule<int>(firstJob);\n"
            "var second = scheduler.schedule<int>(secondJob);\n"
            "return first.result() * 10 + second.result();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
            state->global,
            ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN));
    function = compile_thread_source(state, source, "thread_scheduler_single_worker_queue_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(35, result);

    destroy_thread_test_state(state);
}

static void test_thread_scheduler_rejects_non_send_job_result(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var thread = %import(\"zr.thread\");\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var job = init task.Job<thread.ThreadScheduler>(() => { return scheduler; });\n"
            "var completion = scheduler.schedule(job);\n"
            "return 0;\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NULL(compile_thread_source(state, source, "thread_scheduler_non_send_result_test.zr"));

    destroy_thread_test_state(state);
}

static void test_thread_scheduler_rejects_reused_job(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var thread = %import(\"zr.thread\");\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var job = init task.Job<int>(() => { return 7; });\n"
            "var first = scheduler.schedule<int>(job);\n"
            "var second = scheduler.schedule<int>(job);\n"
            "return first.result() + second.result();\n";
    SZrState *state = create_thread_test_state_with_project_flags(ZR_TRUE, ZR_TRUE);

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NULL(compile_thread_source(state, source, "thread_scheduler_reused_job_test.zr"));

    destroy_thread_test_state(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zr_thread_registers_canonical_scheduler_without_legacy_wrappers);
    RUN_TEST(test_zr_thread_descriptors_express_send_sync_contracts);
    RUN_TEST(test_zr_thread_descriptor_publishes_canonical_thread_scheduler_contract);
    RUN_TEST(test_thread_markers_reject_isolate_alias_ownership_qualifiers);
    RUN_TEST(test_thread_markers_reject_nested_isolate_alias_generic_arguments);
    RUN_TEST(test_lock_guard_is_rejected_after_await_boundary);
    RUN_TEST(test_thread_scheduler_requires_support_multithread);
    RUN_TEST(test_unique_mutex_lock_guard_updates_value);
    RUN_TEST(test_shared_mutex_read_and_write_guards_observe_updates);
    RUN_TEST(test_lock_guard_rejects_transfer_storage);
    RUN_TEST(test_thread_worker_state_attaches_to_caller_gc_domain);
    RUN_TEST(test_thread_scheduler_constructs_with_worker_count_and_consumes_job);
    RUN_TEST(test_thread_scheduler_isolated_domain_completes_caller_task);
    RUN_TEST(test_thread_scheduler_isolated_domain_transfers_scalar_capture);
    RUN_TEST(test_thread_scheduler_isolated_domain_clones_string_capture);
    RUN_TEST(test_thread_scheduler_isolated_domain_clones_string_result);
    RUN_TEST(test_thread_scheduler_isolated_domain_settles_multiple_requests);
    RUN_TEST(test_thread_scheduler_isolated_domain_drains_shared_provider_queue);
    RUN_TEST(test_thread_scheduler_isolated_domain_quota_faults_prepared_task);
    RUN_TEST(test_thread_scheduler_isolated_domain_faults_forbidden_resource_capture);
    RUN_TEST(test_thread_scheduler_isolated_domain_shutdown_faults_later_submission);
    RUN_TEST(test_thread_scheduler_isolated_domain_shutdown_faults_queued_submission);
    RUN_TEST(test_thread_scheduler_drains_one_worker_queue_in_submission_order);
    RUN_TEST(test_thread_scheduler_rejects_non_send_job_result);
    RUN_TEST(test_thread_scheduler_rejects_reused_job);
    return UNITY_END();
}
