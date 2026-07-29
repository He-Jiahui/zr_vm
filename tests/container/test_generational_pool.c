#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "unity.h"

#include "container_test_common.h"
#include "zr_vm_lib_container/generational_pool.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/parser.h"

typedef struct STestElement {
    int value;
    int marker;
} STestElement;

typedef struct STestCallbacks {
    uint64_t initializeCount;
    uint64_t dropCount;
    uint64_t scanCount;
} STestCallbacks;

typedef struct SConcurrentWorker {
    SZrPool *pool;
    int workerId;
    TZrSize iterationCount;
    int failureStep;
} SConcurrentWorker;

static TZrBool test_element_initialize(
        void *destination,
        const void *source,
        void *context) {
    STestCallbacks *callbacks = (STestCallbacks *)context;
    const STestElement *input = (const STestElement *)source;

    callbacks->initializeCount++;
    if (input == ZR_NULL || input->value < 0) {
        return ZR_FALSE;
    }
    memcpy(destination, input, sizeof(*input));
    return ZR_TRUE;
}

static void test_element_drop(void *element, void *context) {
    STestCallbacks *callbacks = (STestCallbacks *)context;
    STestElement *value = (STestElement *)element;

    callbacks->dropCount++;
    value->value = -1;
    value->marker = -1;
}

static void test_element_scan(void *element, void *context) {
    STestCallbacks *callbacks = (STestCallbacks *)context;
    STestElement *value = (STestElement *)element;

    TEST_ASSERT_EQUAL_INT(0x5a5a, value->marker);
    callbacks->scanCount++;
}

static SZrPoolTypeLayout test_layout(
        STestCallbacks *callbacks,
        EZrPoolGcScanKind scanKind,
        TZrSize alignment) {
    SZrPoolTypeLayout layout;

    memset(&layout, 0, sizeof(layout));
    layout.elementSize = sizeof(STestElement);
    layout.elementAlignment = alignment;
    layout.gcScanKind = scanKind;
    layout.initialize = test_element_initialize;
    layout.drop = test_element_drop;
    layout.scan = scanKind == ZR_POOL_GC_SCAN_FREE
                          ? ZR_NULL
                          : test_element_scan;
    layout.context = callbacks;
    return layout;
}

static SZrPoolConfig test_config(TZrSize slabCapacity, uint64_t generationLimit) {
    SZrPoolConfig config;

    config.slabCapacity = slabCapacity;
    config.generationLimit = generationLimit;
    config.concurrencyMode = ZR_POOL_CONCURRENCY_THREAD_LOCAL;
    return config;
}

static const ZrLibTypeDescriptor *find_pooling_type(
        const ZrLibModuleDescriptor *module,
        const TZrChar *name) {
    for (TZrSize index = 0u; index < module->typeCount; index++) {
        if (strcmp(module->types[index].name, name) == 0) {
            return &module->types[index];
        }
    }
    return ZR_NULL;
}

static const ZrLibMethodDescriptor *find_pooling_method(
        const ZrLibTypeDescriptor *type,
        TZrUInt32 role) {
    for (TZrSize index = 0u; index < type->methodCount; index++) {
        if (type->methods[index].contractRole == role) {
            return &type->methods[index];
        }
    }
    return ZR_NULL;
}

static SZrAstNode *parse_pooling_source(
        SZrState *state,
        const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_Create(
            state,
            (TZrNativeString)"generational_pool_metadata.zr",
            strlen("generational_pool_metadata.zr"));

    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrParser_Parse(state, source, strlen(source), sourceName);
}

