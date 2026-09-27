//
// Created by HeJiahui on 2025/8/6.
//

#include "module/module_internal.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_core/call_binding.h"
#include "zr_vm_core/module_call_binding.h"
#include "zr_vm_core/gc.h"
#include "object/object_internal.h"

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
#endif

/* 新模块的反射身份不能复用零代际；跨线程创建共享这一递增源。 */
static volatile TZrUInt32 gNextModuleMetadataGeneration = 0u;

/* 元数据身份的零值留给未初始化状态，回绕时继续寻找非零值。 */
static TZrUInt32 zr_module_next_metadata_generation(void) {
    TZrUInt32 generation;

    do {
#if defined(ZR_PLATFORM_WIN)
        generation = (TZrUInt32) InterlockedIncrement((volatile LONG *) &gNextModuleMetadataGeneration);
#else
        generation = (TZrUInt32) __atomic_add_fetch(&gNextModuleMetadataGeneration, 1u, __ATOMIC_RELAXED);
#endif
    } while (generation == 0u);
    return generation;
}

/* 模块长期持有名称和路径，安装后向 GC 登记跨代引用。 */
static void zr_module_barrier_string_field(SZrState *state,
                                           struct SZrObjectModule *module,
                                           SZrString *stringValue) {
    if (state == ZR_NULL || module == ZR_NULL || stringValue == ZR_NULL) {
        return;
    }

    ZrCore_RawObject_Barrier(state,
                             ZR_CAST_RAW_OBJECT_AS_SUPER(module),
                             ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue));
}

/* protected 导出表绕过普通对象写入接口，键和值都需由模块持有并写屏障。 */
static void zr_module_barrier_hash_pair(SZrState *state,
                                        struct SZrObjectModule *module,
                                        SZrHashKeyValuePair *pair) {
    if (state == ZR_NULL || module == ZR_NULL || pair == ZR_NULL) {
        return;
    }

    ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(module), &pair->key);
    ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(module), &pair->value);
}

/* AOT 成员重编号只作用于 MemberDef 身份；其他 token 保持原表号。 */
static TZrBool zr_module_metadata_token_is_member_def(TZrMetadataToken token) {
    return token != 0u && ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_MEMBER_DEF ? ZR_TRUE : ZR_FALSE;
}

/* 注册表中的映射以有效目标 token 为前提，未匹配时保留源身份。 */
static TZrMetadataToken zr_module_remap_aot_member_token(const SZrAotCodeRegistration *codeRegistration,
                                                         TZrMetadataToken token) {
    if (codeRegistration == ZR_NULL ||
        !zr_module_metadata_token_is_member_def(token) ||
        codeRegistration->memberTokenRemapCount == 0u ||
        codeRegistration->memberTokenRemaps == ZR_NULL) {
        return token;
    }

    for (TZrUInt32 index = 0u; index < codeRegistration->memberTokenRemapCount; ++index) {
        const SZrAotMemberTokenRemap *remap = &codeRegistration->memberTokenRemaps[index];
        if (remap->sourceToken == token && zr_module_metadata_token_is_member_def(remap->targetToken)) {
            return remap->targetToken;
        }
    }

    return token;
}

/* 注册 AOT 元数据前对齐导出 symbol 的 token，供后续反射/导入按同一身份查找。 */
static void zr_module_apply_aot_member_token_remaps_to_exports(SZrFunction *metadataFunction,
                                                               const SZrAotCodeRegistration *codeRegistration) {
    if (metadataFunction == ZR_NULL ||
        metadataFunction->typedExportedSymbols == ZR_NULL ||
        metadataFunction->typedExportedSymbolLength == 0u) {
        return;
    }

    for (TZrUInt32 index = 0u; index < metadataFunction->typedExportedSymbolLength; ++index) {
        SZrFunctionTypedExportSymbol *symbol = &metadataFunction->typedExportedSymbols[index];
        symbol->metadataToken = zr_module_remap_aot_member_token(codeRegistration, symbol->metadataToken);
    }
}

