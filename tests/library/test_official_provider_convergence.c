#include "unity.h"

#include "runtime_support.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_debug/module.h"
#include "zr_vm_lib_iteration/module.h"
#include "zr_vm_lib_testing/module.h"
#include "zr_vm_lib_thread/module.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_parser/attribute_contract.h"
#include "zr_vm_parser/compile_tool.h"

#include <string.h>

typedef struct SZrExpectedOfficialModule {
    const TZrChar *moduleName;
    EZrLibOfficialModuleTier tier;
    EZrLibrary_ProviderPhase phase;
    EZrProviderContractRole providerContractRole;
} SZrExpectedOfficialModule;

typedef struct SZrHostNativeLoaderProbe {
    TZrSize loaderCallCount;
    TZrSize resolverCallCount;
    TZrSize observerCallCount;
} SZrHostNativeLoaderProbe;

static const SZrExpectedOfficialModule k_expected_modules[] = {
        {"zr.builtin", ZR_LIB_OFFICIAL_MODULE_TIER_N0, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
         ZR_PROVIDER_CONTRACT_ROLE_BUILTIN_TYPE_SURFACE},
        {"zr.container", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.iteration", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.math", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.task", ZR_LIB_OFFICIAL_MODULE_TIER_N1, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.debug", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.ffi", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.network", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.network.tcp", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.network.udp", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.pooling", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.reflection", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
         ZR_PROVIDER_CONTRACT_ROLE_REFLECTION},
        {"zr.system", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.assembly", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.console", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.env", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.exception", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.fs", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.gc", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.process", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.system.vm", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.thread", ZR_LIB_OFFICIAL_MODULE_TIER_N2, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.compile", ZR_LIB_OFFICIAL_MODULE_TIER_N3, ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.compile.declaration", ZR_LIB_OFFICIAL_MODULE_TIER_N3, ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL, ZR_PROVIDER_CONTRACT_ROLE_NONE},
        {"zr.testing", ZR_LIB_OFFICIAL_MODULE_TIER_N3, ZR_LIBRARY_PROVIDER_PHASE_TEST, ZR_PROVIDER_CONTRACT_ROLE_NONE},
};

void setUp(void) {}

void tearDown(void) {}

static ZrLibModuleDescriptor make_descriptor(const TZrChar *moduleName,
                                             EZrLibrary_ProviderPhase phase,
                                             const TZrChar *publicContractHash) {
    ZrLibModuleDescriptor descriptor;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION;
    descriptor.moduleName = moduleName;
    descriptor.minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION;
    descriptor.providerPhase = phase;
    descriptor.publicContractHash = publicContractHash;
    return descriptor;
}

static const ZrLibTypeDescriptor *find_descriptor_type(
        const ZrLibModuleDescriptor *descriptor,
        const TZrChar *typeName) {
    TZrSize index;

    if (descriptor == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < descriptor->typeCount; index++) {
        if (descriptor->types[index].name != ZR_NULL &&
            strcmp(descriptor->types[index].name, typeName) == 0) {
            return &descriptor->types[index];
        }
    }
    return ZR_NULL;
}

static void assert_official_runtime_descriptor(
        SZrState *state,
        const ZrLibModuleDescriptor *descriptor,
        const TZrChar *expectedModuleName) {
    const ZrLibOfficialModuleInventoryEntry *entry;

    TEST_ASSERT_NOT_NULL(descriptor);
    TEST_ASSERT_EQUAL_STRING(expectedModuleName, descriptor->moduleName);
    entry = ZrLibrary_OfficialModuleInventory_Find(descriptor->moduleName);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT(entry->phase, descriptor->providerPhase);
    TEST_ASSERT_NOT_NULL(descriptor->publicContractHash);
    TEST_ASSERT_NOT_EQUAL('\0', descriptor->publicContractHash[0]);
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_ValidateModuleDescriptor(
            state->global, descriptor));
}

static SZrObjectModule *host_native_loader_probe(SZrState *state,
                                                 SZrString *moduleName,
                                                 TZrPtr userData) {
    SZrHostNativeLoaderProbe *probe = (SZrHostNativeLoaderProbe *)userData;

    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(moduleName);
    if (probe != ZR_NULL) {
        probe->loaderCallCount++;
    }
    return ZR_NULL;
}