static void test_native_descriptor_publishes_stable_slot_contract_by_role(void) {
    const ZrLibModuleDescriptor *module =
            ZrVmLibContainer_GetPoolingModuleDescriptor();
    const ZrLibTypeDescriptor *pool;
    const ZrLibTypeDescriptor *handle;
    const ZrLibTypeDescriptor *writeRef;
    const ZrLibTypeDescriptor *readRef;
    const ZrLibMethodDescriptor *tryRead;
    const ZrLibMethodDescriptor *tryBorrow;

    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_EQUAL_STRING("zr.pooling", module->moduleName);
    TEST_ASSERT_EQUAL_UINT64(1u, module->constantCount);
    TEST_ASSERT_EQUAL_UINT64(
            ZR_POOL_STABLE_SLOT_CONTRACT_HASH,
            (uint64_t)module->constants[0].intValue);
    pool = find_pooling_type(module, "Pool");
    handle = find_pooling_type(module, "PoolHandle");
    writeRef = find_pooling_type(module, "PoolRef");
    readRef = find_pooling_type(module, "PoolReadRef");
    TEST_ASSERT_NOT_NULL(pool);
    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_NOT_NULL(writeRef);
    TEST_ASSERT_NOT_NULL(readRef);
    TEST_ASSERT_BITS_HIGH(
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_STABLE_SLOT_SOURCE),
            pool->protocolMask);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, handle->prototypeType);
    TEST_ASSERT_EQUAL_UINT64(3u, handle->fieldCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_HANDLE_POOL_ID,
            handle->fields[0].contractRole);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_HANDLE_SLOT,
            handle->fields[1].contractRole);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_HANDLE_GENERATION,
            handle->fields[2].contractRole);
    TEST_ASSERT_NOT_NULL(find_pooling_method(
            pool, ZR_MEMBER_CONTRACT_ROLE_POOL_DELIVER));
    TEST_ASSERT_NOT_NULL(find_pooling_method(
            pool, ZR_MEMBER_CONTRACT_ROLE_POOL_VALIDATE));
    TEST_ASSERT_NOT_NULL(find_pooling_method(
            pool, ZR_MEMBER_CONTRACT_ROLE_POOL_RECYCLE));
    tryRead = find_pooling_method(
            pool, ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_READ);
    tryBorrow = find_pooling_method(
            pool, ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_WRITE);
    TEST_ASSERT_NOT_NULL(tryRead);
    TEST_ASSERT_NOT_NULL(tryBorrow);
    TEST_ASSERT_EQUAL_STRING("bool", tryRead->returnTypeName);
    TEST_ASSERT_EQUAL_STRING("bool", tryBorrow->returnTypeName);
    TEST_ASSERT_EQUAL_UINT64(2u, tryRead->parameterCount);
    TEST_ASSERT_EQUAL_UINT64(2u, tryBorrow->parameterCount);
    TEST_ASSERT_EQUAL_UINT16(2u, tryRead->minArgumentCount);
    TEST_ASSERT_EQUAL_UINT16(2u, tryRead->maxArgumentCount);
    TEST_ASSERT_EQUAL_UINT16(2u, tryBorrow->minArgumentCount);
    TEST_ASSERT_EQUAL_UINT16(2u, tryBorrow->maxArgumentCount);
    TEST_ASSERT_EQUAL_STRING("handle", tryRead->parameters[0].name);
    TEST_ASSERT_EQUAL_STRING("view", tryRead->parameters[1].name);
    TEST_ASSERT_EQUAL_STRING("PoolReadRef<T>", tryRead->parameters[1].typeName);
    TEST_ASSERT_EQUAL_STRING("handle", tryBorrow->parameters[0].name);
    TEST_ASSERT_EQUAL_STRING("view", tryBorrow->parameters[1].name);
    TEST_ASSERT_EQUAL_STRING("PoolRef<T>", tryBorrow->parameters[1].typeName);
    TEST_ASSERT_BITS_HIGH(
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE), writeRef->protocolMask);
    TEST_ASSERT_BITS_HIGH(
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE), readRef->protocolMask);
    TEST_ASSERT_FALSE(writeRef->allowBoxedConstruction);
    TEST_ASSERT_FALSE(readRef->allowBoxedConstruction);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_REF_PROJECTION,
            writeRef->fields[0].contractRole);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_RELEASE,
            writeRef->methods[0].contractRole);
}

