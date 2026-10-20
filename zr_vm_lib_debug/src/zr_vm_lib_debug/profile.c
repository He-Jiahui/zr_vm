#include "zr_vm_lib_debug/profile.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "zr_vm_core/function.h"

/* VM hook 只回传 state，进程级活动链表把事件映射回对应 Profile。 */
/* TODO: 链表和采样数组未同步；核对多 state/同 state 跨线程 hook 的调用约束，
 * 再以并发 profiling 测试决定是否需显式锁或 state 附属存储。 */
static ZrDebugProfile *g_active_profiles = ZR_NULL;

/* 当前激活帧借来的名称/来源只在 hook 内使用，聚合项会复制文本。 */
typedef struct ZrDebugProfileLocation {
    const SZrFunction *function;
    const TZrChar *name;
    const TZrChar *source;
    TZrSize line;
} ZrDebugProfileLocation;

/* 计时按 C 运行库 clock() 的 tick 差换算；POSIX 与 MSVC 的时钟口径不同。 */
/* TODO: POSIX 的进程 CPU 时间会计入其它线程，MSVC CRT 的 clock() 却量墙钟；
 * 用独立线程负载和睡眠测试核对两平台报告，再确定统一的 profile 时间契约。 */
static TZrUInt64 zr_debug_profile_now_ns(void) {
    clock_t ticks = clock();
    if (ticks == (clock_t)-1) {
        return 0u;
    }
    return ((TZrUInt64)ticks * 1000000000ull) / (TZrUInt64)CLOCKS_PER_SEC;
}

static void zr_debug_profile_copy_text(TZrChar *destination, TZrSize destinationSize, const TZrChar *source) {
    TZrSize length;

    if (destination == ZR_NULL || destinationSize == 0u) {
        return;
    }

    if (source == ZR_NULL) {
        source = "";
    }

    length = strlen(source);
    if (length >= destinationSize) {
        length = destinationSize - 1u;
    }
    if (length > 0u) {
        memcpy(destination, source, length);
    }
    destination[length] = '\0';
}

static TZrUInt32 zr_debug_profile_event_mask(EZrDebugHookEvent event) {
    if (event >= ZR_DEBUG_HOOK_EVENT_MAX) {
        return 0u;
    }
    return 1u << event;
}

static ZrDebugProfile *zr_debug_profile_find_active(SZrState *state) {
    ZrDebugProfile *profile;

    for (profile = g_active_profiles; profile != ZR_NULL; profile = profile->next_active) {
        if (profile->state == state) {
            return profile;
        }
    }

    return ZR_NULL;
}

/* 排除同一 state 的第二个 profile，保证 hook 回调能定位唯一会话。 */
static TZrBool zr_debug_profile_register_active(ZrDebugProfile *profile) {
    if (profile == ZR_NULL || profile->state == ZR_NULL) {
        return ZR_FALSE;
    }
    if (zr_debug_profile_find_active(profile->state) != ZR_NULL) {
        return ZR_FALSE;
    }

    profile->next_active = g_active_profiles;
    g_active_profiles = profile;
    return ZR_TRUE;
}

static void zr_debug_profile_unregister_active(ZrDebugProfile *profile) {
    ZrDebugProfile **cursor = &g_active_profiles;

    while (*cursor != ZR_NULL) {
        if (*cursor == profile) {
            *cursor = profile->next_active;
            profile->next_active = ZR_NULL;
            return;
        }
        cursor = &(*cursor)->next_active;
    }
}

/* 函数、帧和源码行样本分别扩容；调用方保留先前已经统计的部分结果。 */
/* TODO: 三类容量倍增及 sizeof(*item) * capacity 未查整数溢出；核对
 * TZrSize 上限与大规模函数/行采样的可达性后建立统一的扩容上限。 */
static TZrBool zr_debug_profile_reserve_entries(ZrDebugProfile *profile, TZrSize minimumCapacity) {
    ZrDebugProfileEntry *entries;
    TZrSize newCapacity;

    if (profile == ZR_NULL) {
        return ZR_FALSE;
    }
    if (profile->entry_capacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = profile->entry_capacity == 0u ? 8u : profile->entry_capacity * 2u;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2u;
    }

    entries = (ZrDebugProfileEntry *)realloc(profile->entries, sizeof(*entries) * newCapacity);
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }

    profile->entries = entries;
    profile->entry_capacity = newCapacity;
    return ZR_TRUE;
}

static TZrBool zr_debug_profile_reserve_frames(ZrDebugProfile *profile, TZrSize minimumCapacity) {
    ZrDebugProfileFrame *frames;
    TZrSize newCapacity;

    if (profile == ZR_NULL) {
        return ZR_FALSE;
    }
    if (profile->frame_capacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = profile->frame_capacity == 0u ? 16u : profile->frame_capacity * 2u;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2u;
    }

    frames = (ZrDebugProfileFrame *)realloc(profile->frames, sizeof(*frames) * newCapacity);
    if (frames == ZR_NULL) {
        return ZR_FALSE;
    }

    profile->frames = frames;
    profile->frame_capacity = newCapacity;
    return ZR_TRUE;
}

static TZrBool zr_debug_profile_reserve_samples(ZrDebugProfile *profile, TZrSize minimumCapacity) {
    ZrDebugProfileSample *samples;
    TZrSize newCapacity;

    if (profile == ZR_NULL) {
        return ZR_FALSE;
    }
    if (profile->sample_capacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = profile->sample_capacity == 0u ? 16u : profile->sample_capacity * 2u;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2u;
    }

    samples = (ZrDebugProfileSample *)realloc(profile->samples, sizeof(*samples) * newCapacity);
    if (samples == ZR_NULL) {
        return ZR_FALSE;
    }

    profile->samples = samples;
    profile->sample_capacity = newCapacity;
    return ZR_TRUE;
}

/* 以 VM 函数原型身份聚合：同原型的闭包实例合并，不同原型即使同名也分开。 */
static TZrSize zr_debug_profile_find_or_add_entry(ZrDebugProfile *profile,
                                                  const SZrFunction *function,
                                                  const TZrChar *name,
                                                  const TZrChar *source) {
    TZrSize index;
    ZrDebugProfileEntry *entry;

    if (profile == ZR_NULL || function == ZR_NULL) {
        return (TZrSize)-1;
    }

    for (index = 0u; index < profile->entry_count; index++) {
        if (profile->entries[index].function == function) {
            return index;
        }
    }

    if (!zr_debug_profile_reserve_entries(profile, profile->entry_count + 1u)) {
        return (TZrSize)-1;
    }

    index = profile->entry_count++;
    entry = &profile->entries[index];
    memset(entry, 0, sizeof(*entry));
    entry->function = function;
    zr_debug_profile_copy_text(entry->name, sizeof(entry->name), name != ZR_NULL && name[0] != '\0' ? name : "<anonymous>");
    zr_debug_profile_copy_text(entry->source, sizeof(entry->source), source);
    return index;
}

