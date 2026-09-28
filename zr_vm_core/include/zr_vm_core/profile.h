//
// Runtime profiling for benchmark/report generation; counters are optional and per global state.
//

#ifndef ZR_VM_CORE_PROFILE_H
#define ZR_VM_CORE_PROFILE_H

#include "zr_vm_core/conf.h"
#include "zr_vm_common/zr_instruction_conf.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

struct SZrGlobalState;
struct SZrState;
/** @brief 统一 profile 指针的线程局部存储修饰符。 */
#define ZR_PROFILE_THREAD_LOCAL ZR_THREAD_LOCAL
/** @brief helperCounts 下标；新增计数项须保留已有数值和报告名称。 */
typedef enum EZrProfileHelperKind {
    ZR_PROFILE_HELPER_VALUE_COPY = 0,
    ZR_PROFILE_HELPER_VALUE_RESET_NULL,
    ZR_PROFILE_HELPER_STACK_GET_VALUE,
    ZR_PROFILE_HELPER_PRECALL,
    ZR_PROFILE_HELPER_GET_MEMBER,
    ZR_PROFILE_HELPER_SET_MEMBER,
    ZR_PROFILE_HELPER_GET_BY_INDEX,
    ZR_PROFILE_HELPER_SET_BY_INDEX,
    ZR_PROFILE_HELPER_VALUE_CONSTRUCT,
    ZR_PROFILE_HELPER_FRAME_VALUE_SLOT_DIRECT,
    ZR_PROFILE_HELPER_FRAME_VALUE_SLOT_CHECKED,
    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_DIRECT,
    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_CHECKED,
    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_EMPTY,
    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_LAYOUT_VISIT,
    ZR_PROFILE_HELPER_FRAME_VALUE_DROP_DIRECT,
    ZR_PROFILE_HELPER_FRAME_VALUE_DROP_CHECKED,
    ZR_PROFILE_HELPER_FRAME_VALUE_INITIALIZATION_DIRECT,
    ZR_PROFILE_HELPER_FRAME_VALUE_INITIALIZATION_CHECKED,
    ZR_PROFILE_HELPER_FRAME_VALUE_COPY_PROBE,
    ZR_PROFILE_HELPER_ENUM_MAX
} EZrProfileHelperKind;
/** @brief slowPathCounts 下标；新增计数项须保留已有数值和报告名称。 */
typedef enum EZrProfileSlowPathKind {
    ZR_PROFILE_SLOWPATH_META_FALLBACK = 0,
    ZR_PROFILE_SLOWPATH_CALLSITE_CACHE_LOOKUP,
    ZR_PROFILE_SLOWPATH_CALLSITE_CACHE_MISS,
    ZR_PROFILE_SLOWPATH_PROTECT_E,
    ZR_PROFILE_SLOWPATH_PROTECT_EH,
    ZR_PROFILE_SLOWPATH_PROTECT_ESH,
    ZR_PROFILE_SLOWPATH_META_CALL_PREPARE,
    ZR_PROFILE_SLOWPATH_ENUM_MAX
} EZrProfileSlowPathKind;
/** @brief quickeningProbeCounts 下标；当前随 recordInstructions 采样，无独立开关。 */
typedef enum EZrProfileQuickeningProbeKind {
    ZR_PROFILE_QUICKENING_PROBE_GET_STACK_TYPED_ARITHMETIC = 0,
    ZR_PROFILE_QUICKENING_PROBE_GET_CONSTANT_TYPED_ARITHMETIC,
    ZR_PROFILE_QUICKENING_PROBE_ENUM_MAX
} EZrProfileQuickeningProbeKind;
/** @brief memoryMetricCounts 下标；写入受 recordMemory 开关控制。 */
typedef enum EZrProfileMemoryMetricKind {
    ZR_PROFILE_MEMORY_ALLOCATION_COUNT = 0,
    ZR_PROFILE_MEMORY_ALLOCATION_BYTES,
    ZR_PROFILE_MEMORY_VALUE_COPY_BYTES,
    ZR_PROFILE_MEMORY_WRITE_BARRIER_COUNT,
    ZR_PROFILE_MEMORY_MINOR_COLLECTION_COUNT,
    ZR_PROFILE_MEMORY_FULL_COLLECTION_COUNT,
    ZR_PROFILE_MEMORY_MARK_OBJECT_COUNT,
    ZR_PROFILE_MEMORY_REWRITE_OBJECT_COUNT,
    ZR_PROFILE_MEMORY_PROMOTED_BYTES,
    ZR_PROFILE_MEMORY_RAW_INT_HIT_COUNT,
    ZR_PROFILE_MEMORY_NODE_MAP_MATERIALIZATION_COUNT,
    ZR_PROFILE_MEMORY_RAW_NODE_SYNC_COUNT,
    ZR_PROFILE_MEMORY_MEMBER_CACHE_HIT_COUNT,
    ZR_PROFILE_MEMORY_MEMBER_CACHE_MISS_COUNT,
    ZR_PROFILE_MEMORY_MEMBER_CACHE_INVALIDATION_COUNT,
    ZR_PROFILE_MEMORY_SCAN_BYTES,
    ZR_PROFILE_MEMORY_MEMBER_CACHE_MONOMORPHIC_HIT_COUNT,
    ZR_PROFILE_MEMORY_MEMBER_CACHE_POLYMORPHIC_HIT_COUNT,
    ZR_PROFILE_MEMORY_MEMBER_CACHE_MEGAMORPHIC_HIT_COUNT,
    ZR_PROFILE_MEMORY_MEMBER_CACHE_META_FALLBACK_COUNT,
    ZR_PROFILE_MEMORY_ENUM_MAX
} EZrProfileMemoryMetricKind;
/** @brief 暂停时长样本环容量；样本满后覆盖最旧项。 */
#define ZR_PROFILE_PAUSE_SAMPLE_CAPACITY ((TZrUInt32)256u)
/** @brief 单个 GlobalState 的 profile 计数、开关、身份文本副本和有界暂停样本环。 */
typedef struct SZrProfileRuntime {
    TZrBool recordInstructions;
    TZrBool recordSlowPaths;
    TZrBool recordHelpers;
    TZrBool recordMemory;
    TZrBool hasOutputPath;
    TZrBool hasCaseName;
    TZrBool hasModeName;
    TZrUInt64 instructionCounts[ZR_INSTRUCTION_ENUM(ENUM_MAX)];
    TZrUInt64 helperCounts[ZR_PROFILE_HELPER_ENUM_MAX];
    TZrUInt64 slowPathCounts[ZR_PROFILE_SLOWPATH_ENUM_MAX];
    TZrUInt64 quickeningProbeCounts[ZR_PROFILE_QUICKENING_PROBE_ENUM_MAX];
    TZrUInt64 memoryMetricCounts[ZR_PROFILE_MEMORY_ENUM_MAX];
    TZrUInt64 pauseCount;
    TZrUInt64 pauseTotalUs;
    TZrUInt64 pauseMaxUs;
    TZrUInt64 pauseSamples[ZR_PROFILE_PAUSE_SAMPLE_CAPACITY];
    TZrUInt32 pauseSampleCount;
    TZrUInt32 pauseSampleNext;
    TZrChar *outputPath;
    TZrChar *caseName;
    TZrChar *modeName;
} SZrProfileRuntime;
/** @brief 当前 OS 线程所绑定的 profileRuntime 借用指针；由 SetCurrentState 维护。 */
#if defined(_MSC_VER)
extern ZR_PROFILE_THREAD_LOCAL SZrProfileRuntime *g_zr_profile_current;
#else
ZR_CORE_API ZR_PROFILE_THREAD_LOCAL SZrProfileRuntime *g_zr_profile_current;
#endif