static void test_native_semantic_import_preserves_ref_like_and_stable_slot_protocols(void) {
    static const TZrChar *source =
            "var {Pool, PoolHandle, PoolRef, PoolReadRef} = "
            "%import(\"zr.pooling\");\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrCompilerState *compiler;
    SZrAstNode *ast;
    const SZrTypePrototypeInfo *pool;
    const SZrTypePrototypeInfo *handle;
    const SZrTypePrototypeInfo *writeRef;
    const SZrTypePrototypeInfo *readRef;
    const SZrTypeMemberInfo *tryRead;
    const SZrTypeMemberInfo *tryBorrow;
    const SZrTypeMemberInfo *valueProjection;

    TEST_ASSERT_NOT_NULL(state);
    compiler = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(compiler);
    ast = parse_pooling_source(state, source);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    compiler->scriptAst = ast;
    compiler->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(compiler->currentFunction);
    for (TZrSize index = 0u;
         index < ast->data.script.statements->count;
         index++) {
        ZrContainerTests_CompileTopLevelStatement(
                compiler, ast->data.script.statements->nodes[index]);
        TEST_ASSERT_FALSE(compiler->hasError);
    }
    pool = ZrContainerTests_FindTypePrototype(compiler, "Pool");
    handle = ZrContainerTests_FindTypePrototype(compiler, "PoolHandle");
    writeRef = ZrContainerTests_FindTypePrototype(compiler, "PoolRef");
    readRef = ZrContainerTests_FindTypePrototype(compiler, "PoolReadRef");
    TEST_ASSERT_NOT_NULL(pool);
    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_NOT_NULL(writeRef);
    TEST_ASSERT_NOT_NULL(readRef);
    TEST_ASSERT_TRUE(
            (pool->protocolMask &
             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_STABLE_SLOT_SOURCE)) != 0u);
    TEST_ASSERT_TRUE(
            (writeRef->modifierFlags & ZR_DECLARATION_MODIFIER_REF_LIKE) != 0u);
    TEST_ASSERT_TRUE(
            (readRef->modifierFlags & ZR_DECLARATION_MODIFIER_REF_LIKE) != 0u);
    TEST_ASSERT_FALSE(writeRef->allowBoxedConstruction);
    TEST_ASSERT_FALSE(readRef->allowBoxedConstruction);
    TEST_ASSERT_EQUAL_UINT64(3u, handle->members.length);
    tryRead = ZrContainerTests_FindTypeMemberByName(pool, "tryRead");
    tryBorrow = ZrContainerTests_FindTypeMemberByName(pool, "tryBorrow");
    valueProjection = ZrContainerTests_FindTypeMemberByName(writeRef, "value");
    TEST_ASSERT_NOT_NULL(tryRead);
    TEST_ASSERT_NOT_NULL(tryBorrow);
    TEST_ASSERT_NOT_NULL(valueProjection);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_READ,
            tryRead->contractRole);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_WRITE,
            tryBorrow->contractRole);
    TEST_ASSERT_EQUAL_INT(
            ZR_MEMBER_CONTRACT_ROLE_POOL_REF_PROJECTION,
            valueProjection->contractRole);
    TEST_ASSERT_EQUAL_UINT32(2u, tryRead->parameterCount);
    TEST_ASSERT_EQUAL_UINT32(2u, tryBorrow->parameterCount);
    TEST_ASSERT_EQUAL_STRING(
            "bool", ZrCore_String_GetNativeString(tryRead->returnTypeName));
    TEST_ASSERT_EQUAL_STRING(
            "bool", ZrCore_String_GetNativeString(tryBorrow->returnTypeName));
    TEST_ASSERT_TRUE(tryRead->parameterPassingModes.isValid);
    TEST_ASSERT_TRUE(tryBorrow->parameterPassingModes.isValid);
    TEST_ASSERT_EQUAL_UINT64(2u, tryRead->parameterPassingModes.length);
    TEST_ASSERT_EQUAL_UINT64(2u, tryBorrow->parameterPassingModes.length);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARAMETER_PASSING_MODE_VALUE,
            *(const EZrParameterPassingMode *)ZrCore_Array_Get(
                    (SZrArray *)&tryRead->parameterPassingModes, 0u));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARAMETER_PASSING_MODE_OUT,
            *(const EZrParameterPassingMode *)ZrCore_Array_Get(
                    (SZrArray *)&tryRead->parameterPassingModes, 1u));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARAMETER_PASSING_MODE_VALUE,
            *(const EZrParameterPassingMode *)ZrCore_Array_Get(
                    (SZrArray *)&tryBorrow->parameterPassingModes, 0u));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARAMETER_PASSING_MODE_OUT,
            *(const EZrParameterPassingMode *)ZrCore_Array_Get(
                    (SZrArray *)&tryBorrow->parameterPassingModes, 1u));

    ZrCore_Function_Free(state, compiler->currentFunction);
    compiler->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, ast);
    ZrContainerTests_DestroyCompilerState(compiler);
    ZrContainerTests_DestroyState(state);
}

