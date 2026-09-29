#ifndef ZR_VM_CORE_GC_YOUNG_ALLOCATION_H
#define ZR_VM_CORE_GC_YOUNG_ALLOCATION_H

/**
 * @file gc_young_allocation.h
 * @brief 为年轻代分配、记忆集和 minor GC 提供可独立测试的标量契约。
 *
 * 这些类型描述分配策略、工作区及状态转换的输入/结果；它们不接管对象图、
 * 执行对象搬迁，也不替运行时暂停或恢复 mutator。调用方必须把此处记录与
 * 自己的堆操作保持一致。公开导出路径为 zr_vm_core.h；仓内直接使用目前由
 * tests/core/test_ssa_young_allocation.c 覆盖，生产收集器仍走 gc_cycle.c 的路径。
 *
 * TODO: 确认 schema/magic 是否已有仓外持久化消费者；当前仓内未发现其读写方，
 * 因而不能据此推断兼容性或版本迁移契约。
 */

#include "zr_vm_core/conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 标记该标量契约版本；目前没有仓内序列化读写者。 */
#define ZR_GC_YOUNG_ALLOCATION_CONTRACT_SCHEMA_VERSION ((TZrUInt32)1u)
/** 契约记录的固定标识；是否属于持久化格式仍待确认，见文件级 TODO。 */
#define ZR_GC_YOUNG_ALLOCATION_MAGIC ((TZrUInt32)0x31474159u) /* YAG1 */
/** card table 将堆偏移按 2 的该次幂换算为卡索引。 */
#define ZR_GC_CARD_SHIFT ((TZrUInt32)9u)
/** 单张卡覆盖的堆字节数；table 初始化时据此计算最低卡数。 */
#define ZR_GC_CARD_BYTES ((TZrSize)1u << ZR_GC_CARD_SHIFT)
/** 未发现 old/permanent 到 young 引用的卡状态。 */
#define ZR_GC_CARD_CLEAN ((TZrByte)0u)
/** 存在需在年轻代回收时重新扫描的跨代存储的卡状态。 */
#define ZR_GC_CARD_DIRTY ((TZrByte)1u)
/** 分配/晋升请求未提供阈值时采用的 large-object 字节界限。 */
#define ZR_GC_YOUNG_DEFAULT_LARGE_OBJECT_THRESHOLD \
    ((TZrSize)256u * (TZrSize)1024u)

/** 诊断返回码；COUNT 是枚举边界，不是可产生的错误。 */
typedef enum EZrGcYoungDiagnosticCode {
    ZR_GC_YOUNG_DIAGNOSTIC_NONE = 0,
    ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_GC_YOUNG_DIAGNOSTIC_INVALID_SCHEMA,
    ZR_GC_YOUNG_DIAGNOSTIC_INVALID_MAGIC,
    ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ALIGNMENT,
    ZR_GC_YOUNG_DIAGNOSTIC_OVERFLOW,
    ZR_GC_YOUNG_DIAGNOSTIC_BOUNDS,
    ZR_GC_YOUNG_DIAGNOSTIC_TLAB_EXHAUSTED,
    ZR_GC_YOUNG_DIAGNOSTIC_SAFEPOINT_REQUIRED,
    ZR_GC_YOUNG_DIAGNOSTIC_INVALID_REGION,
    ZR_GC_YOUNG_DIAGNOSTIC_INVALID_GENERATION,
    ZR_GC_YOUNG_DIAGNOSTIC_TOSPACE_EXHAUSTED,
    ZR_GC_YOUNG_DIAGNOSTIC_MUTATORS_NOT_STOPPED,
    ZR_GC_YOUNG_DIAGNOSTIC_ROOTS_UNAVAILABLE,
    ZR_GC_YOUNG_DIAGNOSTIC_INVALID_PHASE,
    ZR_GC_YOUNG_DIAGNOSTIC_UNRESOLVED_FORWARDING,
    ZR_GC_YOUNG_DIAGNOSTIC_INCONSISTENT_BOUNDARY,
    ZR_GC_YOUNG_DIAGNOSTIC_END_OF_SCAN,
    ZR_GC_YOUNG_DIAGNOSTIC_ALREADY_COMPLETE,
    ZR_GC_YOUNG_DIAGNOSTIC_COUNT
} EZrGcYoungDiagnosticCode;

