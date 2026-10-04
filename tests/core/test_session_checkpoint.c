#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/session_checkpoint.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_common/zr_aot_abi.h"

/* The array append entry point is exported by object_super_array.c but is
 * intentionally absent from the compact object.h surface used by this test
 * fixture.  Keep the declaration local to the fixture while exercising the
 * production API itself. */
ZR_CORE_API TZrBool ZrCore_Object_SuperArrayAddInt(struct SZrState *state,
                                                   SZrTypeValue *receiver,
                                                   const SZrTypeValue *value,
                                                   SZrTypeValue *result);

static SZrGlobalState *g_checkpointTestGlobal;
static SZrSessionCheckpoint *g_checkpointTestCheckpoint;

typedef struct ZrCheckpointFailAllocatorContext {
    FZrAllocator upstream;
    TZrPtr upstreamUserData;
    TZrInt64 failureType;
    TZrUInt32 failuresRemaining;
    TZrUInt32 failuresTriggered;
} ZrCheckpointFailAllocatorContext;

static TZrPtr checkpoint_fail_allocator(TZrPtr userData,
                                        TZrPtr pointer,
                                        TZrSize originalSize,
                                        TZrSize newSize,
                                        TZrInt64 flag) {
    ZrCheckpointFailAllocatorContext *context = (ZrCheckpointFailAllocatorContext *)userData;
    if (pointer == ZR_NULL && newSize > 0u && flag == context->failureType &&
        context->failuresRemaining > 0u) {
        context->failuresRemaining--;
        context->failuresTriggered++;
        return ZR_NULL;
    }
    return context->upstream(context->upstreamUserData, pointer, originalSize, newSize, flag);
}

void setUp(void) {
    g_checkpointTestGlobal = ZR_NULL;
    g_checkpointTestCheckpoint = ZR_NULL;
}

/* Unity 断言失败也会进入 teardown，避免泄漏快照根及其保留的对象图。 */
void tearDown(void) {
    if (g_checkpointTestCheckpoint != ZR_NULL && g_checkpointTestGlobal != ZR_NULL) {
        ZrCore_SessionCheckpoint_Free(g_checkpointTestGlobal->mainThreadState,
                                      g_checkpointTestCheckpoint);
        g_checkpointTestCheckpoint = ZR_NULL;
    }
    if (g_checkpointTestGlobal != ZR_NULL) {
        ZrCore_GlobalState_Free(g_checkpointTestGlobal);
        g_checkpointTestGlobal = ZR_NULL;
    }
}

/* checkpoint API 要求空闲的主线程状态，并依赖已初始化的模块注册表。 */
static void checkpoint_test_assert_remembered_registry_unchanged(
        SZrGarbageCollector *collector,
        SZrRawObject **expectedObjects,
        TZrSize expectedCount,
        TZrSize expectedCapacity) {
    TZrSize index;

    TEST_ASSERT_NOT_NULL(collector);
    TEST_ASSERT_EQUAL_PTR(expectedObjects, collector->rememberedObjects);
    TEST_ASSERT_EQUAL_UINT64(expectedCount, collector->rememberedObjectCount);
    TEST_ASSERT_EQUAL_UINT64(expectedCapacity, collector->rememberedObjectCapacity);
    for (index = 0u; index < expectedCount; index++) {
        TEST_ASSERT_EQUAL_PTR(expectedObjects[index], collector->rememberedObjects[index]);
    }
}

static SZrState *checkpoint_test_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(
            ZrTests_Runtime_Allocator_Default, ZR_NULL, 0x43504f494e54u, &callbacks);

    TEST_ASSERT_NOT_NULL(global);
    g_checkpointTestGlobal = global;
    TEST_ASSERT_NOT_NULL(global->mainThreadState);
    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    return global->mainThreadState;
}

static SZrObjectModule *checkpoint_test_lookup_module(SZrState *state,
                                                       const SZrTypeValue *moduleKey) {
    SZrObject *registry = ZR_CAST_OBJECT(state, state->global->loadedModulesRegistry.value.object);
    const SZrTypeValue *storedModule = ZrCore_Object_GetValue(state, registry, moduleKey);
    return storedModule == ZR_NULL
            ? ZR_NULL
            : (SZrObjectModule *)ZR_CAST_OBJECT(state, storedModule->value.object);
}

static SZrObject *checkpoint_test_lookup_registry_object(SZrState *state,
                                                         const SZrTypeValue *objectKey) {
    SZrObject *registry = ZR_CAST_OBJECT(state, state->global->loadedModulesRegistry.value.object);
    const SZrTypeValue *storedObject = ZrCore_Object_GetValue(state, registry, objectKey);
    return storedObject == ZR_NULL ? ZR_NULL : ZR_CAST_OBJECT(state, storedObject->value.object);
}

static SZrFunction *checkpoint_test_lookup_registry_function(SZrState *state,
                                                              const SZrTypeValue *functionKey) {
    SZrObject *registry = ZR_CAST_OBJECT(state, state->global->loadedModulesRegistry.value.object);
    const SZrTypeValue *storedFunction = ZrCore_Object_GetValue(state, registry, functionKey);
    return storedFunction == ZR_NULL ? ZR_NULL : ZR_CAST_FUNCTION(state, storedFunction->value.object);
}

static SZrFunction *checkpoint_test_lookup_function(SZrState *state,
                                                     const SZrTypeValue *moduleKey,
                                                     const SZrTypeValue *functionKey) {
    SZrObjectModule *module = checkpoint_test_lookup_module(state, moduleKey);
    const SZrTypeValue *storedFunction;
    if (module == ZR_NULL) {
        return ZR_NULL;
    }
    storedFunction = ZrCore_Object_GetValue(
            state, ZR_CAST_OBJECT(state, ZR_CAST_RAW_OBJECT_AS_SUPER(module)), functionKey);
    return storedFunction == ZR_NULL ? ZR_NULL : ZR_CAST_FUNCTION(state, storedFunction->value.object);
}

/* Build a real array through the supported object APIs.  The generic member
 * write creates a standalone GC pair; the production append path then takes a
 * reserved pool slot for the next dense integer index. */
static SZrObject *checkpoint_test_make_mixed_array(SZrState *state,
                                                    SZrObject *registry,
                                                    TZrInt64 registryKeyValue,
                                                    TZrInt64 standaloneKeyValue) {
    SZrObject *receiverObject = ZrCore_Object_New(state, ZR_NULL);
    SZrObject *items = ZrCore_Object_NewCustomized(
            state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    SZrTypeValue registryKey;
    SZrTypeValue itemsValue;
    SZrTypeValue standaloneKey;
    SZrTypeValue standaloneValue;
    SZrTypeValue receiver;
    SZrTypeValue appendValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(receiverObject);
    TEST_ASSERT_NOT_NULL(items);
    receiverObject->prototype = state->global->basicTypeObjectPrototype[ZR_VALUE_TYPE_ARRAY];
    ZrCore_Object_Init(state, receiverObject);
    ZrCore_Object_Init(state, items);
    receiverObject->cachedHiddenItemsObject = items;
    ZrCore_Value_InitAsInt(state, &registryKey, registryKeyValue);
    ZrCore_Value_InitAsRawObject(state, &itemsValue, ZR_CAST_RAW_OBJECT_AS_SUPER(items));
    ZrCore_Object_SetValue(state, registry, &registryKey, &itemsValue);
    items = checkpoint_test_lookup_registry_object(state, &registryKey);
    TEST_ASSERT_NOT_NULL(items);

    ZrCore_Value_InitAsInt(state, &standaloneKey, standaloneKeyValue);
    ZrCore_Value_InitAsInt(state, &standaloneValue, 31);
    ZrCore_Object_SetValue(state, items, &standaloneKey, &standaloneValue);
    items = checkpoint_test_lookup_registry_object(state, &registryKey);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQUAL_UINT64(1u, items->nodeMap.elementCount);
    TEST_ASSERT_EQUAL_UINT64(0u, items->nodeMap.pairPoolUsed);

    ZrCore_Value_InitAsRawObject(state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiver.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsInt(state, &appendValue, 42);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayAddInt(
            state, &receiver, &appendValue, &result));
    items = checkpoint_test_lookup_registry_object(state, &registryKey);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQUAL_UINT64(2u, items->nodeMap.elementCount);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, items->nodeMap.pairPoolUsed);
    return items;
}

static void test_checkpoint_roundtrips_ordinary_object_set_value_map(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    SZrObject *object = ZrCore_Object_New(state, ZR_NULL);
    SZrTypeValue objectKey;
    SZrTypeValue objectValue;
    SZrTypeValue memberKey;
    SZrTypeValue memberValue;
    SZrTypeValue replacement;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    const SZrTypeValue *resolved;

    TEST_ASSERT_NOT_NULL(registry);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);
    ZrCore_Value_InitAsInt(state, &objectKey, 6201);
    ZrCore_Value_InitAsRawObject(state, &objectValue, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    ZrCore_Object_SetValue(state, registry, &objectKey, &objectValue);
    object = checkpoint_test_lookup_registry_object(state, &objectKey);
    TEST_ASSERT_NOT_NULL(object);

    ZrCore_Value_InitAsInt(state, &memberKey, 6202);
    ZrCore_Value_InitAsInt(state, &memberValue, 11);
    ZrCore_Object_SetValue(state, object, &memberKey, &memberValue);
    object = checkpoint_test_lookup_registry_object(state, &objectKey);
    TEST_ASSERT_NOT_NULL(object);
    /* HashSet_Add stores this ordinary member in a standalone pair; the
     * checkpoint restore normalizes it into its prepared reserved pool. */
    TEST_ASSERT_EQUAL_UINT64(0u, object->nodeMap.pairPoolUsed);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    g_checkpointTestCheckpoint = checkpoint;

    ZrCore_Value_InitAsInt(state, &replacement, 99);
    ZrCore_Object_SetValue(state, object, &memberKey, &replacement);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    object = checkpoint_test_lookup_registry_object(state, &objectKey);
    TEST_ASSERT_NOT_NULL(object);
    resolved = ZrCore_Object_GetValue(state, object, &memberKey);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_INT64(11, resolved->value.nativeObject.nativeInt64);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, object->nodeMap.pairPoolUsed);
}

static void test_checkpoint_roundtrips_module_pub_and_pro_exports(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    SZrObjectModule *module = ZR_NULL;
    SZrString *pubName;
    SZrString *proName;
    SZrTypeValue moduleKey;
    SZrTypeValue moduleValue;
    SZrTypeValue pubValue;
    SZrTypeValue proValue;
    SZrTypeValue replacement;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    const SZrTypeValue *resolved;

    TEST_ASSERT_NOT_NULL(registry);
    module = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(module);
    ZrCore_Value_InitAsInt(state, &moduleKey, 6211);
    ZrCore_Value_InitAsRawObject(state, &moduleValue, ZR_CAST_RAW_OBJECT_AS_SUPER(module));
    ZrCore_Object_SetValue(state, registry, &moduleKey, &moduleValue);
    module = checkpoint_test_lookup_module(state, &moduleKey);
    TEST_ASSERT_NOT_NULL(module);
    pubName = ZrCore_String_CreateFromNative(state, (TZrNativeString)"r3-pub");
    proName = ZrCore_String_CreateFromNative(state, (TZrNativeString)"r3-pro");
    TEST_ASSERT_NOT_NULL(pubName);
    TEST_ASSERT_NOT_NULL(proName);
    ZrCore_Value_InitAsInt(state, &pubValue, 17);
    ZrCore_Value_InitAsInt(state, &proValue, 23);
    ZrCore_Module_AddPubExport(state, module, pubName, &pubValue);
    module = checkpoint_test_lookup_module(state, &moduleKey);
    ZrCore_Module_AddProExport(state, module, proName, &proValue);
    module = checkpoint_test_lookup_module(state, &moduleKey);
    TEST_ASSERT_EQUAL_UINT64(0u, module->super.nodeMap.pairPoolUsed);
    TEST_ASSERT_EQUAL_UINT64(0u, module->proNodeMap.pairPoolUsed);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    g_checkpointTestCheckpoint = checkpoint;

    ZrCore_Value_InitAsInt(state, &replacement, 71);
    ZrCore_Module_AddPubExport(state, module, pubName, &replacement);
    ZrCore_Value_InitAsInt(state, &replacement, 73);
    ZrCore_Module_AddProExport(state, module, proName, &replacement);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    module = checkpoint_test_lookup_module(state, &moduleKey);
    TEST_ASSERT_NOT_NULL(module);
    resolved = ZrCore_Module_GetPubExport(state, module, pubName);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_INT64(17, resolved->value.nativeObject.nativeInt64);
    resolved = ZrCore_Module_GetProExport(state, module, proName);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_INT64(23, resolved->value.nativeObject.nativeInt64);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, module->super.nodeMap.pairPoolUsed);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, module->proNodeMap.pairPoolUsed);
}