/* COUNT 样本按函数身份与源码行聚合，供 CLI 输出热点行。 */
static TZrSize zr_debug_profile_find_or_add_sample(ZrDebugProfile *profile,
                                                   const SZrFunction *function,
                                                   TZrSize line,
                                                   const TZrChar *name,
                                                   const TZrChar *source) {
    TZrSize index;
    ZrDebugProfileSample *sample;

    if (profile == ZR_NULL || function == ZR_NULL) {
        return (TZrSize)-1;
    }

    for (index = 0u; index < profile->sample_count; index++) {
        if (profile->samples[index].function == function && profile->samples[index].line == line) {
            return index;
        }
    }

    if (!zr_debug_profile_reserve_samples(profile, profile->sample_count + 1u)) {
        return (TZrSize)-1;
    }

    index = profile->sample_count++;
    sample = &profile->samples[index];
    memset(sample, 0, sizeof(*sample));
    sample->function = function;
    sample->line = line;
    zr_debug_profile_copy_text(sample->name,
                               sizeof(sample->name),
                               name != ZR_NULL && name[0] != '\0' ? name : "<anonymous>");
    zr_debug_profile_copy_text(sample->source, sizeof(sample->source), source);
    return index;
}

static TZrBool zr_debug_profile_capture_location(SZrState *state, ZrDebugProfileLocation *outLocation) {
    SZrDebugActivation activation;
    SZrDebugInfo info;

    if (state == ZR_NULL || outLocation == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outLocation, 0, sizeof(*outLocation));
    memset(&activation, 0, sizeof(activation));
    if (!ZrCore_Debug_GetStack(state, 0u, &activation) || activation.function == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&info, 0, sizeof(info));
    if (ZrCore_Debug_GetInfo(state,
                             &activation,
                             (EZrDebugInfoType)(ZR_DEBUG_INFO_FUNCTION_NAME |
                                                ZR_DEBUG_INFO_SOURCE_FILE |
                                                ZR_DEBUG_INFO_LINE_NUMBER),
                             &info)) {
        outLocation->name = info.name;
        outLocation->source = info.source;
        outLocation->line = info.currentLine;
    }
    outLocation->function = activation.function;
    return ZR_TRUE;
}

static TZrBool zr_debug_profile_capture_frame(ZrDebugProfile *profile,
                                              SZrState *state,
                                              TZrSize *outEntryIndex) {
    ZrDebugProfileLocation location;
    TZrSize entryIndex;

    if (profile == ZR_NULL || outEntryIndex == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!zr_debug_profile_capture_location(state, &location)) {
        return ZR_FALSE;
    }

    entryIndex = zr_debug_profile_find_or_add_entry(profile, location.function, location.name, location.source);
    if (entryIndex == (TZrSize)-1) {
        return ZR_FALSE;
    }

    *outEntryIndex = entryIndex;
    return ZR_TRUE;
}

/* CALL hook 同时累计次数和压入起始时刻；时间结算留到匹配 RETURN。 */
static void zr_debug_profile_record_call(ZrDebugProfile *profile, SZrState *state) {
    TZrSize entryIndex;
    ZrDebugProfileFrame *frame;

    if (!zr_debug_profile_capture_frame(profile, state, &entryIndex)) {
        return;
    }

    profile->entries[entryIndex].call_count++;
    if (!zr_debug_profile_reserve_frames(profile, profile->frame_count + 1u)) {
        return;
    }

    frame = &profile->frames[profile->frame_count++];
    memset(frame, 0, sizeof(*frame));
    frame->entry_index = entryIndex;
    frame->start_time_ns = zr_debug_profile_now_ns();
}

/* RETURN hook 找最近匹配函数帧；栈顶不匹配时丢弃更深的计时帧，
 * 再将子调用耗时回灌父帧，形成 inclusive 与 self 两套口径。 */
static void zr_debug_profile_record_return(ZrDebugProfile *profile, SZrState *state) {
    TZrSize entryIndex;
    TZrSize frameIndex;
    TZrUInt64 now;
    TZrUInt64 elapsed;
    TZrUInt64 selfTime;
    ZrDebugProfileFrame frame;

    if (!zr_debug_profile_capture_frame(profile, state, &entryIndex)) {
        return;
    }

    profile->entries[entryIndex].return_count++;
    if (profile->frame_count == 0u) {
        return;
    }

    frameIndex = profile->frame_count;
    while (frameIndex > 0u) {
        frameIndex--;
        if (profile->frames[frameIndex].entry_index == entryIndex) {
            break;
        }
    }
    if (profile->frames[frameIndex].entry_index != entryIndex) {
        return;
    }

    frame = profile->frames[frameIndex];
    profile->frame_count = frameIndex;
    now = zr_debug_profile_now_ns();
    elapsed = now >= frame.start_time_ns ? now - frame.start_time_ns : 0u;
    selfTime = elapsed >= frame.child_time_ns ? elapsed - frame.child_time_ns : 0u;
    profile->entries[entryIndex].total_time_ns += elapsed;
    profile->entries[entryIndex].self_time_ns += selfTime;
    if (profile->frame_count > 0u) {
        profile->frames[profile->frame_count - 1u].child_time_ns += elapsed;
    }
}

static void zr_debug_profile_record_sample(ZrDebugProfile *profile, SZrState *state) {
    ZrDebugProfileLocation location;
    TZrSize sampleIndex;

    if (profile == ZR_NULL || profile->sample_period == 0u) {
        return;
    }
    if (!zr_debug_profile_capture_location(state, &location)) {
        return;
    }

    sampleIndex = zr_debug_profile_find_or_add_sample(profile,
                                                      location.function,
                                                      location.line,
                                                      location.name,
                                                      location.source);
    if (sampleIndex == (TZrSize)-1) {
        return;
    }

    profile->samples[sampleIndex].sample_count++;
    profile->total_sample_count++;
}

/* Profile 接管 CALL/RETURN/COUNT 后仍按旧 mask 转发给被包裹的 hook。 */
static void zr_debug_profile_hook(SZrState *state, SZrDebugInfo *debugInfo) {
    ZrDebugProfile *profile = zr_debug_profile_find_active(state);

    if (profile != ZR_NULL && debugInfo != ZR_NULL) {
        if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_CALL) {
            zr_debug_profile_record_call(profile, state);
        } else if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_RETURN) {
            zr_debug_profile_record_return(profile, state);
        } else if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_COUNT) {
            zr_debug_profile_record_sample(profile, state);
        }
    }

    if (profile != ZR_NULL &&
        profile->previous_hook != ZR_NULL &&
        debugInfo != ZR_NULL &&
        (profile->previous_mask & zr_debug_profile_event_mask(debugInfo->event)) != 0u) {
        profile->previous_hook(state, debugInfo);
    }
}

ZR_DEBUG_API void ZrDebug_Profile_Init(ZrDebugProfile *profile) {
    if (profile == ZR_NULL) {
        return;
    }
    memset(profile, 0, sizeof(*profile));
}

ZR_DEBUG_API void ZrDebug_Profile_Reset(ZrDebugProfile *profile) {
    if (profile == ZR_NULL) {
        return;
    }
    profile->entry_count = 0u;
    profile->frame_count = 0u;
    profile->sample_count = 0u;
    profile->total_sample_count = 0u;
}

/* 同一 VM 只有一个 hook 和 COUNT 周期；若旧观察者已用不同周期则拒绝叠加。 */
/* TODO: 与 Coverage 类似，活动期间外部 SetHook 可使 Stop 恢复旧快照、覆盖新 hook；
 * 核对非 CLI 宿主的 hook 动态更换契约与需要的协调机制。 */
