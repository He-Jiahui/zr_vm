//
// zr.system.gc callbacks.
//

#include "zr_vm_lib_system/gc.h"

#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/string.h"

#include <string.h>

/**
 * ## Summary
 * 从原生 callback 上下文取得其 State 所属 global 的共享 collector。
 *
 * ## Parameters
 * @param[in] context 借用的 callback 上下文；须含有效 State、global 和 collector。
 * @param[out] outCollector 可写输出槽；仅成功时写入 global 持有 collector 的借用指针。
 *
 * ## Returns
 * @return ZR_TRUE 表示输出已写入；无效参数或归属链缺失时返回 ZR_FALSE 且不改输出。
 *
 * ## Constraints and lifecycle
 * collector 由 global 持有，调用期间 global 必须存活；本 helper 不加锁，同 global 的更新由宿主串行协调。
 *
 * ## References
 * Enable、Disable、SetHeapLimit、SetBudget 和 GetStats 共用此校验与取值入口。
 */
static TZrBool system_gc_get_collector(const ZrLibCallContext *context, SZrGarbageCollector **outCollector) {
    if (context == ZR_NULL || outCollector == ZR_NULL || context->state == ZR_NULL || context->state->global == ZR_NULL ||
        context->state->global->garbageCollector == ZR_NULL) {
        return ZR_FALSE;
    }

    *outCollector = context->state->global->garbageCollector;
    return ZR_TRUE;
}

/**
 * ## Summary
 * 将一个快照整数转换为 VM 值，并同步写入新建的 Stats 对象字段。
 *
 * ## Parameters
 * @param[in] state 有效的当前 VM State，供值转换、GC rooting 和字段键创建使用。
 * @param[in] object 仍由 VM 管理且在写入期间有效的 Stats 对象。
 * @param[in] fieldName NUL 结尾的借用字段名；本模块传入静态字面量。
 * @param[in] value 按值传入的快照整数；不保留调用方存储。
 *
 * ## Returns
 * void；setter 的软失败不向本 helper 报告，底层 VM 分配异常可非局部退出。
 *
 * ## Constraints and lifecycle
 * 临时 TypeValue 只在同步 setter 调用期间借用；void setter 可能因 pin 或字段桶软分配失败而漏写。
 *
 * ## References
 * make_stats_object 逐字段调用本 helper；ZrLib_Object_SetFieldCString 固定 operands 并转交 Object_SetValue。
 */
static void system_gc_write_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetInt(state, &fieldValue, value);
    /* BUG: void setter 无法上报两类静默遗漏：object/key pin 的 ignored-registry RawMalloc 失败，
     * 或新对象 42 项动态字段写入跨桶阈值时 HashSet bucket Rehash 的 RawOOM。 */
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

/**
 * ## Summary
 * 将 GetStats 的 enabled 快照值写为新 Stats 对象的首个布尔字段。
 *
 * ## Parameters
 * @param[in] state 有效的当前 VM State，供 VM 值转换和 rooting 使用。
 * @param[in] object 仍由 VM 管理且在同步写入期间有效的 Stats 对象。
 * @param[in] fieldName NUL 结尾的借用字段名；本调用点传入静态 enabled 字面量。
 * @param[in] value 按值传入的 stop-flag 投影结果。
 *
 * ## Returns
 * void；软失败不向调用者报告，底层 VM 分配异常可非局部退出。
 *
 * ## Constraints and lifecycle
 * 临时 TypeValue 仅借用到 setter 返回；首字段不跨动态字段表负载阈值，不触发 Rehash；Object_Init 初始桶建立或 setter 重试、object/key pin 仍可能失败。
 *
 * ## References
 * make_stats_object 在整数快照字段之前调用本 helper；setter 内部同步 pin 后写入对象。
 */
static void system_gc_write_bool_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetBool(state, &fieldValue, value);
    /* BUG: 首个 enabled 写入不会跨字段桶阈值；但 object/key pin 的 ignored-registry RawMalloc 失败仍会静默漏项。 */
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