ZR_CORE_API void ZrCore_Profile_GlobalInit(struct SZrGlobalState *global); /**< @brief 按环境开关创建可选的 global profile；对一个 global 只初始化一次。 */
ZR_CORE_API void ZrCore_Profile_GlobalShutdown(struct SZrGlobalState *global); /**< @brief 写出可选报告并释放 profile；调用前须停止该 global 的执行线程。 */
ZR_CORE_API void ZrCore_Profile_SetCurrentState(struct SZrState *state); /**< @brief 将调用线程绑定到 state 所属 profile；空 state/global 会清除绑定。 */
ZR_CORE_API SZrProfileRuntime *ZrCore_Profile_Current(void); /**< @brief 返回当前线程绑定的借用 profileRuntime；未启用时为空。 */
ZR_CORE_API SZrProfileRuntime *ZrCore_Profile_FromState(struct SZrState *state); /**< @brief 按 state 查找借用 profileRuntime，不读写 TLS 绑定。 */
ZR_CORE_API const TZrChar *ZrCore_Profile_HelperKindName(EZrProfileHelperKind kind); /**< @brief 返回稳定 helper 报告名；非法值返回 "unknown"。 */
ZR_CORE_API const TZrChar *ZrCore_Profile_SlowPathKindName(EZrProfileSlowPathKind kind); /**< @brief 返回稳定 slow-path 报告名；非法值返回 "unknown"。 */
ZR_CORE_API const TZrChar *ZrCore_Profile_QuickeningProbeKindName(EZrProfileQuickeningProbeKind kind); /**< @brief 返回稳定 quickening 报告名；非法值返回 "unknown"。 */
ZR_CORE_API const TZrChar *ZrCore_Profile_MemoryMetricKindName(EZrProfileMemoryMetricKind kind); /**< @brief 返回稳定内存指标报告名；非法值返回 "unknown"。 */
ZR_CORE_API const TZrChar *ZrCore_Profile_InstructionName(EZrInstructionCode opcode); /**< @brief 返回指令枚举对应报告名；非法值返回 "unknown"。 */
/* 热路径仅在对应开关打开且 enum 下标有效时累加；AtomicAdd 只更新目标计数，不发布其他数据。 BUG: 同一 global 可附着多个 mutator；启用 helper profile 后，并发对象写入经 object_record_helper 对共享 helperCounts 普通自增，产生数据竞争和丢计数。 TODO: 逐项核对 instruction/quickening/slow-path 与 pause 字段的并发写入；另核对未见调用点的 RecordInstructionFromState、RecordSlowPathFromState、MemoryEnabledFromState 是否刻意保留。 */
static ZR_FORCE_INLINE void ZrCore_Profile_AtomicAdd(TZrUInt64 *value, TZrUInt64 amount) {
#if defined(_MSC_VER)
    (void)_InterlockedExchangeAdd64((volatile long long *)value, (long long)amount);
#else
    __atomic_fetch_add(value, amount, __ATOMIC_RELAXED);
#endif
}