static TZrBool zr_debug_profile_start_internal(ZrDebugProfile *profile, SZrState *state, TZrUInt32 samplePeriod) {
    TZrUInt32 mask;
    TZrUInt32 count;

    if (profile == ZR_NULL || state == ZR_NULL || profile->active) {
        return ZR_FALSE;
    }

    profile->state = state;
    profile->previous_hook = ZrCore_Debug_GetHook(state);
    profile->previous_mask = ZrCore_Debug_GetHookMask(state);
    profile->previous_count = ZrCore_Debug_GetHookCount(state);
    if (profile->previous_hook == ZR_NULL && profile->previous_mask != 0u) {
        profile->state = ZR_NULL;
        profile->previous_mask = 0u;
        profile->previous_count = 0u;
        return ZR_FALSE;
    }
    if (samplePeriod != 0u &&
        (profile->previous_mask & ZR_DEBUG_HOOK_MASK_COUNT) != 0u &&
        profile->previous_count != samplePeriod) {
        profile->state = ZR_NULL;
        profile->previous_hook = ZR_NULL;
        profile->previous_mask = 0u;
        profile->previous_count = 0u;
        return ZR_FALSE;
    }
    profile->frame_count = 0u;
    if (!zr_debug_profile_register_active(profile)) {
        profile->state = ZR_NULL;
        profile->previous_hook = ZR_NULL;
        profile->previous_mask = 0u;
        profile->previous_count = 0u;
        return ZR_FALSE;
    }

    mask = profile->previous_mask | ZR_DEBUG_HOOK_MASK_CALL | ZR_DEBUG_HOOK_MASK_RETURN;
    count = profile->previous_count;
    if (samplePeriod != 0u) {
        mask |= ZR_DEBUG_HOOK_MASK_COUNT;
        count = samplePeriod;
    }
    /* 先发布回调读到的周期和活动标志，再让 VM 开始分派 hook。 */
    profile->sample_period = samplePeriod;
    profile->active = ZR_TRUE;
    ZrCore_Debug_SetHook(state, zr_debug_profile_hook, mask, count);
    return ZR_TRUE;
}

ZR_DEBUG_API TZrBool ZrDebug_Profile_Start(ZrDebugProfile *profile, SZrState *state) {
    return zr_debug_profile_start_internal(profile, state, 0u);
}

ZR_DEBUG_API TZrBool ZrDebug_Profile_StartWithSampling(ZrDebugProfile *profile,
                                                       SZrState *state,
                                                       TZrUInt32 samplePeriod) {
    if (samplePeriod == 0u) {
        return ZR_FALSE;
    }
    return zr_debug_profile_start_internal(profile, state, samplePeriod);
}

ZR_DEBUG_API void ZrDebug_Profile_Stop(ZrDebugProfile *profile) {
    SZrState *state;

    if (profile == ZR_NULL || !profile->active) {
        return;
    }

    state = profile->state;
    if (state != ZR_NULL) {
        ZrCore_Debug_SetHook(state, profile->previous_hook, profile->previous_mask, profile->previous_count);
    }
    zr_debug_profile_unregister_active(profile);
    profile->state = ZR_NULL;
    profile->previous_hook = ZR_NULL;
    profile->previous_mask = 0u;
    profile->previous_count = 0u;
    profile->active = ZR_FALSE;
    profile->frame_count = 0u;
    profile->sample_period = 0u;
}

ZR_DEBUG_API void ZrDebug_Profile_Destroy(ZrDebugProfile *profile) {
    if (profile == ZR_NULL) {
        return;
    }
    if (profile->active) {
        ZrDebug_Profile_Stop(profile);
    }
    free(profile->entries);
    free(profile->frames);
    free(profile->samples);
    ZrDebug_Profile_Init(profile);
}

ZR_DEBUG_API TZrSize ZrDebug_Profile_GetEntryCount(const ZrDebugProfile *profile) {
    return profile != ZR_NULL ? profile->entry_count : 0u;
}

ZR_DEBUG_API const ZrDebugProfileEntry *ZrDebug_Profile_GetEntry(const ZrDebugProfile *profile, TZrSize index) {
    if (profile == ZR_NULL || index >= profile->entry_count) {
        return ZR_NULL;
    }
    return &profile->entries[index];
}

ZR_DEBUG_API const ZrDebugProfileEntry *ZrDebug_Profile_FindByName(const ZrDebugProfile *profile,
                                                                   const TZrChar *name) {
    TZrSize index;

    if (profile == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0u; index < profile->entry_count; index++) {
        if (strcmp(profile->entries[index].name, name) == 0) {
            return &profile->entries[index];
        }
    }

    return ZR_NULL;
}

ZR_DEBUG_API TZrSize ZrDebug_Profile_GetSampleCount(const ZrDebugProfile *profile) {
    return profile != ZR_NULL ? profile->sample_count : 0u;
}

ZR_DEBUG_API TZrUInt64 ZrDebug_Profile_GetTotalSampleCount(const ZrDebugProfile *profile) {
    return profile != ZR_NULL ? profile->total_sample_count : 0u;
}

ZR_DEBUG_API const ZrDebugProfileSample *ZrDebug_Profile_GetSample(const ZrDebugProfile *profile, TZrSize index) {
    if (profile == ZR_NULL || index >= profile->sample_count) {
        return ZR_NULL;
    }
    return &profile->samples[index];
}

/**
 * @var ZrDebugProfile *g_active_profiles
 * ## Summary
 * Core hook 的参数没有 Profile 用户上下文，进程级链表把回调中的 state 映射回调用方持有的活动 Profile；同一 state 正常只登记一个对象。
 *
 * ## Value model
 * 初始为NULL；register_active把有效对象借用链接到表头，unregister_active按对象身份摘链。链不分配或拥有节点，也不保存线程局部副本。
 *
 * ## Invariants and lifecycle
 * Profile和借用state必须活到Stop完成；不可复制仍在链中的对象。TODO: 既有并发家族保持不变，链表与三个数组没有同步，应核对多state/同state跨线程hook入口并以并发profiling场景明确约束后再决定锁或state附属存储。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:61 — 进程活动表登记/查找/摘链或事件位映射。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:385 — 按Core事件记录并向旧hook过滤转发的实际派发。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:1161 — Core借用事件对象、抑制普通递归及回调正常返回边界。
 */

/**
 * @struct ZrDebugProfileLocation
 * ## Summary
 * 一次hook内部使用的栈帧元数据临时视图，把Core当前函数与借用文本交给聚合查找/复制层，不把临时文字当拥有的结果。
 *
 * ## Fields
 * - function: GetStack选出的借用函数元数据身份；成功返回时非NULL。
 * - name: GetInfo得到的借用名称，缺少元数据时可为NULL。
 * - source: GetInfo得到的借用来源文本，缺少时可为NULL。
 * - line: GetInfo解析的当前源码行，未取得时保持0。
 *
 * ## Invariants and lifecycle
 * 由capture_location清零并填写，通常是调用栈上的短期对象；名称和来源在聚合项创建时复制，function仍只作借用身份。不添加GC根，不跨VM执行/元数据销毁保存该视图。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:247 — 当前栈帧和借用元数据的捕获。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:279 — 元数据视图转函数索引的调用。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:360 — COUNT样本成功记录、分配失败丢弃与总量更新。
 */