/* 创建由 GC 管理的模块根；初始化公开/受保护两表和独立元数据代际。 */
struct SZrObjectModule *ZrCore_Module_Create(SZrState *state) {
    SZrObject *object;
    struct SZrObjectModule *module;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrCore_Object_NewCustomized(state, sizeof(struct SZrObjectModule), ZR_OBJECT_INTERNAL_TYPE_MODULE);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    module = (struct SZrObjectModule *)object;
    module->moduleName = ZR_NULL;
    module->pathHash = 0;
    module->fullPath = ZR_NULL;
    module->initState = ZR_MODULE_INIT_STATE_UNINITIALIZED;
    module->reserved0 = 0;
    module->reserved1 = 0;
    module->metadataGeneration = zr_module_next_metadata_generation();
    module->exportDescriptors = ZR_NULL;
    module->exportDescriptorLength = 0;
    module->hasMetadataRuntime = ZR_FALSE;
    memset(&module->metadataRuntime, 0, sizeof(module->metadataRuntime));

    ZrCore_HashSet_Init(state, &module->super.nodeMap, ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2);
    ZrCore_HashSet_Construct(&module->proNodeMap);
    ZrCore_HashSet_Init(state, &module->proNodeMap, ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2);
    return module;
}

/* 加载器、AOT 和项目模块共同设置可追踪的模块身份；字符串仍归 GC 管理。 */
void ZrCore_Module_SetInfo(SZrState *state,
                           struct SZrObjectModule *module,
                           SZrString *moduleName,
                           TZrUInt64 pathHash,
                           SZrString *fullPath) {
    if (state == ZR_NULL || module == ZR_NULL) {
        return;
    }

    module->moduleName = moduleName;
    module->pathHash = pathHash;
    module->fullPath = fullPath;
    zr_module_barrier_string_field(state, module, moduleName);
    zr_module_barrier_string_field(state, module, fullPath);
}

/* 公开导出同时进入对象属性和本模块的 protected 表，供运行时访问与内部解析复用。 */
void ZrCore_Module_AddPubExport(SZrState *state,
                                struct SZrObjectModule *module,
                                SZrString *name,
                                const SZrTypeValue *value) {
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL || value == ZR_NULL) {
        return;
    }

    zr_module_init_string_key(state, &key, name);
    ZrCore_Object_SetValue(state, &module->super, &key, value);

    pair = ZrCore_HashSet_Find(state, &module->proNodeMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrCore_HashSet_Add(state, &module->proNodeMap, &key);
        if (pair == ZR_NULL) {
            return;
        }
    }
    ZrCore_Value_Copy(state, &pair->value, value);
    zr_module_barrier_hash_pair(state, module, pair);
    ZrCore_GarbageCollector_MarkValueEscaped(state,
                                             value,
                                             ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT,
                                             ZR_GC_SCOPE_DEPTH_NONE,
                                             ZR_GARBAGE_COLLECT_PROMOTION_REASON_MODULE_ROOT);
}

/* protected 导出仅对模块内部与编译期投影可见，不写入普通对象公开属性。 */
void ZrCore_Module_AddProExport(SZrState *state,
                                struct SZrObjectModule *module,
                                SZrString *name,
                                const SZrTypeValue *value) {
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL || value == ZR_NULL) {
        return;
    }

    zr_module_init_string_key(state, &key, name);
    pair = ZrCore_HashSet_Find(state, &module->proNodeMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrCore_HashSet_Add(state, &module->proNodeMap, &key);
        if (pair == ZR_NULL) {
            return;
        }
    }
    ZrCore_Value_Copy(state, &pair->value, value);
    zr_module_barrier_hash_pair(state, module, pair);
    ZrCore_GarbageCollector_MarkValueEscaped(state,
                                             value,
                                             ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT,
                                             ZR_GC_SCOPE_DEPTH_NONE,
                                             ZR_GARBAGE_COLLECT_PROMOTION_REASON_MODULE_ROOT);
}

