//
// zr.system.vm callbacks.
//

#include "zr_vm_lib_system/vm.h"

#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/native_registry.h"

#include <stdlib.h>
#include <string.h>

/**
 * ## Summary
 * 将有符号整数投影写入新建的 VM 结构对象；字段和值的复制交由 native object setter 完成。
 *
 * ## Parameters
 * @param[in] state 当前 VM state，借用至 setter 返回。
 * @param[in] object 待填充的 VM 对象；须在本次写入期间保持有效。
 * @param[in] fieldName 非空 NUL 结尾字段名，借用至 setter 返回。
 * @param[in] value 已按调用方定义转换的有符号值。
 *
 * ## Returns
 * 无返回值；空参数时不写入。`ZrLib_Object_SetFieldCString` 为 void，写入失败不会反馈给调用者。
 *
 * ## Constraints and lifecycle
 * 只用于尚未发布的快照对象。setter 会临时固定对象和值，并创建字段键；底层软失败可留下缺字段。
 *
 * ## References
 * 由 `system_vm_make_state` 与 `system_vm_make_loaded_module_info` 填充声明类型的字段。
 */
static void system_vm_write_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetInt(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

/**
 * ## Summary
 * 将布尔值投影写入新建的 VM 结构对象；不自行创建键或返回字段写入状态。
 *
 * ## Parameters
 * @param[in] state 当前 VM state，借用至 setter 返回。
 * @param[in] object 待填充的 VM 对象，须在调用期间保持有效。
 * @param[in] fieldName 非空 NUL 结尾字段名，借用至 setter 返回。
 * @param[in] value 要写入的布尔值。
 *
 * ## Returns
 * 无返回值；空参数或底层 void setter 的失败都表现为字段未写入。
 *
 * ## Constraints and lifecycle
 * 面向本模块尚未发布的快照对象。setter 会临时固定相关对象；新对象 nodeMap 初始化或忽略根登记失败可使写入静默落空。
 *
 * ## References
 * 由 `system_vm_make_loaded_module_info` 写入类型提示和插件身份字段。
 */
static void system_vm_write_bool_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetBool(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

/**
 * ## Summary
 * 将 NUL 结尾文本复制为 VM string，再尝试写入快照对象字段。
 *
 * ## Parameters
 * @param[in] state 当前 VM state；字符串创建和字段写入期间借用。
 * @param[in] object 尚未发布的目标 VM 对象，须在调用期间保持有效。
 * @param[in] fieldName 非空 NUL 结尾字段名，借用至 setter 返回。
 * @param[in] value 非空 NUL 结尾源文本；字符串 API 按 C 字符串长度读取，借用至 `ZrLib_Value_SetString` 返回。
 *
 * ## Returns
 * 无返回值。该实现未检查 `ZrLib_Value_SetString` 是否成功；短串驻留表扩容软失败时 `fieldValue` 未初始化，随后 void setter 仍读取它。
 *
 * ## Constraints and lifecycle
 * 仅在字符串对象构造期间源文本保持有效时调用。BUG: `LoadedModules` 可把固定短字面值 `sourceKind` 传入此处；当短串缓存扩容的 `ZrCore_Memory_Allocate` 返回 null 时，创建返回 null 而不会抛异常或写入 `fieldValue`，本函数继续传递未初始化值并发布记录。该范围不包括会抛 VM 异常的 `GcMalloc` 路径。
 *
 * ## References
 * `system_vm_make_loaded_module_info` 是本 helper 的调用方；字符串写入经过 `ZrLib_Value_SetString`、`ZrCore_String_CreateTryHitCache` 和 `ZrLib_Object_SetFieldCString`。
 */
static void system_vm_write_string_field(SZrState *state,
                                         SZrObject *object,
                                         const TZrChar *fieldName,
                                         const TZrChar *value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetString(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

/**
 * ## Summary
 * 从原生 callback 的参数槽读取 VM string，并返回其 C 字符串视图供同步调用使用。
 *
 * ## Parameters
 * @param[in] context 当前 callback 上下文；字符串对象由参数槽保持可达。
 * @param[in] index 参数的零基索引。
 * @param[out] outText 接收借用的 NUL 结尾文本指针；调用者不得释放，且不得越过源 VM 字符串有效期保存。
 *
 * ## Returns
 * @return 参数读取成功且 native string 非空时为真；无效 context/output 或读取返回空值时为假。注册参数类型错误可由 `ReadString` 走 VM 异常路径。
 *
 * ## Constraints and lifecycle
 * 只在 callback 同步窗口内使用输出视图；若之后可能触发会搬迁字符串的 GC，调用者须先建立受支持的根或复制文本。
 *
 * ## References
 * `ZrSystem_Vm_CallModuleExport` 用索引 0、1 读取模块名和导出名；底层读取由 `ZrLib_CallContext_ReadString` 提供。
 */
static TZrBool system_vm_read_string_argument(const ZrLibCallContext *context,
                                              TZrSize index,
                                              const TZrChar **outText) {
    SZrString *textString = ZR_NULL;

    if (context == ZR_NULL || outText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_CallContext_ReadString(context, index, &textString) || textString == ZR_NULL) {
        return ZR_FALSE;
    }

    *outText = ZrCore_String_GetNativeString(textString);
    return *outText != ZR_NULL;
}

/**
 * ## Summary
 * 统计 `global->loadedModulesRegistry` 中当前缓存的实例条目；注册但未导入的 native descriptor 不计数。
 *
 * ## Parameters
 * @param[in] state 用于访问所属 global 的当前 VM state。
 *
 * ## Returns
 * @return 当前 nodeMap 链表项数量转换为 `TZrInt64`；state、global、缓存对象或有效 nodeMap 不存在时返回零。
 *
 * ## Constraints and lifecycle
 * 该遍历只读共享 global registry，不触发导入；调用期间 registry 结构和 pair 链必须保持可遍历。TODO: 核对长期运行时计数超出 `TZrInt64` 可表示范围的约定。
 * TODO: 同 global 多 mutator 可经 Module_AddToCache 改写 registry；沿 State_MutatorLaunch 核实调用方串行化或其他同步协议。
 *
 * ## References
 * 仅由 `system_vm_make_state` 调用；实例缓存由 Core module loader 管理。
 */
static TZrInt64 system_vm_loaded_module_count(SZrState *state) {
    SZrObject *registry;
    TZrSize bucket;
    TZrSize count = 0;

    if (state == ZR_NULL || state->global == ZR_NULL ||
        !ZrCore_Value_IsGarbageCollectable(&state->global->loadedModulesRegistry) ||
        state->global->loadedModulesRegistry.type != ZR_VALUE_TYPE_OBJECT) {
        return 0;
    }

    registry = ZR_CAST_OBJECT(state, state->global->loadedModulesRegistry.value.object);
    if (registry == ZR_NULL || !registry->nodeMap.isValid) {
        return 0;
    }

    for (bucket = 0; bucket < registry->nodeMap.capacity; bucket++) {
        SZrHashKeyValuePair *pair = registry->nodeMap.buckets[bucket];
        while (pair != ZR_NULL) {
            count++;
            pair = pair->next;
        }
    }

    return (TZrInt64)count;
}

/**
 * ## Summary
 * 创建 `SystemVmState` VM 对象，并把已加载模块数、collector 数据及当前线程栈/帧深度写成值字段。
 *
 * ## Parameters
 * @param[in] state 当前执行线程 state；对象由该 state 所属 VM 管理。
 * @param[in] loadedModuleCount 已由调用方读取的模块实例数值快照。
 *
 * ## Returns
 * @return 类型实例构造成功时返回 VM 管理的对象；state 无效或原型/对象构造失败时返回 null。字段写入是 void 操作，返回非空不保证六字段全部存在。
 *
 * ## Constraints and lifecycle
 * GC 值来自 global collector，栈/帧来自当前 state，读取和字段物化不是原子事务。
 * TODO: gcMode/debt/threshold 是共享字段；沿 Core GC 写入口和 mutator 协议核实读取同步，非原子说明不等于读写已受保护。
 * BUG: 初始 nodeMap 分配及首个 setter 的重试都软失败，或第四个字段跨越容量 4 的 3/4 负载阈值而桶扩容失败时，void setter 静默跳过字段，本函数仍发布对象；ignored-root pin 表扩容软失败也会独立造成缺字段。
 *
 * ## References
 * 由 `ZrSystem_Vm_State` 调用；字段布局由 `kVmStateFields` 声明。
 */
static SZrObject *system_vm_make_state(SZrState *state, TZrInt64 loadedModuleCount) {
    SZrObject *object;
    SZrGarbageCollector *garbageCollector;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Type_NewInstance(state, "SystemVmState");
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    garbageCollector = state->global != ZR_NULL ? state->global->garbageCollector : ZR_NULL;
    system_vm_write_int_field(state, object, "loadedModuleCount", loadedModuleCount);
    system_vm_write_int_field(state,
                              object,
                              "garbageCollectionMode",
                              garbageCollector != ZR_NULL ? (TZrInt64)garbageCollector->gcMode : 0);
    system_vm_write_int_field(state,
                              object,
                              "garbageCollectionDebt",
                              garbageCollector != ZR_NULL ? (TZrInt64)ZrCore_GarbageCollector_GetDebtSpace(garbageCollector) : 0);
    system_vm_write_int_field(state,
                              object,
                              "garbageCollectionThreshold",
                              garbageCollector != ZR_NULL ? (TZrInt64)garbageCollector->gcPauseThresholdPercent : 0);
    system_vm_write_int_field(state,
                              object,
                              "stackDepth",
                              (TZrInt64)(state->stackTop.valuePointer - state->stackBase.valuePointer));
    system_vm_write_int_field(state, object, "frameDepth", (TZrInt64)state->callInfoListLength);
    return object;
}

/**
 * ## Summary
 * 从已加载模块实例和 NativeRegistry 描述符信息组装 `SystemLoadedModuleInfo` 快照；注册来源与插件标志以 registry 信息为准。
 *
 * ## Parameters
 * @param[in] state 当前 VM state，负责创建并管理返回对象。
 * @param[in] name 模块名文本；非空时写入 name 字段，调用期间借用。
 * @param[in] sourceKind 模块来源类别文本，通常是 `native` 或 `source`。
 * @param[in] sourcePath 注册路径/原生模块标识文本；可能为空并因此不写字段。
 * @param[in] registrationKind 注册方式文本；可能为空并因此不写字段。
 * @param[in] hasTypeHints 是否有可用的 hint 元数据。
 * @param[in] moduleVersion 描述符版本文本；为空时省略字段。
 * @param[in] runtimeAbiVersion 描述符最低 ABI 版本或运行时默认值。
 * @param[in] requiredCapabilities 描述符能力位掩码，写入 VM signed integer 字段。
 * @param[in] isDescriptorPlugin 是否来自动态 descriptor plugin。
 *
 * ## Returns
 * @return 实例构造成功时返回 VM 管理对象；构造失败返回 null。逐字段 setter 为 void，部分写入失败不会改变返回对象状态。
 *
 * ## Constraints and lifecycle
 * 文本指针由 registry/模块对象借用，调用方须保证它们在各字符串创建调用期间有效。fieldName/type 元数据来自静态 descriptor。
 * BUG: nodeMap 初始分配及首个 setter 的重试都软失败，或第四个已插入字段跨越容量 4 的 3/4 负载阈值且桶扩容软失败时，void setter 不报告缺字段，本函数仍返回对象；ignored-root pin 表扩容软失败是另一条独立缺字段路径。
 *
 * ## References
 * 由 `ZrSystem_Vm_LoadedModules` 调用，布局见 `kLoadedModuleInfoFields`；descriptor 来源查询见 `ZrLibrary_NativeRegistry_GetModuleInfo`。
 */
static SZrObject *system_vm_make_loaded_module_info(SZrState *state,
                                                    const TZrChar *name,
                                                    const TZrChar *sourceKind,
                                                    const TZrChar *sourcePath,
                                                    const TZrChar *registrationKind,
                                                    TZrBool hasTypeHints,
                                                    const TZrChar *moduleVersion,
                                                    TZrUInt32 runtimeAbiVersion,
                                                    TZrUInt64 requiredCapabilities,
                                                    TZrBool isDescriptorPlugin) {
    SZrObject *object;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Type_NewInstance(state, "SystemLoadedModuleInfo");
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    if (name != ZR_NULL) {
        system_vm_write_string_field(state, object, "name", name);
    }
    if (sourceKind != ZR_NULL) {
        system_vm_write_string_field(state, object, "sourceKind", sourceKind);
    }
    if (sourcePath != ZR_NULL) {
        system_vm_write_string_field(state, object, "sourcePath", sourcePath);
    }
    if (registrationKind != ZR_NULL) {
        system_vm_write_string_field(state, object, "registrationKind", registrationKind);
    }
    system_vm_write_bool_field(state, object, "hasTypeHints", hasTypeHints);
    if (moduleVersion != ZR_NULL) {
        system_vm_write_string_field(state, object, "moduleVersion", moduleVersion);
    }
    system_vm_write_int_field(state, object, "runtimeAbiVersion", (TZrInt64)runtimeAbiVersion);
    system_vm_write_int_field(state, object, "requiredCapabilities", (TZrInt64)requiredCapabilities);
    system_vm_write_bool_field(state, object, "isDescriptorPlugin", isDescriptorPlugin);
    return object;
}

TZrBool ZrSystem_Vm_LoadedModules(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *array;
    SZrObject *registry;
    TZrSize bucket;

    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL || context->state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    array = ZrLib_Array_New(context->state);
    if (array == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrCore_Value_IsGarbageCollectable(&context->state->global->loadedModulesRegistry) ||
        context->state->global->loadedModulesRegistry.type != ZR_VALUE_TYPE_OBJECT) {
        ZrLib_Value_SetObject(context->state, result, array, ZR_VALUE_TYPE_ARRAY);
        return ZR_TRUE;
    }

    registry = ZR_CAST_OBJECT(context->state, context->state->global->loadedModulesRegistry.value.object);
    /* TODO: 此处取出的 registry、pair、module/name/sourcePath/descriptor 均跨越后续 VM 分配；
     * 核对分配触发搬迁 GC 时这些借用地址及静态 plugin image 的驻留期。
     * 同 global 多 mutator 导入会改 registry；沿 Module_AddToCache/GC MutationBegin 核实是否需外层串行化。 */
    for (bucket = 0; registry != ZR_NULL && bucket < registry->nodeMap.capacity; bucket++) {
        SZrHashKeyValuePair *pair = registry->nodeMap.buckets[bucket];
        while (pair != ZR_NULL) {
            const TZrChar *name =
                    pair->key.type == ZR_VALUE_TYPE_STRING
                            ? ZrCore_String_GetNativeString(ZR_CAST_STRING(context->state, pair->key.value.object))
                            : ZR_NULL;
            SZrObjectModule *module =
                    pair->value.type == ZR_VALUE_TYPE_OBJECT ? (SZrObjectModule *)pair->value.value.object : ZR_NULL;
            const TZrChar *sourcePath =
                    module != ZR_NULL && module->fullPath != ZR_NULL ? ZrCore_String_GetNativeString(module->fullPath) : name;
            const TZrChar *sourceKind =
                    (module != ZR_NULL &&
                     module->moduleName != ZR_NULL &&
                     module->fullPath != ZR_NULL &&
                     strcmp(ZrCore_String_GetNativeString(module->moduleName),
                            ZrCore_String_GetNativeString(module->fullPath)) == 0)
                            ? "native"
                            : "source";
            ZrLibRegisteredModuleInfo moduleInfo;
            const ZrLibModuleDescriptor *descriptor = ZR_NULL;
            const TZrChar *registrationKind = ZR_NULL;
            const TZrChar *metadataSourcePath = sourcePath;
            TZrBool isDescriptorPlugin = ZR_FALSE;

            memset(&moduleInfo, 0, sizeof(moduleInfo));
            if (name != ZR_NULL && ZrLibrary_NativeRegistry_GetModuleInfo(context->state->global, name, &moduleInfo)) {
                descriptor = moduleInfo.descriptor;
                metadataSourcePath = moduleInfo.sourcePath != ZR_NULL ? moduleInfo.sourcePath : metadataSourcePath;
                registrationKind =
                        moduleInfo.registrationKind == ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_DESCRIPTOR_PLUGIN
                                ? "descriptor-plugin"
                                : "builtin";
                isDescriptorPlugin = moduleInfo.isDescriptorPlugin;
            }
            SZrObject *info = system_vm_make_loaded_module_info(
                    context->state,
                    name,
                    sourceKind,
                    metadataSourcePath,
                    registrationKind,
                    descriptor != ZR_NULL && (descriptor->typeHintsJson != ZR_NULL || descriptor->typeHintCount > 0),
                    descriptor != ZR_NULL ? descriptor->moduleVersion : ZR_NULL,
                    descriptor != ZR_NULL && descriptor->minRuntimeAbi != 0 ? descriptor->minRuntimeAbi
                                                                            : ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
                    descriptor != ZR_NULL ? descriptor->requiredCapabilities : 0,
                    isDescriptorPlugin);

            if (info != ZR_NULL) {
                SZrTypeValue entryValue;
                ZrLib_Value_SetObject(context->state, &entryValue, info, ZR_VALUE_TYPE_OBJECT);
                /* BUG: PushValue 因 ignored-root pin registry 的 RawMallocWithType 软失败可返回 false；
                 * 这里忽略状态，仍发布数组并返回 true，导致某条已加载模块记录静默缺失。 */
                ZrLib_Array_PushValue(context->state, array, &entryValue);
            }

            pair = pair->next;
        }
    }

    ZrLib_Value_SetObject(context->state, result, array, ZR_VALUE_TYPE_ARRAY);
    return ZR_TRUE;
}

TZrBool ZrSystem_Vm_State(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *vmState;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    vmState = system_vm_make_state(context->state, system_vm_loaded_module_count(context->state));
    if (vmState == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, vmState, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrSystem_Vm_CallModuleExport(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *moduleName = ZR_NULL;
    const TZrChar *exportName = ZR_NULL;
    SZrObject *argumentsArray = ZR_NULL;
    SZrTypeValue *arguments = ZR_NULL;
    TZrSize argumentCount;
    TZrSize index;
    TZrBool success;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!system_vm_read_string_argument(context, 0, &moduleName) ||
        !system_vm_read_string_argument(context, 1, &exportName) ||
        !ZrLib_CallContext_ReadArray(context, 2, &argumentsArray)) {
        return ZR_FALSE;
    }

    /* 参数先从脚本数组借出再交给通用原生调用器，调用器负责目标导入、arity 和异常。 */
    argumentCount = ZrLib_Array_Length(argumentsArray);
    if (argumentCount > 0) {
        arguments = (SZrTypeValue *)malloc(sizeof(SZrTypeValue) * argumentCount);
        if (arguments == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    for (index = 0; index < argumentCount; index++) {
        const SZrTypeValue *item = ZrLib_Array_Get(context->state, argumentsArray, index);
        if (item == ZR_NULL) {
            free(arguments);
            return ZR_FALSE;
        }
        arguments[index] = *item;
    }

    /* TODO: 目标导入可能在 CallValue 将参数放上 VM 栈前触发 GC；需以 GC 压力测试确认
     * malloc 中的浅拷贝值及 moduleName/exportName 借用指针是否保持有效。 */
    success = ZrLib_CallModuleExport(context->state, moduleName, exportName, arguments, argumentCount, result);
    free(arguments);
    return success;
}