/**
 * @fn TZrUInt64 zr_debug_profile_now_ns(void)
 * ## Summary
 * 在 CALL 建帧和 RETURN 结算时读取 C 运行库 clock()，把同一计时源的 tick 换算为纳秒形式，供本层计算相对耗时；它不是跨平台统一的单调墙钟。
 *
 * ## Returns
 * @return clock() 报 (clock_t)-1 时返回 0；否则返回先转无符号并乘 10^9 再除 CLOCKS_PER_SEC 的值。未对乘法溢出、时钟回绕或平台口径做归一化。
 *
 * ## Constraints and lifecycle
 * 无分配和对象转移。调用者把时间倒退钳为零，但不能据此推导计时准确或饱和语义。TODO: 沿用既有时钟口径问题，以独立线程负载、睡眠及长运行场景核对 POSIX 进程 CPU 时间与 MSVC CRT 墙钟的差异，再确定统一契约。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:315 — CALL先计数再分配帧的部分成功路径。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:350 — RETURN匹配、钳零计时与父帧累计的实现。
 * - tests/profile/test_profile_deterministic.c:59 — 现有计时用例的调用、计数/包含时间关系断言及销毁顺序（本报告未执行）。
 */

/**
 * @fn void zr_debug_profile_copy_text(TZrChar *destination, TZrSize destinationSize, const TZrChar *source)
 * ## Summary
 * 为聚合项和行样本复制名称、来源的字节快照，避免把 Core 暂借的文本指针保存到结果数组。
 *
 * ## Parameters
 * @param[out] destination 调用方提供的可写缓冲区；NULL 时不写入。
 * @param[in] destinationSize 缓冲区总字节容量，包含结尾 NUL；为零时不访问 destination。
 * @param[in] source 借用的 NUL 结尾字节串；NULL 按空串处理，在本次复制结束前有效。
 *
 * ## Returns
 * 无返回值；在非空、非零容量目标中最多复制容量减一字节并补 NUL，不报告截断。
 *
 * ## Constraints and lifecycle
 * 不分配、不接管 source。目标容量必须真实可写；source 与目标不可有 memcpy 不允许的重叠。按字节截断，不能承诺保持 UTF-8 字符边界或完整路径。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:207 — 函数指针键、函数表扩容与文字快照。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:240 — 函数/行键查找、样本扩容与文字快照。
 */

/**
 * @fn TZrUInt32 zr_debug_profile_event_mask(EZrDebugHookEvent event)
 * ## Summary
 * 把 Core 的事件序号映射为一个订阅位，使包装 hook 只向旧观察者转发其原本订阅的事件。
 *
 * ## Parameters
 * @param[in] event Core 产生的非负事件序号；有效事件与 EZrDebugHookMask 位序对应。负值不是合法输入。
 *
 * ## Returns
 * @return 序号小于 EVENT_MAX 时返回 1u 左移该序号；大于等于 EVENT_MAX 时返回 0。
 *
 * ## Constraints and lifecycle
 * 纯值转换，不持有 state。该函数只防护上界，不承担任意负枚举值的安全移位检查；实际入口由 Core 的 CALL/RETURN/LINE/COUNT 常量派发。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:398 — 按Core事件记录并向旧hook过滤转发的实际派发。
 * - zr_vm_core/include/zr_vm_core/debug.h:76 — Core事件枚举和掩码位的对应定义。
 */

/**
 * @fn ZrDebugProfile *zr_debug_profile_find_active(SZrState *state)
 * ## Summary
 * 从进程级活动表按 state 身份查回唯一采集对象，弥补 Core hook 回调没有用户上下文参数的限制。
 *
 * ## Parameters
 * @param[in] state 用于指针身份比较的 VM state；本函数不取得其所有权。
 *
 * ## Returns
 * @return 返回活动链中第一个 state 相等的 Profile 借用指针；找不到时返回 NULL。
 *
 * ## Constraints and lifecycle
 * 不分配、不遍历 state 内部。结果仅在对应对象未摘链、销毁或迁移前可用；链表要求宿主串行访问，不是线程局部表。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:78 — 进程活动表登记/查找/摘链或事件位映射。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:385 — 按Core事件记录并向旧hook过滤转发的实际派发。
 */

/**
 * @fn TZrBool zr_debug_profile_register_active(ZrDebugProfile *profile)
 * ## Summary
 * 在安装包装 hook 前把已设置 state 的 Profile 加入活动表，并拒绝同一 state 的第二个活动采集对象。
 *
 * ## Parameters
 * @param[in,out] profile 已 Init、state 非 NULL、尚未登记的调用方对象；成功时改写 next_active 并成为链首。
 *
 * ## Returns
 * @return 对象或 state 为空、或者已有相同 state 时返回 FALSE且不链接；否则返回 TRUE。
 *
 * ## Constraints and lifecycle
 * 不分配、不接管对象；必须在对象仍存活时由 unregister_active 摘除。只检查 state 唯一性，不提供锁或并发原子发布。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:434 — 启动检查、链表登记与安装包装hook的实际执行顺序。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:469 — 启动检查、链表登记与安装包装hook的实际执行顺序。
 */

/**
 * @fn void zr_debug_profile_unregister_active(ZrDebugProfile *profile)
 * ## Summary
 * 在停止采集时从活动表摘除指定 Profile，防止后续 hook 继续按 state 找到该对象。
 *
 * ## Parameters
 * @param[in,out] profile 待摘除的对象身份；若不存在则无操作。命中时清空其 next_active。
 *
 * ## Returns
 * 无返回值；以链表更新体现移除，不释放对象或数组。
 *
 * ## Constraints and lifecycle
 * 使用前驱链接地址处理链首和中间节点。调用方保证表未被并发修改；单独摘链不恢复 Core hook，正常顺序由 Stop 协调。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:495 — Stop恢复hook、摘链并丢弃开放计时帧的生命周期依据。
 */

/**
 * @fn TZrBool zr_debug_profile_reserve_entries(ZrDebugProfile *profile, TZrSize minimumCapacity)
 * ## Summary
 * 在新增函数聚合项前保证 entries 容量，保留已累计结果并把重新分配的地址交回 Profile。
 *
 * ## Parameters
 * @param[in,out] profile 已 Init 且 entries/entry_capacity 相符的对象；NULL 返回 FALSE。
 * @param[in] minimumCapacity 所需元素数；已有容量足够时不调用 realloc。
 *
 * ## Returns
 * @return 容量已足或 realloc 成功返回 TRUE；对象为空或 realloc 返回 NULL 时返回 FALSE。普通分配失败保留旧指针、容量和已写元素。
 *
 * ## Constraints and lifecycle
 * 从 8 起按倍增扩容，成功后旧借用 entries 指针可能失效。使用 C realloc/free 配对而非 VM allocator。TODO: 既有容量家族仍未检查倍增及 sizeof 乘法溢出，应沿新增函数达到容量的路径核对 TZrSize 上限和统一扩容界限。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:193 — 函数指针键、函数表扩容与文字快照。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:516 — Destroy释放数组或公开结果访问的有效期依据。
 */

