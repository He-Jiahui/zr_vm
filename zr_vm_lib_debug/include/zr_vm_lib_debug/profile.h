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

/**
 * @struct ZrDebugProfileEntry
 * ## Summary
 * 函数元数据指针身份对应的调用/返回与 inclusive/self 计时聚合项，由 Profile 内部数组按值保存，供停止后报告。
 *
 * ## Fields
 * - function: 借用的 SZrFunction 元数据身份键；共享该指针的调用合并，本项不添加 GC 根。
 * - name: 新建项时复制的定长函数名快照，空名替为<anonymous>；容量含NUL。
 * - source: 新建项时复制的定长来源字节串，NULL来源为空；可能字节截断。
 * - call_count: 成功取得聚合项的CALL事件数；帧分配失败后也可保留一次计数。
 * - return_count: 成功取得聚合项的RETURN事件数；无匹配计时帧时也可递增。
 * - total_time_ns: 匹配返回时累计的inclusive clock换算差值，不包含Stop丢弃的开放帧。
 * - self_time_ns: 每次匹配返回以elapsed减child_time且钳零后累计的self时间。
 *
 * ## Invariants and lifecycle
 * 容器拥有内嵌文本和计数；function/state由宿主维持。Reset使逻辑记录失效，entries扩容或Destroy使借用地址失效。无并发快照或跨平台统一时钟、异常/OOM完整计数承诺；不能把布局当稳定序列化ABI。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:193 — 函数指针键、函数表扩容与文字快照。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:303 — CALL先计数再分配帧的部分成功路径。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:328 — RETURN匹配、钳零计时与父帧累计的实现。
 * - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:139 — CLI停止后遍历并按值输出函数聚合。
 */

/**
 * @struct ZrDebugProfileSample
 * ## Summary
 * COUNT热点表中的函数/源码行聚合项，把暂借的元数据文字复制为报告可读取的内嵌快照。
 *
 * ## Fields
 * - function: 借用的函数元数据身份，与 line 共同构成键，不由样本表保持存活。
 * - name: 创建时复制的名称；NULL/空名替为<anonymous>，按字节截断。
 * - source: 创建时复制的来源路径或空串，内容属于样本记录。
 * - line: Core解析的源码行；元数据不足时可为0，并未强制非零。
 * - sample_count: 成功记录到该键的样本数；丢弃或扩容失败的事件不计入。
 *
 * ## Invariants and lifecycle
 * 通过GetSample借用，不可独立free。samples扩容/Destroy可使地址失效，Reset使逻辑身份失效；活动期可变且未同步。记录是热点统计，不证明行覆盖率、绝对采样频率或稳定ABI。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:225 — 函数/行键查找、样本扩容与文字快照。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:360 — COUNT样本成功记录、分配失败丢弃与总量更新。
 * - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:160 — CLI遍历GetSample并按值输出热点行。
 */

/**
 * @struct ZrDebugProfileFrame
 * ## Summary
 * Profile内部计时栈帧，通过entries索引保存一个已观察CALL的计时起点及已结算子调用耗时，供匹配RETURN计算self时间。
 *
 * ## Fields
 * - entry_index: 函数聚合项的逻辑索引，不是entry指针；entries扩容后索引仍可定位。
 * - start_time_ns: CALL时clock换算起点，计时口径取决于C运行库。
 * - child_time_ns: 返回并匹配的直接子帧elapsed累加量，用于从父elapsed中扣除。
 *
 * ## Invariants and lifecycle
 * frames数组由Profile持有并用free释放，不独立拥有函数或VM。RETURN向下匹配时可丢弃更深帧，Stop/Reset将frame_count清零而不补结算。该类型公开布局供本实现协作，不是可跨版本保存的栈ABI。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:307 — CALL先计数再分配帧的部分成功路径。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:337 — RETURN匹配、钳零计时与父帧累计的实现。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:499 — Stop恢复hook、摘链并丢弃开放计时帧的生命周期依据。
 */

/**
 * @struct ZrDebugProfile
 * ## Summary
 * 调用方分配的采集会话，包装一个state的Core hook并拥有函数、行样本和计时帧三类聚合数组；以Init/Start/Stop/Destroy控制生命周期。
 *
 * ## Fields
 * - state: 活动期间借用的VM，必须活到Stop恢复hook；Stop后置NULL。
 * - previous_hook: Start保存的原回调，包装hook按previous_mask转发。
 * - previous_mask: Start保存的原事件订阅位；不代表并发所有权令牌。
 * - previous_count: 原COUNT基础周期，不含剩余倒计数相位。
 * - active: 安装会话状态；发布在SetHook之前，Stop清零。
 * - entries: 本对象拥有的C realloc函数聚合数组，Destroy配对free。
 * - entry_count: entries中逻辑有效项数；Reset归零而非擦除字节。
 * - entry_capacity: entries已分配元素容量，与逻辑长度分开。
 * - frames: 本对象拥有的C realloc计时栈数组。
 * - frame_count: 当前未匹配计时帧数，Start/Stop/Reset会清零。
 * - frame_capacity: frames已分配容量，Reset保留。
 * - samples: 本对象拥有的C realloc函数/行样本数组。
 * - sample_count: 有效样本组合数，不是累计命中数。
 * - sample_capacity: samples已分配容量，Reset保留。
 * - total_sample_count: 成功记录的COUNT命中总数，Reset归零。
 * - sample_period: 本对象采样周期，0不采样但可转发旧COUNT。
 * - next_active: 无锁进程级活动链表的借用链接，Stop摘除并置NULL。
 *
 * ## Invariants and lifecycle
 * Init仅适用于新对象或已释放对象；成功Start不清已有聚合，Stop保留结果并丢未结算帧，Destroy释放数组再Init。不复制活动对象，不并发访问，不在hook回调中重启/销毁。state和元数据均不由Profile保留；结构布局不是持久化或稳定跨模块版本ABI。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:406 — Init建立全零空态的实现。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:426 — 启动检查、链表登记与安装包装hook的实际执行顺序。
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c:488 — Stop恢复hook、摘链并丢弃开放计时帧的生命周期依据。
 * - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:995 — CLI分配对象、Init并按COUNT周期1启动的生产入口。
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
 * @fn TZrBool ZrDebug_Profile_Start(ZrDebugProfile *profile, struct SZrState *state)
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
 * @fn TZrBool ZrDebug_Profile_StartWithSampling(ZrDebugProfile *profile, struct SZrState *state, TZrUInt32 samplePeriod)
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