static void test_checkpoint_roundtrips_mixed_standalone_and_reserved_map(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    SZrObject *items;
    SZrTypeValue itemsKey;
    SZrTypeValue standaloneKey;
    SZrTypeValue replacement;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    const SZrTypeValue *resolved;

    TEST_ASSERT_NOT_NULL(registry);
    ZrCore_Value_InitAsInt(state, &itemsKey, 6221);
    items = checkpoint_test_make_mixed_array(state, registry, 6221, 6222);
    ZrCore_Value_InitAsInt(state, &standaloneKey, 6222);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    g_checkpointTestCheckpoint = checkpoint;

    ZrCore_Value_InitAsInt(state, &replacement, 91);
    ZrCore_Object_SetValue(state, items, &standaloneKey, &replacement);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    items = checkpoint_test_lookup_registry_object(state, &itemsKey);
    TEST_ASSERT_NOT_NULL(items);
    resolved = ZrCore_Object_GetValue(state, items, &standaloneKey);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_INT64(31, resolved->value.nativeObject.nativeInt64);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, items->nodeMap.pairPoolUsed);
}

static void test_checkpoint_roundtrips_removed_reserved_pool_slot(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    SZrObject *items;
    SZrTypeValue itemsKey;
    SZrTypeValue standaloneKey;
    SZrTypeValue removedKey;
    SZrTypeValue replacement;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    const SZrTypeValue *resolved;

    TEST_ASSERT_NOT_NULL(registry);
    ZrCore_Value_InitAsInt(state, &itemsKey, 6231);
    items = checkpoint_test_make_mixed_array(state, registry, 6231, 6232);
    ZrCore_Value_InitAsInt(state, &standaloneKey, 6232);
    ZrCore_Value_InitAsInt(state, &removedKey, 1);
    ZrCore_HashSet_Remove(state, &items->nodeMap, &removedKey);
    TEST_ASSERT_EQUAL_UINT64(1u, items->nodeMap.elementCount);
    TEST_ASSERT_EQUAL_UINT64(1u, items->nodeMap.pairPoolUsed);
    resolved = ZrCore_Object_GetValue(state, items, &standaloneKey);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_INT64(31, resolved->value.nativeObject.nativeInt64);
    TEST_ASSERT_NULL(ZrCore_Object_GetValue(state, items, &removedKey));
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    g_checkpointTestCheckpoint = checkpoint;

    ZrCore_Value_InitAsInt(state, &replacement, 97);
    ZrCore_Object_SetValue(state, items, &standaloneKey, &replacement);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    items = checkpoint_test_lookup_registry_object(state, &itemsKey);
    TEST_ASSERT_NOT_NULL(items);
    resolved = ZrCore_Object_GetValue(state, items, &standaloneKey);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_INT64(31, resolved->value.nativeObject.nativeInt64);
    TEST_ASSERT_NULL(ZrCore_Object_GetValue(state, items, &removedKey));
    TEST_ASSERT_EQUAL_UINT64(1u, items->nodeMap.elementCount);
    TEST_ASSERT_EQUAL_UINT64(1u, items->nodeMap.pairPoolUsed);
}

static void test_checkpoint_restores_cycles_aliases_and_loaded_module_registry(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    SZrObject *registry;
    SZrObject *retained;
    SZrObject *lazyModule;
    SZrObjectModule *plainModule;
    SZrTypeValue retainedKey;
    SZrTypeValue selfKey;
    SZrTypeValue counterKey;
    SZrTypeValue lazyKey;
    SZrTypeValue plainModuleKey;
    SZrTypeValue retainedValue;
    SZrTypeValue counterValue;
    SZrTypeValue lazyValue;
    SZrTypeValue plainModuleValue;
    const SZrTypeValue *restored;

    ZrCore_Value_InitAsInt(state, &retainedKey, 1001);
    ZrCore_Value_InitAsInt(state, &selfKey, 1002);
    ZrCore_Value_InitAsInt(state, &counterKey, 1003);
    ZrCore_Value_InitAsInt(state, &lazyKey, 1004);
    ZrCore_Value_InitAsInt(state, &plainModuleKey, 1005);
    retained = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(retained);
    ZrCore_Object_Init(state, retained);
    registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    TEST_ASSERT_NOT_NULL(registry);
    ZrCore_Value_InitAsRawObject(state, &retainedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(retained));
    ZrCore_Value_InitAsInt(state, &counterValue, 1);
    ZrCore_Object_SetValue(state, registry, &retainedKey, &retainedValue);
    retained = checkpoint_test_lookup_registry_object(state, &retainedKey);
    TEST_ASSERT_NOT_NULL(retained);
    ZrCore_Value_InitAsRawObject(state, &retainedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(retained));
    ZrCore_Object_SetValue(state, retained, &selfKey, &retainedValue);
    ZrCore_Object_SetValue(state, retained, &counterKey, &counterValue);
    plainModule = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(plainModule);
    ZrCore_Value_InitAsRawObject(state, &plainModuleValue,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(plainModule));
    registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    ZrCore_Object_SetValue(state, registry, &plainModuleKey, &plainModuleValue);

    /* 快照保留对象身份及环/别名，随后 registry 的新增项应在 rollback 后消失。 */
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    g_checkpointTestCheckpoint = checkpoint;
    TEST_ASSERT_NOT_NULL(checkpoint);

    ZrCore_Value_InitAsInt(state, &counterValue, 99);
    retained = checkpoint_test_lookup_registry_object(state, &retainedKey);
    ZrCore_Object_SetValue(state, retained, &counterKey, &counterValue);
    lazyModule = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(lazyModule);
    ZrCore_Object_Init(state, lazyModule);
    ZrCore_Value_InitAsRawObject(state, &lazyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(lazyModule));
    registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    ZrCore_Object_SetValue(state, registry, &lazyKey, &lazyValue);

    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    retained = checkpoint_test_lookup_registry_object(state, &retainedKey);
    registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    restored = ZrCore_Object_GetValue(state, retained, &counterKey);
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT_EQUAL_INT64(1, restored->value.nativeObject.nativeInt64);
    restored = ZrCore_Object_GetValue(state, retained, &selfKey);
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT_EQUAL_PTR(retained, restored->value.object);
    TEST_ASSERT_NULL(ZrCore_Object_GetValue(state, registry, &lazyKey));
    plainModule = checkpoint_test_lookup_module(state, &plainModuleKey);
    TEST_ASSERT_NOT_NULL(plainModule);
    TEST_ASSERT_FALSE(plainModule->hasMetadataRuntime);
    TEST_ASSERT_NULL(plainModule->metadataRuntime.module);
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    plainModule = checkpoint_test_lookup_module(state, &plainModuleKey);
    TEST_ASSERT_NOT_NULL(plainModule);
    TEST_ASSERT_FALSE(plainModule->hasMetadataRuntime);
    TEST_ASSERT_NULL(plainModule->metadataRuntime.module);
}

static void test_checkpoint_restores_module_identity_descriptors_runtime_and_function_constants(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    SZrObjectModule *module = ZR_NULL;
    SZrFunction *function = ZR_NULL;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    SZrTypeValue moduleKey;
    SZrTypeValue functionKey;
    SZrTypeValue moduleValue;
    SZrTypeValue functionValue;
    SZrString *oldModuleName = ZR_NULL;
    SZrString *oldFullPath = ZR_NULL;
    SZrAotCodeRegistration oldRegistration = {0};
    SZrAotCodeRegistration newRegistration = {0};
    SZrModuleExportDescriptor descriptor = {0};
    SZrModuleExportDescriptor extraDescriptor = {0};
    TZrUInt64 oldFunctionGeneration;

    TEST_ASSERT_NOT_NULL(registry);
    ZrCore_Value_InitAsInt(state, &moduleKey, 1701);
    ZrCore_Value_InitAsInt(state, &functionKey, 1702);

    module = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(module);
    ZrCore_Value_InitAsRawObject(state, &moduleValue, ZR_CAST_RAW_OBJECT_AS_SUPER(module));
    registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    ZrCore_Object_SetValue(state, registry, &moduleKey, &moduleValue);
    module = checkpoint_test_lookup_module(state, &moduleKey);
    TEST_ASSERT_NOT_NULL(module);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Value_InitAsRawObject(state, &functionValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    module = checkpoint_test_lookup_module(state, &moduleKey);
    ZrCore_Object_SetValue(state, ZR_CAST_OBJECT(state, ZR_CAST_RAW_OBJECT_AS_SUPER(module)),
                           &functionKey, &functionValue);

    oldModuleName = ZrCore_String_CreateFromNative(state, (TZrNativeString)"retained-module");
    TEST_ASSERT_NOT_NULL(oldModuleName);
    module = checkpoint_test_lookup_module(state, &moduleKey);
    ZrCore_Module_SetInfo(state, module, oldModuleName, 0x1111222233334444u, ZR_NULL);
    oldFullPath = ZrCore_String_CreateFromNative(state, (TZrNativeString)"/old/module.zr");
    TEST_ASSERT_NOT_NULL(oldFullPath);
    module = checkpoint_test_lookup_module(state, &moduleKey);
    ZrCore_Module_SetInfo(state, module, module->moduleName, 0x1111222233334444u, oldFullPath);

    function = checkpoint_test_lookup_function(state, &moduleKey, &functionKey);
    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            global, sizeof(*function->constantValueList), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    function->constantValueLength = 1u;
    {
        SZrString *oldConstant = ZrCore_String_CreateFromNative(state, (TZrNativeString)"constant-A");
        TEST_ASSERT_NOT_NULL(oldConstant);
        function = checkpoint_test_lookup_function(state, &moduleKey, &functionKey);
        ZrCore_Value_InitAsRawObject(state, &function->constantValueList[0],
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(oldConstant));
    }
    ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function),
                         &function->constantValueList[0]);

    descriptor.name = ZrCore_String_CreateFromNative(state, (TZrNativeString)"tick");
    TEST_ASSERT_NOT_NULL(descriptor.name);
    descriptor.accessModifier = 1u;
    descriptor.exportKind = 2u;
    descriptor.readiness = 3u;
    descriptor.isReady = 1u;
    module = checkpoint_test_lookup_module(state, &moduleKey);
    TEST_ASSERT_TRUE(ZrCore_Module_RegisterExportDescriptor(state, module, &descriptor));

    oldRegistration.functionCount = 3u;
    newRegistration.functionCount = 7u;
    module = checkpoint_test_lookup_module(state, &moduleKey);
    function = checkpoint_test_lookup_function(state, &moduleKey, &functionKey);
    TEST_ASSERT_NOT_NULL(ZrCore_Module_AttachMetadataRuntime(module, function, &oldRegistration));
    function->hasDecoratorMetadata = ZR_TRUE;
    ZrCore_Value_InitAsInt(state, &function->decoratorMetadataValue, 41);
    module->metadataGeneration = 17u;
    module->reserved0 = 1u;
    module->reserved1 = 0x55aau;
    module->metadataRuntime.methodRecordCacheToken = 0x1234u;
    module->metadataRuntime.typeLayoutCacheNextIndex = 5u;
    oldFunctionGeneration = function->callBindingGeneration;
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    g_checkpointTestCheckpoint = checkpoint;

    {
        SZrString *newModuleName = ZrCore_String_CreateFromNative(
                state, (TZrNativeString)"replacement-module");
        TEST_ASSERT_NOT_NULL(newModuleName);
        module = checkpoint_test_lookup_module(state, &moduleKey);
        ZrCore_Module_SetInfo(state, module, newModuleName, 0xaaaabbbbccccddddull,
                              module->fullPath);
    }
    {
        SZrString *newFullPath = ZrCore_String_CreateFromNative(
                state, (TZrNativeString)"/new/module.zr");
        TEST_ASSERT_NOT_NULL(newFullPath);
        module = checkpoint_test_lookup_module(state, &moduleKey);
        ZrCore_Module_SetInfo(state, module, module->moduleName, 0xaaaabbbbccccddddull,
                              newFullPath);
    }
    {
        SZrString *newDescriptorName = ZrCore_String_CreateFromNative(
                state, (TZrNativeString)"replaced-tick");
        TEST_ASSERT_NOT_NULL(newDescriptorName);
        module = checkpoint_test_lookup_module(state, &moduleKey);
        module->exportDescriptors[0].name = newDescriptorName;
        module->exportDescriptors[0].accessModifier = 9u;
        module->exportDescriptors[0].exportKind = 8u;
        module->exportDescriptors[0].readiness = 7u;
        module->exportDescriptors[0].isReady = 0u;
        ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(module),
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(newDescriptorName));
    }
    extraDescriptor.name = ZrCore_String_CreateFromNative(state, (TZrNativeString)"extra");
    TEST_ASSERT_NOT_NULL(extraDescriptor.name);
    extraDescriptor.accessModifier = 6u;
    extraDescriptor.exportKind = 5u;
    extraDescriptor.readiness = 4u;
    extraDescriptor.isReady = 1u;
    module = checkpoint_test_lookup_module(state, &moduleKey);
    TEST_ASSERT_TRUE(ZrCore_Module_RegisterExportDescriptor(state, module, &extraDescriptor));

    {
        SZrString *newConstant = ZrCore_String_CreateFromNative(
                state, (TZrNativeString)"constant-B");
        SZrTypeValue replacementConstant;
        TEST_ASSERT_NOT_NULL(newConstant);
        function = checkpoint_test_lookup_function(state, &moduleKey, &functionKey);
        ZrCore_Value_InitAsRawObject(state, &replacementConstant,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(newConstant));
        ZrCore_Value_Copy(state, &function->constantValueList[0], &replacementConstant);
        ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function),
                             &function->constantValueList[0]);
    }
    module = checkpoint_test_lookup_module(state, &moduleKey);
    function = checkpoint_test_lookup_function(state, &moduleKey, &functionKey);
    TEST_ASSERT_NOT_NULL(ZrCore_Module_AttachMetadataRuntime(module, function, &newRegistration));
    module = checkpoint_test_lookup_module(state, &moduleKey);
    module->reserved0 = 0u;
    module->reserved1 = 0u;

    /* The checkpoint must resolve saved GC references by handle after compaction. */
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    module = checkpoint_test_lookup_module(state, &moduleKey);
    TEST_ASSERT_NOT_NULL(module);
    function = checkpoint_test_lookup_function(state, &moduleKey, &functionKey);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    TEST_ASSERT_EQUAL_STRING("retained-module", ZrCore_String_GetNativeString(module->moduleName));
    TEST_ASSERT_EQUAL_UINT64(0x1111222233334444u, module->pathHash);
    TEST_ASSERT_EQUAL_STRING("/old/module.zr", ZrCore_String_GetNativeString(module->fullPath));
    TEST_ASSERT_EQUAL_UINT32(17u, module->metadataGeneration);
    TEST_ASSERT_EQUAL_UINT8(1u, module->reserved0);
    TEST_ASSERT_EQUAL_UINT16(0x55aau, module->reserved1);
    TEST_ASSERT_EQUAL_UINT32(1u, module->exportDescriptorLength);
    TEST_ASSERT_EQUAL_STRING("tick", ZrCore_String_GetNativeString(module->exportDescriptors[0].name));
    TEST_ASSERT_EQUAL_UINT8(1u, module->exportDescriptors[0].accessModifier);
    TEST_ASSERT_EQUAL_UINT8(2u, module->exportDescriptors[0].exportKind);
    TEST_ASSERT_EQUAL_UINT8(3u, module->exportDescriptors[0].readiness);
    TEST_ASSERT_EQUAL_UINT8(1u, module->exportDescriptors[0].isReady);
    TEST_ASSERT_TRUE(module->hasMetadataRuntime);
    TEST_ASSERT_EQUAL_PTR(module, module->metadataRuntime.module);
    TEST_ASSERT_EQUAL_PTR(function, module->metadataRuntime.metadataFunction);
    TEST_ASSERT_EQUAL_PTR(&oldRegistration, module->metadataRuntime.codeRegistration);
    TEST_ASSERT_EQUAL_UINT32(3u, module->metadataRuntime.functionCount);
    TEST_ASSERT_EQUAL_UINT32(0x1234u, module->metadataRuntime.methodRecordCacheToken);
    TEST_ASSERT_EQUAL_UINT32(5u, module->metadataRuntime.typeLayoutCacheNextIndex);
    TEST_ASSERT_EQUAL_UINT64(oldFunctionGeneration, function->callBindingGeneration);
    TEST_ASSERT_TRUE(function->hasDecoratorMetadata);
    TEST_ASSERT_EQUAL_INT64(41, function->decoratorMetadataValue.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_STRING("constant-A",
                             ZrCore_String_GetNativeString(ZR_CAST_STRING(
                                     state, function->constantValueList[0].value.object)));

    /* The module self-reference must survive a moving collection after rollback. */
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    module = checkpoint_test_lookup_module(state, &moduleKey);
    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_EQUAL_PTR(module, module->metadataRuntime.module);
    TEST_ASSERT_EQUAL_UINT32(17u, module->metadataRuntime.module->metadataGeneration);
}