/**
 * ## Summary
 * 读取控制 API 的整数参数，并将非负 int64 结果转换为内部无符号预算或字节数。
 *
 * ## Parameters
 * @param[in] context 借用的 callback context；注册 dispatcher 负责先检查参数个数。
 * @param[in] index 零基参数索引；当前两个调用点都使用 0。
 * @param[out] outValue 可写输出槽；仅成功读到非负值后写入。
 *
 * ## Returns
 * @return ZR_TRUE 表示结果已写入；空指针或转换后为负数时返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * ReadInt 的缺参/类型错误以 VM 异常退出，不是普通 false；其浮点窄化边界见函数内 TODO 与共享入口 BUG。
 *
 * ## References
 * SetHeapLimit 与 SetBudget 共用此 helper，底层读取由 ZrLib_CallContext_ReadInt 完成。
 */
static TZrBool system_gc_read_non_negative_int_argument(const ZrLibCallContext *context,
                                                        TZrSize index,
                                                        TZrUInt64 *outValue) {
    TZrInt64 value = 0;

    if (context == ZR_NULL || outValue == ZR_NULL ||
        !ZrLib_CallContext_ReadInt(context, index, &value) ||
        value < 0) {
        return ZR_FALSE;
    }

    /* TODO: ReadInt 对 UINT64 > INT64_MAX 的 signed 转换由实现定义；仅 int64 可表示的非负输入可靠。 */
    *outValue = (TZrUInt64)value;
    return ZR_TRUE;
}

/**
 * ## Summary
 * 将 collect callback 的可选脚本 token 解析为 Core collection kind。
 *
 * ## Parameters
 * @param[in] context 借用的 callback context；零参数表示采用默认 full。
 * @param[out] outKind 可写枚举输出；仅解析成功或应用默认值时写入。
 *
 * ## Returns
 * @return ZR_TRUE 表示 kind 已写入；未知 token 或无效 native view 返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * ReadString 的缺参/类型错误以 VM 异常退出；native 字符串视图借用 VM 字符串且可能含嵌入 NUL。
 *
 * ## References
 * Collect 调用本 helper；接受的 token 与全字节精确性要求见 gc.h 及函数内 BUG 记录。
 */
static TZrBool system_gc_read_collection_kind(const ZrLibCallContext *context,
                                              EZrGarbageCollectCollectionKind *outKind) {
    SZrString *kindString = ZR_NULL;
    const TZrChar *kindText = ZR_NULL;

    if (context == ZR_NULL || outKind == ZR_NULL) {
        return ZR_FALSE;
    }

    if (context->argumentCount == 0) {
        *outKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL;
        return ZR_TRUE;
    }

    if (!ZrLib_CallContext_ReadString(context, 0, &kindString) || kindString == ZR_NULL) {
        return ZR_FALSE;
    }

    kindText = ZrCore_String_GetNativeString(kindString);
    if (kindText == ZR_NULL) {
        return ZR_FALSE;
    }

    /* BUG: VM native-string view 可含内嵌 NUL；合法 token "full\x00tail" 会被 strcmp 当作 "full" 接受。
     * API 要求整段字节精确匹配 minor/major/full；此处需要长度感知比较。 */
    if (strcmp(kindText, "minor") == 0) {
        *outKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
        return ZR_TRUE;
    }
    if (strcmp(kindText, "major") == 0) {
        *outKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR;
        return ZR_TRUE;
    }
    if (strcmp(kindText, "full") == 0) {
        *outKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

/**
 * ## Summary
 * 按已解析的 kind 调用 Core 的 full 收集入口，或排程并推进一次 minor/major step。
 *
 * ## Parameters
 * @param[in] state 当前调用 State；其 global 必须在本次调用期间存活。
 * @param[in] kind 由 read_collection_kind 产生的有效 Core 收集枚举。
 *
 * ## Returns
 * void；Core full 超时或 minor/major 单步后是否完成周期不由此 helper 报告。
 *
 * ## Constraints and lifecycle
 * full 使用同步 GcFull；minor/major 仅 ScheduleCollection 后调用一次 GcStep，不承诺整轮完成。
 *
 * ## References
 * Collect 将该 helper 的副作用作为 callback 操作；Core 路径分别进入 GcFull 或 ScheduleCollection/GcStep。
 */
static void system_gc_run_collection(SZrState *state, EZrGarbageCollectCollectionKind kind) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return;
    }

    if (kind == ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL) {
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
        return;
    }

    ZrCore_GarbageCollector_ScheduleCollection(state->global, kind);
    ZrCore_GarbageCollector_GcStep(state);
}