/**
 * @fn TZrBool zr_debug_profile_reserve_frames(ZrDebugProfile *profile, TZrSize minimumCapacity)
 * ## Summary
 * 在 CALL 记录中为新的计时栈帧提供容量，使数组搬移不改变以索引引用的函数聚合项。
 *
 * ## Parameters
 * @param[in,out] profile 已 Init 且 frames/frame_capacity 相符的对象；NULL 返回 FALSE。
 * @param[in] minimumCapacity 所需计时帧数；已有容量足够时不分配。
 *
 * ## Returns
 * @return 已有容量或扩容成功为 TRUE；NULL 对象或普通 realloc 失败为 FALSE，失败保留原 frames 和容量。
 *
 * ## Constraints and lifecycle
 * 从 16 起倍增；只在成功后发布新地址。调用者可能在本次扩容前已递增 call_count，因此 FALSE不表示整个事件回滚。TODO: 保留与另两类数组同族的容量倍增和字节乘法上限核查，需在真正增长处验证。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:303 — CALL先计数再分配帧的部分成功路径。
 */

/**
 * @fn TZrBool zr_debug_profile_reserve_samples(ZrDebugProfile *profile, TZrSize minimumCapacity)
 * ## Summary
 * 在首次记录某函数/源码行组合时扩容 samples，保留已有行热点数据。
 *
 * ## Parameters
 * @param[in,out] profile 已 Init 且 samples/sample_capacity 相符的对象；NULL 返回 FALSE。
 * @param[in] minimumCapacity 所需行样本元素数；不超过当前容量时不分配。
 *
 * ## Returns
 * @return 已有容量或 realloc 成功为 TRUE；NULL 或普通 realloc 失败为 FALSE，失败不改原地址、容量和元素。
 *
 * ## Constraints and lifecycle
 * 从 16 起倍增并使用 C realloc/free。成功搬移使旧样本借用地址失效。TODO: 延续既有容量问题，核对倍增和元素大小乘法溢出及可达大规模行采样入口，不宣称已注入 OOM。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:225 — 函数/行键查找、样本扩容与文字快照。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:371 — COUNT样本成功记录、分配失败丢弃与总量更新。
 */

/**
 * @fn TZrSize zr_debug_profile_find_or_add_entry(ZrDebugProfile *profile, const SZrFunction *function, const TZrChar *name, const TZrChar *source)
 * ## Summary
 * 以 SZrFunction 元数据指针身份查找或创建函数聚合项；共享该指针的调用合并，同名但不同指针分别计数。
 *
 * ## Parameters
 * @param[in,out] profile 已 Init 的聚合容器，可在新增项时扩容；NULL 被拒绝。
 * @param[in] function 非 NULL 的 VM 元数据身份，仅借用，不添加 GC 根或保留计数。
 * @param[in] name 当前帧的借用名称；NULL或空串使用 <anonymous>，新建时复制到定长字段。
 * @param[in] source 当前帧借用来源文本；NULL复制为空串，新建时复制。
 *
 * ## Returns
 * @return 命中或新建的 entries 索引；NULL输入或扩容失败返回 (TZrSize)-1。命中已有项时不刷新其文本快照。
 *
 * ## Constraints and lifecycle
 * 新项先清零再发布函数身份及文本；普通 realloc 失败不增加 entry_count。成功增长可移动 entries；function 的存活由 VM/宿主保证，跨会话复用数据不自动区分被回收后重用的地址。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:285 — 元数据视图转函数索引的调用。
 * - tests/profile/test_profile_deterministic.c:55 — 现有计时用例的调用、计数/包含时间关系断言及销毁顺序（本报告未执行）。
 */

/**
 * @fn TZrSize zr_debug_profile_find_or_add_sample(ZrDebugProfile *profile, const SZrFunction *function, TZrSize line, const TZrChar *name, const TZrChar *source)
 * ## Summary
 * 把 COUNT 事件按函数身份与源码行组成的键聚合到样本表，以便 CLI 输出热点行；名称本身不是键。
 *
 * ## Parameters
 * @param[in,out] profile 可追加或扩容的样本容器；NULL被拒绝。
 * @param[in] function 非 NULL 的借用函数元数据身份；不由样本表持有。
 * @param[in] line Core 给出的源码行；元数据缺失时可为 0，按该值参与键比较。
 * @param[in] name 仅在新建时复制的借用名称；空或NULL使用 <anonymous>。
 * @param[in] source 仅在新建时复制的借用来源；NULL按空串处理。
 *
 * ## Returns
 * @return 已有或新建的 samples 索引；输入为空或扩容失败返回 (TZrSize)-1，不增加 sample_count。
 *
 * ## Constraints and lifecycle
 * 样本文本按字节截断为内嵌快照，function仍借用。成功扩容使旧样本地址失效；不同函数元数据指针即使同名、同行也不合并。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:367 — COUNT样本成功记录、分配失败丢弃与总量更新。
 * - tests/profile/test_profile_deterministic.c:102 — 现有采样用例的调用及热点行/正样本断言（本报告未执行）。
 */

/**
 * @fn TZrBool zr_debug_profile_capture_location(SZrState *state, ZrDebugProfileLocation *outLocation)
 * ## Summary
 * 在 hook 回调内借用当前可识别栈帧的函数、名称、来源和行号；元数据文字失败时仍可用函数身份继续聚合。
 *
 * ## Parameters
 * @param[in] state 当前回调所属的有效 VM；NULL返回 FALSE。
 * @param[out] outLocation 可写临时视图；NULL被拒绝。合法参数时先清零，成功后填借用字段。
 *
 * ## Returns
 * @return GetStack 得到非 NULL函数后返回 TRUE；GetInfo 不成功时文字/行保持零值但函数仍交付。非法参数或无函数帧为 FALSE。
 *
 * ## Constraints and lifecycle
 * GetInfo 只请求 FUNCTION_NAME/SOURCE_FILE/LINE_NUMBER，不请求会压 VM 栈的 PUSH_FUNCTION。借用文字和函数不可跨 VM 元数据失效点保留；本层随后复制文字，不接管函数。失败不承诺非法输出指针被清零。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:290 — GetStack经metadata function取得当前可识别帧。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:570 — GetInfo借用名称/来源/行且PUSH_FUNCTION为独立请求位。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:285 — 元数据视图转函数索引的调用。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:367 — COUNT样本成功记录、分配失败丢弃与总量更新。
 */

/**
 * @fn TZrBool zr_debug_profile_capture_frame(ZrDebugProfile *profile, SZrState *state, TZrSize *outEntryIndex)
 * ## Summary
 * 为 CALL/RETURN 把当前激活帧解析成函数聚合索引，统一处理元数据缺失与新增项分配失败。
 *
 * ## Parameters
 * @param[in,out] profile 已 Init 的目标聚合容器，可能因新函数扩容；NULL失败。
 * @param[in] state 借用的当前 VM；由 capture_location 验证当前帧。
 * @param[out] outEntryIndex 可写索引槽；仅成功时写入，失败保持调用前内容。
 *
 * ## Returns
 * @return 成功解析并取得聚合项为 TRUE；空参数、无帧或新增失败为 FALSE，调用者忽略本次事件。
 *
 * ## Constraints and lifecycle
 * 不创建独立拥有的帧视图，临时文字只活到本次调用。函数表可能在成功路径增长；失败并不统一回滚其他此前统计。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:303 — CALL先计数再分配帧的部分成功路径。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:328 — RETURN匹配、钳零计时与父帧累计的实现。
 */