static void test_checkpoint_rejects_zero_length_inline_layout_objects(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrObject *object = ZrCore_Object_New(state, ZR_NULL);
    SZrFunction *layoutFunction;
    SZrObject *registry;
    const SZrTypeValue *storedLayout;
    SZrTypeValue objectKey;
    SZrTypeValue objectValue;
    SZrTypeValue layoutKey;
    SZrTypeValue layoutValue;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;

    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);
    ZrCore_Value_InitAsInt(state, &objectKey, 1901);
    ZrCore_Value_InitAsInt(state, &layoutKey, 1902);
    ZrCore_Value_InitAsRawObject(state, &objectValue, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    ZrCore_Object_SetValue(state, registry, &objectKey, &objectValue);

    layoutFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(layoutFunction);
    ZrCore_Value_InitAsRawObject(state, &layoutValue, ZR_CAST_RAW_OBJECT_AS_SUPER(layoutFunction));
    registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    ZrCore_Object_SetValue(state, registry, &layoutKey, &layoutValue);
    object = checkpoint_test_lookup_registry_object(state, &objectKey);
    TEST_ASSERT_NOT_NULL(object);
    registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    storedLayout = ZrCore_Object_GetValue(state, registry, &layoutKey);
    TEST_ASSERT_NOT_NULL(storedLayout);
    layoutFunction = ZR_CAST_FUNCTION(state, storedLayout->value.object);
    object->inlineArrayLayoutFunction = layoutFunction;
    object->inlineArrayObjectByteSize = sizeof(SZrObject);
    object->inlineArrayElementByteSize = sizeof(TZrInt64);
    object->inlineArrayLength = 0u;
    ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object),
                             ZR_CAST_RAW_OBJECT_AS_SUPER(layoutFunction));

    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NULL(checkpoint);
}

static void test_checkpoint_restore_preparation_failure_keeps_live_array_and_map(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    SZrObject *items = ZrCore_Object_NewCustomized(
            state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    SZrTypeValue itemsKey;
    SZrTypeValue itemsValue;
    SZrTypeValue originalKey;
    SZrTypeValue originalValue;
    SZrTypeValue candidateKey;
    SZrTypeValue candidateValue;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    ZrCheckpointFailAllocatorContext allocatorContext;
    FZrAllocator originalAllocator;
    TZrPtr originalAllocatorUserData;
    TZrInt64 *candidateRawData;
    TZrSize snapshotCapacity;
    TZrSize candidateCapacity;
    TZrBool rollbackResult;
    TZrBool rejectedArrayUnchanged;
    TZrBool rejectedHadCurrentException;
    TZrBool rejectedToBeClosedList;
    TZrInt32 rejectedThreadStatus;
    TZrUInt32 rejectedPendingKind;
    const SZrTypeValue *restored;

    TEST_ASSERT_NOT_NULL(items);
    ZrCore_Object_Init(state, items);
    ZrCore_Value_InitAsInt(state, &itemsKey, 801);
    ZrCore_Value_InitAsRawObject(state, &itemsValue, ZR_CAST_RAW_OBJECT_AS_SUPER(items));
    registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    ZrCore_Object_SetValue(state, registry, &itemsKey, &itemsValue);
    items = checkpoint_test_lookup_registry_object(state, &itemsKey);
    TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayEnsureRawIntCapacity(state, items, 2u));
    items->superArrayRawIntData[0] = 11;
    items->superArrayRawIntData[1] = 22;
    items->superArrayRawIntLength = 2u;
    items->superArrayStorageMode = ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL;
    items->superArrayRawIntDirty = ZR_TRUE;

    ZrCore_Value_InitAsInt(state, &originalKey, 701);
    ZrCore_Value_InitAsInt(state, &originalValue, 31);
    ZrCore_Object_SetValue(state, items, &originalKey, &originalValue);
    items = checkpoint_test_lookup_registry_object(state, &itemsKey);
    snapshotCapacity = items->superArrayRawIntCapacity;
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    g_checkpointTestCheckpoint = checkpoint;

    TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayEnsureRawIntCapacity(
            state, items, snapshotCapacity + 32u));
    items->superArrayRawIntData[0] = 91;
    items->superArrayRawIntData[1] = 92;
    items->superArrayRawIntData[2] = 93;
    items->superArrayRawIntLength = 3u;
    items->superArrayRawIntDirty = ZR_FALSE;
    candidateRawData = items->superArrayRawIntData;
    candidateCapacity = items->superArrayRawIntCapacity;
    TEST_ASSERT_GREATER_THAN_UINT64(snapshotCapacity, candidateCapacity);

    ZrCore_Value_InitAsInt(state, &candidateKey, 702);
    ZrCore_Value_InitAsInt(state, &candidateValue, 77);
    ZrCore_Object_SetValue(state, items, &candidateKey, &candidateValue);
    items = checkpoint_test_lookup_registry_object(state, &itemsKey);
    candidateRawData = items->superArrayRawIntData;
    candidateCapacity = items->superArrayRawIntCapacity;

    memset(&allocatorContext, 0, sizeof(allocatorContext));
    originalAllocator = global->allocator;
    originalAllocatorUserData = global->userAllocationArguments;
    allocatorContext.upstream = originalAllocator;
    allocatorContext.upstreamUserData = originalAllocatorUserData;
    allocatorContext.failureType = ZR_MEMORY_NATIVE_TYPE_ARRAY;
    allocatorContext.failuresRemaining = 1u;
    global->allocator = checkpoint_fail_allocator;
    global->userAllocationArguments = &allocatorContext;
    state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
    state->currentExceptionStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
    state->hasCurrentException = ZR_TRUE;
    rollbackResult = ZrCore_SessionCheckpoint_Rollback(state, checkpoint);
    global->allocator = originalAllocator;
    global->userAllocationArguments = originalAllocatorUserData;

    TEST_ASSERT_FALSE(rollbackResult);
    TEST_ASSERT_EQUAL_UINT32(1u, allocatorContext.failuresTriggered);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, state->threadStatus);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, state->currentExceptionStatus);
    TEST_ASSERT_TRUE(state->hasCurrentException);
    items = checkpoint_test_lookup_registry_object(state, &itemsKey);
    TEST_ASSERT_EQUAL_PTR(candidateRawData, items->superArrayRawIntData);
    TEST_ASSERT_EQUAL_UINT64(candidateCapacity, items->superArrayRawIntCapacity);
    TEST_ASSERT_EQUAL_UINT64(3u, items->superArrayRawIntLength);
    TEST_ASSERT_FALSE(items->superArrayRawIntDirty);
    TEST_ASSERT_EQUAL_INT64(91, items->superArrayRawIntData[0]);
    TEST_ASSERT_EQUAL_INT64(92, items->superArrayRawIntData[1]);
    TEST_ASSERT_EQUAL_INT64(93, items->superArrayRawIntData[2]);
    restored = ZrCore_Object_GetValue(state, items, &candidateKey);
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT_EQUAL_INT64(77, restored->value.nativeObject.nativeInt64);

    state->pendingControl.kind = ZR_VM_PENDING_CONTROL_BREAK;
    state->pendingControl.callInfo = &state->baseCallInfo;
    rollbackResult = ZrCore_SessionCheckpoint_Rollback(state, checkpoint);
    rejectedThreadStatus = state->threadStatus;
    rejectedHadCurrentException = state->hasCurrentException;
    rejectedPendingKind = (TZrUInt32)state->pendingControl.kind;
    items = checkpoint_test_lookup_registry_object(state, &itemsKey);
    rejectedArrayUnchanged = (TZrBool)(items->superArrayRawIntData == candidateRawData &&
                                       items->superArrayRawIntCapacity == candidateCapacity &&
                                       items->superArrayRawIntLength == 3u);
    state->pendingControl.kind = ZR_VM_PENDING_CONTROL_NONE;
    state->pendingControl.callInfo = ZR_NULL;
    state->pendingControl.targetInstructionOffset = 0u;
    state->pendingControl.valueSlot = 0u;
    ZrCore_Value_ResetAsNull(&state->pendingControl.value);
    state->pendingControl.hasValue = ZR_FALSE;

    TEST_ASSERT_FALSE(rollbackResult);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, rejectedThreadStatus);
    TEST_ASSERT_TRUE(rejectedHadCurrentException);
    TEST_ASSERT_EQUAL_UINT32(ZR_VM_PENDING_CONTROL_BREAK, rejectedPendingKind);
    TEST_ASSERT_TRUE(rejectedArrayUnchanged);

    state->toBeClosedValueList.valuePointer = state->stackBase.valuePointer + 1;
    rollbackResult = ZrCore_SessionCheckpoint_Rollback(state, checkpoint);
    rejectedToBeClosedList =
            (TZrBool)(state->toBeClosedValueList.valuePointer == state->stackBase.valuePointer + 1);
    state->toBeClosedValueList.valuePointer = state->stackBase.valuePointer;
    TEST_ASSERT_FALSE(rollbackResult);
    TEST_ASSERT_TRUE(rejectedToBeClosedList);

    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    TEST_ASSERT_FALSE(state->hasCurrentException);
    items = checkpoint_test_lookup_registry_object(state, &itemsKey);
    TEST_ASSERT_EQUAL_UINT64(snapshotCapacity, items->superArrayRawIntCapacity);
    TEST_ASSERT_EQUAL_UINT64(2u, items->superArrayRawIntLength);
    TEST_ASSERT_TRUE(items->superArrayRawIntDirty);
    TEST_ASSERT_EQUAL_INT64(11, items->superArrayRawIntData[0]);
    TEST_ASSERT_EQUAL_INT64(22, items->superArrayRawIntData[1]);
    TEST_ASSERT_NULL(ZrCore_Object_GetValue(state, items, &candidateKey));
    restored = ZrCore_Object_GetValue(state, items, &originalKey);
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT_EQUAL_INT64(31, restored->value.nativeObject.nativeInt64);
}