/**
 * ## Summary
 * 先复制共享 collector 的统计快照，再构造并填充 VM 管理的 SystemGcStats 对象。
 *
 * ## Parameters
 * @param[in] state 有效且正在执行 callback 的 VM State，用于分配和管理结果对象。
 * @param[in] collector 借用的同一 global collector；本 helper 不取得所有权。
 *
 * ## Returns
 * @return 新建的 VM 管理对象，供调用方写入 callback result；无效输入或构造失败返回 ZR_NULL。
 *
 * ## Constraints and lifecycle
 * 快照先于对象/键/字段分配，后续物化可触发 GC；setter 软失败可能留下缺字段，硬 VM 异常可非局部退出。
 *
 * ## References
 * GetStats 传入从 State global 取得的 collector，并立即把成功返回的对象交付给 VM result。
 */
static SZrObject *system_gc_make_stats_object(SZrState *state, SZrGarbageCollector *collector) {
    SZrObject *object;
    SZrGarbageCollectorStatsSnapshot snapshot;

    if (state == ZR_NULL || collector == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    memset(&snapshot, 0, sizeof(snapshot));
    /* 先快照再创建对象、字段键并写入动态字段；这些 VM/native 分配可触发后续 GC，查询会改变后续观测。
     * Core snapshot 不承诺与同 global 的 GC 更新并发安全；调用方须按宿主调度协调 mutator。
     * TODO: 核对 task worker 等多 State 同 global 调用入口的实际同步策略及可承诺快照一致性。 */
    ZrCore_GarbageCollector_GetStatsSnapshot(state->global, &snapshot);

    /* BUG: Type_NewInstance 可在 nodeMap 初始化软失败后返回对象；字段写入会重试 void Object_Init，若仍失败只记录日志，
     * 后续投影仍继续，GetStats 因而可能成功发布缺字段对象。 */
    object = ZrLib_Type_NewInstance(state, "SystemGcStats");
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    /* enabled 只反映 stop request flag，不代表 gcMode 或后台 worker 的实际运行状态。 */
    system_gc_write_bool_field(state, object, "enabled", !collector->stopGcFlag);
    system_gc_write_int_field(state, object, "heapLimitBytes", (TZrInt64)snapshot.heapLimitBytes);
    system_gc_write_int_field(state, object, "managedMemoryBytes", (TZrInt64)snapshot.managedMemoryBytes);
    system_gc_write_int_field(state, object, "gcDebtBytes", (TZrInt64)snapshot.gcDebtBytes);
    system_gc_write_int_field(state, object, "pauseBudgetUs", (TZrInt64)snapshot.pauseBudgetUs);
    system_gc_write_int_field(state, object, "remarkBudgetUs", (TZrInt64)snapshot.remarkBudgetUs);
    system_gc_write_int_field(state, object, "workerCount", (TZrInt64)snapshot.workerCount);
    system_gc_write_int_field(state, object, "ignoredObjectCount", (TZrInt64)snapshot.ignoredObjectCount);
    system_gc_write_int_field(state, object, "rememberedObjectCount", (TZrInt64)snapshot.rememberedObjectCount);
    system_gc_write_int_field(state, object, "regionCount", (TZrInt64)snapshot.regionCount);
    system_gc_write_int_field(state, object, "edenRegionCount", (TZrInt64)snapshot.edenRegionCount);
    system_gc_write_int_field(state, object, "survivorRegionCount", (TZrInt64)snapshot.survivorRegionCount);
    system_gc_write_int_field(state, object, "oldRegionCount", (TZrInt64)snapshot.oldRegionCount);
    system_gc_write_int_field(state, object, "pinnedRegionCount", (TZrInt64)snapshot.pinnedRegionCount);
    system_gc_write_int_field(state, object, "largeRegionCount", (TZrInt64)snapshot.largeRegionCount);
    system_gc_write_int_field(state, object, "permanentRegionCount", (TZrInt64)snapshot.permanentRegionCount);
    system_gc_write_int_field(state, object, "edenUsedBytes", (TZrInt64)snapshot.edenUsedBytes);
    system_gc_write_int_field(state, object, "survivorUsedBytes", (TZrInt64)snapshot.survivorUsedBytes);
    system_gc_write_int_field(state, object, "oldUsedBytes", (TZrInt64)snapshot.oldUsedBytes);
    system_gc_write_int_field(state, object, "pinnedUsedBytes", (TZrInt64)snapshot.pinnedUsedBytes);
    system_gc_write_int_field(state, object, "largeUsedBytes", (TZrInt64)snapshot.largeUsedBytes);
    system_gc_write_int_field(state, object, "permanentUsedBytes", (TZrInt64)snapshot.permanentUsedBytes);
    system_gc_write_int_field(state, object, "edenLiveBytes", (TZrInt64)snapshot.edenLiveBytes);
    system_gc_write_int_field(state, object, "survivorLiveBytes", (TZrInt64)snapshot.survivorLiveBytes);
    system_gc_write_int_field(state, object, "oldLiveBytes", (TZrInt64)snapshot.oldLiveBytes);
    system_gc_write_int_field(state, object, "pinnedLiveBytes", (TZrInt64)snapshot.pinnedLiveBytes);
    system_gc_write_int_field(state, object, "largeLiveBytes", (TZrInt64)snapshot.largeLiveBytes);
    system_gc_write_int_field(state, object, "permanentLiveBytes", (TZrInt64)snapshot.permanentLiveBytes);
    system_gc_write_int_field(state, object, "lastStepDurationUs", (TZrInt64)snapshot.lastStepDurationUs);
    system_gc_write_int_field(state, object, "lastStepWork", (TZrInt64)snapshot.lastStepWork);
    system_gc_write_int_field(state, object, "lastCollectionKind", (TZrInt64)snapshot.lastCollectionKind);
    system_gc_write_int_field(state,
                              object,
                              "lastRequestedCollectionKind",
                              (TZrInt64)snapshot.lastRequestedCollectionKind);
    system_gc_write_int_field(state, object, "collectionPhase", (TZrInt64)snapshot.collectionPhase);
    system_gc_write_int_field(state, object, "minorCollectionCount", (TZrInt64)snapshot.minorCollectionCount);
    system_gc_write_int_field(state, object, "majorCollectionCount", (TZrInt64)snapshot.majorCollectionCount);
    system_gc_write_int_field(state, object, "fullCollectionCount", (TZrInt64)snapshot.fullCollectionCount);
    system_gc_write_int_field(state,
                              object,
                              "minorCollectionTotalDurationUs",
                              (TZrInt64)snapshot.minorCollectionTotalDurationUs);
    system_gc_write_int_field(state,
                              object,
                              "majorCollectionTotalDurationUs",
                              (TZrInt64)snapshot.majorCollectionTotalDurationUs);
    system_gc_write_int_field(state,
                              object,
                              "fullCollectionTotalDurationUs",
                              (TZrInt64)snapshot.fullCollectionTotalDurationUs);
    system_gc_write_int_field(state,
                              object,
                              "minorCollectionMaxDurationUs",
                              (TZrInt64)snapshot.minorCollectionMaxDurationUs);
    system_gc_write_int_field(state,
                              object,
                              "majorCollectionMaxDurationUs",
                              (TZrInt64)snapshot.majorCollectionMaxDurationUs);
    system_gc_write_int_field(state,
                              object,
                              "fullCollectionMaxDurationUs",
                              (TZrInt64)snapshot.fullCollectionMaxDurationUs);
    return object;
}

/* descriptor 注册为零参；callback 切换共享 collector 模式并重新允许自动 step。 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_Enable(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrGarbageCollector *collector = ZR_NULL;

    if (result == ZR_NULL || !system_gc_get_collector(context, &collector)) {
        return ZR_FALSE;
    }

    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    collector->stopGcFlag = ZR_FALSE;
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

/* 只设置 stop request；不恢复模式、不取消当前 step，也不保证所有自动路径都读取该 flag。 */
TZrBool ZrSystem_Gc_Disable(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrGarbageCollector *collector = ZR_NULL;

    if (result == ZR_NULL || !system_gc_get_collector(context, &collector)) {
        return ZR_FALSE;
    }

    collector->stopGcFlag = ZR_TRUE;
    /* TODO: generational step、CheckGc 与普通 minor 对 flag 的覆盖范围不同；核对整体停机契约。 */
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

/* registry 允许 0..1 参数；成功只表示 Core 调用已返回，不代表 minor/major 周期结束。 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_Collect(ZrLibCallContext *context, SZrTypeValue *result) {
    EZrGarbageCollectCollectionKind kind;

    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL ||
        !system_gc_read_collection_kind(context, &kind)) {
        return ZR_FALSE;
    }

    system_gc_run_collection(context->state, kind);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

/* 转换到 Core offset 类型前检查非负；0 由 Core 解释为清除堆阈值。 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_SetHeapLimit(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrUInt64 heapLimitBytes = 0;
    SZrGarbageCollector *collector = ZR_NULL;

    if (result == ZR_NULL ||
        !system_gc_get_collector(context, &collector) ||
        !system_gc_read_non_negative_int_argument(context, 0, &heapLimitBytes)) {
        return ZR_FALSE;
    }

    /* TODO: 这里从 uint64 窄化为 TZrMemoryOffset；核对 32 位目标的可表示范围与超界策略。 */
    ZrCore_GarbageCollector_SetHeapLimitBytes(context->state->global, (TZrMemoryOffset)heapLimitBytes);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

/* 将同一输入配置写入 pause 与 remark；切片是否完成仍由 Core 阶段推进决定。 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_SetBudget(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrUInt64 pauseBudgetUs = 0;
    SZrGarbageCollector *collector = ZR_NULL;

    if (result == ZR_NULL ||
        !system_gc_get_collector(context, &collector) ||
        !system_gc_read_non_negative_int_argument(context, 0, &pauseBudgetUs)) {
        return ZR_FALSE;
    }

    /* TODO: API 不规定哪些并发 major 阶段必须受预算约束；Core 尚有初始暂停阶段待核，
     * 其 remark 同步尾段明确未限时，且 Core 将 pause 值窄化为 TZrSize；不承诺硬墙钟上限。 */
    ZrCore_GarbageCollector_SetPauseBudgetUs(context->state->global, pauseBudgetUs, pauseBudgetUs);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

/* 快照复制到普通 VM 对象；字段写入失败边界由投影 helper 记录。 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_GetStats(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrGarbageCollector *collector = ZR_NULL;
    SZrObject *statsObject;

    if (result == ZR_NULL || !system_gc_get_collector(context, &collector)) {
        return ZR_FALSE;
    }

    statsObject = system_gc_make_stats_object(context->state, collector);
    if (statsObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, statsObject, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