static ZR_FORCE_INLINE void ZrCore_Profile_RecordHelperCurrent(EZrProfileHelperKind kind) {
#if defined(_MSC_VER)
    SZrProfileRuntime *runtime = ZrCore_Profile_Current();
#else
    SZrProfileRuntime *runtime = g_zr_profile_current;
#endif
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordHelpers)) {
        runtime->helperCounts[kind]++;
    }
}

static ZR_FORCE_INLINE void ZrCore_Profile_RecordHelperFromState(struct SZrState *state, EZrProfileHelperKind kind) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordHelpers)) {
        runtime->helperCounts[kind]++;
    }
}

static ZR_FORCE_INLINE void ZrCore_Profile_RecordSlowPathCurrent(EZrProfileSlowPathKind kind) {
#if defined(_MSC_VER)
    SZrProfileRuntime *runtime = ZrCore_Profile_Current();
#else
    SZrProfileRuntime *runtime = g_zr_profile_current;
#endif
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordSlowPaths)) {
        runtime->slowPathCounts[kind]++;
    }
}

static ZR_FORCE_INLINE void ZrCore_Profile_RecordInstructionFromState(struct SZrState *state, EZrInstructionCode opcode) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordInstructions)) {
        runtime->instructionCounts[opcode]++;
    }
}