static const TZrChar *host_provider_resolver_probe(
        TZrUInt32 providerRole,
        TZrPtr userData) {
    SZrHostNativeLoaderProbe *probe = (SZrHostNativeLoaderProbe *)userData;

    if (probe != ZR_NULL) {
        probe->resolverCallCount++;
    }
    return providerRole == 99u ? "host.provider" : ZR_NULL;
}

static void host_owner_observer_probe(SZrState *state,
                                      SZrRawObject *object,
                                      TZrInt32 delta,
                                      TZrPtr userData) {
    SZrHostNativeLoaderProbe *probe = (SZrHostNativeLoaderProbe *)userData;

    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(object);
    ZR_UNUSED_PARAMETER(delta);
    if (probe != ZR_NULL) {
        probe->observerCallCount++;
    }
}

static void test_official_inventory_is_frozen_unique_and_phase_typed(void) {
    TZrSize index;
    TZrSize otherIndex;

    TEST_ASSERT_EQUAL_UINT64(ZR_ARRAY_COUNT(k_expected_modules),
                             ZrLibrary_OfficialModuleInventory_GetCount());
    for (index = 0U; index < ZR_ARRAY_COUNT(k_expected_modules); index++) {
        const ZrLibOfficialModuleInventoryEntry *entry =
                ZrLibrary_OfficialModuleInventory_GetAt(index);
        const ZrLibOfficialModuleInventoryEntry *found;

        TEST_ASSERT_NOT_NULL(entry);
        TEST_ASSERT_EQUAL_STRING(k_expected_modules[index].moduleName, entry->moduleName);
        TEST_ASSERT_EQUAL_INT(k_expected_modules[index].tier, entry->tier);
        TEST_ASSERT_EQUAL_INT(k_expected_modules[index].phase, entry->phase);
        TEST_ASSERT_EQUAL_INT(k_expected_modules[index].providerContractRole,
                              entry->providerContractRole);
        found = ZrLibrary_OfficialModuleInventory_Find(entry->moduleName);
        TEST_ASSERT_EQUAL_PTR(entry, found);

        for (otherIndex = index + 1U;
             otherIndex < ZR_ARRAY_COUNT(k_expected_modules);
             otherIndex++) {
            TEST_ASSERT_NOT_EQUAL(0, strcmp(entry->moduleName,
                                            k_expected_modules[otherIndex].moduleName));
        }
    }

    TEST_ASSERT_NULL(ZrLibrary_OfficialModuleInventory_GetAt(
            ZrLibrary_OfficialModuleInventory_GetCount()));
    TEST_ASSERT_NULL(ZrLibrary_OfficialModuleInventory_Find("debug"));
    TEST_ASSERT_NULL(ZrLibrary_OfficialModuleInventory_Find("zr.unknown"));
    TEST_ASSERT_NULL(ZrLibrary_OfficialModuleInventory_Find(ZR_NULL));
}

