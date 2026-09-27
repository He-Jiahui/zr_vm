#include "native_binding/native_binding_internal.h"

/* core 按稳定提供者角色查模块名时先使用本注册表，再退回宿主原有解析器。 */
const TZrChar *native_registry_resolve_provider_module_name(
        TZrUInt32 providerRole,
        TZrPtr userData) {
    ZrLibrary_NativeRegistryState *registry =
            (ZrLibrary_NativeRegistryState *)userData;

    if (registry == ZR_NULL || !registry->moduleRecords.isValid ||
        providerRole == (TZrUInt32)ZR_PROVIDER_CONTRACT_ROLE_NONE) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < registry->moduleRecords.length; index++) {
        const ZrLibRegisteredModuleRecord *record =
                (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(
                        &registry->moduleRecords, index);
        if (record != ZR_NULL && record->descriptor != ZR_NULL &&
            (TZrUInt32)record->descriptor->providerContractRole == providerRole) {
            return record->descriptor->moduleName;
        }
    }
    return registry->hostProviderModuleNameResolver != ZR_NULL
                   ? registry->hostProviderModuleNameResolver(
                             providerRole,
                             registry->hostProviderModuleNameResolverUserData)
                   : ZR_NULL;
}

/* 模块对象的强引用计数是插件卸载门禁；同时保留宿主已有观察行为。 */
static void native_registry_observe_owner_strong_ref(SZrState *state,
                                                     SZrRawObject *object,
                                                     TZrInt32 delta,
                                                     TZrPtr userData) {
    ZrLibrary_NativeRegistryState *registry = (ZrLibrary_NativeRegistryState *)userData;
    SZrObject *objectValue;
    SZrObjectModule *module;
    const TZrChar *moduleName;
    ZrLibRegisteredModuleRecord *record;

    if (state != ZR_NULL && object != ZR_NULL && registry != ZR_NULL && delta != 0 &&
        object->type == ZR_RAW_OBJECT_TYPE_OBJECT) {
        objectValue = ZR_CAST_OBJECT(state, object);
        if (objectValue != ZR_NULL &&
            objectValue->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
            module = (SZrObjectModule *)objectValue;
            moduleName = module->moduleName != ZR_NULL
                                 ? ZrCore_String_GetNativeString(module->moduleName)
                                 : ZR_NULL;
            record = moduleName != ZR_NULL
                             ? (ZrLibRegisteredModuleRecord *)native_registry_find_record(
                                       registry, moduleName)
                             : ZR_NULL;
            if (record != ZR_NULL) {
                if (delta > 0) {
                    record->ownerRefCount += (TZrUInt32)delta;
                } else {
                    TZrUInt32 decrement = (TZrUInt32)(-delta);
                    record->ownerRefCount = decrement >= record->ownerRefCount
                                                    ? 0u
                                                    : record->ownerRefCount - decrement;
                }
            }
        }
    }
    if (registry != ZR_NULL && registry->hostOwnershipStrongRefObserver != ZR_NULL) {
        registry->hostOwnershipStrongRefObserver(
                state,
                object,
                delta,
                registry->hostOwnershipStrongRefObserverUserData);
    }
}

/* core 全局销毁回调只释放仍由本注册表持有的状态，避免重复清理。 */
static void native_registry_cleanup_global_state(
        SZrGlobalState *global,
        TZrPtr state) {
    if (global != ZR_NULL && global->nativeRegistryState == state) {
        ZrLibrary_NativeRegistry_Free(global);
    }
}