/**
 * @fn void zr_debug_profile_record_call(ZrDebugProfile *profile, SZrState *state)
 * ## Summary
 * 在 CALL 事件先累计函数调用数，再压入计时起点；匹配 RETURN 才结算 inclusive/self 时间。
 *
 * ## Parameters
 * @param[in,out] profile 当前活动对象；可能新建函数项并增长 frames。
 * @param[in] state Core hook 对应的当前 VM，借用到函数元数据捕获结束。
 *
 * ## Returns
 * 无返回值；无法捕获聚合项时丢弃事件。帧扩容失败时已增加的 call_count 保留，但不压计时帧，不报告完整回滚或完整计时。
 *
 * ## Constraints and lifecycle
 * 以 entries 索引保存在 frame 中，entries 自身扩容不使该索引失效。无同步；调用者应在同一串行 VM 事件序列中配对 CALL/RETURN，异常退出或丢帧只提供部分结果。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:389 — 按Core事件记录并向旧hook过滤转发的实际派发。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:320 — RETURN匹配、钳零计时与父帧累计的实现。
 * - tests/profile/test_profile_deterministic.c:59 — 现有计时用例的调用、计数/包含时间关系断言及销毁顺序（本报告未执行）。
 */

/**
 * @fn void zr_debug_profile_record_return(ZrDebugProfile *profile, SZrState *state)
 * ## Summary
 * 在 RETURN 事件增加返回计数，并从计时栈顶向下找最近的同一聚合索引，结算该帧及父帧的子调用时间。
 *
 * ## Parameters
 * @param[in,out] profile 当前活动对象；更新聚合时间、返回数和计时栈深度。
 * @param[in] state 仍包含返回函数元数据的当前 VM；由 Core 在退栈前派发 RETURN。
 *
 * ## Returns
 * 无返回值；捕获失败不记事件；空栈或找不到匹配帧时只保留已增加的 return_count。匹配后弹出该帧及更深帧，时间倒退及 child超过elapsed均钳零。
 *
 * ## Constraints and lifecycle
 * 已弹出的不匹配深层帧不会独立补结算；Stop 也不自动结算未完成帧。parent_self_time 块将已结算 elapsed 只加给保留的栈顶父帧，计数、耗时不承诺异常/OOM情况下完整配对。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/function/function.c:5360 — Core函数返回前的HookReturn真实调用入口。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:1178 — Core在退栈前派发RETURN，保证当前函数元数据仍可读取。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:337 — RETURN匹配、钳零计时与父帧累计的实现。
 */

/**
 * @fn void zr_debug_profile_record_sample(ZrDebugProfile *profile, SZrState *state)
 * ## Summary
 * 在本对象主动开启 COUNT 采样时取得当前位置，并把一次样本累计到函数/行组合与会话总数。
 *
 * ## Parameters
 * @param[in,out] profile 活动采集容器；NULL或 sample_period为零直接忽略。
 * @param[in] state COUNT 事件对应的 VM，采样时借用其当前帧元数据。
 *
 * ## Returns
 * 无返回值；无可识别帧或行表扩容失败则不增加样本数；成功时单项与 total_sample_count各加一。
 *
 * ## Constraints and lifecycle
 * 总数只统计本层成功记录的事件，不等于指令数或采样定时器触发总数。计时模式中旧观察者的 COUNT 仍可被转发，但本对象 sample_period=0不会采样。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:393 — 按Core事件记录并向旧hook过滤转发的实际派发。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:1048 — VM指令COUNT倒计数及重置派发，不是墙钟采样。
 * - tests/profile/test_profile_deterministic.c:95 — 现有采样用例的调用及热点行/正样本断言（本报告未执行）。
 */

/**
 * @fn void zr_debug_profile_hook(SZrState *state, SZrDebugInfo *debugInfo)
 * ## Summary
 * 作为 SetHook 安装的包装回调，先记录 CALL/RETURN/COUNT，再仅把旧 mask订阅的同一事件交给 previous_hook。
 *
 * ## Parameters
 * @param[in,out] state Core 当前派发的 VM；本层借此反查活动对象，旧观察者也可访问它。
 * @param[in,out] debugInfo Core 栈上的借用事件描述；NULL时不记录/不转发，不可保存到回调返回之后。
 *
 * ## Returns
 * 无返回值；本层记录失败静默丢弃或保留部分数据，仍按旧掩码转发。没有活动对象时不处理事件。
 *
 * ## Constraints and lifecycle
 * 不保存 debugInfo。Core 普通派发暂时抑制同 state递归 hook，但不是进程锁；previous_hook须普通返回并遵守Core约束，不在回调内销毁/重装此会话。非局部退出没有本函数的恢复承诺；并发和外部换hook仍属已登记TODO范围。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:469 — 启动检查、链表登记与安装包装hook的实际执行顺序。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:1140 — Core借用事件对象、抑制普通递归及回调正常返回边界。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:54 — 进程活动表登记/查找/摘链或事件位映射。
 */

/**
 * @fn void ZrDebug_Profile_Init(ZrDebugProfile *profile)
 * ## Summary
 * 建立调用方分配的 Profile 初始空态，供首次 Start 或后续重新使用；对象本身的存储不由库分配。
 *
 * ## Parameters
 * @param[out] profile 可写的未初始化对象，或已 Destroy 后可重新初始化的对象；NULL无操作。
 *
 * ## Returns
 * 无返回值；以整对象清零建立未活动、空数组和零计数状态。
 *
 * ## Constraints and lifecycle
 * 不得对仍持有数组或仍在活动链中的对象再次 Init，否则会失去资源/链状态；复用已有实例先 Destroy。配套Destroy负责释放内部 C数组，不能替代释放调用方对象。
 *
 * ## References
 * - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:995 — CLI分配对象、Init并按COUNT周期1启动的生产入口。
 * - tests/profile/test_profile_deterministic.c:47 — 现有计时用例的调用、计数/包含时间关系断言及销毁顺序（本报告未执行）。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:509 — Destroy释放数组或公开结果访问的有效期依据。
 */

/**
 * @fn void ZrDebug_Profile_Reset(ZrDebugProfile *profile)
 * ## Summary
 * 在停止采集后开始新统计窗口，清逻辑计数并保留 entries/frames/samples 的已分配容量供复用。
 *
 * ## Parameters
 * @param[in,out] profile 已 Init 的对象；NULL无操作，建议先 Stop。
 *
 * ## Returns
 * 无返回值；entry_count、frame_count、sample_count和total_sample_count归零，数组内旧字节不擦除或释放。
 *
 * ## Constraints and lifecycle
 * 不摘活动链、不恢复hook、不关闭active或sample_period；活动中调用会丢弃逻辑数据及未结算帧。此前结果指针即使地址仍存在，也不再表示有效逻辑记录。串行访问，不提供并发快照。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:413 — Reset清逻辑计数而保留容量的实现。
 * - zr_vm_lib_debug/include/zr_vm_lib_debug/profile.h:71 — 配套公开声明和对象/结果类型。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:488 — Stop恢复hook、摘链并丢弃开放计时帧的生命周期依据。
 */