static void test_identity_recycle_and_retirement_preserve_guarded_value(void) {
    STestCallbacks callbacks = {0};
    SZrPoolTypeLayout layout = test_layout(
            &callbacks, ZR_POOL_GC_SCAN_MAPPED, 16u);
    SZrPoolConfig config = test_config(1u, UINT64_MAX);
    SZrPool *pool = ZR_NULL;
    SZrPool *otherPool = ZR_NULL;
    SZrPoolHandle first;
    SZrPoolHandle second;
    SZrPoolGuard readGuard = {0};
    STestElement firstValue = {11, 0x5a5a};
    STestElement secondValue = {22, 0x5a5a};

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &otherPool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &firstValue, &first));
    TEST_ASSERT_EQUAL_UINT64(1u, first.generation);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_WRONG_POOL, ZrPool_Validate(otherPool, first));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_TryRead(pool, first, &readGuard));
    TEST_ASSERT_EQUAL_INT(
            11,
            ((const STestElement *)ZrPoolGuard_ReadOnlyValue(&readGuard))->value);

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Recycle(pool, first));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_ENTITY_RETIRED, ZrPool_Validate(pool, first));
    TEST_ASSERT_EQUAL_INT(
            11,
            ((const STestElement *)ZrPoolGuard_ReadOnlyValue(&readGuard))->value);
    {
        SZrPoolGuard rejected = {0};
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_ENTITY_RETIRED,
                ZrPool_TryRead(pool, first, &rejected));
        TEST_ASSERT_FALSE(rejected.active);
    }

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&readGuard));
    TEST_ASSERT_EQUAL_UINT64(1u, callbacks.dropCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &secondValue, &second));
    TEST_ASSERT_EQUAL_UINT64(
            (uint64_t)first.slotIndex, (uint64_t)second.slotIndex);
    TEST_ASSERT_EQUAL_UINT64(first.generation + 1u, second.generation);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_HANDLE_STALE, ZrPool_Validate(pool, first));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_HANDLE_STALE, ZrPool_Recycle(pool, first));

    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&otherPool));
    TEST_ASSERT_NULL(pool);
    TEST_ASSERT_NULL(otherPool);
    TEST_ASSERT_EQUAL_UINT64(2u, callbacks.dropCount);
}

static void test_reader_writer_conflicts_and_guard_release_are_deterministic(void) {
    STestCallbacks callbacks = {0};
    SZrPoolTypeLayout layout = test_layout(
            &callbacks, ZR_POOL_GC_SCAN_MAPPED, 8u);
    SZrPoolConfig config = test_config(4u, UINT64_MAX);
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle handle;
    SZrPoolGuard firstRead = {0};
    SZrPoolGuard secondRead = {0};
    SZrPoolGuard writer = {0};
    STestElement value = {7, 0x5a5a};

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &value, &handle));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_TryRead(pool, handle, &firstRead));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_TryRead(pool, handle, &secondRead));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_BORROW_CONFLICT,
            ZrPool_TryBorrow(pool, handle, &writer));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&firstRead));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&secondRead));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_TryBorrow(pool, handle, &writer));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_BORROW_CONFLICT,
            ZrPool_TryRead(pool, handle, &firstRead));
    ((STestElement *)ZrPoolGuard_Value(&writer))->value = 9;
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&writer));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_INVALID_ARGUMENT, ZrPoolGuard_Release(&writer));

    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
}