static void test_checkpoint_create_rejects_live_extra_stack_value(void) {
    SZrState *state = checkpoint_test_state();
    SZrObject *stackObject = ZrCore_Object_New(state, ZR_NULL);
    TZrStackValuePointer extraSlot;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    TZrStackValuePointer originalStackTop;

    TEST_ASSERT_NOT_NULL(stackObject);
    ZrCore_Object_Init(state, stackObject);
    extraSlot = state->stackBase.valuePointer + 1;
    originalStackTop = state->stackTop.valuePointer;
    ZrCore_Value_InitAsRawObject(state, &extraSlot->value,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(stackObject));
    state->stackTop.valuePointer = extraSlot + 1;

    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NULL(checkpoint);
    TEST_ASSERT_EQUAL_PTR(extraSlot + 1, state->stackTop.valuePointer);

    state->stackTop.valuePointer = originalStackTop;
    ZrCore_Value_ResetAsNull(&extraSlot->value);
}

static void test_checkpoint_create_rejects_pending_object_value(void) {
    SZrState *state = checkpoint_test_state();
    SZrObject *pendingObject = ZrCore_Object_New(state, ZR_NULL);
    SZrSessionCheckpoint *checkpoint = ZR_NULL;

    TEST_ASSERT_NOT_NULL(pendingObject);
    ZrCore_Object_Init(state, pendingObject);
    state->pendingControl.kind = ZR_VM_PENDING_CONTROL_RETURN;
    state->pendingControl.callInfo = &state->baseCallInfo;
    ZrCore_Value_InitAsRawObject(state, &state->pendingControl.value,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(pendingObject));
    state->pendingControl.hasValue = ZR_TRUE;

    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NULL(checkpoint);
    TEST_ASSERT_EQUAL_PTR(pendingObject, state->pendingControl.value.value.object);
    TEST_ASSERT_TRUE(state->pendingControl.hasValue);

    state->pendingControl.kind = ZR_VM_PENDING_CONTROL_NONE;
    state->pendingControl.callInfo = ZR_NULL;
    state->pendingControl.targetInstructionOffset = 0u;
    state->pendingControl.valueSlot = 0u;
    ZrCore_Value_ResetAsNull(&state->pendingControl.value);
    state->pendingControl.hasValue = ZR_FALSE;
}

static void test_checkpoint_rejects_pair_pool_free_size_overflow(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    SZrObject *items;
    SZrTypeValue itemsKey;
    SZrHashSet *set;
    SZrHashPairPoolBlock *block;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    TZrSize originalBlockCapacity;
    TZrSize originalPoolCapacity;
    TZrSize largestFreeableCapacity;
    TZrSize oversizedBlockCapacity;
    TZrSize originalElementCount;
    TZrSize originalMapCapacity;
    TZrSize originalBucketSize;
    SZrHashKeyValuePair **originalBuckets;
    SZrHashPairPoolBlock *originalPoolHead;
    TZrBool rollbackResult;

    TEST_ASSERT_NOT_NULL(registry);
    ZrCore_Value_InitAsInt(state, &itemsKey, 6101);
    items = checkpoint_test_make_mixed_array(state, registry, 6101, 6102);
    TEST_ASSERT_NOT_NULL(items);
    set = &items->nodeMap;
    block = set->pairPoolHead;
    TEST_ASSERT_NOT_NULL(block);
    TEST_ASSERT_TRUE(set->pairPoolCapacity >= block->capacity);
    TEST_ASSERT_TRUE(block->capacity > 0u);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    g_checkpointTestCheckpoint = checkpoint;

    registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    items = checkpoint_test_lookup_registry_object(state, &itemsKey);
    TEST_ASSERT_NOT_NULL(items);
    set = &items->nodeMap;
    block = set->pairPoolHead;
    originalBlockCapacity = block->capacity;
    originalPoolCapacity = set->pairPoolCapacity;
    originalElementCount = set->elementCount;
    originalMapCapacity = set->capacity;
    originalBucketSize = set->bucketSize;
    originalBuckets = set->buckets;
    originalPoolHead = set->pairPoolHead;
    largestFreeableCapacity = ((TZrSize)-1 - sizeof(*block)) /
                              sizeof(SZrHashKeyValuePair) +
                              ZR_HASH_PAIR_POOL_INLINE_COUNT;
    oversizedBlockCapacity = largestFreeableCapacity + 1u;
    TEST_ASSERT_TRUE(oversizedBlockCapacity > originalBlockCapacity);

    block->capacity = oversizedBlockCapacity;
    set->pairPoolCapacity = originalPoolCapacity - originalBlockCapacity +
                            oversizedBlockCapacity;
    rollbackResult = ZrCore_SessionCheckpoint_Rollback(state, checkpoint);
    if (!rollbackResult) {
        registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
        items = checkpoint_test_lookup_registry_object(state, &itemsKey);
        TEST_ASSERT_NOT_NULL(items);
        set = &items->nodeMap;
        block = set->pairPoolHead;
        block->capacity = originalBlockCapacity;
        set->pairPoolCapacity = originalPoolCapacity;
    }

    TEST_ASSERT_FALSE(rollbackResult);
    TEST_ASSERT_EQUAL_PTR(originalBuckets, set->buckets);
    TEST_ASSERT_EQUAL_PTR(originalPoolHead, set->pairPoolHead);
    TEST_ASSERT_EQUAL_UINT64(originalMapCapacity, set->capacity);
    TEST_ASSERT_EQUAL_UINT64(originalBucketSize, set->bucketSize);
    TEST_ASSERT_EQUAL_UINT64(originalPoolCapacity, set->pairPoolCapacity);
    TEST_ASSERT_EQUAL_UINT64(originalElementCount, set->elementCount);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
}