/**
 * 单次契约失败的机器可读摘要。
 * field/expected/actual 供诊断呈现，不是可跨 API 稳定解释的字段编号。
 * TODO: 若调用方要按 field 编号分流，先定义并版本化跨函数编号表。
 */
typedef struct SZrGcYoungDiagnostic {
    EZrGcYoungDiagnosticCode code;
    TZrUInt32 field;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrGcYoungDiagnostic;

/**
 * Worker-local TLAB bump 状态。begin/cursor/limit 指向调用方管理的当前存储区；
 * 结构本身不拥有该内存，退役后不得继续使用旧区指针。
 */
typedef struct SZrGcTlab {
    TZrByte *begin;
    TZrByte *cursor;
    TZrByte *limit;
    TZrUInt32 regionId;
    TZrSize capacity;
    TZrSize wasteLimit;
    TZrSize retiredBytes;
    TZrSize wasteBytes;
    TZrSize allocationCount;
    TZrSize refillCount;
} SZrGcTlab;

/** 清除可选诊断输出；传 NULL 是安全的无操作。 */
ZR_CORE_API void ZrCore_GcYoung_DiagnosticClear(
        SZrGcYoungDiagnostic *diagnostic);
/** 将诊断码映射为静态名称；未知数值返回通用名称，返回字符串不可释放。 */
ZR_CORE_API const TZrChar *ZrCore_GcYoung_DiagnosticName(
        EZrGcYoungDiagnosticCode code);

/** 初始化一个由调用方提供 backing storage 的 worker-local TLAB。 */
ZR_CORE_API TZrBool ZrCore_GcTlab_Init(
        SZrGcTlab *tlab,
        TZrByte *storage,
        TZrSize capacity,
        TZrUInt32 regionId,
        TZrSize wasteLimit);
/** 检查 TLAB 指针区间与容量记录是否相符；退役状态也视为有效。 */
ZR_CORE_API TZrBool ZrCore_GcTlab_Validate(
        const SZrGcTlab *tlab,
        SZrGcYoungDiagnostic *diagnostic);
/**
 * 在当前 TLAB 中分配并清零一个对齐对象，不申请新 region。
 * @pre storage 生命周期由调用方保证；alignment 必须为非零 2 的幂。
 * @return 成功时返回对象起址；容量不足或契约无效时返回 NULL 且 cursor 不前移。
 */
ZR_CORE_API TZrPtr ZrCore_GcTlab_AllocateFast(
        SZrGcTlab *tlab,
        TZrSize size,
        TZrSize alignment,
        SZrGcYoungDiagnostic *diagnostic);
/**
 * 结算当前 TLAB 的退役与浪费字节；重复退役成功且不重复结算。
 * BUG: 第二个累计值溢出时，retiredBytes 可能已写入后才返回失败；继续重试会重复
 *      计入退役字节。可达路径是累计值接近 SIZE_MAX 后再次 Retire/Refill；代码见
 *      gc_tlab.c 中两个顺序相连的 gc_young_add_size 调用。
 */
ZR_CORE_API TZrBool ZrCore_GcTlab_Retire(
        SZrGcTlab *tlab,
        SZrGcYoungDiagnostic *diagnostic);
/**
 * 在 safepoint 更换 backing storage；旧 TLAB 若仍活动会先退役并累计统计。
 * @pre 新存储区与旧区不应以别名方式同时作为两个活动 TLAB 使用。
 */
ZR_CORE_API TZrBool ZrCore_GcTlab_Refill(
        SZrGcTlab *tlab,
        TZrByte *storage,
        TZrSize capacity,
        TZrUInt32 regionId,
        TZrSize wasteLimit,
        TZrBool atSafepoint,
        SZrGcYoungDiagnostic *diagnostic);

/** 分配策略输出类别；函数仅选择目标，不执行实际分配。 */
typedef enum EZrGcYoungAllocationTarget {
    ZR_GC_YOUNG_ALLOCATION_INVALID = 0,
    ZR_GC_YOUNG_ALLOCATION_TLAB,
    ZR_GC_YOUNG_ALLOCATION_YOUNG_REGION,
    ZR_GC_YOUNG_ALLOCATION_OLD,
    ZR_GC_YOUNG_ALLOCATION_PINNED,
    ZR_GC_YOUNG_ALLOCATION_LARGE
} EZrGcYoungAllocationTarget;

/**
 * 纯标量分配选择输入；可用字节由运行时在同一决策边界采样后提供。
 * nativeVisible/pinned 优先于 TLAB 和 large-object 路径。
 */
typedef struct SZrGcYoungAllocationRequest {
    TZrSize objectBytes;
    TZrSize tlabAvailableBytes;
    TZrSize regionAvailableBytes;
    TZrSize largeObjectThreshold;
    TZrBool nativeVisible;
    TZrBool pinned;
    TZrBool atSafepoint;
} SZrGcYoungAllocationRequest;

/** 策略选择结果；target 与 region/storage/reason 应作为同一组解释。 */
typedef struct SZrGcYoungAllocationDecision {
    EZrGcYoungAllocationTarget target;
    EZrGarbageCollectRegionKind regionKind;
    EZrGarbageCollectStorageKind storageKind;
    EZrGarbageCollectPromotionReason reason;
    TZrSize alignedBytes;
    TZrBool requiresSafepoint;
    TZrBool usesYoungGeneration;
} SZrGcYoungAllocationDecision;

/** 设置默认 large-object 阈值并清零其余请求字段。 */
ZR_CORE_API void ZrCore_GcYoungAllocationRequest_Init(
        SZrGcYoungAllocationRequest *request);
/**
 * 按 native/pin、大小、TLAB 和 region 余量顺序选目标。
 * 成功仅表示策略结果有效；requiresSafepoint 为真时调用方须先完成 safepoint，
 * 再执行 region refill，不能把结果当成已分配对象。
 */
ZR_CORE_API TZrBool ZrCore_GcYoung_SelectAllocation(
        const SZrGcYoungAllocationRequest *request,
        SZrGcYoungAllocationDecision *decision,
        SZrGcYoungDiagnostic *diagnostic);

/** 对象晋升请求；escapeFlags 与 pinFlags 来自调用方已分析的可达/固定约束。 */
typedef struct SZrGcPromotionRequest {
    TZrSize objectBytes;
    TZrSize largeObjectThreshold;
    TZrUInt32 survivalAge;
    TZrUInt32 survivorAgeThreshold;
    TZrUInt32 escapeFlags;
    TZrUInt32 pinFlags;
} SZrGcPromotionRequest;

/** 年轻代存活对象的晋升去向；INVALID 为未成功决策的哨兵。 */
typedef enum EZrGcYoungPromotionTarget {
    ZR_GC_YOUNG_PROMOTION_INVALID = 0,
    ZR_GC_YOUNG_PROMOTION_SURVIVOR,
    ZR_GC_YOUNG_PROMOTION_OLD,
    ZR_GC_YOUNG_PROMOTION_PINNED,
    ZR_GC_YOUNG_PROMOTION_LARGE
} EZrGcYoungPromotionTarget;

/** 晋升目标与 region/storage/reason 的联动结果及下一存活年龄。 */
typedef struct SZrGcPromotionDecision {
    EZrGcYoungPromotionTarget target;
    EZrGarbageCollectRegionKind regionKind;
    EZrGarbageCollectStorageKind storageKind;
    EZrGarbageCollectPromotionReason reason;
    TZrUInt32 nextSurvivalAge;
} SZrGcPromotionDecision;

/** 初始化默认阈值和 survivor 年龄门槛。 */
ZR_CORE_API void ZrCore_GcPromotionRequest_Init(
        SZrGcPromotionRequest *request);
/** 选择 survivor/old/pinned/large 目标；不移动对象或更新存活头信息。 */
ZR_CORE_API TZrBool ZrCore_GcYoung_DecidePromotion(
        const SZrGcPromotionRequest *request,
        SZrGcPromotionDecision *decision,
        SZrGcYoungDiagnostic *diagnostic);

/**
 * 调用方拥有的 card bitmap 与其堆地址窗口。dirtyCount 必须与 cards 内容一致；
 * 本结构不管理缓冲区寿命，也不提供并发同步。
 */
typedef struct SZrGcCardTable {
    TZrByte *cards;
    TZrSize cardCount;
    TZrByte *heapBegin;
    TZrSize heapBytes;
    TZrSize dirtyCount;
} SZrGcCardTable;

/** 记忆根 token 的解释方式；两类 token 互不混用。 */
typedef enum EZrGcRememberedRootKind {
    ZR_GC_REMEMBERED_ROOT_CARD = 0,
    ZR_GC_REMEMBERED_ROOT_OBJECT = 1
} EZrGcRememberedRootKind;

/**
 * 一条可枚举的记忆根：CARD token 是 card index，OBJECT token 是调用方稳定 ID，
 * 不是进程指针；消费方须按 kind 解释 token。
 */
typedef struct SZrGcRememberedRoot {
    EZrGcRememberedRootKind kind;
    TZrUInt64 token;
} SZrGcRememberedRoot;

/**
 * 固定容量、调用方供给的去重记忆根数组。
 * count 指向已初始化前缀，epoch 由创建方标识扫描批次；本 API 不验证 token 是否
 * 属于该 epoch，也不拥有 entries。
 * TODO: 确认 Clear 是否应重置 epoch，以及复用 set 时 epoch 的推进责任。
 */
typedef struct SZrGcRememberedRootSet {
    SZrGcRememberedRoot *entries;
    TZrSize capacity;
    TZrSize count;
    TZrUInt32 epoch;
} SZrGcRememberedRootSet;

/** 初始化 table 并清零调用方提供的 cardCount 字节缓冲区。 */
ZR_CORE_API TZrBool ZrCore_GcCardTable_Init(
        SZrGcCardTable *table,
        TZrByte *cards,
        TZrSize cardCount,
        TZrByte *heapBegin,
        TZrSize heapBytes);
/** 校验 bitmap 覆盖范围和 dirtyCount 上界，不检查缓冲区真实分配长度。 */
ZR_CORE_API TZrBool ZrCore_GcCardTable_Validate(
        const SZrGcCardTable *table,
        SZrGcYoungDiagnostic *diagnostic);
/**
 * 按写入范围及 owner/value generation 标记跨代卡；零字节写入不改变状态。
 * @pre 调用方负责把写屏障调用与实际存储排序，并在并发访问时提供外部同步。
 * TODO: 当前实现使用普通字节与计数更新；确认该表只在停 mutator/单线程阶段操作，
 * 否则需由实际写屏障调用方提供原子性或锁保护。
 */
ZR_CORE_API TZrBool ZrCore_GcCardTable_RecordStore(
        SZrGcCardTable *table,
        TZrByte *address,
        TZrSize size,
        EZrGarbageCollectHeapGenerationKind ownerGeneration,
        EZrGarbageCollectHeapGenerationKind valueGeneration,
        SZrGcYoungDiagnostic *diagnostic);
/** 安全查询给定索引是否为 dirty；越界或无效 table 返回 false。 */
ZR_CORE_API TZrBool ZrCore_GcCardTable_IsDirty(
        const SZrGcCardTable *table,
        TZrSize cardIndex);
/** 读取累计 dirty 数；NULL table 返回 0，其他字段一致性由调用方确保。 */
ZR_CORE_API TZrSize ZrCore_GcCardTable_DirtyCardCount(
        const SZrGcCardTable *table);
/** 清空整个 card 缓冲区并重置 dirtyCount，须与记录写入串行化。 */
ZR_CORE_API TZrBool ZrCore_GcCardTable_Clear(
        SZrGcCardTable *table,
        SZrGcYoungDiagnostic *diagnostic);

/** 初始化固定容量记忆根集并清零其 backing array。 */
ZR_CORE_API TZrBool ZrCore_GcRememberedRootSet_Init(
        SZrGcRememberedRootSet *set,
        SZrGcRememberedRoot *entries,
        TZrSize capacity,
        TZrUInt32 epoch);
/** 记录 card index；相同 kind/token 已存在时保持幂等。 */
ZR_CORE_API TZrBool ZrCore_GcRememberedRootSet_RecordCard(
        SZrGcRememberedRootSet *set,
        TZrSize cardIndex,
        SZrGcYoungDiagnostic *diagnostic);
/** 记录非零、由调用方管理的稳定对象 token；不会将其解析为指针。 */
ZR_CORE_API TZrBool ZrCore_GcRememberedRootSet_RecordObject(
        SZrGcRememberedRootSet *set,
        TZrUInt64 objectToken,
        SZrGcYoungDiagnostic *diagnostic);
/** 按 cursor 顺序复制下一条 root；结束时返回 false 并报告 END_OF_SCAN。 */
ZR_CORE_API TZrBool ZrCore_GcRememberedRootSet_ScanNext(
        const SZrGcRememberedRootSet *set,
        TZrSize *cursor,
        SZrGcRememberedRoot *root,
        SZrGcYoungDiagnostic *diagnostic);
/** 清空容量范围内 entries 并重置 count；保留 epoch 供同一批次后续使用。 */
ZR_CORE_API TZrBool ZrCore_GcRememberedRootSet_Clear(
        SZrGcRememberedRootSet *set,
        SZrGcYoungDiagnostic *diagnostic);

/** Minor transaction 的状态序列；RESUME 只表示记录满足检查，不会唤醒线程。 */
typedef enum EZrGcMinorPhase {
    ZR_GC_MINOR_PHASE_IDLE = 0,
    ZR_GC_MINOR_PHASE_EVACUATE,
    ZR_GC_MINOR_PHASE_REWRITE_REFERENCES,
    ZR_GC_MINOR_PHASE_VERIFY,
    ZR_GC_MINOR_PHASE_RESUME,
    ZR_GC_MINOR_PHASE_COMPLETE,
    ZR_GC_MINOR_PHASE_ABORTED,
    ZR_GC_MINOR_PHASE_COUNT
} EZrGcMinorPhase;

/**
 * 调用方驱动的 minor-GC 进度账本。rootsTraced 等标志是调用方提供的事实声明；
 * 它们不证明对象图扫描、复制、引用改写或调度器暂停真的已经发生。
 */
typedef struct SZrGcMinorTransaction {
    EZrGcMinorPhase phase;
    TZrUInt32 regionCount;
    TZrUInt32 regionCursor;
    TZrUInt64 toSpaceBytes;
    TZrUInt64 toSpaceUsedBytes;
    TZrUInt64 evacuatedBytes;
    TZrUInt64 rewrittenReferences;
    TZrUInt64 promotedObjects;
    TZrUInt64 unresolvedForwarding;
    TZrBool mutatorsStopped;
    TZrBool rootsTraced;
    TZrBool consistentBoundary;
} SZrGcMinorTransaction;

/** 清零事务并设为 IDLE；空指针安全地不执行操作。 */
ZR_CORE_API void ZrCore_GcMinorTransaction_Init(
        SZrGcMinorTransaction *transaction);
/**
 * 从 IDLE/终态开始记录一次停世界工作；本函数只接受调用方提供的停 mutator 与
 * roots 可用标志，不执行停顿、root trace 或预算调度。
 * TODO: workBudget 当前只拒绝零值，具体单位及消费位置尚无调用链证据。
 */
ZR_CORE_API TZrBool ZrCore_GcMinorTransaction_Begin(
        SZrGcMinorTransaction *transaction,
        TZrUInt32 regionCount,
        TZrUInt64 toSpaceBytes,
        TZrUInt32 workBudget,
        TZrBool mutatorsStopped,
        TZrBool rootsAvailable,
        SZrGcYoungDiagnostic *diagnostic);
/**
 * 登记当前 region 的 liveBytes/objectCount 并推进 cursor，不执行复制。
 * @pre 调用方须保证 region 操作已完整提交或在失败时未部分提交，且 mutator 停止。
 * TODO: 确认 regionCount 是否包括空 region，以及 objectCount 是存活数还是晋升数。
 */
ZR_CORE_API TZrBool ZrCore_GcMinorTransaction_EvacuateRegion(
        SZrGcMinorTransaction *transaction,
        TZrUInt64 liveBytes,
        TZrUInt32 objectCount,
        SZrGcYoungDiagnostic *diagnostic);
/** 登记调用方已完成的引用改写数量；要求全部 region 已登记且 mutator 仍停止。 */
ZR_CORE_API TZrBool ZrCore_GcMinorTransaction_RewriteReferences(
        SZrGcMinorTransaction *transaction,
        TZrUInt64 rewrittenReferences,
        SZrGcYoungDiagnostic *diagnostic);
/** 登记调用方扫描得到的未解 forwarding 数；只有零值才开放恢复边界。 */
ZR_CORE_API TZrBool ZrCore_GcMinorTransaction_VerifyForwarding(
        SZrGcMinorTransaction *transaction,
        TZrUInt64 unresolvedForwarding,
        SZrGcYoungDiagnostic *diagnostic);
/** 只读检查事务记录是否满足恢复门槛，不改变状态，也不验证真实堆内容。 */
ZR_CORE_API TZrBool ZrCore_GcMinorTransaction_CanResumeMutators(
        const SZrGcMinorTransaction *transaction);
/** 将满足恢复条件的事务记为 COMPLETE；真实线程恢复仍由运行时调度器负责。 */
ZR_CORE_API TZrBool ZrCore_GcMinorTransaction_ResumeMutators(
        SZrGcMinorTransaction *transaction,
        SZrGcYoungDiagnostic *diagnostic);
/**
 * 仅把活动账本标为 ABORTED，不回滚堆/root，也不自动恢复 mutator。
 * TODO: 仓内无调用方证明 abort 后的 root 恢复策略；调用方须维持停顿并自行恢复或重启。
 */
ZR_CORE_API TZrBool ZrCore_GcMinorTransaction_Abort(
        SZrGcMinorTransaction *transaction,
        SZrGcYoungDiagnostic *diagnostic);

/**
 * 不透明 runtime state 占位类型。当前 scalar façade 不读取该对象；保留参数形状
 * 以便调用方适配运行时入口，不构成 collector 已集成的证据。
 */
struct SZrState;
/** 单次标量 minor transaction 入口所需的 region、to-space 与外部状态声明。 */
typedef struct SZrGcMinorRequest {
    TZrUInt32 regionCount;
    TZrUInt64 toSpaceBytes;
    TZrUInt32 workBudget;
    TZrBool mutatorsStopped;
    TZrBool rootsAvailable;
} SZrGcMinorRequest;

/**
 * façade 成功时返回的账本快照；当前仅为 EVACUATE 初始边界，不含对象搬迁结果。
 * mutatorsResumed 在该入口始终为 false。
 */
typedef struct SZrGcMinorResult {
    EZrGcMinorPhase phase;
    TZrUInt32 regionCursor;
    TZrUInt64 evacuatedBytes;
    TZrUInt64 rewrittenReferences;
    TZrUInt64 promotedObjects;
    TZrBool mutatorsResumed;
    TZrBool consistentBoundary;
} SZrGcMinorResult;

/**
 * 校验 minor 请求并产生初始账本快照；不会使用 state、收集对象或恢复 mutator。
 * 成功仅表示入口标量约束通过，不表示一次 minor GC 已完成。
 */
ZR_CORE_API TZrBool ZrCore_Gc_RunMinorTransaction(
        struct SZrState *state,
        const SZrGcMinorRequest *request,
        SZrGcMinorResult *result,
        SZrGcYoungDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_GC_YOUNG_ALLOCATION_H */