static void test_pool_destroy_retires_live_slots_and_waits_for_active_guard(void) {
    STestCallbacks callbacks = {0};
    SZrPoolTypeLayout layout = test_layout(
            &callbacks, ZR_POOL_GC_SCAN_MAPPED, 8u);
    SZrPoolConfig config = test_config(2u, UINT64_MAX);
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle handle;
    SZrPoolGuard guard = {0};
    STestElement value = {31, 0x5a5a};

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &value, &handle));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_TryBorrow(pool, handle, &guard));
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_POOL_BUSY, ZrPool_Destroy(&pool));
    TEST_ASSERT_NOT_NULL(pool);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_POOL_DESTROYED, ZrPool_Validate(pool, handle));
    TEST_ASSERT_EQUAL_INT(
            31, ((STestElement *)ZrPoolGuard_Value(&guard))->value);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&guard));
    TEST_ASSERT_EQUAL_UINT64(1u, callbacks.dropCount);
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    TEST_ASSERT_NULL(pool);
}

static void test_construction_failure_and_generation_exhaustion_never_publish_aliases(void) {
    STestCallbacks callbacks = {0};
    SZrPoolTypeLayout layout = test_layout(
            &callbacks, ZR_POOL_GC_SCAN_FREE, 8u);
    SZrPoolConfig config = test_config(1u, 2u);
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle first = {0};
    SZrPoolHandle second = {0};
    SZrPoolHandle third = {0};
    SZrPoolStats stats;
    STestElement rejected = {-1, 0x5a5a};
    STestElement accepted = {1, 0x5a5a};
    uint64_t scannedSlots = UINT64_MAX;
    uint64_t scannedBytes = UINT64_MAX;

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_CONSTRUCTION_FAILED,
            ZrPool_Deliver(pool, &rejected, &first));
    TEST_ASSERT_EQUAL_UINT64(0u, first.poolId);

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &accepted, &first));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Recycle(pool, first));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &accepted, &second));
    TEST_ASSERT_EQUAL_UINT64(2u, second.generation);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Recycle(pool, second));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &accepted, &third));
    TEST_ASSERT_NOT_EQUAL(first.slotIndex, third.slotIndex);
    TEST_ASSERT_EQUAL_UINT64(1u, third.generation);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_HANDLE_STALE, ZrPool_Validate(pool, second));

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(pool, &stats));
    TEST_ASSERT_EQUAL_UINT64(1u, (uint64_t)stats.exhaustedCount);
    TEST_ASSERT_EQUAL_UINT64(1u, stats.reuseCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Scan(pool, &scannedSlots, &scannedBytes));
    TEST_ASSERT_EQUAL_UINT64(0u, scannedSlots);
    TEST_ASSERT_EQUAL_UINT64(0u, scannedBytes);
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
}

static void test_slabs_preserve_alignment_and_scan_only_initialized_slots(void) {
    STestCallbacks callbacks = {0};
    SZrPoolTypeLayout layout = test_layout(
            &callbacks, ZR_POOL_GC_SCAN_BARRIERED, 64u);
    SZrPoolConfig config = test_config(2u, UINT64_MAX);
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle handles[5];
    SZrPoolGuard guard = {0};
    SZrPoolStats stats;
    uint64_t scannedSlots = 0u;
    uint64_t scannedBytes = 0u;

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    for (int index = 0; index < 5; index++) {
        STestElement value = {index, 0x5a5a};
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK,
                ZrPool_Deliver(pool, &value, &handles[index]));
    }
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_TryRead(pool, handles[4], &guard));
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            (uint64_t)(uintptr_t)ZrPoolGuard_ReadOnlyValue(&guard) % 64u);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&guard));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Scan(pool, &scannedSlots, &scannedBytes));
    TEST_ASSERT_EQUAL_UINT64(5u, scannedSlots);
    TEST_ASSERT_EQUAL_UINT64(5u * sizeof(STestElement), scannedBytes);
    TEST_ASSERT_EQUAL_UINT64(5u, callbacks.scanCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(pool, &stats));
    TEST_ASSERT_EQUAL_UINT64(3u, (uint64_t)stats.slabCount);
    TEST_ASSERT_EQUAL_UINT64(6u, (uint64_t)stats.slotCount);
    TEST_ASSERT_EQUAL_UINT64(5u, (uint64_t)stats.liveCount);
    TEST_ASSERT_EQUAL_UINT64(1u, (uint64_t)stats.freeCount);

    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    TEST_ASSERT_EQUAL_UINT64(5u, callbacks.dropCount);
}