static void test_checkpoint_restores_prototype_layout_and_function_caches(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrObject *registry;
    SZrStructPrototype *structPrototype = (SZrStructPrototype *)global->stackFramePrototype;
    SZrObjectPrototype *dispatchA = global->errorPrototype;
    SZrObjectPrototype *dispatchB = global->basicTypeObjectPrototype[ZR_VALUE_TYPE_OBJECT];
    SZrFunction *functionA;
    SZrFunction *functionB;
    SZrClosure *replacementClosure;
    SZrClosure *savedClosure;
    SZrObjectPrototype **replacementInstances;
    SZrObjectPrototype **previousInstances;
    SZrTypeValue functionAKey;
    SZrTypeValue functionBKey;
    SZrTypeValue functionValue;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    SZrMemberDescriptor descriptor = {0};
    SZrIndexContract indexContract = {0};
    SZrIterableContract iterableContract = {0};
    SZrIteratorContract iteratorContract = {0};
    SZrString *savedName;
    SZrObjectPrototype *savedSuperPrototype;
    TZrUInt64 savedShapeId;
    TZrUInt64 savedShapeGeneration;
    TZrUInt64 savedLayoutGeneration;
    TZrUInt64 savedProtocolMask;
    TZrUInt32 savedModifierFlags;
    TZrUInt32 savedNextVirtualSlotIndex;
    TZrUInt32 savedNextPropertyIdentity;
    TZrUInt32 savedLayoutByteSize;
    TZrUInt32 savedLayoutByteAlign;
    TZrUInt8 savedReserved0;
    TZrUInt16 savedReserved1;
    TZrBool savedDynamicMemberCapable;
    TZrUInt32 savedDescriptorCount;
    TZrUInt32 savedManagedFieldCount;
    TZrUInt32 savedInterfaceDispatchCount;
    TZrSize savedKeyOffsetCount;
    TZrInt64 savedKeyOffset = 0;
    TZrUInt32 savedKeyOffsetType = 0u;
    SZrTypeValue replacementKeyOffset;
    TZrBool foundKeyOffset = ZR_FALSE;
    TZrBool foundSavedKeyOffset = ZR_FALSE;
    ZrCheckpointFailAllocatorContext allocatorContext;
    FZrAllocator originalAllocator;
    TZrPtr originalAllocatorUserData;
    SZrString *candidateName;
    TZrUInt32 candidateDescriptorCount;
    TZrUInt32 candidateManagedFieldCount;
    TZrUInt32 bucketIndex;

    TEST_ASSERT_NOT_NULL(structPrototype);
    TEST_ASSERT_NOT_NULL(dispatchA);
    TEST_ASSERT_NOT_NULL(dispatchB);
    ZrCore_Value_InitAsInt(state, &functionAKey, 2401);
    ZrCore_Value_InitAsInt(state, &functionBKey, 2402);

    functionA = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(functionA);
    ZrCore_Value_InitAsRawObject(state, &functionValue, ZR_CAST_RAW_OBJECT_AS_SUPER(functionA));
    registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    ZrCore_Object_SetValue(state, registry, &functionAKey, &functionValue);
    functionB = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(functionB);
    ZrCore_Value_InitAsRawObject(state, &functionValue, ZR_CAST_RAW_OBJECT_AS_SUPER(functionB));
    registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    ZrCore_Object_SetValue(state, registry, &functionBKey, &functionValue);
    functionA = checkpoint_test_lookup_registry_function(state, &functionAKey);
    functionB = checkpoint_test_lookup_registry_function(state, &functionBKey);
    TEST_ASSERT_NOT_NULL(functionA);
    TEST_ASSERT_NOT_NULL(functionB);

    /* Give a real rooted function both identity-bearing runtime caches. */
    {
        SZrClosure *closure = ZrCore_Closure_New(state, 0u);
        TEST_ASSERT_NOT_NULL(closure);
        functionA = checkpoint_test_lookup_registry_function(state, &functionAKey);
        functionB = checkpoint_test_lookup_registry_function(state, &functionBKey);
        structPrototype = (SZrStructPrototype *)global->stackFramePrototype;
        dispatchA = global->errorPrototype;
        dispatchB = global->basicTypeObjectPrototype[ZR_VALUE_TYPE_OBJECT];
        closure->function = functionA;
        functionA->cachedStatelessClosure = closure;
        ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(functionA),
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
        functionA->prototypeInstances = (SZrObjectPrototype **)ZrCore_Memory_RawMalloc(
                global, sizeof(*functionA->prototypeInstances));
        TEST_ASSERT_NOT_NULL(functionA->prototypeInstances);
        functionA->prototypeInstances[0] = &structPrototype->super;
        functionA->prototypeInstancesLength = 1u;
        ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(functionA),
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(&structPrototype->super));
    }

    descriptor.name = global->metaFunctionName[0];
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_METHOD;
    descriptor.getterFunction = functionA;
    descriptor.setterFunction = functionA;
    descriptor.initializerFunction = functionA;
    descriptor.methodFunction = functionA;
    descriptor.ownerTypeName = global->metaFunctionName[0];
    descriptor.baseDefinitionOwnerTypeName = global->metaFunctionName[0];
    descriptor.baseDefinitionName = global->metaFunctionName[0];
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(
            state, &structPrototype->super, &descriptor));
    ZrCore_ObjectPrototype_AddManagedField(state, &structPrototype->super,
            global->metaFunctionName[0], 12u, 4u, 0u, ZR_FALSE, ZR_FALSE, 1u);
    ZrCore_ObjectPrototype_AddMeta(state, &structPrototype->super, (EZrMetaType)0u, functionA);

    indexContract.getByIndexFunction = functionA;
    indexContract.setByIndexFunction = functionA;
    indexContract.containsKeyFunction = functionA;
    indexContract.getLengthFunction = functionA;
    indexContract.getByIndexKnownNativeCallable = ZR_CAST_RAW_OBJECT_AS_SUPER(functionA);
    indexContract.setByIndexKnownNativeCallable = ZR_CAST_RAW_OBJECT_AS_SUPER(functionA);
    iterableContract.iterInitFunction = functionA;
    iteratorContract.moveNextFunction = functionA;
    iteratorContract.currentFunction = functionA;
    iteratorContract.currentMemberName = global->metaFunctionName[0];
    ZrCore_ObjectPrototype_SetIndexContract(&structPrototype->super, &indexContract);
    ZrCore_ObjectPrototype_SetIterableContract(&structPrototype->super, &iterableContract);
    ZrCore_ObjectPrototype_SetIteratorContract(&structPrototype->super, &iteratorContract);

    if (structPrototype->super.interfaceDispatchEntries == ZR_NULL ||
        structPrototype->super.interfaceDispatchCount == 0u) {
        structPrototype->super.interfaceDispatchEntries =
                (SZrInterfaceDispatchEntry *)ZrCore_Memory_RawMalloc(
                        global, sizeof(*structPrototype->super.interfaceDispatchEntries));
        TEST_ASSERT_NOT_NULL(structPrototype->super.interfaceDispatchEntries);
        structPrototype->super.interfaceDispatchCount = 1u;
    }
    structPrototype->super.interfaceDispatchEntries[0].interfacePrototype = dispatchA;
    structPrototype->super.interfaceDispatchEntries[0].implementationPrototype = dispatchB;
    structPrototype->super.interfaceDispatchEntries[0].interfaceSlot = 8u;
    structPrototype->super.interfaceDispatchEntries[0].descriptorIndex = 3u;
    ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(structPrototype),
                             ZR_CAST_RAW_OBJECT_AS_SUPER(dispatchA));
    ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(structPrototype),
                             ZR_CAST_RAW_OBJECT_AS_SUPER(dispatchB));

    for (bucketIndex = 0u; bucketIndex < structPrototype->keyOffsetMap.capacity; bucketIndex++) {
        for (SZrHashKeyValuePair *pair = structPrototype->keyOffsetMap.buckets[bucketIndex];
             pair != ZR_NULL; pair = pair->next) {
            savedKeyOffset = pair->value.value.nativeObject.nativeInt64;
            savedKeyOffsetType = (TZrUInt32)pair->value.type;
            foundKeyOffset = ZR_TRUE;
            break;
        }
        if (foundKeyOffset) {
            break;
        }
    }
    TEST_ASSERT_TRUE(foundKeyOffset);
    savedName = structPrototype->super.name;
    savedSuperPrototype = structPrototype->super.superPrototype;
    savedShapeId = structPrototype->super.shapeId;
    savedShapeGeneration = structPrototype->super.shapeGeneration;
    savedLayoutGeneration = structPrototype->super.layoutGeneration;
    savedProtocolMask = structPrototype->super.protocolMask;
    savedModifierFlags = structPrototype->super.modifierFlags;
    savedNextVirtualSlotIndex = structPrototype->super.nextVirtualSlotIndex;
    savedNextPropertyIdentity = structPrototype->super.nextPropertyIdentity;
    savedLayoutByteSize = structPrototype->super.layoutByteSize;
    savedLayoutByteAlign = structPrototype->super.layoutByteAlign;
    savedReserved0 = structPrototype->super.reserved0;
    savedReserved1 = structPrototype->super.reserved1;
    savedDynamicMemberCapable = structPrototype->super.dynamicMemberCapable;
    savedDescriptorCount = structPrototype->super.memberDescriptorCount;
    savedManagedFieldCount = structPrototype->super.managedFieldCount;
    savedInterfaceDispatchCount = structPrototype->super.interfaceDispatchCount;
    savedKeyOffsetCount = structPrototype->keyOffsetMap.elementCount;

    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    g_checkpointTestCheckpoint = checkpoint;

    /* The replacement closure may collect; reacquire every movable object after it. */
    replacementClosure = ZrCore_Closure_New(state, 0u);
    TEST_ASSERT_NOT_NULL(replacementClosure);
    functionA = checkpoint_test_lookup_registry_function(state, &functionAKey);
    functionB = checkpoint_test_lookup_registry_function(state, &functionBKey);
    structPrototype = (SZrStructPrototype *)global->stackFramePrototype;
    dispatchA = global->errorPrototype;
    dispatchB = global->basicTypeObjectPrototype[ZR_VALUE_TYPE_OBJECT];
    savedName = structPrototype->super.name;
    savedSuperPrototype = structPrototype->super.superPrototype;
    savedClosure = functionA->cachedStatelessClosure;
    replacementClosure->function = functionA;
    functionA->cachedStatelessClosure = replacementClosure;
    ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(functionA),
                             ZR_CAST_RAW_OBJECT_AS_SUPER(replacementClosure));

    previousInstances = functionA->prototypeInstances;
    replacementInstances = (SZrObjectPrototype **)ZrCore_Memory_RawMalloc(
            global, sizeof(*replacementInstances));
    TEST_ASSERT_NOT_NULL(replacementInstances);
    replacementInstances[0] = dispatchB;
    functionA->prototypeInstances = replacementInstances;
    functionA->prototypeInstancesLength = 1u;
    ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(functionA),
                             ZR_CAST_RAW_OBJECT_AS_SUPER(dispatchB));
    ZrCore_Memory_RawFree(global, previousInstances, sizeof(*previousInstances));

    structPrototype->super.name = global->basicTypeObjectPrototype[ZR_VALUE_TYPE_STRING]->name;
    structPrototype->super.superPrototype = dispatchB;
    structPrototype->super.shapeId = 999u;
    structPrototype->super.shapeGeneration = 998u;
    structPrototype->super.layoutGeneration = 997u;
    structPrototype->super.protocolMask = 0u;
    structPrototype->super.dynamicMemberCapable = ZR_TRUE;
    structPrototype->super.reserved0 = 91u;
    structPrototype->super.reserved1 = 321u;
    structPrototype->super.modifierFlags = 456u;
    structPrototype->super.nextVirtualSlotIndex = 654u;
    structPrototype->super.nextPropertyIdentity = 987u;
    structPrototype->super.layoutByteSize = 1234u;
    structPrototype->super.layoutByteAlign = 16u;
    ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(structPrototype),
                             ZR_CAST_RAW_OBJECT_AS_SUPER(structPrototype->super.name));
    ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(structPrototype),
                             ZR_CAST_RAW_OBJECT_AS_SUPER(dispatchB));
    structPrototype->super.metaTable.metas[0]->function = functionB;
    ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(structPrototype),
                             ZR_CAST_RAW_OBJECT_AS_SUPER(functionB));
    ZrCore_ObjectPrototype_AddMeta(state, &structPrototype->super, (EZrMetaType)1u, functionB);
    ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(structPrototype),
                             ZR_CAST_RAW_OBJECT_AS_SUPER(functionB));
    indexContract.getByIndexFunction = functionB;
    indexContract.setByIndexFunction = functionB;
    indexContract.containsKeyFunction = functionB;
    indexContract.getLengthFunction = functionB;
    indexContract.getByIndexKnownNativeCallable = ZR_CAST_RAW_OBJECT_AS_SUPER(functionB);
    indexContract.setByIndexKnownNativeCallable = ZR_CAST_RAW_OBJECT_AS_SUPER(functionB);
    ZrCore_ObjectPrototype_SetIndexContract(&structPrototype->super, &indexContract);
    iterableContract.iterInitFunction = functionB;
    ZrCore_ObjectPrototype_SetIterableContract(&structPrototype->super, &iterableContract);
    iteratorContract.moveNextFunction = functionB;
    iteratorContract.currentFunction = functionB;
    iteratorContract.currentMemberName = global->basicTypeObjectPrototype[ZR_VALUE_TYPE_STRING]->name;
    ZrCore_ObjectPrototype_SetIteratorContract(&structPrototype->super, &iteratorContract);
    structPrototype->super.interfaceDispatchEntries[0].interfacePrototype = dispatchB;
    structPrototype->super.interfaceDispatchEntries[0].implementationPrototype = dispatchA;
    ZrCore_Memory_RawFree(global, structPrototype->super.interfaceDispatchEntries,
            savedInterfaceDispatchCount * sizeof(*structPrototype->super.interfaceDispatchEntries));
    structPrototype->super.interfaceDispatchEntries = ZR_NULL;
    structPrototype->super.interfaceDispatchCount = 0u;
    for (TZrUInt32 index = 0u; index < 64u; index++) {
        descriptor.methodFunction = functionB;
        descriptor.getterFunction = functionB;
        descriptor.name = global->metaFunctionName[0];
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(
                state, &structPrototype->super, &descriptor));
        ZrCore_ObjectPrototype_AddManagedField(state, &structPrototype->super,
                global->metaFunctionName[0], index + 100u, 8u, 0u,
                ZR_FALSE, ZR_FALSE, index + 2u);
    }
    TEST_ASSERT_GREATER_THAN_UINT32(savedDescriptorCount,
                                    structPrototype->super.memberDescriptorCount);
    TEST_ASSERT_GREATER_THAN_UINT32(savedManagedFieldCount,
                                    structPrototype->super.managedFieldCount);
    {
        TZrBool changedKeyOffset = ZR_FALSE;
        ZrCore_Value_InitAsInt(state, &replacementKeyOffset, 987654321);
        for (bucketIndex = 0u; bucketIndex < structPrototype->keyOffsetMap.capacity; bucketIndex++) {
            for (SZrHashKeyValuePair *pair = structPrototype->keyOffsetMap.buckets[bucketIndex];
                 pair != ZR_NULL; pair = pair->next) {
                ZrCore_Value_Copy(state, &pair->value, &replacementKeyOffset);
                changedKeyOffset = ZR_TRUE;
                break;
            }
            if (changedKeyOffset) {
                break;
            }
        }
        TEST_ASSERT_TRUE(changedKeyOffset);
    }

    candidateName = structPrototype->super.name;
    candidateDescriptorCount = structPrototype->super.memberDescriptorCount;
    candidateManagedFieldCount = structPrototype->super.managedFieldCount;
    memset(&allocatorContext, 0, sizeof(allocatorContext));
    originalAllocator = global->allocator;
    originalAllocatorUserData = global->userAllocationArguments;
    allocatorContext.upstream = originalAllocator;
    allocatorContext.upstreamUserData = originalAllocatorUserData;
    allocatorContext.failureType = ZR_MEMORY_NATIVE_TYPE_GLOBAL;
    allocatorContext.failuresRemaining = 1u;
    global->allocator = checkpoint_fail_allocator;
    global->userAllocationArguments = &allocatorContext;
    {
        TZrBool rollbackResult = ZrCore_SessionCheckpoint_Rollback(state, checkpoint);
        global->allocator = originalAllocator;
        global->userAllocationArguments = originalAllocatorUserData;
        TEST_ASSERT_FALSE(rollbackResult);
    }
    TEST_ASSERT_EQUAL_UINT32(1u, allocatorContext.failuresTriggered);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    structPrototype = (SZrStructPrototype *)global->stackFramePrototype;
    functionA = checkpoint_test_lookup_registry_function(state, &functionAKey);
    TEST_ASSERT_EQUAL_PTR(candidateName, structPrototype->super.name);
    TEST_ASSERT_EQUAL_UINT32(candidateDescriptorCount, structPrototype->super.memberDescriptorCount);
    TEST_ASSERT_EQUAL_UINT32(candidateManagedFieldCount, structPrototype->super.managedFieldCount);
    TEST_ASSERT_EQUAL_PTR(functionB, structPrototype->super.metaTable.metas[0]->function);
    TEST_ASSERT_NOT_NULL(structPrototype->super.metaTable.metas[1]);
    TEST_ASSERT_EQUAL_PTR(replacementClosure, functionA->cachedStatelessClosure);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    structPrototype = (SZrStructPrototype *)global->stackFramePrototype;
    functionA = checkpoint_test_lookup_registry_function(state, &functionAKey);
    TEST_ASSERT_EQUAL_UINT32(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, structPrototype->super.type);
    TEST_ASSERT_EQUAL_PTR(savedName, structPrototype->super.name);
    TEST_ASSERT_EQUAL_PTR(savedSuperPrototype, structPrototype->super.superPrototype);
    TEST_ASSERT_EQUAL_UINT64(savedShapeId, structPrototype->super.shapeId);
    TEST_ASSERT_EQUAL_UINT64(savedShapeGeneration, structPrototype->super.shapeGeneration);
    TEST_ASSERT_EQUAL_UINT64(savedLayoutGeneration, structPrototype->super.layoutGeneration);
    TEST_ASSERT_EQUAL_UINT64(savedProtocolMask, structPrototype->super.protocolMask);
    TEST_ASSERT_EQUAL_UINT32(savedModifierFlags, structPrototype->super.modifierFlags);
    TEST_ASSERT_EQUAL_UINT32(savedNextVirtualSlotIndex, structPrototype->super.nextVirtualSlotIndex);
    TEST_ASSERT_EQUAL_UINT32(savedNextPropertyIdentity, structPrototype->super.nextPropertyIdentity);
    TEST_ASSERT_EQUAL_UINT32(savedLayoutByteSize, structPrototype->super.layoutByteSize);
    TEST_ASSERT_EQUAL_UINT32(savedLayoutByteAlign, structPrototype->super.layoutByteAlign);
    TEST_ASSERT_EQUAL_UINT8(savedReserved0, structPrototype->super.reserved0);
    TEST_ASSERT_EQUAL_UINT16(savedReserved1, structPrototype->super.reserved1);
    TEST_ASSERT_EQUAL_INT(savedDynamicMemberCapable,
                          structPrototype->super.dynamicMemberCapable);
    TEST_ASSERT_EQUAL_UINT32(savedDescriptorCount, structPrototype->super.memberDescriptorCount);
    TEST_ASSERT_EQUAL_PTR(functionA,
            structPrototype->super.memberDescriptors[savedDescriptorCount - 1u].methodFunction);
    TEST_ASSERT_EQUAL_PTR(global->metaFunctionName[0],
            structPrototype->super.memberDescriptors[savedDescriptorCount - 1u].name);
    TEST_ASSERT_EQUAL_PTR(functionA,
            structPrototype->super.memberDescriptors[savedDescriptorCount - 1u].getterFunction);
    TEST_ASSERT_EQUAL_PTR(functionA,
            structPrototype->super.memberDescriptors[savedDescriptorCount - 1u].setterFunction);
    TEST_ASSERT_EQUAL_PTR(functionA,
            structPrototype->super.memberDescriptors[savedDescriptorCount - 1u].initializerFunction);
    TEST_ASSERT_EQUAL_PTR(global->metaFunctionName[0],
            structPrototype->super.memberDescriptors[savedDescriptorCount - 1u].ownerTypeName);
    TEST_ASSERT_EQUAL_PTR(global->metaFunctionName[0],
            structPrototype->super.memberDescriptors[savedDescriptorCount - 1u].baseDefinitionOwnerTypeName);
    TEST_ASSERT_EQUAL_PTR(global->metaFunctionName[0],
            structPrototype->super.memberDescriptors[savedDescriptorCount - 1u].baseDefinitionName);
    TEST_ASSERT_EQUAL_UINT32(savedManagedFieldCount, structPrototype->super.managedFieldCount);
    TEST_ASSERT_EQUAL_PTR(global->metaFunctionName[0],
            structPrototype->super.managedFields[savedManagedFieldCount - 1u].name);
    TEST_ASSERT_EQUAL_UINT32(savedInterfaceDispatchCount,
                             structPrototype->super.interfaceDispatchCount);
    TEST_ASSERT_EQUAL_PTR(dispatchA,
            structPrototype->super.interfaceDispatchEntries[0].interfacePrototype);
    TEST_ASSERT_EQUAL_PTR(dispatchB,
            structPrototype->super.interfaceDispatchEntries[0].implementationPrototype);
    TEST_ASSERT_EQUAL_PTR(functionA, structPrototype->super.metaTable.metas[0]->function);
    TEST_ASSERT_NULL(structPrototype->super.metaTable.metas[1]);
    TEST_ASSERT_EQUAL_PTR(functionA, structPrototype->super.indexContract.getByIndexFunction);
    TEST_ASSERT_NULL(structPrototype->super.indexContract.getByIndexKnownNativeCallable);
    TEST_ASSERT_NULL(structPrototype->super.indexContract.setByIndexKnownNativeCallable);
    TEST_ASSERT_NULL(structPrototype->super.indexContract.getByIndexKnownNativeFunction);
    TEST_ASSERT_NULL(structPrototype->super.indexContract.setByIndexKnownNativeFunction);
    TEST_ASSERT_EQUAL_PTR(functionA, structPrototype->super.iterableContract.iterInitFunction);
    TEST_ASSERT_EQUAL_PTR(functionA, structPrototype->super.iteratorContract.moveNextFunction);
    TEST_ASSERT_EQUAL_PTR(global->metaFunctionName[0],
            structPrototype->super.iteratorContract.currentMemberName);
    TEST_ASSERT_EQUAL_UINT64(savedKeyOffsetCount, structPrototype->keyOffsetMap.elementCount);
    for (bucketIndex = 0u; bucketIndex < structPrototype->keyOffsetMap.capacity; bucketIndex++) {
        for (SZrHashKeyValuePair *pair = structPrototype->keyOffsetMap.buckets[bucketIndex];
             pair != ZR_NULL; pair = pair->next) {
            TEST_ASSERT_FALSE(pair->value.type == replacementKeyOffset.type &&
                    pair->value.value.nativeObject.nativeInt64 == 987654321);
            if ((TZrUInt32)pair->value.type == savedKeyOffsetType &&
                pair->value.value.nativeObject.nativeInt64 == savedKeyOffset) {
                foundSavedKeyOffset = ZR_TRUE;
            }
        }
    }
    TEST_ASSERT_TRUE(foundSavedKeyOffset);
    TEST_ASSERT_EQUAL_PTR(savedClosure, functionA->cachedStatelessClosure);
    TEST_ASSERT_EQUAL_UINT32(1u, functionA->prototypeInstancesLength);
    TEST_ASSERT_EQUAL_PTR(&structPrototype->super, functionA->prototypeInstances[0]);

    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    structPrototype = global->stackFramePrototype;
    functionA = checkpoint_test_lookup_registry_function(state, &functionAKey);
    dispatchA = global->errorPrototype;
    dispatchB = global->basicTypeObjectPrototype[ZR_VALUE_TYPE_OBJECT];
    TEST_ASSERT_NOT_NULL(functionA->cachedStatelessClosure);
    TEST_ASSERT_EQUAL_PTR(functionA,
            functionA->cachedStatelessClosure->function);
    TEST_ASSERT_EQUAL_PTR(&structPrototype->super,
            functionA->prototypeInstances[0]);
    TEST_ASSERT_EQUAL_PTR(functionA,
            structPrototype->super.memberDescriptors[savedDescriptorCount - 1u].methodFunction);
    TEST_ASSERT_EQUAL_PTR(global->metaFunctionName[0],
            structPrototype->super.memberDescriptors[savedDescriptorCount - 1u].name);
    TEST_ASSERT_EQUAL_PTR(dispatchA,
            structPrototype->super.interfaceDispatchEntries[0].interfacePrototype);
    TEST_ASSERT_EQUAL_PTR(dispatchB,
            structPrototype->super.interfaceDispatchEntries[0].implementationPrototype);
    TEST_ASSERT_EQUAL_PTR(functionA, structPrototype->super.metaTable.metas[0]->function);
}