static void test_registered_provider_contract_owns_reflection_type_roles(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const ZrLibModuleDescriptor *builtinProvider;
    const ZrLibModuleDescriptor *reflectionProvider;
    ZrLibRegisteredCanonicalTypeRole typeInfoRole;
    ZrLibRegisteredCanonicalTypeRole typeRole;
    ZrLibRegisteredCanonicalTypeRole typeIdRole;
    ZrLibRegisteredCanonicalTypeRole structProjectionRole;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_Attach(state->global));
    builtinProvider = ZrLibrary_NativeRegistry_FindModuleByProviderRole(
            state->global, ZR_PROVIDER_CONTRACT_ROLE_BUILTIN_TYPE_SURFACE);
    reflectionProvider = ZrLibrary_NativeRegistry_FindModuleByProviderRole(
            state->global, ZR_PROVIDER_CONTRACT_ROLE_REFLECTION);
    TEST_ASSERT_NOT_NULL(builtinProvider);
    TEST_ASSERT_NOT_NULL(reflectionProvider);
    TEST_ASSERT_EQUAL_STRING("zr.builtin", builtinProvider->moduleName);
    TEST_ASSERT_EQUAL_STRING("zr.reflection", reflectionProvider->moduleName);

    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_FindCanonicalTypeRole(
            state->global, ZR_CANONICAL_TYPE_ROLE_BUILTIN_METADATA_ROOT, &typeInfoRole));
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_FindCanonicalTypeRole(
            state->global, ZR_CANONICAL_TYPE_ROLE_REFLECTION_TYPE, &typeRole));
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_FindCanonicalTypeRoleByName(
            state->global, "zr.reflection.TypeId", &typeIdRole));
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_FindCanonicalTypeRoleByProjection(
            state->global,
            ZR_PROVIDER_CONTRACT_ROLE_REFLECTION,
            ZR_CANONICAL_TYPE_PROJECTION_STRUCT,
            &structProjectionRole));
    TEST_ASSERT_EQUAL_PTR(builtinProvider, typeInfoRole.provider);
    TEST_ASSERT_EQUAL_PTR(reflectionProvider, typeRole.provider);
    TEST_ASSERT_EQUAL_PTR(reflectionProvider, typeIdRole.provider);
    TEST_ASSERT_EQUAL_STRING("zr.builtin.TypeInfo", typeInfoRole.typeRole->canonicalName);
    TEST_ASSERT_EQUAL_STRING("zr.reflection.Type", typeRole.typeRole->canonicalName);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_ROLE_REFLECTION_TYPE_ID,
                          typeIdRole.typeRole->role);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_ROLE_REFLECTION_STRUCT_TYPE_OF,
                          structProjectionRole.typeRole->role);
    TEST_ASSERT_TRUE(reflectionProvider->isContractOnly);

    ZrLibrary_NativeRegistry_Free(state->global);
    TEST_ASSERT_NULL(ZrLibrary_NativeRegistry_FindModuleByProviderRole(
            state->global, ZR_PROVIDER_CONTRACT_ROLE_REFLECTION));
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_FindCanonicalTypeRole(
            state->global, ZR_CANONICAL_TYPE_ROLE_REFLECTION_TYPE, &typeRole));
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_Attach(state->global));
    ZrTests_Runtime_State_Destroy(state);
}

static void test_registry_rejects_spoofed_reflection_provider_contract(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrLibCanonicalTypeRoleDescriptor spoofedTypeRole = {
            "zr.reflection.Type",
            ZR_CANONICAL_TYPE_ROLE_REFLECTION_TYPE,
            ZR_CANONICAL_TYPE_ROLE_NONE,
            ZR_CANONICAL_TYPE_SURFACE_RUNTIME_TYPE_MEMBERS,
            ZR_CANONICAL_TYPE_PROJECTION_ERASED,
    };
    ZrLibModuleDescriptor spoofedProvider = make_descriptor(
            "third.party.reflection", ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, "spoof-v1");
    ZrLibModuleDescriptor missingOfficialRole = make_descriptor(
            "zr.reflection", ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, "reflection-v1");

    TEST_ASSERT_NOT_NULL(state);
    spoofedProvider.providerContractRole = ZR_PROVIDER_CONTRACT_ROLE_REFLECTION;
    spoofedProvider.canonicalTypeRoles = &spoofedTypeRole;
    spoofedProvider.canonicalTypeRoleCount = 1u;
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_RegisterModule(
            state->global, &spoofedProvider));
    TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_PROVIDER_CONTRACT_MISMATCH,
                          ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_RegisterModule(
            state->global, &missingOfficialRole));
    TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_PROVIDER_CONTRACT_MISMATCH,
                          ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));
    ZrTests_Runtime_State_Destroy(state);
}

static void test_registry_rejects_malformed_projection_and_parent_graphs(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const ZrLibModuleDescriptor *reflectionProvider;
    ZrLibModuleDescriptor malformedProvider;
    ZrLibCanonicalTypeRoleDescriptor roles[11];

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_Attach(state->global));
    reflectionProvider = ZrLibrary_NativeRegistry_FindModuleByProviderRole(
            state->global, ZR_PROVIDER_CONTRACT_ROLE_REFLECTION);
    TEST_ASSERT_NOT_NULL(reflectionProvider);
    TEST_ASSERT_EQUAL_UINT64(ZR_ARRAY_COUNT(roles),
                             reflectionProvider->canonicalTypeRoleCount);
    malformedProvider = *reflectionProvider;
    malformedProvider.canonicalTypeRoles = roles;

    memcpy(roles,
           reflectionProvider->canonicalTypeRoles,
           sizeof(roles));
    roles[0].parentRole = roles[0].role;
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_ValidateModuleDescriptor(
            state->global, &malformedProvider));
    TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_INVALID_CANONICAL_TYPE_ROLE,
                          ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));

    memcpy(roles,
           reflectionProvider->canonicalTypeRoles,
           sizeof(roles));
    roles[2].parentRole = roles[3].role;
    roles[3].parentRole = roles[2].role;
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_ValidateModuleDescriptor(
            state->global, &malformedProvider));
    TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_INVALID_CANONICAL_TYPE_ROLE,
                          ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));

    memcpy(roles,
           reflectionProvider->canonicalTypeRoles,
           sizeof(roles));
    roles[10].projectionKind = ZR_CANONICAL_TYPE_PROJECTION_STRUCT;
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_ValidateModuleDescriptor(
            state->global, &malformedProvider));
    TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_INVALID_CANONICAL_TYPE_ROLE,
                          ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));

    malformedProvider.canonicalTypeRoles = ZR_NULL;
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_ValidateModuleDescriptor(
            state->global, &malformedProvider));
    TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_INVALID_CANONICAL_TYPE_ROLE,
                          ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));
    ZrTests_Runtime_State_Destroy(state);
}