/* 返回模块对象持有的公开值借用指针；调用方不得越过 GC/表更新保存地址。 */
const SZrTypeValue *ZrCore_Module_GetPubExport(SZrState *state,
                                               struct SZrObjectModule *module,
                                               SZrString *name) {
    SZrTypeValue key;

    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    zr_module_init_string_key(state, &key, name);
    return ZrCore_Object_GetValue(state, &module->super, &key);
}

/* 内部类型解析读取 protected 表；返回值由模块哈希表持有。 */
const SZrTypeValue *ZrCore_Module_GetProExport(SZrState *state,
                                               struct SZrObjectModule *module,
                                               SZrString *name) {
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    zr_module_init_string_key(state, &key, name);
    pair = ZrCore_HashSet_Find(state, &module->proNodeMap, &key);
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }
    return &pair->value;
}

/* 将完整路径的原始字节映射为缓存/模块身份哈希；空路径使用零哨兵。 */
TZrUInt64 ZrCore_Module_CalculatePathHash(SZrState *state, SZrString *fullPath) {
    TZrNativeString pathStr;
    TZrSize pathLen;

    ZR_UNUSED_PARAMETER(state);
    if (fullPath == ZR_NULL) {
        return 0;
    }

    if (fullPath->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        pathStr = ZrCore_String_GetNativeStringShort(fullPath);
        pathLen = fullPath->shortStringLength;
    } else {
        pathStr = *ZrCore_String_GetNativeStringLong(fullPath);
        pathLen = fullPath->longStringLength;
    }

    if (pathStr == ZR_NULL || pathLen == 0) {
        return 0;
    }

    return XXH3_64bits(pathStr, pathLen);
}

/* 只接受注册表中真实模块对象，防止路径键被其他对象值冒充。 */
struct SZrObjectModule *ZrCore_Module_GetFromCache(SZrState *state, SZrString *path) {
    SZrObject *registry;
    SZrTypeValue key;
    const SZrTypeValue *cachedValue;
    SZrObject *cachedObject;

    if (state == ZR_NULL || path == ZR_NULL) {
        return ZR_NULL;
    }

    registry = zr_module_get_loaded_modules_registry(state);
    if (registry == ZR_NULL) {
        return ZR_NULL;
    }