static ZR_FORCE_INLINE void ZrCore_Profile_RecordSlowPathFromState(struct SZrState *state,
                                                                   EZrProfileSlowPathKind kind) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordSlowPaths)) {
        runtime->slowPathCounts[kind]++;
    }
}

static ZR_FORCE_INLINE TZrBool ZrCore_Profile_MemoryEnabledFromState(struct SZrState *state) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    return (TZrBool)(runtime != ZR_NULL && runtime->recordMemory);
}

static ZR_FORCE_INLINE void ZrCore_Profile_RecordMemoryCurrent(EZrProfileMemoryMetricKind kind,
                                                               TZrUInt64 amount) {
#if defined(_MSC_VER)
    SZrProfileRuntime *runtime = ZrCore_Profile_Current();
#else
    SZrProfileRuntime *runtime = g_zr_profile_current;
#endif
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordMemory)) {
        ZrCore_Profile_AtomicAdd(&runtime->memoryMetricCounts[kind], amount);
    }
}

static ZR_FORCE_INLINE void ZrCore_Profile_RecordMemoryFromState(struct SZrState *state,
                                                                 EZrProfileMemoryMetricKind kind,
                                                                 TZrUInt64 amount) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordMemory)) {
        ZrCore_Profile_AtomicAdd(&runtime->memoryMetricCounts[kind], amount);
    }
}

static ZR_FORCE_INLINE void ZrCore_Profile_RecordValueCopyCurrent(TZrUInt64 bytes) {
#if defined(_MSC_VER)
    SZrProfileRuntime *runtime = ZrCore_Profile_Current();
#else
    SZrProfileRuntime *runtime = g_zr_profile_current;
#endif
    if (ZR_UNLIKELY(runtime != ZR_NULL)) {
        if (runtime->recordHelpers) {
            ZrCore_Profile_AtomicAdd(&runtime->helperCounts[ZR_PROFILE_HELPER_VALUE_COPY], 1u);
        }
        if (runtime->recordMemory) {
            ZrCore_Profile_AtomicAdd(&runtime->memoryMetricCounts[ZR_PROFILE_MEMORY_VALUE_COPY_BYTES], bytes);
        }
    }
}

static ZR_FORCE_INLINE void ZrCore_Profile_RecordValueCopyFromState(struct SZrState *state,
                                                                    TZrUInt64 bytes) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    if (ZR_UNLIKELY(runtime != ZR_NULL)) {
        if (runtime->recordHelpers) {
            ZrCore_Profile_AtomicAdd(&runtime->helperCounts[ZR_PROFILE_HELPER_VALUE_COPY], 1u);
        }
        if (runtime->recordMemory) {
            ZrCore_Profile_AtomicAdd(&runtime->memoryMetricCounts[ZR_PROFILE_MEMORY_VALUE_COPY_BYTES], bytes);
        }
    }
}
/* recordMemory 开启时由 GC 周期协调路径记录微秒值；累计量保留全部事件，样本环只保留最近的固定容量窗口。 */
static ZR_FORCE_INLINE void ZrCore_Profile_RecordPauseFromState(struct SZrState *state,
                                                                TZrUInt64 durationUs) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordMemory)) {
        TZrUInt32 sampleIndex = runtime->pauseSampleNext;

        runtime->pauseCount++;
        runtime->pauseTotalUs += durationUs;
        if (runtime->pauseMaxUs < durationUs) {
            runtime->pauseMaxUs = durationUs;
        }
        runtime->pauseSamples[sampleIndex] = durationUs;
        runtime->pauseSampleNext = (sampleIndex + 1u) % ZR_PROFILE_PAUSE_SAMPLE_CAPACITY;
        if (runtime->pauseSampleCount < ZR_PROFILE_PAUSE_SAMPLE_CAPACITY) {
            runtime->pauseSampleCount++;
        }
    }
}

#endif