static void test_million_handle_validation_and_rejection_stress(void) {
    const TZrSize handleCount = 1000000u;
    STestCallbacks callbacks = {0};
    SZrPoolTypeLayout layout = test_layout(
            &callbacks, ZR_POOL_GC_SCAN_FREE, 8u);
    SZrPoolConfig config = test_config(4096u, UINT64_MAX);
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle *handles = (SZrPoolHandle *)malloc(
            sizeof(*handles) * handleCount);
    STestElement value = {42, 0x5a5a};
    SZrPoolStats stats;

    TEST_ASSERT_NOT_NULL(handles);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    for (TZrSize index = 0u; index < handleCount; index++) {
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK,
                ZrPool_Deliver(pool, &value, &handles[index]));
    }
    for (TZrSize index = 0u; index < handleCount; index++) {
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK,
                ZrPool_Validate(pool, handles[index]));
    }
    for (TZrSize index = 0u; index < handleCount; index += 2u) {
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK,
                ZrPool_Recycle(pool, handles[index]));
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_HANDLE_STALE,
                ZrPool_Validate(pool, handles[index]));
    }
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(pool, &stats));
    TEST_ASSERT_EQUAL_UINT64(
            (uint64_t)(handleCount / 2u), (uint64_t)stats.liveCount);
    TEST_ASSERT_EQUAL_UINT64(
            (uint64_t)(stats.slotCount - stats.liveCount),
            (uint64_t)stats.freeCount);
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    TEST_ASSERT_EQUAL_UINT64(handleCount, callbacks.dropCount);
    free(handles);
}

static void run_concurrent_worker(SConcurrentWorker *worker) {
    for (TZrSize iteration = 0u;
         iteration < worker->iterationCount;
         iteration++) {
        int value = worker->workerId;
        SZrPoolHandle handle;
        SZrPoolGuard guard = {0};

        if (ZrPool_Deliver(worker->pool, &value, &handle) !=
            ZR_POOL_STATUS_OK) {
            worker->failureStep = 1;
            return;
        }
        if (ZrPool_TryBorrow(worker->pool, handle, &guard) !=
            ZR_POOL_STATUS_OK) {
            worker->failureStep = 2;
            return;
        }
        (*(int *)ZrPoolGuard_Value(&guard))++;
        if (ZrPoolGuard_Release(&guard) != ZR_POOL_STATUS_OK) {
            worker->failureStep = 3;
            return;
        }
        if (ZrPool_TryRead(worker->pool, handle, &guard) !=
            ZR_POOL_STATUS_OK) {
            worker->failureStep = 4;
            return;
        }
        if (*(const int *)ZrPoolGuard_ReadOnlyValue(&guard) !=
            worker->workerId + 1) {
            worker->failureStep = 5;
            return;
        }
        if (ZrPoolGuard_Release(&guard) != ZR_POOL_STATUS_OK) {
            worker->failureStep = 6;
            return;
        }
        if (ZrPool_Recycle(worker->pool, handle) != ZR_POOL_STATUS_OK) {
            worker->failureStep = 7;
            return;
        }
        if (ZrPool_Validate(worker->pool, handle) !=
            ZR_POOL_STATUS_HANDLE_STALE) {
            worker->failureStep = 8;
            return;
        }
    }
}