    zr_module_init_string_key(state, &key, path);
    cachedValue = ZrCore_Object_GetValue(state, registry, &key);
    if (cachedValue == ZR_NULL || cachedValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    cachedObject = ZR_CAST_OBJECT(state, cachedValue->value.object);
    if (cachedObject == ZR_NULL || cachedObject->internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        return ZR_NULL;
    }

    return (struct SZrObjectModule *)cachedObject;
}

/* 缓存可先登记 INITIALIZING 模块，使循环导入看到同一个模块身份。 */
void ZrCore_Module_AddToCache(SZrState *state, SZrString *path, struct SZrObjectModule *module) {
    SZrObject *registry;
    SZrTypeValue key;
    SZrTypeValue moduleValue;

    if (state == ZR_NULL || path == ZR_NULL || module == ZR_NULL) {
        return;
    }

    registry = zr_module_get_loaded_modules_registry(state);
    if (registry == ZR_NULL) {
        return;
    }

    zr_module_init_string_key(state, &key, path);
    zr_module_init_object_value(state, &moduleValue, ZR_CAST_RAW_OBJECT_AS_SUPER(module));
    ZrCore_Object_SetValue(state, registry, &key, &moduleValue);
}

/* 从缓存移除前让函数图目标过期，再撤销哈希表引用，防止旧 provider 被继续调用。 */
void ZrCore_Module_RemoveFromCache(SZrState *state, SZrString *path) {
    SZrObject *registry;
    SZrTypeValue key;
    SZrObjectModule *module;
    SZrFunction *function;

    if (state == ZR_NULL || path == ZR_NULL) {
        return;
    }

    registry = zr_module_get_loaded_modules_registry(state);
    if (registry == ZR_NULL) {
        return;
    }

    module = ZrCore_Module_GetFromCache(state, path);
    function = ZrCore_CallBinding_GetModuleFunction(state, module);
    if (function != ZR_NULL && !ZrCore_CallBinding_AdvanceGeneration(function)) return;
    zr_module_init_string_key(state, &key, path);
    object_reset_hot_field_pair_cache(registry);
    ZrCore_HashSet_Remove(state, &registry->nodeMap, &key);
}

/* 导出状态表按字符串身份查询，供访问检查与导入签名校验共享。 */
const SZrModuleExportDescriptor *ZrCore_Module_FindExportDescriptor(struct SZrObjectModule *module, SZrString *name) {
    TZrUInt32 index;

    if (module == ZR_NULL || name == ZR_NULL || module->exportDescriptors == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < module->exportDescriptorLength; ++index) {
        SZrModuleExportDescriptor *descriptor = &module->exportDescriptors[index];
        if (descriptor->name == name || (descriptor->name != ZR_NULL && ZrCore_String_Equal(descriptor->name, name))) {
            return descriptor;
        }
    }

    return ZR_NULL;
}

/* 仅模块加载阶段修改描述符；返回地址属于模块的可替换原生数组。 */
SZrModuleExportDescriptor *ZrCore_Module_FindExportDescriptorMutable(struct SZrObjectModule *module, SZrString *name) {
    return (SZrModuleExportDescriptor *)ZrCore_Module_FindExportDescriptor(module, name);
}

/* 加载前登记导出可见性和就绪契约，同名注册覆盖旧描述符以支持重新发布。 */
TZrBool ZrCore_Module_RegisterExportDescriptor(SZrState *state,
                                               struct SZrObjectModule *module,
                                               const SZrModuleExportDescriptor *descriptor) {
    SZrModuleExportDescriptor *newDescriptors;
    TZrUInt32 newLength;
    TZrUInt32 index;

    if (state == ZR_NULL || module == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < module->exportDescriptorLength; ++index) {
        SZrModuleExportDescriptor *existing = &module->exportDescriptors[index];
        if (existing->name == descriptor->name ||
            (existing->name != ZR_NULL && ZrCore_String_Equal(existing->name, descriptor->name))) {
            *existing = *descriptor;
            zr_module_barrier_string_field(state, module, existing->name);
            return ZR_TRUE;
        }
    }

    newLength = module->exportDescriptorLength + 1;
    newDescriptors = (SZrModuleExportDescriptor *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                                   sizeof(*newDescriptors) * newLength,
                                                                                   ZR_MEMORY_NATIVE_TYPE_OBJECT);
    if (newDescriptors == ZR_NULL) {
        return ZR_FALSE;
    }

    if (module->exportDescriptors != ZR_NULL && module->exportDescriptorLength > 0) {
        ZrCore_Memory_RawCopy(newDescriptors,
                              module->exportDescriptors,
                              sizeof(*newDescriptors) * module->exportDescriptorLength);
        ZrCore_Memory_RawFreeWithType(state->global,
                                      module->exportDescriptors,
                                      sizeof(*newDescriptors) * module->exportDescriptorLength,
                                      ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }

    newDescriptors[module->exportDescriptorLength] = *descriptor;
    module->exportDescriptors = newDescriptors;
    module->exportDescriptorLength = newLength;
    zr_module_barrier_string_field(state, module, newDescriptors[newLength - 1].name);
    return ZR_TRUE;
}

/* entry 执行与循环导入解锁通过此位控制，缺少描述符时无操作。 */
void ZrCore_Module_SetExportDescriptorReady(struct SZrObjectModule *module, struct SZrString *name, TZrBool isReady) {
    SZrModuleExportDescriptor *descriptor;

    if (module == ZR_NULL || name == ZR_NULL) {
        return;
    }

    descriptor = ZrCore_Module_FindExportDescriptorMutable(module, name);
    if (descriptor != ZR_NULL) {
        descriptor->isReady = isReady ? 1 : 0;
    }
}

/* 导入器在缓存可见期间发布 INITIALIZING、READY 或 FAILED 状态。 */
void ZrCore_Module_SetInitializationState(struct SZrObjectModule *module, EZrModuleInitializationState state) {
    if (module == ZR_NULL) {
        return;
    }

    module->initState = (TZrUInt8)state;
}

/* AOT 注册或重载时替换模块内嵌元数据运行时；返回指针由模块持有。 */
SZrMetadataRuntime *ZrCore_Module_AttachMetadataRuntime(SZrObjectModule *module,
                                                        SZrFunction *metadataFunction,
                                                        const SZrAotCodeRegistration *codeRegistration) {
    SZrFunction *previousMetadataFunction;

    if (module == ZR_NULL || codeRegistration == ZR_NULL) {
        return ZR_NULL;
    }

    previousMetadataFunction = module->hasMetadataRuntime ? module->metadataRuntime.metadataFunction : ZR_NULL;
    /* BUG: 重挂先改变反射身份代际，再尝试可能失败的函数图失效；失败返回时
     * 旧 metadataRuntime 仍在，但 reflection type identity 已按新代际导出。 */
    if (module->hasMetadataRuntime) {
        if (module->metadataGeneration == UINT32_MAX) {
            module->metadataGeneration = 1u;
        } else {
            ++module->metadataGeneration;
            if (module->metadataGeneration == 0u) {
                module->metadataGeneration = 1u;
            }
        }
    }
    if (previousMetadataFunction != ZR_NULL &&
        !ZrCore_CallBinding_AdvanceGeneration(previousMetadataFunction)) return ZR_NULL;
    memset(&module->metadataRuntime, 0, sizeof(module->metadataRuntime));
    module->metadataRuntime.module = module;
    module->metadataRuntime.metadataFunction = metadataFunction;
    module->metadataRuntime.codeRegistration = codeRegistration;
    module->metadataRuntime.functionCount = codeRegistration->functionCount;
    module->metadataRuntime.methodInfoCount = codeRegistration->methodInfoCount;
    module->metadataRuntime.methodTokenCount = codeRegistration->methodTokenCount;
    module->metadataRuntime.memberTokenRemapCount = codeRegistration->memberTokenRemapCount;
    module->metadataRuntime.manifestExports = codeRegistration->manifestExports;
    module->metadataRuntime.manifestExportCount = codeRegistration->manifestExportCount;
    module->metadataRuntime.invokerCount = codeRegistration->invokerCount;
    module->metadataRuntime.typeLayoutCount = codeRegistration->typeLayoutCount;
    module->metadataRuntime.typeLayoutTokenCount = codeRegistration->typeLayoutTokenCount;
    module->metadataRuntime.gcDescriptorCount = codeRegistration->gcDescriptorCount;
    zr_module_apply_aot_member_token_remaps_to_exports(metadataFunction, codeRegistration);
    ZrCore_MetadataRuntime_AttachFunction(&module->metadataRuntime, metadataFunction);
    module->hasMetadataRuntime = ZR_TRUE;
    return &module->metadataRuntime;
}

/* 只在完成 Attach 后暴露模块内嵌运行时；返回借用指针。 */
SZrMetadataRuntime *ZrCore_Module_GetMetadataRuntime(SZrObjectModule *module) {
    if (module == ZR_NULL || !module->hasMetadataRuntime) {
        return ZR_NULL;
    }

    return &module->metadataRuntime;
}