static void test_checkpoint_restores_decorator_metadata_scalar_and_object(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    SZrFunction *function = ZrCore_Function_New(state);
    SZrObject *decoratorObject = ZrCore_Object_New(state, ZR_NULL);
    SZrTypeValue functionKey;
    SZrTypeValue functionValue;
    SZrTypeValue objectKey;
    SZrTypeValue objectValue;
    SZrTypeValue replacement;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;

    TEST_ASSERT_NOT_NULL(registry);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(decoratorObject);
    ZrCore_Object_Init(state, decoratorObject);
    ZrCore_Value_InitAsInt(state, &functionKey, 4801);
    ZrCore_Value_InitAsRawObject(state, &functionValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    ZrCore_Object_SetValue(state, registry, &functionKey, &functionValue);
    function = checkpoint_test_lookup_registry_function(state, &functionKey);
    TEST_ASSERT_NOT_NULL(function);

    function->hasDecoratorMetadata = ZR_TRUE;
    ZrCore_Value_InitAsInt(state, &function->decoratorMetadataValue, 41);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    g_checkpointTestCheckpoint = checkpoint;
    ZrCore_Value_InitAsInt(state, &replacement, 99);
    ZrCore_Value_Copy(state, &function->decoratorMetadataValue, &replacement);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    TEST_ASSERT_TRUE(function->hasDecoratorMetadata);
    TEST_ASSERT_EQUAL_INT64(41, function->decoratorMetadataValue.value.nativeObject.nativeInt64);
    ZrCore_SessionCheckpoint_Free(state, checkpoint);
    g_checkpointTestCheckpoint = ZR_NULL;
    checkpoint = ZR_NULL;

    ZrCore_Value_InitAsInt(state, &objectKey, 4802);
    ZrCore_Value_InitAsRawObject(state, &objectValue, ZR_CAST_RAW_OBJECT_AS_SUPER(decoratorObject));
    ZrCore_Object_SetValue(state, registry, &objectKey, &objectValue);
    function = checkpoint_test_lookup_registry_function(state, &functionKey);
    decoratorObject = checkpoint_test_lookup_registry_object(state, &objectKey);
    ZrCore_Value_InitAsRawObject(state, &function->decoratorMetadataValue,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(decoratorObject));
    ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function),
                         &function->decoratorMetadataValue);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    g_checkpointTestCheckpoint = checkpoint;
    ZrCore_Value_InitAsInt(state, &replacement, 100);
    ZrCore_Value_Copy(state, &function->decoratorMetadataValue, &replacement);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    TEST_ASSERT_TRUE(function->hasDecoratorMetadata);
    TEST_ASSERT_EQUAL_PTR(decoratorObject, function->decoratorMetadataValue.value.object);
}

