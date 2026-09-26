#ifndef ZR_VM_DEBUG_PROFILE_H
#define ZR_VM_DEBUG_PROFILE_H

#include "zr_vm_lib_debug/conf.h"
#include "zr_vm_core/debug.h"

#define ZR_DEBUG_PROFILE_NAME_CAPACITY ZR_DEBUG_NAME_CAPACITY
#define ZR_DEBUG_PROFILE_SOURCE_CAPACITY ZR_DEBUG_TEXT_CAPACITY

/**
 * @brief 一个函数原型的调用和计时聚合项。
 *
 * 时间字段由 C 运行库 `clock()` 的 tick 差换算为纳秒；POSIX 实现通常
 * 计进程 CPU 时间，MSVC CRT 计墙钟时间，跨平台不能直接比较。函数指针
 * 借用 VM 函数，内嵌字符串和计数属于 profile 对象，Destroy 后失效。
 */
typedef struct ZrDebugProfileEntry {
    const struct SZrFunction *function;
    TZrChar name[ZR_DEBUG_PROFILE_NAME_CAPACITY];
    TZrChar source[ZR_DEBUG_PROFILE_SOURCE_CAPACITY];
    TZrUInt64 call_count;
    TZrUInt64 return_count;
    TZrUInt64 total_time_ns;
    TZrUInt64 self_time_ns;
} ZrDebugProfileEntry;

/** @brief COUNT hook 采样按函数和源码行聚合后的结果。 */
typedef struct ZrDebugProfileSample {
    const struct SZrFunction *function;
    TZrChar name[ZR_DEBUG_PROFILE_NAME_CAPACITY];
    TZrChar source[ZR_DEBUG_PROFILE_SOURCE_CAPACITY];
    TZrSize line;
    TZrUInt64 sample_count;
} ZrDebugProfileSample;

/** @brief 内部调用栈帧，用于把 inclusive 时间扣除 child 时间。 */
typedef struct ZrDebugProfileFrame {
    TZrSize entry_index;
    TZrUInt64 start_time_ns;
    TZrUInt64 child_time_ns;
} ZrDebugProfileFrame;

/**
 * @brief 一个 state 上的 profiling 会话及其聚合存储。
 *
 * Start 会保存并暂时扩展 VM hook；同一 state 只允许一个活动 profile。
 * 对象本身由调用方分配；Init 初始化、Destroy 释放其内部数组。
 */
typedef struct ZrDebugProfile {
    struct SZrState *state;
    FZrDebugHook previous_hook;
    TZrUInt32 previous_mask;
    TZrUInt32 previous_count;
    TZrBool active;
    ZrDebugProfileEntry *entries;
    TZrSize entry_count;
    TZrSize entry_capacity;
    ZrDebugProfileFrame *frames;
    TZrSize frame_count;
    TZrSize frame_capacity;
    ZrDebugProfileSample *samples;
    TZrSize sample_count;
    TZrSize sample_capacity;
    TZrUInt64 total_sample_count;
    TZrUInt32 sample_period;
    struct ZrDebugProfile *next_active;
} ZrDebugProfile;

/** @brief 仅初始化未初始化的调用方对象；复用已有对象须先 Destroy。 */
ZR_DEBUG_API void ZrDebug_Profile_Init(ZrDebugProfile *profile);
/** @brief 清除聚合计数但保留容量；活动会话应先 Stop。 */
ZR_DEBUG_API void ZrDebug_Profile_Reset(ZrDebugProfile *profile);
/** @brief 采集调用/返回计时，不开启 COUNT 采样。 */
ZR_DEBUG_API TZrBool ZrDebug_Profile_Start(ZrDebugProfile *profile, struct SZrState *state);
/** @brief 以正数周期开启 COUNT 采样；周期为 0 直接失败。 */
ZR_DEBUG_API TZrBool ZrDebug_Profile_StartWithSampling(ZrDebugProfile *profile,
                                                       struct SZrState *state,
                                                       TZrUInt32 samplePeriod);
/** @brief 停止采集并恢复原 VM hook 配置。 */
ZR_DEBUG_API void ZrDebug_Profile_Stop(ZrDebugProfile *profile);
/** @brief 停止活动会话并释放 entries/frames/samples。 */
ZR_DEBUG_API void ZrDebug_Profile_Destroy(ZrDebugProfile *profile);
/** @brief 返回函数聚合项数；空指针返回 0。 */
ZR_DEBUG_API TZrSize ZrDebug_Profile_GetEntryCount(const ZrDebugProfile *profile);
/** @brief 借用函数结果；活动采集扩容、Reset 或 Destroy 都会使指针失效。 */
ZR_DEBUG_API const ZrDebugProfileEntry *ZrDebug_Profile_GetEntry(const ZrDebugProfile *profile, TZrSize index);
/** @brief 按函数名借用首个聚合项；有效期同 GetEntry。 */
ZR_DEBUG_API const ZrDebugProfileEntry *ZrDebug_Profile_FindByName(const ZrDebugProfile *profile,
                                                                   const TZrChar *name);
/** @brief 返回源码行采样项数；空指针返回 0。 */
ZR_DEBUG_API TZrSize ZrDebug_Profile_GetSampleCount(const ZrDebugProfile *profile);
/** @brief 返回本会话累计 COUNT 样本数。 */
ZR_DEBUG_API TZrUInt64 ZrDebug_Profile_GetTotalSampleCount(const ZrDebugProfile *profile);
/** @brief 借用行样本；活动采集扩容、Reset 或 Destroy 都会使指针失效。 */
ZR_DEBUG_API const ZrDebugProfileSample *ZrDebug_Profile_GetSample(const ZrDebugProfile *profile, TZrSize index);

#endif