/**
 * @fn TZrBool zr_debug_profile_start_internal(ZrDebugProfile *profile, SZrState *state, TZrUInt32 samplePeriod)
 * ## Summary
 * 协调当前state的唯一采集对象、旧hook备份和CALL/RETURN/可选COUNT订阅，以发布完成的活动状态后安装包装回调。
 *
 * ## Parameters
 * @param[in,out] profile 已 Init、非活动且存活到 Stop的对象；保留既有聚合数组和计数。
 * @param[in,out] state 有效且存活到 Stop的VM；成功时改写其hook配置。
 * @param[in] samplePeriod 0只采调用/返回；正数请求COUNT采样，已有COUNT订阅必须使用相同基础周期。
 *
 * ## Returns
 * @return NULL/已active、旧hook为空但mask非零、COUNT周期冲突或同state已登记时为 FALSE；成功登记并安装hook为 TRUE。普通拒绝不改VM hook；不同拒绝分支会清理备份字段，登记失败前已清空frame_count。
 *
 * ## Constraints and lifecycle
 * 保存callback/mask/base count，不保存COUNT剩余倒计数；SetHook会重置计数相位。成功不Reset聚合，必须保持state/元数据和对象有效；Stop恢复快照而非与外部hook更换合并。TODO: 沿用既有动态hook协调问题，核对非CLI宿主活动期间SetHook或回调重入的契约，不能称线程原子切换。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:475 — 公开Start包装入口的真实调用。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:663 — SetHook归一化/重置COUNT相位与GetHookCount读取基础周期。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:438 — 启动检查、链表登记与安装包装hook的实际执行顺序。
 */

/**
 * @fn TZrBool ZrDebug_Profile_Start(ZrDebugProfile *profile, SZrState *state)
 * ## Summary
 * 以不新增本对象COUNT采样的模式启动调用/返回计时；已有观察者的订阅由包装hook保留并过滤转发。
 *
 * ## Parameters
 * @param[in,out] profile 已 Init且非active对象，成功后活动直到Stop；NULL失败。
 * @param[in,out] state 有效且持续存活的目标VM；同一state只可登记一个活动Profile。
 *
 * ## Returns
 * @return 内部注册及安装成功为 TRUE；空参数、重复活动或旧hook配置不一致等拒绝为 FALSE。
 *
 * ## Constraints and lifecycle
 * 不清以前entries/samples及总数；需要新窗口先Stop/Reset。活动表和结果无锁；宿主串行控制并在释放VM/函数元数据前Stop并处理或Destroy结果。该模式sample_period=0，旧COUNT事件只转发不新增样本。
 *
 * ## References
 * - tests/profile/test_profile_deterministic.c:47 — 现有计时用例的调用、计数/包含时间关系断言及销毁顺序（本报告未执行）。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:426 — 启动检查、链表登记与安装包装hook的实际执行顺序。
 */

/**
 * @fn TZrBool ZrDebug_Profile_StartWithSampling(ZrDebugProfile *profile, SZrState *state, TZrUInt32 samplePeriod)
 * ## Summary
 * 启动CALL/RETURN计时并按VM基础指令COUNT间隔采集函数/行热点；CLI当前以周期1使用。
 *
 * ## Parameters
 * @param[in,out] profile 已 Init且非active的对象，成功后持有内部数组与旧hook快照。
 * @param[in,out] state 有效且存活到Stop的VM；其hook槽会被包装。
 * @param[in] samplePeriod 正的VM COUNT基础间隔，不是纳秒、线程定时器或保证每个热点都命中的样本数。
 *
 * ## Returns
 * @return 周期为0立即返回FALSE且不调用内部启动；正数还须满足同state唯一性及旧COUNT基础周期相同等条件，成功为TRUE。
 *
 * ## Constraints and lifecycle
 * 保留已有聚合数据；新样本仅在成功捕获并分配记录时累计。不会固定硬件采样率或保证全行覆盖；串行调用，完成后Stop恢复旧callback/mask/baseperiod，剩余倒计数相位不保留。
 *
 * ## References
 * - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:995 — CLI分配对象、Init并按COUNT周期1启动的生产入口。
 * - tests/profile/test_profile_deterministic.c:94 — 现有采样用例的调用及热点行/正样本断言（本报告未执行）。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:1048 — VM指令COUNT倒计数及重置派发，不是墙钟采样。
 */

/**
 * @fn void ZrDebug_Profile_Stop(ZrDebugProfile *profile)
 * ## Summary
 * 停止活动采集，在state仍有效时恢复启动时保存的hook配置，然后摘活动表，保留聚合结果供读取。
 *
 * ## Parameters
 * @param[in,out] profile 已Init对象；NULL或非active时无操作。活动对象和借用state仍须存活。
 *
 * ## Returns
 * 无返回值；普通完成后active为FALSE、state/旧hook快照/采样周期清零，计时帧数清零；结果数组与聚合计数不释放。
 *
 * ## Constraints and lifecycle
 * 不结算尚未匹配RETURN的计时帧。恢复的是callback/mask/base COUNT间隔，Core SetHook重置倒计数；不会合并活动期间外部SetHook的新配置。串行且在VM销毁前执行，之后可读结果、Reset或Destroy。
 *
 * ## References
 * - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:1191 — CLI成功执行后Stop、输出聚合再Destroy的实际顺序。
 * - tests/profile/test_profile_deterministic.c:53 — 现有计时用例的调用、计数/包含时间关系断言及销毁顺序（本报告未执行）。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:663 — SetHook归一化/重置COUNT相位与GetHookCount读取基础周期。
 */

/**
 * @fn void ZrDebug_Profile_Destroy(ZrDebugProfile *profile)
 * ## Summary
 * 结束活动会话并释放Profile拥有的三类C数组，最终回到可复用的Init空态，不释放调用方对象或VM。
 *
 * ## Parameters
 * @param[in,out] profile 已Init的对象；NULL无操作。若active，借用state必须仍有效以便Stop恢复hook。
 *
 * ## Returns
 * 无返回值；释放entries/frames/samples并清零对象，所有结果借用指针失效。
 *
 * ## Constraints and lifecycle
 * 活动时先Stop再free，非活动时直接释放；不得在hook回调或与读取/记录并发时调用。函数元数据与state仍由宿主负责，库不为结果数组添加GC根。
 *
 * ## References
 * - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:75 — CLI异常清理先Stop再Destroy的实际调用。
 * - tests/profile/test_profile_deterministic.c:67 — 现有计时用例的调用、计数/包含时间关系断言及销毁顺序（本报告未执行）。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:513 — Destroy释放数组或公开结果访问的有效期依据。
 */

/**
 * @fn TZrSize ZrDebug_Profile_GetEntryCount(const ZrDebugProfile *profile)
 * ## Summary
 * 读取当前有效函数聚合项数，供停止后的结果遍历；它不是调用次数或闭包实例总数。
 *
 * ## Parameters
 * @param[in] profile 借用的已Init对象；NULL被视为空。
 *
 * ## Returns
 * @return 非NULL返回entry_count，NULL返回0；不分配、不转移所有权。
 *
 * ## Constraints and lifecycle
 * 活动采集或Reset会改变计数，本函数不提供同步快照；遍历应在串行停止窗口进行。
 *
 * ## References
 * - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:139 — CLI停止后遍历并按值输出函数聚合。
 */