static void test_checkpoint_create_allocator_failure_preserves_live_graph(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    SZrObject *retained = ZrCore_Object_New(state, ZR_NULL);
    SZrTypeValue retainedKey;
    SZrTypeValue selfKey;
    SZrTypeValue retainedValue;
    ZrCheckpointFailAllocatorContext allocatorContext;
    FZrAllocator originalAllocator;
    TZrPtr originalAllocatorUserData;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    TZrBool createResult;
    SZrObject *originalRetained;
    SZrObject *originalRegistry;
    SZrHashKeyValuePair **originalRegistryBuckets;
    TZrSize originalRegistryElementCount;
    TZrSize originalRegistryCapacity;
    TZrSize originalRegistryBucketSize;

    TEST_ASSERT_NOT_NULL(registry);
    TEST_ASSERT_NOT_NULL(retained);
    ZrCore_Object_Init(state, retained);
    ZrCore_Value_InitAsInt(state, &retainedKey, 4901);
    ZrCore_Value_InitAsInt(state, &selfKey, 4902);
    ZrCore_Value_InitAsRawObject(state, &retainedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(retained));
    ZrCore_Object_SetValue(state, retained, &selfKey, &retainedValue);
    ZrCore_Object_SetValue(state, registry, &retainedKey, &retainedValue);
    for (TZrInt64 index = 0; index < 64; index++) {
        SZrObject *extra = ZrCore_Object_New(state, ZR_NULL);
        SZrTypeValue key;
        SZrTypeValue value;
        TEST_ASSERT_NOT_NULL(extra);
        ZrCore_Object_Init(state, extra);
        ZrCore_Value_InitAsInt(state, &key, 5000 + index);
        ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(extra));
        ZrCore_Object_SetValue(state, registry, &key, &value);
    }
    originalRetained = checkpoint_test_lookup_registry_object(state, &retainedKey);
    originalRegistry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    TEST_ASSERT_NOT_NULL(originalRetained);
    TEST_ASSERT_NOT_NULL(originalRegistry);
    originalRegistryBuckets = originalRegistry->nodeMap.buckets;
    originalRegistryElementCount = originalRegistry->nodeMap.elementCount;
    originalRegistryCapacity = originalRegistry->nodeMap.capacity;
    originalRegistryBucketSize = originalRegistry->nodeMap.bucketSize;
    TEST_ASSERT_EQUAL_PTR(originalRetained,
            ZrCore_Object_GetValue(state, originalRetained, &selfKey)->value.object);
    memset(&allocatorContext, 0, sizeof(allocatorContext));
    originalAllocator = global->allocator;
    originalAllocatorUserData = global->userAllocationArguments;
    allocatorContext.upstream = originalAllocator;
    allocatorContext.upstreamUserData = originalAllocatorUserData;
    allocatorContext.failureType = ZR_MEMORY_NATIVE_TYPE_ARRAY;
    allocatorContext.failuresRemaining = 1u;
    global->allocator = checkpoint_fail_allocator;
    global->userAllocationArguments = &allocatorContext;
    createResult = ZrCore_SessionCheckpoint_Create(state, &checkpoint);
    global->allocator = originalAllocator;
    global->userAllocationArguments = originalAllocatorUserData;

    TEST_ASSERT_FALSE(createResult);
    TEST_ASSERT_EQUAL_UINT32(1u, allocatorContext.failuresTriggered);
    TEST_ASSERT_NULL(checkpoint);
    TEST_ASSERT_NOT_NULL(originalRetained);
    TEST_ASSERT_NOT_NULL(originalRegistry);
    retained = originalRetained;
    registry = originalRegistry;
    TEST_ASSERT_EQUAL_PTR(originalRetained, retained);
    TEST_ASSERT_EQUAL_PTR(originalRetained,
            ZrCore_Object_GetValue(state, retained, &selfKey)->value.object);
    TEST_ASSERT_EQUAL_PTR(originalRegistry, registry);
    TEST_ASSERT_EQUAL_PTR(originalRegistryBuckets, registry->nodeMap.buckets);
    TEST_ASSERT_EQUAL_UINT64(originalRegistryElementCount, registry->nodeMap.elementCount);
    TEST_ASSERT_EQUAL_UINT64(originalRegistryCapacity, registry->nodeMap.capacity);
    TEST_ASSERT_EQUAL_UINT64(originalRegistryBucketSize, registry->nodeMap.bucketSize);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
}

static void test_checkpoint_restore_barrier_reservation_failure_is_atomic(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrGarbageCollector *collector = global->garbageCollector;
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    SZrObject *retained = ZrCore_Object_New(state, ZR_NULL);
    SZrTypeValue retainedKey;
    SZrTypeValue selfKey;
    SZrTypeValue retainedValue;
    ZrCheckpointFailAllocatorContext allocatorContext;
    FZrAllocator originalAllocator;
    TZrPtr originalAllocatorUserData;
    TZrSize originalRememberedCapacity;
    TZrSize originalRememberedCount;
    SZrRawObject **originalRememberedObjects;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    TZrBool rollbackResult;
    SZrObject *originalRetained;
    SZrObject *originalRegistry;
    SZrHashKeyValuePair **originalRegistryBuckets;
    TZrSize originalRegistryElementCount;

    TEST_ASSERT_NOT_NULL(collector);
    TEST_ASSERT_NOT_NULL(registry);
    TEST_ASSERT_NOT_NULL(retained);
    ZrCore_Object_Init(state, retained);
    ZrCore_Value_InitAsInt(state, &retainedKey, 4951);
    ZrCore_Value_InitAsInt(state, &selfKey, 4952);
    ZrCore_Value_InitAsRawObject(state, &retainedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(retained));
    ZrCore_Object_SetValue(state, retained, &selfKey, &retainedValue);
    ZrCore_Object_SetValue(state, registry, &retainedKey, &retainedValue);
    for (TZrInt64 index = 0; index < 16; index++) {
        SZrObject *extra = ZrCore_Object_New(state, ZR_NULL);
        SZrTypeValue key;
        SZrTypeValue value;
        TEST_ASSERT_NOT_NULL(extra);
        ZrCore_Object_Init(state, extra);
        ZrCore_Value_InitAsInt(state, &key, 4960 + index);
        ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(extra));
        ZrCore_Object_SetValue(state, registry, &key, &value);
    }
    originalRetained = checkpoint_test_lookup_registry_object(state, &retainedKey);
    originalRegistry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    TEST_ASSERT_NOT_NULL(originalRetained);
    TEST_ASSERT_NOT_NULL(originalRegistry);
    originalRegistryBuckets = originalRegistry->nodeMap.buckets;
    originalRegistryElementCount = originalRegistry->nodeMap.elementCount;
    TEST_ASSERT_EQUAL_PTR(originalRetained,
            ZrCore_Object_GetValue(state, originalRetained, &selfKey)->value.object);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    g_checkpointTestCheckpoint = checkpoint;
    originalRememberedObjects = collector->rememberedObjects;
    originalRememberedCount = collector->rememberedObjectCount;
    originalRememberedCapacity = collector->rememberedObjectCapacity;
    collector->rememberedObjectCapacity = collector->rememberedObjectCount;
    memset(&allocatorContext, 0, sizeof(allocatorContext));
    originalAllocator = global->allocator;
    originalAllocatorUserData = global->userAllocationArguments;
    allocatorContext.upstream = originalAllocator;
    allocatorContext.upstreamUserData = originalAllocatorUserData;
    allocatorContext.failureType = ZR_MEMORY_NATIVE_TYPE_ARRAY;
    allocatorContext.failuresRemaining = 1u;
    global->allocator = checkpoint_fail_allocator;
    global->userAllocationArguments = &allocatorContext;
    rollbackResult = ZrCore_SessionCheckpoint_Rollback(state, checkpoint);
    global->allocator = originalAllocator;
    global->userAllocationArguments = originalAllocatorUserData;
    collector->rememberedObjectCapacity = originalRememberedCapacity;

    TEST_ASSERT_FALSE(rollbackResult);
    TEST_ASSERT_EQUAL_UINT32(1u, allocatorContext.failuresTriggered);
    TEST_ASSERT_NOT_NULL(originalRetained);
    TEST_ASSERT_NOT_NULL(originalRegistry);
    retained = originalRetained;
    registry = originalRegistry;
    TEST_ASSERT_EQUAL_PTR(originalRetained, retained);
    TEST_ASSERT_EQUAL_PTR(originalRetained,
            ZrCore_Object_GetValue(state, retained, &selfKey)->value.object);
    TEST_ASSERT_EQUAL_PTR(originalRegistry, registry);
    TEST_ASSERT_EQUAL_PTR(originalRegistryBuckets, registry->nodeMap.buckets);
    TEST_ASSERT_EQUAL_UINT64(originalRegistryElementCount, registry->nodeMap.elementCount);
    checkpoint_test_assert_remembered_registry_unchanged(collector,
            originalRememberedObjects, originalRememberedCount, originalRememberedCapacity);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
}

static void test_checkpoint_old_to_young_restore_edge_survives_reservation_failure(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrGarbageCollector *collector = global->garbageCollector;
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    SZrObject *parent = ZrCore_Object_New(state, ZR_NULL);
    SZrObject *child = ZrCore_Object_New(state, ZR_NULL);
    SZrTypeValue parentKey;
    SZrTypeValue childKey;
    SZrTypeValue parentValue;
    SZrTypeValue childValue;
    SZrTypeValue replacement;
    const SZrTypeValue *resolved;
    ZrCheckpointFailAllocatorContext allocatorContext;
    FZrAllocator originalAllocator;
    TZrPtr originalAllocatorUserData;
    TZrSize originalRememberedCapacity;
    TZrSize originalRememberedCount;
    SZrRawObject **originalRememberedObjects;
    TZrUInt64 priorMinorCollectionCount;
    SZrObject *originalParent;
    SZrObject *originalChild;
    SZrObject *originalRegistry;
    SZrHashKeyValuePair **originalRegistryBuckets;
    TZrSize originalRegistryElementCount;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    TZrBool rollbackResult;

    TEST_ASSERT_NOT_NULL(collector);
    TEST_ASSERT_NOT_NULL(registry);
    TEST_ASSERT_NOT_NULL(parent);
    TEST_ASSERT_NOT_NULL(child);
    ZrCore_Object_Init(state, parent);
    ZrCore_Object_Init(state, child);
    ZrCore_Value_InitAsInt(state, &parentKey, 4971);
    ZrCore_Value_InitAsInt(state, &childKey, 4972);

    /* Build a real old-to-young edge through the production write barrier. */
    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_RawObject_MarkAsReferenced(ZR_CAST_RAW_OBJECT_AS_SUPER(parent));
    ZrCore_RawObject_SetStorageKind(ZR_CAST_RAW_OBJECT_AS_SUPER(parent),
                                    ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
    ZrCore_RawObject_SetRegionKind(ZR_CAST_RAW_OBJECT_AS_SUPER(parent),
                                   ZR_GARBAGE_COLLECT_REGION_KIND_OLD);
    ZrCore_Value_InitAsRawObject(state, &childValue, ZR_CAST_RAW_OBJECT_AS_SUPER(child));
    ZrCore_Object_SetValue(state, parent, &childKey, &childValue);
    ZrCore_Value_InitAsRawObject(state, &parentValue, ZR_CAST_RAW_OBJECT_AS_SUPER(parent));
    ZrCore_Object_SetValue(state, registry, &parentKey, &parentValue);
    originalParent = checkpoint_test_lookup_registry_object(state, &parentKey);
    originalChild = child;
    originalRegistry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    TEST_ASSERT_NOT_NULL(originalParent);
    TEST_ASSERT_NOT_NULL(originalRegistry);
    originalRegistryBuckets = originalRegistry->nodeMap.buckets;
    originalRegistryElementCount = originalRegistry->nodeMap.elementCount;
    TEST_ASSERT_EQUAL_PTR(originalParent,
            ZrCore_Object_GetValue(state, originalParent, &childKey)->value.object);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(
            global, ZR_CAST_RAW_OBJECT_AS_SUPER(parent)));

    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    g_checkpointTestCheckpoint = checkpoint;
    ZrCore_Value_InitAsInt(state, &replacement, 17);
    ZrCore_Object_SetValue(state, parent, &childKey, &replacement);
    resolved = ZrCore_Object_GetValue(state, parent, &childKey);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_INT64(17, resolved->value.nativeObject.nativeInt64);

    /* Force the final preflight reservation to fail. The current scalar edge
     * and thread boundary must survive; the checkpoint's old-to-young edge
     * must not be partially installed before the fallible step completes. */
    originalRememberedObjects = collector->rememberedObjects;
    originalRememberedCount = collector->rememberedObjectCount;
    originalRememberedCapacity = collector->rememberedObjectCapacity;
    collector->rememberedObjectCapacity = collector->rememberedObjectCount;
    memset(&allocatorContext, 0, sizeof(allocatorContext));
    originalAllocator = global->allocator;
    originalAllocatorUserData = global->userAllocationArguments;
    allocatorContext.upstream = originalAllocator;
    allocatorContext.upstreamUserData = originalAllocatorUserData;
    allocatorContext.failureType = ZR_MEMORY_NATIVE_TYPE_ARRAY;
    allocatorContext.failuresRemaining = 1u;
    global->allocator = checkpoint_fail_allocator;
    global->userAllocationArguments = &allocatorContext;
    rollbackResult = ZrCore_SessionCheckpoint_Rollback(state, checkpoint);
    global->allocator = originalAllocator;
    global->userAllocationArguments = originalAllocatorUserData;
    collector->rememberedObjectCapacity = originalRememberedCapacity;

    TEST_ASSERT_FALSE(rollbackResult);
    TEST_ASSERT_EQUAL_UINT32(1u, allocatorContext.failuresTriggered);
    TEST_ASSERT_NOT_NULL(originalParent);
    TEST_ASSERT_NOT_NULL(originalRegistry);
    parent = originalParent;
    registry = originalRegistry;
    TEST_ASSERT_EQUAL_PTR(originalParent, parent);
    TEST_ASSERT_EQUAL_PTR(originalRegistry, registry);
    TEST_ASSERT_EQUAL_PTR(originalRegistryBuckets, registry->nodeMap.buckets);
    TEST_ASSERT_EQUAL_UINT64(originalRegistryElementCount, registry->nodeMap.elementCount);
    checkpoint_test_assert_remembered_registry_unchanged(collector,
            originalRememberedObjects, originalRememberedCount, originalRememberedCapacity);
    resolved = ZrCore_Object_GetValue(state, parent, &childKey);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_INT64(17, resolved->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);

    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    parent = checkpoint_test_lookup_registry_object(state, &parentKey);
    TEST_ASSERT_NOT_NULL(parent);
    resolved = ZrCore_Object_GetValue(state, parent, &childKey);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_TRUE(resolved->isGarbageCollectable);
    TEST_ASSERT_EQUAL_PTR(originalChild, resolved->value.object);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(
            global, ZR_CAST_RAW_OBJECT_AS_SUPER(parent)));

    /* A real minor collection must keep the restored young descendant alive
     * through the restored old parent edge. */
    collector->gcDebtSize = 4096;
    collector->gcLastStepWork = 0u;
    priorMinorCollectionCount = collector->statsSnapshot.minorCollectionCount;
    ZrCore_GarbageCollector_GcStep(state);
    TEST_ASSERT_GREATER_THAN_UINT64(priorMinorCollectionCount,
                                    collector->statsSnapshot.minorCollectionCount);
    TEST_ASSERT_EQUAL_INT(ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE,
                          collector->collectionPhase);
    parent = checkpoint_test_lookup_registry_object(state, &parentKey);
    TEST_ASSERT_NOT_NULL(parent);
    resolved = ZrCore_Object_GetValue(state, parent, &childKey);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_TRUE(resolved->isGarbageCollectable);
    TEST_ASSERT_NOT_NULL(resolved->value.object);
}