static void test_reflection_provider_contract_is_not_a_loadable_empty_module(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *reflectionName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_Attach(state->global));
    reflectionName = ZrCore_String_CreateFromNative(state, "zr.reflection");
    TEST_ASSERT_NOT_NULL(reflectionName);
    TEST_ASSERT_NOT_NULL(state->global->nativeModuleLoader);
    TEST_ASSERT_NULL(state->global->nativeModuleLoader(
            state, reflectionName, state->global->nativeModuleLoaderUserData));
    ZrTests_Runtime_State_Destroy(state);
}

static void test_registry_composes_and_restores_host_native_loader(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrHostNativeLoaderProbe probe = {0u};
    SZrString *reflectionName;
    SZrString *unknownModuleName;

    TEST_ASSERT_NOT_NULL(state);
    ZrLibrary_NativeRegistry_Free(state->global);
    ZrCore_GlobalState_SetNativeModuleLoader(
            state->global, host_native_loader_probe, &probe);
    ZrCore_GlobalState_SetProviderModuleNameResolver(
            state->global, host_provider_resolver_probe, &probe);
    ZrCore_GlobalState_SetOwnershipStrongRefObserver(
            state->global, host_owner_observer_probe, &probe);
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_Attach(state->global));
    TEST_ASSERT_NOT_NULL(state->global->nativeRegistryState);
    reflectionName = ZrCore_String_CreateFromNative(state, "zr.reflection");
    unknownModuleName = ZrCore_String_CreateFromNative(
            state, "host.custom.module");
    TEST_ASSERT_NOT_NULL(reflectionName);
    TEST_ASSERT_NOT_NULL(unknownModuleName);
    TEST_ASSERT_NOT_NULL(state->global->nativeModuleLoader);
    TEST_ASSERT_NULL(state->global->nativeModuleLoader(
            state, reflectionName, state->global->nativeModuleLoaderUserData));
    TEST_ASSERT_EQUAL_UINT64(0u, probe.loaderCallCount);
    TEST_ASSERT_NULL(state->global->nativeModuleLoader(
            state, unknownModuleName, state->global->nativeModuleLoaderUserData));
    TEST_ASSERT_EQUAL_UINT64(1u, probe.loaderCallCount);
    TEST_ASSERT_EQUAL_STRING(
            "host.provider",
            ZrCore_GlobalState_ResolveProviderModuleName(state->global, 99u));
    TEST_ASSERT_EQUAL_UINT64(1u, probe.resolverCallCount);
    state->global->ownershipStrongRefObserver(
            state,
            ZR_NULL,
            1,
            state->global->ownershipStrongRefObserverUserData);
    TEST_ASSERT_EQUAL_UINT64(1u, probe.observerCallCount);

    ZrLibrary_NativeRegistry_Free(state->global);
    TEST_ASSERT_NULL(state->global->nativeRegistryState);
    TEST_ASSERT_EQUAL_PTR(host_native_loader_probe,
                          state->global->nativeModuleLoader);
    TEST_ASSERT_EQUAL_PTR(&probe, state->global->nativeModuleLoaderUserData);
    TEST_ASSERT_EQUAL_PTR(host_provider_resolver_probe,
                          state->global->providerModuleNameResolver);
    TEST_ASSERT_EQUAL_PTR(&probe,
                          state->global->providerModuleNameResolverUserData);
    TEST_ASSERT_EQUAL_PTR(host_owner_observer_probe,
                          state->global->ownershipStrongRefObserver);
    TEST_ASSERT_EQUAL_PTR(&probe,
                          state->global->ownershipStrongRefObserverUserData);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_owner_descriptors_converge_on_inventory_phase(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const ZrLibModuleDescriptor *taskModule;
    const ZrLibModuleDescriptor *iterationModule =
            ZrVmLibIteration_GetModuleDescriptor();
    const ZrLibModuleDescriptor *containerModule =
            ZrVmLibContainer_GetModuleDescriptor();
    const ZrLibModuleDescriptor *poolingModule =
            ZrVmLibContainer_GetPoolingModuleDescriptor();
#if defined(ZR_TEST_HAS_THREAD_PROVIDER)
    const ZrLibModuleDescriptor *threadModule = ZR_NULL;
#endif
#if defined(ZR_TEST_HAS_DEBUG_PROVIDER)
    const ZrLibModuleDescriptor *debugModule = ZR_NULL;
#endif
    const ZrLibModuleDescriptor *reflectionModule;
    const SZrParserCompileToolModuleDescriptor *compileModule =
            ZrParser_CompileTool_FindModule("zr.compile");
    const SZrParserCompileToolModuleDescriptor *declarationModule =
            ZrParser_CompileTool_FindModule("zr.compile.declaration");
    const ZrLibModuleDescriptor *testingModule = ZrVmLibTesting_GetModuleDescriptor();
    const ZrLibOfficialModuleInventoryEntry *entry;
    const SZrParserAttributeSchema *schema;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_Attach(state->global));
    TEST_ASSERT_TRUE(ZrCore_TaskRuntime_RegisterBuiltins(state->global));
    taskModule = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.task");
    reflectionModule = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.reflection");