/**
 * @fn const ZrDebugProfileEntry *ZrDebug_Profile_GetEntry(const ZrDebugProfile *profile, TZrSize index)
 * ## Summary
 * 按逻辑索引借用一个函数聚合项，向CLI或宿主交付调用次数、返回次数与时间快照。
 *
 * ## Parameters
 * @param[in] profile 借用的已Init对象；NULL返回NULL。
 * @param[in] index 小于entry_count的逻辑索引；不以数组容量作为有效范围。
 *
 * ## Returns
 * @return 有效时返回内部entries元素的const借用指针；NULL对象或越界为NULL，调用者不得free。
 *
 * ## Constraints and lifecycle
 * entries成功扩容或Destroy可使地址失效；Reset使逻辑记录失效，后续写入可复用该地址。active期间内容仍可改变；function仅借用VM元数据，读者须保证其存活，不获得GC保留权。
 *
 * ## References
 * - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:139 — CLI停止后遍历并按值输出函数聚合。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:103 — 容量倍增、C realloc成功发布及普通失败保留旧地址。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:413 — Reset清逻辑计数而保留容量的实现。
 */

/**
 * @fn const ZrDebugProfileEntry *ZrDebug_Profile_FindByName(const ZrDebugProfile *profile, const TZrChar *name)
 * ## Summary
 * 在当前函数聚合表中查找名称快照完全相同的第一项，方便测试及宿主按可读名称定位结果。
 *
 * ## Parameters
 * @param[in] profile 借用的已Init结果对象；NULL返回NULL。
 * @param[in] name 借用的NUL结尾查询串，需在查找期间有效；NULL返回NULL。
 *
 * ## Returns
 * @return 返回entries顺序中第一个strcmp相等项的const借用指针；未命中或参数为空返回NULL。
 *
 * ## Constraints and lifecycle
 * 名称是定长、按字节截断的快照；同名不同原型及截断后同名会有歧义，不是唯一函数身份。有效期与GetEntry相同，不接管查询串或返回对象。
 *
 * ## References
 * - tests/profile/test_profile_deterministic.c:55 — 现有计时用例的调用、计数/包含时间关系断言及销毁顺序（本报告未执行）。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:207 — 函数指针键、函数表扩容与文字快照。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:526 — Destroy释放数组或公开结果访问的有效期依据。
 */

/**
 * @fn TZrSize ZrDebug_Profile_GetSampleCount(const ZrDebugProfile *profile)
 * ## Summary
 * 读取当前有效函数/行样本组合数，供样本表遍历，不等于COUNT触发或累计样本总数。
 *
 * ## Parameters
 * @param[in] profile 借用的已Init对象；NULL视为空。
 *
 * ## Returns
 * @return 非NULL返回sample_count，NULL返回0；无分配和所有权转移。
 *
 * ## Constraints and lifecycle
 * active、Reset和新采样会改变结果；需要稳定遍历时先Stop并串行读取。
 *
 * ## References
 * - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:154 — CLI区分行组合数与累计样本总量。
 * - tests/profile/test_profile_deterministic.c:102 — 现有采样用例的调用及热点行/正样本断言（本报告未执行）。
 */

/**
 * @fn TZrUInt64 ZrDebug_Profile_GetTotalSampleCount(const ZrDebugProfile *profile)
 * ## Summary
 * 读取本对象成功记录的COUNT样本累计数，用于报告总量而非热点项数量。
 *
 * ## Parameters
 * @param[in] profile 借用的已Init对象；NULL返回0。
 *
 * ## Returns
 * @return 返回total_sample_count，NULL为0；只在record_sample成功记录后递增，Reset归零。
 *
 * ## Constraints and lifecycle
 * 不自动包含丢弃或分配失败的事件，不保证active读取是同步快照；无饱和溢出检查承诺。
 *
 * ## References
 * - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:154 — CLI区分行组合数与累计样本总量。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:380 — COUNT样本成功记录、分配失败丢弃与总量更新。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:417 — Reset清逻辑计数而保留容量的实现。
 */

/**
 * @fn const ZrDebugProfileSample *ZrDebug_Profile_GetSample(const ZrDebugProfile *profile, TZrSize index)
 * ## Summary
 * 按逻辑索引借用一个函数/源码行的热点样本，供CLI按值输出名称、来源、行和命中数。
 *
 * ## Parameters
 * @param[in] profile 借用的已Init结果对象；NULL返回NULL。
 * @param[in] index 小于sample_count的逻辑索引。
 *
 * ## Returns
 * @return 有效时返回samples元素的const借用指针；NULL对象或越界为NULL，不可由调用者释放。
 *
 * ## Constraints and lifecycle
 * samples扩容或Destroy使地址失效；Reset使旧逻辑记录失效。文本内嵌但function仍借用；active期间数据可变化，不提供并发快照。
 *
 * ## References
 * - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:160 — CLI遍历GetSample并按值输出热点行。
 * - tests/profile/test_profile_deterministic.c:103 — 现有采样用例的调用及热点行/正样本断言（本报告未执行）。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:155 — 容量倍增、C realloc成功发布及普通失败保留旧地址。
 */

/**
 * @anchor zr_debug_profile_start_internal_count_compatibility
 * ## Summary
 * 共享同一VM hook槽时，只允许新采样请求与旧COUNT观察者使用相同基础周期，避免包装层改变旧观察者的触发间隔。
 *
 * ## Value model
 * 仅当samplePeriod非零且previous_mask包含COUNT时比较previous_count；不相等则清备份并返回FALSE，VM原hook保持未动。计时模式samplePeriod=0不提出新周期要求。
 *
 * ## Invariants and lifecycle
 * 这是启动前的普通拒绝，不是对现有COUNT剩余倒计数的保存；真正SetHook仍重置相位。调用方可停止或调整外部观察者后再尝试，不能把本块称并发协调/原子安装。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:434 — 启动检查、链表登记与安装包装hook的实际执行顺序。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:663 — SetHook归一化/重置COUNT相位与GetHookCount读取基础周期。
 */

/**
 * @anchor zr_debug_profile_start_internal_publish_before_hook
 * ## Summary
 * 在Core可能开始派发包装hook之前，先让回调可见的采样周期和活动标志就绪，配合已登记的state映射避免半初始化会话。
 *
 * ## Value model
 * register_active已成功后，顺序写sample_period、active，再SetHook；hook随后可通过state找到本对象并判定是否记录COUNT。
 *
 * ## Invariants and lifecycle
 * 普通串行调用顺序不等同C原子发布或跨线程内存屏障；活动链和数组依旧无锁。Start保留旧聚合结果，安装后必须以Stop在state仍活时恢复快照。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:453 — 启动检查、链表登记与安装包装hook的实际执行顺序。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:364 — COUNT样本成功记录、分配失败丢弃与总量更新。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:385 — 按Core事件记录并向旧hook过滤转发的实际派发。
 */

/**
 * @anchor zr_debug_profile_record_return_parent_self_time
 * ## Summary
 * 把已匹配返回帧的elapsed回灌给剩余计时栈顶父帧，供父帧之后从inclusive时间中扣除子调用耗时。
 *
 * ## Value model
 * 匹配帧及更深帧先弹出；用elapsed与child_time计算并钳零后累计当前项，再仅当还有父帧时把elapsed加到父帧child_time_ns。
 *
 * ## Invariants and lifecycle
 * 不补结算被丢弃的不匹配帧，也不保证异常/OOM时完整调用树。没有匹配帧则该回灌块不执行；计时口径继承clock平台差异。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:337 — RETURN匹配、钳零计时与父帧累计的实现。
 * - tests/profile/test_profile_deterministic.c:59 — 现有计时用例的调用、计数/包含时间关系断言及销毁顺序（本报告未执行）。
 */