/* 注册表以 global 为生命周期边界，将 core 的解析、加载和强引用钩子接到统一提供者。 */
TZrBool ZrLibrary_NativeRegistry_Attach(SZrGlobalState *global) {
    ZrLibrary_NativeRegistryState *registry;
    SZrState *state;
    TZrSize cacheIndex;

    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        return ZR_FALSE;
    }

    if (native_registry_get(global) != ZR_NULL) {
        return ZR_TRUE;
    }

    state = global->mainThreadState;
    registry = (ZrLibrary_NativeRegistryState *)global->allocator(global->userAllocationArguments,
                                                                  ZR_NULL,
                                                                  0,
                                                                  sizeof(ZrLibrary_NativeRegistryState),
                                                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (registry == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(registry, 0, sizeof(*registry));
    /* 三类记录在发布钩子前建立，基础模块注册失败时统一交给 Free 清理。
     * BUG: Array_Init 分配 moduleRecords.head 失败仍将 isValid 设为 true；
     * Attach 继续注册基础模块，后续 Array_Push 断言/解引用空 head。 */
    ZrCore_Array_Construct(&registry->moduleRecords);
    ZrCore_Array_Construct(&registry->bindingEntries);
    ZrCore_Array_Construct(&registry->pluginHandles);
    ZrCore_Array_Init(state,
                      &registry->moduleRecords,
                      sizeof(ZrLibRegisteredModuleRecord),
                      ZR_LIBRARY_NATIVE_MODULE_RECORDS_INITIAL_CAPACITY);
    ZrCore_Array_Init(state,
                      &registry->bindingEntries,
                      sizeof(ZrLibBindingEntry),
                      ZR_LIBRARY_NATIVE_BINDING_ENTRIES_INITIAL_CAPACITY);
    ZrCore_Array_Init(state,
                      &registry->pluginHandles,
                      sizeof(ZrLibPluginHandleRecord),
                      ZR_LIBRARY_NATIVE_PLUGIN_HANDLES_INITIAL_CAPACITY);
    for (cacheIndex = 0; cacheIndex < ZR_LIBRARY_NATIVE_BINDING_LOOKUP_CACHE_CAPACITY; cacheIndex++) {
        registry->bindingLookupHotIndices[cacheIndex] = ZR_LIBRARY_NATIVE_BINDING_LOOKUP_CACHE_INVALID_INDEX;
    }
    native_registry_clear_error(registry);

    registry->hostNativeModuleLoader = global->nativeModuleLoader;
    registry->hostNativeModuleLoaderUserData = global->nativeModuleLoaderUserData;
    registry->hostProviderModuleNameResolver = global->providerModuleNameResolver;
    registry->hostProviderModuleNameResolverUserData =
            global->providerModuleNameResolverUserData;
    registry->hostCallBindingModuleResolver = global->callBindingModuleResolver;
    registry->hostCallBindingModuleResolverUserData = global->callBindingModuleResolverUserData;
    registry->hostTypedCallBindingResolver = global->typedCallBindingResolver;
    registry->hostTypedCallBindingResolverUserData = global->typedCallBindingResolverUserData;
    registry->hostOwnershipStrongRefObserver = global->ownershipStrongRefObserver;
    registry->hostOwnershipStrongRefObserverUserData =
            global->ownershipStrongRefObserverUserData;

    /* 保存宿主原钩子并完成替换后，基础模块注册才会走同一校验路径。 */
    global->nativeRegistryState = registry;
    global->nativeRegistryStateCleanup = native_registry_cleanup_global_state;
    global->callBindingModuleResolver = native_registry_link_call_binding;
    global->callBindingModuleResolverUserData = registry;
    global->typedCallBindingResolver = native_registry_resolve_typed_call_binding;
    global->typedCallBindingResolverUserData = registry;
    ZrCore_GlobalState_SetNativeModuleLoader(global, native_registry_loader, registry);
    ZrCore_GlobalState_SetProviderModuleNameResolver(
            global, native_registry_resolve_provider_module_name, registry);
    ZrCore_GlobalState_SetOwnershipStrongRefObserver(global,
                                                     native_registry_observe_owner_strong_ref,
                                                     registry);
    if (!ZrLibrary_NativeRegistry_RegisterModule(global, ZrLibrary_BuiltinModule_GetDescriptor())) {
        ZrLibrary_NativeRegistry_Free(global);
        return ZR_FALSE;
    }
    if (!ZrLibrary_NativeRegistry_RegisterModule(
                global, ZrLibrary_ReflectionContract_GetDescriptor())) {
        ZrLibrary_NativeRegistry_Free(global);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/* 清理插件和记录，并在释放 registry 前恢复仍指向它的宿主钩子。 */
void ZrLibrary_NativeRegistry_Free(SZrGlobalState *global) {
    ZrLibrary_NativeRegistryState *registry;
    SZrState *state;
    TZrSize index;

    if (global == ZR_NULL) {
        return;
    }

    registry = native_registry_get(global);
    if (registry == ZR_NULL || global->mainThreadState == ZR_NULL) {
        return;
    }
    if (global->callBindingModuleResolver == native_registry_link_call_binding &&
        global->callBindingModuleResolverUserData == registry) {
        global->callBindingModuleResolver = registry->hostCallBindingModuleResolver;
        global->callBindingModuleResolverUserData = registry->hostCallBindingModuleResolverUserData;
    }
    if (global->typedCallBindingResolver == native_registry_resolve_typed_call_binding &&
        global->typedCallBindingResolverUserData == registry) {
        global->typedCallBindingResolver = registry->hostTypedCallBindingResolver;
        global->typedCallBindingResolverUserData = registry->hostTypedCallBindingResolverUserData;
    }

    if (global->ownershipStrongRefObserver ==
                native_registry_observe_owner_strong_ref &&
        global->ownershipStrongRefObserverUserData == registry) {
        ZrCore_GlobalState_SetOwnershipStrongRefObserver(
                global,
                registry->hostOwnershipStrongRefObserver,
                registry->hostOwnershipStrongRefObserverUserData);
    }

    state = global->mainThreadState;
    if (registry->pluginHandles.isValid) {
        for (index = 0; index < registry->pluginHandles.length; index++) {
            ZrLibPluginHandleRecord *handleRecord =
                (ZrLibPluginHandleRecord *)ZrCore_Array_Get(&registry->pluginHandles, index);
            if (handleRecord != ZR_NULL) {
                native_registry_release_plugin_handle_record(global, handleRecord);
            }
        }
    }

    if (registry->moduleRecords.isValid) {
        for (index = 0; index < registry->moduleRecords.length; index++) {
            ZrLibRegisteredModuleRecord *record =
                    (ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index);
            if (record != ZR_NULL && record->moduleName != ZR_NULL) {
                global->allocator(global->userAllocationArguments,
                                  record->moduleName,
                                  strlen(record->moduleName) + 1,
                                  0,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                record->moduleName = ZR_NULL;
            }
            if (record != ZR_NULL && record->sourcePath != ZR_NULL) {
                global->allocator(global->userAllocationArguments,
                                  record->sourcePath,
                                  strlen(record->sourcePath) + 1,
                                  0,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                record->sourcePath = ZR_NULL;
            }
        }
    }

    ZrCore_Array_Free(state, &registry->pluginHandles);
    ZrCore_Array_Free(state, &registry->bindingEntries);
    ZrCore_Array_Free(state, &registry->moduleRecords);

    if (global->nativeModuleLoader == native_registry_loader &&
        global->nativeModuleLoaderUserData == registry) {
        ZrCore_GlobalState_SetNativeModuleLoader(
                global,
                registry->hostNativeModuleLoader,
                registry->hostNativeModuleLoaderUserData);
    }
    if (global->providerModuleNameResolver ==
                native_registry_resolve_provider_module_name &&
        global->providerModuleNameResolverUserData == registry) {
        ZrCore_GlobalState_SetProviderModuleNameResolver(
                global,
                registry->hostProviderModuleNameResolver,
                registry->hostProviderModuleNameResolverUserData);
    }
    global->nativeRegistryState = ZR_NULL;
    if (global->nativeRegistryStateCleanup == native_registry_cleanup_global_state) {
        global->nativeRegistryStateCleanup = ZR_NULL;
    }
    global->allocator(global->userAllocationArguments,
                      registry,
                      sizeof(ZrLibrary_NativeRegistryState),
                      0,
                      ZR_MEMORY_NATIVE_TYPE_GLOBAL);
}

/* 静态模块与插件共用底层注册规则；此入口只声明提供者不可由插件卸载。 */
TZrBool ZrLibrary_NativeRegistry_RegisterModule(SZrGlobalState *global, const ZrLibModuleDescriptor *descriptor) {
    return native_registry_register_module_record(global,
                                                  descriptor,
                                                  ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_BUILTIN,
                                                  descriptor != ZR_NULL ? descriptor->moduleName : ZR_NULL,
                                                  ZR_FALSE);
}

/* 编译器及导入器查已注册描述符；结果借用，注册表重载后必须重查。 */
const ZrLibModuleDescriptor *ZrLibrary_NativeRegistry_FindModule(SZrGlobalState *global, const TZrChar *moduleName) {
    ZrLibrary_NativeRegistryState *registry;
    const ZrLibRegisteredModuleRecord *record;

    if (global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    registry = native_registry_get(global);
    if (registry == ZR_NULL || !registry->moduleRecords.isValid) {
        return ZR_NULL;
    }

    record = native_registry_find_record(registry, moduleName);
    return record != ZR_NULL ? record->descriptor : ZR_NULL;
}

/* 类型角色入口先确定唯一提供者，避免调用者依赖可变化的模块别名。 */
const ZrLibModuleDescriptor *ZrLibrary_NativeRegistry_FindModuleByProviderRole(
        SZrGlobalState *global,
        EZrProviderContractRole providerRole) {
    ZrLibrary_NativeRegistryState *registry = native_registry_get(global);

    if (registry == ZR_NULL || !registry->moduleRecords.isValid ||
        providerRole == ZR_PROVIDER_CONTRACT_ROLE_NONE) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < registry->moduleRecords.length; index++) {
        const ZrLibRegisteredModuleRecord *record =
                (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(
                        &registry->moduleRecords, index);
        if (record != ZR_NULL && record->descriptor != ZR_NULL &&
            record->descriptor->providerContractRole == providerRole) {
            return record->descriptor;
        }
    }
    return ZR_NULL;
}

/* 按稳定角色返回提供者和类型表中的借用指针，失败时输出保持清空。 */
TZrBool ZrLibrary_NativeRegistry_FindCanonicalTypeRole(
        SZrGlobalState *global,
        EZrCanonicalTypeRole role,
        ZrLibRegisteredCanonicalTypeRole *outRole) {
    ZrLibrary_NativeRegistryState *registry = native_registry_get(global);

    if (outRole == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outRole, 0, sizeof(*outRole));
    if (registry == ZR_NULL || !registry->moduleRecords.isValid ||
        role == ZR_CANONICAL_TYPE_ROLE_NONE) {
        return ZR_FALSE;
    }
    for (TZrSize moduleIndex = 0u;
         moduleIndex < registry->moduleRecords.length;
         moduleIndex++) {
        const ZrLibRegisteredModuleRecord *record =
                (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(
                        &registry->moduleRecords, moduleIndex);
        const ZrLibModuleDescriptor *provider =
                record != ZR_NULL ? record->descriptor : ZR_NULL;

        if (provider == ZR_NULL) {
            continue;
        }
        for (TZrSize roleIndex = 0u;
             roleIndex < provider->canonicalTypeRoleCount;
             roleIndex++) {
            if (provider->canonicalTypeRoles[roleIndex].role == role) {
                outRole->provider = provider;
                outRole->typeRole = &provider->canonicalTypeRoles[roleIndex];
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

/* 按规范名支持反射和类型推断的静态类型身份查询。 */
TZrBool ZrLibrary_NativeRegistry_FindCanonicalTypeRoleByName(
        SZrGlobalState *global,
        const TZrChar *canonicalName,
        ZrLibRegisteredCanonicalTypeRole *outRole) {
    ZrLibrary_NativeRegistryState *registry = native_registry_get(global);

    if (outRole == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outRole, 0, sizeof(*outRole));
    if (registry == ZR_NULL || !registry->moduleRecords.isValid ||
        canonicalName == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize moduleIndex = 0u;
         moduleIndex < registry->moduleRecords.length;
         moduleIndex++) {
        const ZrLibRegisteredModuleRecord *record =
                (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(
                        &registry->moduleRecords, moduleIndex);
        const ZrLibModuleDescriptor *provider =
                record != ZR_NULL ? record->descriptor : ZR_NULL;

        if (provider == ZR_NULL) {
            continue;
        }
        for (TZrSize roleIndex = 0u;
             roleIndex < provider->canonicalTypeRoleCount;
             roleIndex++) {
            if (provider->canonicalTypeRoles[roleIndex].canonicalName != ZR_NULL &&
                strcmp(provider->canonicalTypeRoles[roleIndex].canonicalName,
                       canonicalName) == 0) {
                outRole->provider = provider;
                outRole->typeRole = &provider->canonicalTypeRoles[roleIndex];
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

/* 特定提供者的投影必须唯一，歧义不能静默选择第一个候选。 */
TZrBool ZrLibrary_NativeRegistry_FindCanonicalTypeRoleByProjection(
        SZrGlobalState *global,
        EZrProviderContractRole providerRole,
        EZrCanonicalTypeProjectionKind projectionKind,
        ZrLibRegisteredCanonicalTypeRole *outRole) {
    const ZrLibModuleDescriptor *provider;
    const ZrLibCanonicalTypeRoleDescriptor *match = ZR_NULL;

    if (outRole == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outRole, 0, sizeof(*outRole));
    if (projectionKind == ZR_CANONICAL_TYPE_PROJECTION_ERASED ||
        projectionKind > ZR_CANONICAL_TYPE_PROJECTION_ENUM) {
        return ZR_FALSE;
    }
    provider = ZrLibrary_NativeRegistry_FindModuleByProviderRole(
            global, providerRole);
    if (provider == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0u; index < provider->canonicalTypeRoleCount; index++) {
        const ZrLibCanonicalTypeRoleDescriptor *candidate =
                &provider->canonicalTypeRoles[index];

        if (candidate->projectionKind != projectionKind) {
            continue;
        }
        if (match != ZR_NULL) {
            return ZR_FALSE;
        }
        match = candidate;
    }
    if (match == ZR_NULL) {
        return ZR_FALSE;
    }
    outRole->provider = provider;
    outRole->typeRole = match;
    return ZR_TRUE;
}

/* 供插件加载与测试预检描述符，不将其放入当前注册表。 */
TZrBool ZrLibrary_NativeRegistry_ValidateModuleDescriptor(
        SZrGlobalState *global,
        const ZrLibModuleDescriptor *descriptor) {
    ZrLibrary_NativeRegistryState *registry;

    if (!ZrLibrary_NativeRegistry_Attach(global)) {
        return ZR_FALSE;
    }
    registry = native_registry_get(global);
    return registry != ZR_NULL &&
           native_registry_validate_official_descriptor(registry, descriptor) &&
           native_registry_validate_descriptor_compatibility(registry, descriptor);
}

/* 查询当前注册记录的借用快照，供工具与生命周期检查。 */
TZrBool ZrLibrary_NativeRegistry_GetModuleInfo(SZrGlobalState *global,
                                               const TZrChar *moduleName,
                                               ZrLibRegisteredModuleInfo *outInfo) {
    ZrLibrary_NativeRegistryState *registry;
    const ZrLibRegisteredModuleRecord *record;

    if (outInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outInfo, 0, sizeof(*outInfo));
    if (global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    registry = native_registry_get(global);
    record = native_registry_find_record(registry, moduleName);
    if (record == ZR_NULL) {
        return ZR_FALSE;
    }

    outInfo->descriptor = record->descriptor;
    outInfo->moduleName = record->moduleName;
    outInfo->sourcePath = record->sourcePath;
    outInfo->registrationKind = record->registrationKind;
    outInfo->isDescriptorPlugin = record->isDescriptorPlugin;
    outInfo->ownerRefCount = record->ownerRefCount;
    return ZR_TRUE;
}

/* LSP 按当前数量枚举模块；枚举期间注册表不可异步失效。 */
TZrSize ZrLibrary_NativeRegistry_GetModuleCount(SZrGlobalState *global) {
    ZrLibrary_NativeRegistryState *registry;

    if (global == ZR_NULL) {
        return 0;
    }

    registry = native_registry_get(global);
    if (registry == ZR_NULL || !registry->moduleRecords.isValid) {
        return 0;
    }

    return registry->moduleRecords.length;
}

/* 公开模块所有权计数，供插件卸载约束和回归测试核对。 */
TZrUInt32 ZrLibrary_NativeRegistry_GetModuleRefCount(SZrGlobalState *global, const TZrChar *moduleName) {
    ZrLibrary_NativeRegistryState *registry;
    const ZrLibRegisteredModuleRecord *record;

    if (global == ZR_NULL || moduleName == ZR_NULL) {
        return 0u;
    }

    registry = native_registry_get(global);
    record = native_registry_find_record(registry, moduleName);
    return record != ZR_NULL ? record->ownerRefCount : 0u;
}

/* 与 GetModuleCount 配合枚举；索引及返回指针只对当前注册表世代有效。 */
TZrBool ZrLibrary_NativeRegistry_GetModuleInfoAt(SZrGlobalState *global,
                                                 TZrSize index,
                                                 ZrLibRegisteredModuleInfo *outInfo) {
    ZrLibrary_NativeRegistryState *registry;
    const ZrLibRegisteredModuleRecord *record;

    if (outInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outInfo, 0, sizeof(*outInfo));
    if (global == ZR_NULL) {
        return ZR_FALSE;
    }

    registry = native_registry_get(global);
    if (registry == ZR_NULL || !registry->moduleRecords.isValid || index >= registry->moduleRecords.length) {
        return ZR_FALSE;
    }

    record = (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index);
    if (record == ZR_NULL) {
        return ZR_FALSE;
    }

    outInfo->descriptor = record->descriptor;
    outInfo->moduleName = record->moduleName;
    outInfo->sourcePath = record->sourcePath;
    outInfo->registrationKind = record->registrationKind;
    outInfo->isDescriptorPlugin = record->isDescriptorPlugin;
    outInfo->ownerRefCount = record->ownerRefCount;
    return ZR_TRUE;
}

/* 插件测试按来源路径确认记录，路径比较遵循平台大小写与斜杠规则。 */
TZrBool ZrLibrary_NativeRegistry_GetModuleInfoBySourcePath(SZrGlobalState *global,
                                                           const TZrChar *sourcePath,
                                                           ZrLibRegisteredModuleInfo *outInfo) {
    ZrLibrary_NativeRegistryState *registry;

    if (outInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outInfo, 0, sizeof(*outInfo));
    if (global == ZR_NULL || sourcePath == ZR_NULL) {
        return ZR_FALSE;
    }

    registry = native_registry_get(global);
    if (registry == ZR_NULL || !registry->moduleRecords.isValid) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < registry->moduleRecords.length; index++) {
        const ZrLibRegisteredModuleRecord *record =
            (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index);

        if (record == ZR_NULL || record->sourcePath == ZR_NULL ||
            !native_registry_source_paths_equal(record->sourcePath, sourcePath)) {
            continue;
        }

        outInfo->descriptor = record->descriptor;
        outInfo->moduleName = record->moduleName;
        outInfo->sourcePath = record->sourcePath;
        outInfo->registrationKind = record->registrationKind;
        outInfo->isDescriptorPlugin = record->isDescriptorPlugin;
        outInfo->ownerRefCount = record->ownerRefCount;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

/* 失效前先确认不存在被强引用的插件模块；之后撤销缓存、句柄和描述符借用。 */
TZrBool ZrLibrary_NativeRegistry_InvalidateDescriptorPluginSource(SZrGlobalState *global,
                                                                 const TZrChar *sourcePath) {
    ZrLibrary_NativeRegistryState *registry;
    SZrState *state;
    TZrBool matched = ZR_FALSE;

    if (global == ZR_NULL || sourcePath == ZR_NULL) {
        return ZR_FALSE;
    }

    registry = native_registry_get(global);
    state = global->mainThreadState;
    if (registry == ZR_NULL || !registry->moduleRecords.isValid) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < registry->moduleRecords.length; index++) {
        const ZrLibRegisteredModuleRecord *record =
            (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index);
        if (record != ZR_NULL &&
            record->isDescriptorPlugin &&
            native_registry_source_paths_equal(record->sourcePath, sourcePath)) {
            matched = ZR_TRUE;
            break;
        }
    }

    /* TODO: 即使 sourcePath 未命中，任一插件也会使此调用继续清空全部插件；
     * 多插件热重载场景需核对失效范围与缓存依赖是否要求如此。 */
    if (!matched) {
        for (TZrSize index = 0; index < registry->moduleRecords.length; index++) {
            const ZrLibRegisteredModuleRecord *record =
                (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index);
            if (record != ZR_NULL && record->isDescriptorPlugin) {
                matched = ZR_TRUE;
                break;
            }
        }
    }

    if (!matched) {
        return ZR_FALSE;
    }

    {
        const ZrLibRegisteredModuleRecord *liveRecord =
                native_registry_find_live_descriptor_plugin_record(registry);
        if (liveRecord != ZR_NULL) {
            native_registry_set_descriptor_plugin_in_use_error(registry, liveRecord, "invalidate");
            return ZR_FALSE;
        }
    }

    /* 先撤销模块缓存里的对象，再释放提供其回调/描述符的动态句柄。 */
    if (state != ZR_NULL) {
        for (TZrSize index = 0; index < registry->moduleRecords.length; index++) {
            const ZrLibRegisteredModuleRecord *record =
                (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index);

            if (record == ZR_NULL || !record->isDescriptorPlugin || record->moduleName == ZR_NULL) {
                continue;
            }

            {
                SZrString *moduleName = native_binding_create_string(state, record->moduleName);
                if (moduleName != ZR_NULL) {
                    ZrCore_Module_RemoveFromCache(state, moduleName);
                }
            }
        }
    }

    if (registry->pluginHandles.isValid) {
        for (TZrSize index = 0; index < registry->pluginHandles.length; index++) {
            ZrLibPluginHandleRecord *handleRecord =
                (ZrLibPluginHandleRecord *)ZrCore_Array_Get(&registry->pluginHandles, index);
            if (handleRecord != ZR_NULL) {
                native_registry_release_plugin_handle_record(global, handleRecord);
            }
        }
        registry->pluginHandles.length = 0;
    }

    for (TZrSize index = registry->moduleRecords.length; index > 0; index--) {
        ZrLibRegisteredModuleRecord *record =
            (ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index - 1);

        if (record == ZR_NULL || !record->isDescriptorPlugin) {
            continue;
        }

        if (record->moduleName != ZR_NULL) {
            global->allocator(global->userAllocationArguments,
                              record->moduleName,
                              strlen(record->moduleName) + 1,
                              0,
                              ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            record->moduleName = ZR_NULL;
        }

        if (record->sourcePath != ZR_NULL) {
            global->allocator(global->userAllocationArguments,
                              record->sourcePath,
                              strlen(record->sourcePath) + 1,
                              0,
                              ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            record->sourcePath = ZR_NULL;
        }

        if (index < registry->moduleRecords.length) {
            memmove(registry->moduleRecords.head + (index - 1) * registry->moduleRecords.elementSize,
                    registry->moduleRecords.head + index * registry->moduleRecords.elementSize,
                    (registry->moduleRecords.length - index) * registry->moduleRecords.elementSize);
        }
        registry->moduleRecords.length--;
    }

    native_registry_clear_error(registry);
    return ZR_TRUE;
}

/* 最近错误只属于当前注册表操作周期，不跨下一次成功注册保持。 */
EZrLibNativeRegistryErrorCode ZrLibrary_NativeRegistry_GetLastErrorCode(SZrGlobalState *global) {
    ZrLibrary_NativeRegistryState *registry = native_registry_get(global);
    return registry != ZR_NULL ? registry->lastErrorCode : ZR_LIB_NATIVE_REGISTRY_ERROR_NONE;
}

/* 返回注册表内部消息缓冲区，后续注册/加载会覆盖该借用字符串。 */
const TZrChar *ZrLibrary_NativeRegistry_GetLastErrorMessage(SZrGlobalState *global) {
    ZrLibrary_NativeRegistryState *registry = native_registry_get(global);
    return registry != ZR_NULL ? registry->lastErrorMessage : ZR_NULL;
}

/* 提示侧车的协议版本与 public header 常量保持一致。 */
const TZrChar *ZrLibrary_NativeHints_GetSchemaId(void) {
    return ZR_VM_NATIVE_HINTS_SCHEMA_ID;
}

/* 提示 JSON 随描述符生命周期存在；本入口不复制或解析。 */
const TZrChar *ZrLibrary_NativeHints_GetModuleJson(const ZrLibModuleDescriptor *descriptor) {
    return descriptor != ZR_NULL ? descriptor->typeHintsJson : ZR_NULL;
}

/* 将提供者生成的 JSON 原样写入侧车，供不执行 VM 的工具读取。 */
TZrBool ZrLibrary_NativeHints_WriteSidecar(const ZrLibModuleDescriptor *descriptor, const TZrChar *outputPath) {
    FILE *file;

    if (descriptor == ZR_NULL || outputPath == ZR_NULL || descriptor->typeHintsJson == ZR_NULL) {
        return ZR_FALSE;
    }

    file = fopen(outputPath, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    /* BUG: 未检查短写及 fclose 错误；例如目标文件系统写满时仍报告成功。 */
    fwrite(descriptor->typeHintsJson, 1, strlen(descriptor->typeHintsJson), file);
    fclose(file);
    return ZR_TRUE;
}