#if defined(ZR_TEST_HAS_THREAD_PROVIDER)
    threadModule = ZrVmThread_GetModuleDescriptor();
#endif
#if defined(ZR_TEST_HAS_DEBUG_PROVIDER)
    debugModule = ZrVmLibDebug_GetModuleDescriptor();
#endif

    assert_official_runtime_descriptor(state, taskModule, "zr.task");
    assert_official_runtime_descriptor(state, iterationModule, "zr.iteration");
    assert_official_runtime_descriptor(state, containerModule, "zr.container");
    assert_official_runtime_descriptor(state, poolingModule, "zr.pooling");
    assert_official_runtime_descriptor(state, reflectionModule, "zr.reflection");
#if defined(ZR_TEST_HAS_THREAD_PROVIDER)
    assert_official_runtime_descriptor(state, threadModule, "zr.thread");
#endif
#if defined(ZR_TEST_HAS_DEBUG_PROVIDER)
    assert_official_runtime_descriptor(state, debugModule, "zr.debug");
#endif

    TEST_ASSERT_NOT_NULL(find_descriptor_type(taskModule, "Task"));
    TEST_ASSERT_NOT_NULL(find_descriptor_type(taskModule, "Job"));
    TEST_ASSERT_NOT_NULL(find_descriptor_type(taskModule, "Scheduler"));
    TEST_ASSERT_NULL(find_descriptor_type(iterationModule, "Task"));
    TEST_ASSERT_NOT_NULL(find_descriptor_type(iterationModule, "Iterable"));
    TEST_ASSERT_NOT_NULL(find_descriptor_type(iterationModule, "Enumerator"));
    TEST_ASSERT_NOT_NULL(find_descriptor_type(iterationModule, "Iterator"));
    TEST_ASSERT_NOT_NULL(find_descriptor_type(iterationModule, "AsyncIterator"));
#if defined(ZR_TEST_HAS_THREAD_PROVIDER)
    TEST_ASSERT_NOT_NULL(find_descriptor_type(threadModule, "ThreadScheduler"));
    TEST_ASSERT_NOT_NULL(find_descriptor_type(threadModule, "Send"));
    TEST_ASSERT_NULL(find_descriptor_type(threadModule, "Task"));
    TEST_ASSERT_NULL(find_descriptor_type(threadModule, "Job"));