static void test_checkpoint_rejects_malformed_graph_shapes(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    SZrTypeValue key;
    SZrTypeValue value;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    SZrClosureNative *nativeClosure;
    SZrClosure *closure;
    SZrFunction *function;
    SZrHashSet *set;
    TZrBool originalValid;
    TZrSize originalCapacity;
    TZrSize originalBucketSize;
    SZrHashPairPoolBlock *originalActive;
    TZrSize originalChildLength;

    TEST_ASSERT_NOT_NULL(registry);
    ZrCore_Value_InitAsInt(state, &key, 5101);
    nativeClosure = ZrCore_ClosureNative_New(state, 0u);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));
    ZrCore_Object_SetValue(state, registry, &key, &value);
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NULL(checkpoint);

    /* Remove the rejected native closure from the reachable graph before
     * exercising the independent map/function/capture shape validators. */
    ZrCore_Value_ResetAsNull(&value);
    ZrCore_Object_SetValue(state, registry, &key, &value);

    set = &registry->nodeMap;
    originalValid = set->isValid;
    set->isValid = ZR_FALSE;
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NULL(checkpoint);
    set->isValid = originalValid;

    originalCapacity = set->capacity;
    originalBucketSize = set->bucketSize;
    originalActive = set->pairPoolActive;
    set->capacity = 3u;
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NULL(checkpoint);
    set->capacity = originalCapacity;
    set->bucketSize = 0u;
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NULL(checkpoint);
    set->bucketSize = originalBucketSize;
    if (set->pairPoolCapacity > 0u) {
        set->pairPoolActive = ZR_NULL;
        TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
        TEST_ASSERT_NULL(checkpoint);
        set->pairPoolActive = originalActive;
    }

    ZrCore_Value_InitAsInt(state, &key, 5102);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    ZrCore_Object_SetValue(state, registry, &key, &value);
    originalChildLength = function->childFunctionLength;
    function->childFunctionLength = 1u;
    function->childFunctionList = ZR_NULL;
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NULL(checkpoint);
    function->childFunctionLength = originalChildLength;
    function->childFunctionLength = (TZrSize)-1;
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NULL(checkpoint);
    function->childFunctionLength = originalChildLength;

    ZrCore_Value_InitAsInt(state, &key, 5103);
    closure = ZrCore_Closure_New(state, 1u);
    TEST_ASSERT_NOT_NULL(closure);
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    ZrCore_Object_SetValue(state, registry, &key, &value);
    closure->closureValuesExtend[0] = ZR_NULL;
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NULL(checkpoint);
    closure->closureValueCount = 0u;
    closure->closureValueCount = (TZrSize)-1;
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NULL(checkpoint);
    closure->closureValueCount = 0u;
}

static void test_checkpoint_rejects_pair_pool_cycle_and_yield_counter(void) {
    SZrState *state = checkpoint_test_state();
    SZrObject *registry = ZR_CAST_OBJECT(state, state->global->loadedModulesRegistry.value.object);
    SZrObject *items;
    SZrTypeValue itemsKey;
    TZrSize originalElementCount;
    SZrHashPairPoolBlock *block;
    SZrHashSet *set;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    SZrHashPairPoolBlock *originalNext;

    TEST_ASSERT_NOT_NULL(registry);
    ZrCore_Value_InitAsInt(state, &itemsKey, 5201);
    items = checkpoint_test_make_mixed_array(state, registry, 5201, 5202);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    g_checkpointTestCheckpoint = checkpoint;
    items = checkpoint_test_lookup_registry_object(state, &itemsKey);
    TEST_ASSERT_NOT_NULL(items);
    set = &items->nodeMap;
    block = set->pairPoolHead;
    TEST_ASSERT_NOT_NULL(block);
    originalElementCount = set->elementCount;
    originalNext = block->next;
    block->next = block;
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    block->next = originalNext;

    set->elementCount = originalElementCount + 1u;
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    set->elementCount = originalElementCount;

    state->nestedNativeCalls = 1u;
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    TEST_ASSERT_EQUAL_UINT32(1u, state->nestedNativeCalls);
    state->nestedNativeCalls = 0u;
    state->nestedNativeCallYieldFlag = 1u;
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    TEST_ASSERT_EQUAL_UINT32(1u, state->nestedNativeCallYieldFlag);
    state->nestedNativeCallYieldFlag = 0u;
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
}

static void test_checkpoint_rejects_active_call_frame_and_receiver_boundary(void) {
    SZrState *state = checkpoint_test_state();
    SZrCallInfo *activeCallInfo;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    TZrStackValuePointer originalFunctionBase;
    TZrStackValuePointer originalFunctionTop;
    TZrStackValuePointer originalStackTop;
    TZrSize originalCallInfoListLength;
    SZrTypeValue originalStackValue;
    TZrStackValuePointer originalReturnDestination;
    TZrStackPointer originalArgumentSourceFrameBase;
    TZrBool originalHasReturnDestination;
    TZrBool originalHasArgumentSourceFrame;
    TZrUInt32 originalAotGcRootFrameDepth;

    activeCallInfo = ZrCore_CallInfo_Extend(state);
    TEST_ASSERT_NOT_NULL(activeCallInfo);
    originalFunctionBase = activeCallInfo->functionBase.valuePointer;
    originalFunctionTop = activeCallInfo->functionTop.valuePointer;
    originalStackTop = state->stackTop.valuePointer;
    originalCallInfoListLength = state->callInfoListLength;
    originalStackValue = state->stackBase.valuePointer->value;
    originalReturnDestination = activeCallInfo->returnDestination;
    originalArgumentSourceFrameBase = activeCallInfo->argumentSourceFrameBase;
    originalHasReturnDestination = activeCallInfo->hasReturnDestination;
    originalHasArgumentSourceFrame = activeCallInfo->hasArgumentSourceFrame;
    originalAotGcRootFrameDepth = state->aotGcRootFrameDepth;
    activeCallInfo->functionBase.valuePointer = state->stackBase.valuePointer + 1;
    activeCallInfo->functionTop.valuePointer = state->stackTop.valuePointer + 1;
    state->callInfoList = activeCallInfo;
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NULL(checkpoint);
    TEST_ASSERT_EQUAL_PTR(activeCallInfo, state->callInfoList);
    TEST_ASSERT_EQUAL_PTR(originalStackTop, state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_UINT64(originalCallInfoListLength, state->callInfoListLength);
    TEST_ASSERT_EQUAL_MEMORY(&originalStackValue,
                              &state->stackBase.valuePointer->value,
                              sizeof(originalStackValue));

    /* Keep the frame itself in bounds, then exercise the production
     * return-destination guard independently. */
    activeCallInfo->functionBase.valuePointer = originalFunctionBase;
    activeCallInfo->functionTop.valuePointer = originalFunctionTop;
    activeCallInfo->hasReturnDestination = ZR_TRUE;
    activeCallInfo->returnDestination = state->stackTail.valuePointer;
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NULL(checkpoint);
    TEST_ASSERT_EQUAL_PTR(activeCallInfo, state->callInfoList);
    TEST_ASSERT_EQUAL_PTR(originalStackTop, state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_UINT64(originalCallInfoListLength, state->callInfoListLength);
    TEST_ASSERT_EQUAL_MEMORY(&originalStackValue,
                              &state->stackBase.valuePointer->value,
                              sizeof(originalStackValue));

    /* The argument-source frame must also point into the live stack. */
    activeCallInfo->hasReturnDestination = originalHasReturnDestination;
    activeCallInfo->returnDestination = originalReturnDestination;
    activeCallInfo->hasArgumentSourceFrame = ZR_TRUE;
    activeCallInfo->argumentSourceFrameBase.valuePointer = ZR_NULL;
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NULL(checkpoint);
    TEST_ASSERT_EQUAL_PTR(activeCallInfo, state->callInfoList);
    TEST_ASSERT_EQUAL_PTR(originalStackTop, state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_UINT64(originalCallInfoListLength, state->callInfoListLength);
    TEST_ASSERT_EQUAL_MEMORY(&originalStackValue,
                              &state->stackBase.valuePointer->value,
                              sizeof(originalStackValue));

    /* AOT root-frame depth is a separate create-boundary guard and must not
     * be masked by an otherwise valid call-info chain. */
    activeCallInfo->hasArgumentSourceFrame = originalHasArgumentSourceFrame;
    activeCallInfo->argumentSourceFrameBase = originalArgumentSourceFrameBase;
    state->callInfoList = &state->baseCallInfo;
    state->aotGcRootFrameDepth = 1u;
    TEST_ASSERT_FALSE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NULL(checkpoint);
    TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
    TEST_ASSERT_EQUAL_PTR(originalStackTop, state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_UINT64(originalCallInfoListLength, state->callInfoListLength);
    TEST_ASSERT_EQUAL_MEMORY(&originalStackValue,
                              &state->stackBase.valuePointer->value,
                              sizeof(originalStackValue));

    activeCallInfo->hasReturnDestination = originalHasReturnDestination;
    activeCallInfo->returnDestination = originalReturnDestination;
    activeCallInfo->hasArgumentSourceFrame = originalHasArgumentSourceFrame;
    activeCallInfo->argumentSourceFrameBase = originalArgumentSourceFrameBase;
    state->aotGcRootFrameDepth = originalAotGcRootFrameDepth;
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NOT_NULL(checkpoint);
    ZrCore_SessionCheckpoint_Free(state, checkpoint);
    checkpoint = ZR_NULL;
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_checkpoint_roundtrips_ordinary_object_set_value_map);
    RUN_TEST(test_checkpoint_roundtrips_module_pub_and_pro_exports);
    RUN_TEST(test_checkpoint_roundtrips_mixed_standalone_and_reserved_map);
    RUN_TEST(test_checkpoint_roundtrips_removed_reserved_pool_slot);
    RUN_TEST(test_checkpoint_restores_cycles_aliases_and_loaded_module_registry);
    RUN_TEST(test_checkpoint_restores_module_identity_descriptors_runtime_and_function_constants);
    RUN_TEST(test_checkpoint_rejects_zero_length_inline_layout_objects);
    RUN_TEST(test_checkpoint_restore_preparation_failure_keeps_live_array_and_map);
    RUN_TEST(test_checkpoint_create_rejects_live_extra_stack_value);
    RUN_TEST(test_checkpoint_create_rejects_pending_object_value);
    RUN_TEST(test_checkpoint_rejects_pair_pool_free_size_overflow);
    RUN_TEST(test_checkpoint_restores_prototype_layout_and_function_caches);
    RUN_TEST(test_checkpoint_restores_decorator_metadata_scalar_and_object);
    RUN_TEST(test_checkpoint_create_allocator_failure_preserves_live_graph);
    RUN_TEST(test_checkpoint_restore_barrier_reservation_failure_is_atomic);
    RUN_TEST(test_checkpoint_old_to_young_restore_edge_survives_reservation_failure);
    RUN_TEST(test_checkpoint_rejects_malformed_graph_shapes);
    RUN_TEST(test_checkpoint_rejects_pair_pool_cycle_and_yield_counter);
    RUN_TEST(test_checkpoint_rejects_active_call_frame_and_receiver_boundary);
    return UNITY_END();
}