#if defined(_WIN32)
static DWORD WINAPI concurrent_worker_entry(LPVOID parameter) {
    run_concurrent_worker((SConcurrentWorker *)parameter);
    return 0u;
}
#else
static void *concurrent_worker_entry(void *parameter) {
    run_concurrent_worker((SConcurrentWorker *)parameter);
    return ZR_NULL;
}
#endif

static void test_concurrent_pool_serializes_state_without_charging_thread_local_mode(void) {
    enum {
        workerCount = 4,
        iterationsPerWorker = 25000
    };
    SZrPoolTypeLayout layout = {0};
    SZrPoolConfig config = test_config(64u, UINT64_MAX);
    SZrPool *pool = ZR_NULL;
    SConcurrentWorker workers[workerCount];
    SZrPoolStats stats;

    layout.elementSize = sizeof(int);
    layout.elementAlignment = sizeof(int);
    layout.gcScanKind = ZR_POOL_GC_SCAN_FREE;
    config.concurrencyMode = ZR_POOL_CONCURRENCY_CONCURRENT;
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    for (int index = 0; index < workerCount; index++) {
        workers[index].pool = pool;
        workers[index].workerId = index;
        workers[index].iterationCount = iterationsPerWorker;
        workers[index].failureStep = 0;
    }

#if defined(_WIN32)
    {
        HANDLE threads[workerCount];

        for (int index = 0; index < workerCount; index++) {
            threads[index] = CreateThread(
                    ZR_NULL,
                    0u,
                    concurrent_worker_entry,
                    &workers[index],
                    0u,
                    ZR_NULL);
            TEST_ASSERT_NOT_NULL(threads[index]);
        }
        TEST_ASSERT_EQUAL_UINT32(
                WAIT_OBJECT_0,
                WaitForMultipleObjects(workerCount, threads, TRUE, INFINITE));
        for (int index = 0; index < workerCount; index++) {
            CloseHandle(threads[index]);
        }
    }
#else
    {
        pthread_t threads[workerCount];

        for (int index = 0; index < workerCount; index++) {
            TEST_ASSERT_EQUAL_INT(
                    0,
                    pthread_create(
                            &threads[index],
                            ZR_NULL,
                            concurrent_worker_entry,
                            &workers[index]));
        }
        for (int index = 0; index < workerCount; index++) {
            TEST_ASSERT_EQUAL_INT(0, pthread_join(threads[index], ZR_NULL));
        }
    }
#endif

    for (int index = 0; index < workerCount; index++) {
        TEST_ASSERT_EQUAL_INT(0, workers[index].failureStep);
    }
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(pool, &stats));
    TEST_ASSERT_EQUAL_UINT64(
            (uint64_t)workerCount * iterationsPerWorker,
            stats.deliverCount);
    TEST_ASSERT_EQUAL_UINT64(stats.deliverCount, stats.recycleCount);
    TEST_ASSERT_EQUAL_UINT64(stats.deliverCount, stats.dropCount);
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)stats.liveCount);
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)stats.retiredCount);
    TEST_ASSERT_EQUAL_UINT64(
            (uint64_t)stats.slotCount, (uint64_t)stats.freeCount);
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_native_descriptor_publishes_stable_slot_contract_by_role);
    RUN_TEST(test_native_semantic_import_preserves_ref_like_and_stable_slot_protocols);
    RUN_TEST(test_identity_recycle_and_retirement_preserve_guarded_value);
    RUN_TEST(test_reader_writer_conflicts_and_guard_release_are_deterministic);
    RUN_TEST(test_pool_destroy_retires_live_slots_and_waits_for_active_guard);
    RUN_TEST(test_construction_failure_and_generation_exhaustion_never_publish_aliases);
    RUN_TEST(test_slabs_preserve_alignment_and_scan_only_initialized_slots);
    RUN_TEST(test_million_handle_validation_and_rejection_stress);
    RUN_TEST(test_concurrent_pool_serializes_state_without_charging_thread_local_mode);
    return UNITY_END();
}