#endif
    TEST_ASSERT_NOT_NULL(find_descriptor_type(poolingModule, "Pool"));
    TEST_ASSERT_NOT_NULL(find_descriptor_type(poolingModule, "PoolRef"));

    TEST_ASSERT_NOT_NULL(compileModule);
    TEST_ASSERT_NOT_NULL(declarationModule);
    TEST_ASSERT_NOT_NULL(testingModule);

    entry = ZrLibrary_OfficialModuleInventory_Find(compileModule->moduleName);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT(entry->phase, compileModule->providerPhase);
    entry = ZrLibrary_OfficialModuleInventory_Find(declarationModule->moduleName);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT(entry->phase, declarationModule->providerPhase);
    entry = ZrLibrary_OfficialModuleInventory_Find(testingModule->moduleName);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT(entry->phase, testingModule->providerPhase);

    schema = ZrParser_AttributeContract_FindBuiltinByRole(ZR_PARSER_ATTRIBUTE_ROLE_USAGE);
    TEST_ASSERT_NOT_NULL(schema);
    entry = ZrLibrary_OfficialModuleInventory_Find(schema->ownerModule);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT(entry->phase, schema->providerPhase);
    schema = ZrParser_AttributeContract_FindBuiltinByRole(ZR_PARSER_ATTRIBUTE_ROLE_DECLARATION_TRANSFORM);
    TEST_ASSERT_NOT_NULL(schema);
    entry = ZrLibrary_OfficialModuleInventory_Find(schema->ownerModule);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT(entry->phase, schema->providerPhase);
    schema = ZrParser_AttributeContract_FindBuiltinByRole(ZR_PARSER_ATTRIBUTE_ROLE_TEST);
    TEST_ASSERT_NOT_NULL(schema);
    entry = ZrLibrary_OfficialModuleInventory_Find(schema->ownerModule);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT(entry->phase, schema->providerPhase);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_registry_rejects_legacy_phase_and_duplicate_official_providers(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrLibModuleDescriptor legacyDebug = make_descriptor(
            "debug", ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, "legacy-debug");
    ZrLibModuleDescriptor wrongTestingPhase = make_descriptor(
            "zr.testing", ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, "testing-v1");
    ZrLibModuleDescriptor testing = make_descriptor(
            "zr.testing", ZR_LIBRARY_PROVIDER_PHASE_TEST, "testing-v1");
    ZrLibModuleDescriptor equivalentTesting = make_descriptor(
            "zr.testing", ZR_LIBRARY_PROVIDER_PHASE_TEST, "testing-v1");
    ZrLibModuleDescriptor conflictingTesting = make_descriptor(
            "zr.testing", ZR_LIBRARY_PROVIDER_PHASE_TEST, "testing-v2");

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &legacyDebug));
    TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_RESERVED_OFFICIAL_MODULE,
                          ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &wrongTestingPhase));
    TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_PHASE_MISMATCH,
                          ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));

    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &testing));
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &testing));
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &equivalentTesting));
    TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_DUPLICATE_OFFICIAL_PROVIDER,
                          ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &conflictingTesting));
    TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_DUPLICATE_OFFICIAL_PROVIDER,
                          ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_provider_phase_admission_is_host_specific(void) {
    TEST_ASSERT_TRUE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME));
    TEST_ASSERT_FALSE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_LIBRARY_PROVIDER_PHASE_TEST));
    TEST_ASSERT_FALSE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL));
    TEST_ASSERT_TRUE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_TEST, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME));
    TEST_ASSERT_TRUE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_TEST, ZR_LIBRARY_PROVIDER_PHASE_TEST));
    TEST_ASSERT_FALSE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_TEST, ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL));
    TEST_ASSERT_TRUE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL, ZR_LIBRARY_PROVIDER_PHASE_RUNTIME));
    TEST_ASSERT_FALSE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL, ZR_LIBRARY_PROVIDER_PHASE_TEST));
    TEST_ASSERT_TRUE(ZrLibrary_ProviderPhase_CanConsume(
            ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL, ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_official_inventory_is_frozen_unique_and_phase_typed);
    RUN_TEST(test_registered_provider_contract_owns_reflection_type_roles);
    RUN_TEST(test_registry_rejects_spoofed_reflection_provider_contract);
    RUN_TEST(test_registry_rejects_malformed_projection_and_parent_graphs);
    RUN_TEST(test_reflection_provider_contract_is_not_a_loadable_empty_module);
    RUN_TEST(test_registry_composes_and_restores_host_native_loader);
    RUN_TEST(test_owner_descriptors_converge_on_inventory_phase);
    RUN_TEST(test_registry_rejects_legacy_phase_and_duplicate_official_providers);
    RUN_TEST(test_provider_phase_admission_is_host_specific);
    return UNITY_END();
}
