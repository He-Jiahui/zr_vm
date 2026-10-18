//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_OBJECT_CONF_H
#define ZR_OBJECT_CONF_H

#include "zr_vm_common/zr_type_conf.h"

#if defined(ZR_DEBUG)
#define ZR_DEBUG_GARBAGE_COLLECT_MEM_TEST
#endif

/* 传统 GC 阈值与增量步长；调度器和统计快照共享单位，调整时须核对停顿预算。 */
#define ZR_GARBAGE_COLLECT_DEBT_SIZE (-2000)
#define ZR_GARBAGE_COLLECT_MINOR_MULTIPLIER 20
#define ZR_GARBAGE_COLLECT_MAJOR_MULTIPLIER 100
#define ZR_GARBAGE_COLLECT_STEP_MULTIPLIER_PERCENT 100
// how much to allocate before next GC step (log2)
#define ZR_GARBAGE_COLLECT_STEP_LOG2_SIZE 17 /* 128KB */
// wait memory to 200% before starting new gc cycle
#define ZR_GARBAGE_COLLECT_PAUSE_THRESHOLD_PERCENT 200
#define ZR_OBJECT_DENSE_LOOKUP_CACHE_SLOT_COUNT 4
#define ZR_GC_SCOPE_DEPTH_NONE ((TZrUInt32)0xFFFFFFFFu)


/** @brief collector 的代际/增量调度模式，控制扫描路径而非对象公开类型。 */
enum EZrGarbageCollectMode {
    ZR_GARBAGE_COLLECT_MODE_GENERATIONAL,
    ZR_GARBAGE_COLLECT_MODE_INCREMENTAL,
    ZR_GARBAGE_COLLECT_MODE_MAX
};

typedef enum EZrGarbageCollectMode EZrGarbageCollectMode;


/** @brief 增量标记颜色及回收进度；写屏障须保持已扫描对象对子对象的可达性。 */
enum EZrGarbageCollectIncrementalObjectStatus {
    // gc ignore
    ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT,
    // (white) still not scanned on this turn
    ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED,
    // (gray) object is referenced but its children remain unscanned
    ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN,
    // (black) scanned and marked as referenced
    ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED,
    // (white) not referenced and wait to be released
    ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED,
    // (white) mark destructed object as released
    ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED,

    ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_MAX
};

typedef enum EZrGarbageCollectIncrementalObjectStatus EZrGarbageCollectIncrementalObjectStatus;

/** @brief 代际回收的年龄与扫描状态，供 minor/major 周期迁移对象。 */
enum EZrGarbageCollectGenerationalObjectStatus {
    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW,
    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL,
    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_BARRIER,
    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_ALIVE,
    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_LONG_ALIVE,
    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SCANNED,
    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SCANNED_PREVIOUS,

    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_MAX
};

typedef enum EZrGarbageCollectGenerationalObjectStatus EZrGarbageCollectGenerationalObjectStatus;

/** @brief collector 全局运行许可状态；用户停用、自动停用和退出具有不同恢复意图。 */
enum EZrGarbageCollectStatus {
    ZR_GARBAGE_COLLECT_STATUS_RUNNING,
    ZR_GARBAGE_COLLECT_STATUS_STOP_BY_USER,
    ZR_GARBAGE_COLLECT_STATUS_STOP_BY_SELF,
    ZR_GARBAGE_COLLECT_STATUS_STOP_BY_EXITING,
    ZR_GARBAGE_COLLECT_STATUS_MAX,
};

typedef enum EZrGarbageCollectStatus EZrGarbageCollectStatus;

/** @brief 一轮增量回收的阶段游标，决定后续 safepoint 从何处继续。 */
enum EZrGarbageCollectRunningStatus {
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_FLAG_PROPAGATION,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_BEFORE_ATOMIC,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_ATOMIC,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_WAIT_TO_RELEASE_OBJECTS,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_RELEASED_OBJECTS,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_END,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_MAX
};

typedef enum EZrGarbageCollectRunningStatus EZrGarbageCollectRunningStatus;

/** @brief 增量回收周期的 A/B 存活标记；collector 翻转代号后据此识别未触及对象。
 *  此字段与 young/old/permanent 堆代无关，初始化、扫描和搬迁必须使用当前周期代号。 */
enum EZrGarbageCollectGeneration {
    ZR_GARBAGE_COLLECT_GENERATION_INVALID,
    ZR_GARBAGE_COLLECT_GENERATION_A,
    ZR_GARBAGE_COLLECT_GENERATION_B,
};

typedef enum EZrGarbageCollectGeneration EZrGarbageCollectGeneration;

/** @brief 对象所属堆代，用于调度和报告；与物理 region 类别分开。 */
enum EZrGarbageCollectHeapGenerationKind {
    ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG = 0,
    ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_OLD = 1,
    ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_PERMANENT = 2,
    ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_MAX
};

typedef enum EZrGarbageCollectHeapGenerationKind EZrGarbageCollectHeapGenerationKind;

/** @brief region 的统计与分配类别，GC 快照按此聚合 used/live 字节。 */
enum EZrGarbageCollectRegionKind {
    ZR_GARBAGE_COLLECT_REGION_KIND_INVALID = 0,
    ZR_GARBAGE_COLLECT_REGION_KIND_EDEN = 1,
    ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR = 2,
    ZR_GARBAGE_COLLECT_REGION_KIND_OLD = 3,
    ZR_GARBAGE_COLLECT_REGION_KIND_PINNED = 4,
    ZR_GARBAGE_COLLECT_REGION_KIND_LARGE = 5,
    ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT = 6,
    ZR_GARBAGE_COLLECT_REGION_KIND_MAX
};

typedef enum EZrGarbageCollectRegionKind EZrGarbageCollectRegionKind;

/** @brief 对象物理存储策略，决定是否允许移动及是否需要保持 pin。 */
enum EZrGarbageCollectStorageKind {
    ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE = 0,
    ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE = 1,
    ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_PINNED = 2,
    ZR_GARBAGE_COLLECT_STORAGE_KIND_LARGE_PERSISTENT = 3,
    ZR_GARBAGE_COLLECT_STORAGE_KIND_MAX
};

typedef enum EZrGarbageCollectStorageKind EZrGarbageCollectStorageKind;

/** @brief 对象逃逸原因位集；返回、闭包、宿主句柄或跨线程引用会影响晋升和根处理。 */
enum EZrGarbageCollectEscapeKind {
    ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE = 0,
    ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN = 1 << 0,
    ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE = 1 << 1,
    ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT = 1 << 2,
    ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT = 1 << 3,
    ZR_GARBAGE_COLLECT_ESCAPE_KIND_HOST_HANDLE = 1 << 4,
    ZR_GARBAGE_COLLECT_ESCAPE_KIND_OLD_REFERENCE = 1 << 5,
    ZR_GARBAGE_COLLECT_ESCAPE_KIND_PINNED_REFERENCE = 1 << 6,
    ZR_GARBAGE_COLLECT_ESCAPE_KIND_CROSS_THREAD = 1 << 7,
    ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE = 1 << 8
};

typedef enum EZrGarbageCollectEscapeKind EZrGarbageCollectEscapeKind;

/** @brief pin 来源位集；不同持有者可独立设置，释放时须只撤销自身来源。 */
enum EZrGarbageCollectPinKind {
    ZR_GARBAGE_COLLECT_PIN_KIND_NONE = 0,
    ZR_GARBAGE_COLLECT_PIN_KIND_HOST_HANDLE = 1 << 0,
    ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE = 1 << 1,
    ZR_GARBAGE_COLLECT_PIN_KIND_PERSISTENT_ROOT = 1 << 2,
    ZR_GARBAGE_COLLECT_PIN_KIND_LARGE_OBJECT = 1 << 3
};

typedef enum EZrGarbageCollectPinKind EZrGarbageCollectPinKind;

/** @brief 对象晋升的单一诊断原因，供 GC 状态检查与统计解释。 */
enum EZrGarbageCollectPromotionReason {
    ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE = 0,
    ZR_GARBAGE_COLLECT_PROMOTION_REASON_SURVIVAL = 1,
    ZR_GARBAGE_COLLECT_PROMOTION_REASON_ESCAPE = 2,
    ZR_GARBAGE_COLLECT_PROMOTION_REASON_OLD_REFERENCE = 3,
    ZR_GARBAGE_COLLECT_PROMOTION_REASON_PINNED = 4,
    ZR_GARBAGE_COLLECT_PROMOTION_REASON_MODULE_ROOT = 5,
    ZR_GARBAGE_COLLECT_PROMOTION_REASON_GLOBAL_ROOT = 6,
    ZR_GARBAGE_COLLECT_PROMOTION_REASON_HOST_HANDLE = 7,
    ZR_GARBAGE_COLLECT_PROMOTION_REASON_LARGE_OBJECT = 8
};

typedef enum EZrGarbageCollectPromotionReason EZrGarbageCollectPromotionReason;

/** @brief GC 原始对象标签与 VM 值标签共用对应编号，扫描器依此选择对象布局。 */
enum EZrRawObjectType {
    ZR_RAW_OBJECT_TYPE_INVALID,
    ZR_RAW_OBJECT_TYPE_STRING = ZR_VALUE_TYPE_STRING,
    ZR_RAW_OBJECT_TYPE_BUFFER = ZR_VALUE_TYPE_BUFFER,
    ZR_RAW_OBJECT_TYPE_ARRAY = ZR_VALUE_TYPE_ARRAY,
    ZR_RAW_OBJECT_TYPE_FUNCTION = ZR_VALUE_TYPE_FUNCTION,
    ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE = ZR_VALUE_TYPE_CLOSURE_VALUE,
    ZR_RAW_OBJECT_TYPE_CLOSURE = ZR_VALUE_TYPE_CLOSURE,
    ZR_RAW_OBJECT_TYPE_OBJECT = ZR_VALUE_TYPE_OBJECT,
    ZR_RAW_OBJECT_TYPE_THREAD = ZR_VALUE_TYPE_THREAD,
    ZR_RAW_OBJECT_TYPE_NATIVE_POINTER = ZR_VALUE_TYPE_NATIVE_POINTER,
    ZR_RAW_OBJECT_TYPE_NATIVE_DATA = ZR_VALUE_TYPE_NATIVE_DATA,

    ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX
};

typedef enum EZrRawObjectType EZrRawObjectType;

/** @brief 模块、类、接口等原型类别；运行时成员解析按类别选择继承与实例化规则。 */
enum EZrObjectPrototypeType {
    ZR_OBJECT_PROTOTYPE_TYPE_INVALID,
    ZR_OBJECT_PROTOTYPE_TYPE_MODULE,
    ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
    ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
    ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
    ZR_OBJECT_PROTOTYPE_TYPE_ENUM,
    ZR_OBJECT_PROTOTYPE_TYPE_NATIVE,
    ZR_OBJECT_PROTOTYPE_TYPE_UNION,
    ZR_OBJECT_PROTOTYPE_TYPE_MAX
};

typedef enum EZrObjectPrototypeType EZrObjectPrototypeType;

/** @brief 每个 GC 对象的状态头；颜色、周期代号、堆代、region、逃逸与 pin 信息共同约束回收和搬迁。
 *  forwardingAddress 在搬迁后的引用修正阶段指向副本，阶段结束由 collector 清理。
 *  TODO: forwardingRefLocation 当前只见初始化与清零，未见写入有效地址；需确认它是否仍属于搬迁协议。 */
struct SZrGarbageCollectionObjectMark {
    EZrGarbageCollectIncrementalObjectStatus status;
    EZrGarbageCollectGenerationalObjectStatus generationalStatus;
    EZrGarbageCollectGeneration generation;
    TZrUInt32 minorScanEpoch;
    EZrGarbageCollectHeapGenerationKind heapGenerationKind;
    EZrGarbageCollectRegionKind regionKind;
    EZrGarbageCollectStorageKind storageKind;
    TZrUInt32 regionId;
    TZrUInt32 survivalAge;
    TZrUInt32 escapeFlags;
    TZrUInt32 anchorScopeDepth;
    TZrUInt32 pinFlags;
    EZrGarbageCollectPromotionReason promotionReason;
    /* Shared ignored-root ownership, protected by the domain mutation lock. */
    TZrSize temporaryRootRefCount;
    TZrSize nativeCallPinRefCount;
    TZrBool explicitIgnoredRoot;
    TZrBool nativeCallOwnsPinKind;
    TZrSize ignoredRegistryIndex;
    TZrSize rememberedRegistryIndex;
    TZrSize regionDescriptorIndex;
    TZrPtr forwardingAddress;
    TZrPtr forwardingRefLocation;
};

typedef struct SZrGarbageCollectionObjectMark SZrGarbageCollectionObjectMark;


#endif // ZR_OBJECT_CONF_H

/**
 * @def ZR_DEBUG_GARBAGE_COLLECT_MEM_TEST
 * @brief ZR_DEBUG_GARBAGE_COLLECT_MEM_TEST：让 debug 内存检查路径在既定条件下触发额外 GC。
 *
 * ## Summary
 * 让 debug 内存检查路径在既定条件下触发额外 GC。
 *
 * ## Value model
 * 只在 ZR_DEBUG 分支定义；这是配置输入或哨兵，不是即时运行结果。
 *
 * ## Invariants and lifecycle
 * 这是 debug GC 检查分支的编译期开关；启用时仅在 collector 运行且不处于 immediate GC 或 concurrent major 的既定检查点触发额外完整收集。宏定义本身不执行收集、不提供保活或线程同步。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:11：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:1019：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_GARBAGE_COLLECT_MINOR_MULTIPLIER
 * @brief ZR_GARBAGE_COLLECT_MINOR_MULTIPLIER：初始化 collector 的 minor 调度倍率。
 *
 * ## Summary
 * 初始化 collector 的 minor 调度倍率。
 *
 * ## Value model
 * 20；这是配置输入或哨兵，不是即时运行结果。
 *
 * ## Invariants and lifecycle
 * 后续实际调度可复制到 collector 字段；宏定义本身不调用 GC、不提供资源保活或线程同步。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:16：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:449：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_GARBAGE_COLLECT_MAJOR_MULTIPLIER
 * @brief ZR_GARBAGE_COLLECT_MAJOR_MULTIPLIER：初始化 collector 的 major 调度倍率。
 *
 * ## Summary
 * 初始化 collector 的 major 调度倍率。
 *
 * ## Value model
 * 100；这是配置输入或哨兵，不是即时运行结果。
 *
 * ## Invariants and lifecycle
 * 后续实际调度可复制到 collector 字段；宏定义本身不调用 GC、不提供资源保活或线程同步。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:17：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:448：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_GARBAGE_COLLECT_STEP_MULTIPLIER_PERCENT
 * @brief ZR_GARBAGE_COLLECT_STEP_MULTIPLIER_PERCENT：初始化增量 step 百分倍率。
 *
 * ## Summary
 * 初始化增量 step 百分倍率。
 *
 * ## Value model
 * 100；这是配置输入或哨兵，不是即时运行结果。
 *
 * ## Invariants and lifecycle
 * 后续实际调度可复制到 collector 字段；宏定义本身不调用 GC、不提供资源保活或线程同步。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:18：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:450：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_GARBAGE_COLLECT_STEP_LOG2_SIZE
 * @brief ZR_GARBAGE_COLLECT_STEP_LOG2_SIZE：初始化触发 step 的对数步长。
 *
 * ## Summary
 * 初始化触发 step 的对数步长。
 *
 * ## Value model
 * 17，即 1<<17 为 131072 字节；这是配置输入或哨兵，不是即时运行结果。
 *
 * ## Invariants and lifecycle
 * 后续实际调度可复制到 collector 字段；宏定义本身不调用 GC、不提供资源保活或线程同步。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:20：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:451：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_GARBAGE_COLLECT_PAUSE_THRESHOLD_PERCENT
 * @brief ZR_GARBAGE_COLLECT_PAUSE_THRESHOLD_PERCENT：初始化周期暂停阈值百分数。
 *
 * ## Summary
 * 初始化周期暂停阈值百分数。
 *
 * ## Value model
 * 200；这是配置输入或哨兵，不是即时运行结果。
 *
 * ## Invariants and lifecycle
 * 后续实际调度可复制到 collector 字段；宏定义本身不调用 GC、不提供资源保活或线程同步。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:22：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:452：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_GC_SCOPE_DEPTH_NONE
 * @brief ZR_GC_SCOPE_DEPTH_NONE：表示对象尚无可用锚定 scope 深度。
 *
 * ## Summary
 * 表示对象尚无可用锚定 scope 深度。
 *
 * ## Value model
 * UINT32_MAX（显式 TZrUInt32）；这是配置输入或哨兵，不是即时运行结果。
 *
 * ## Invariants and lifecycle
 * 对象锚定深度的无 scope 哨兵；scope 建立、合并与逃逸推广路径按此值解释或更新锚定深度，不作可搬迁或保活凭据；宏定义本身不调用 GC，也不提供线程同步。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:24：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/raw_object.h:69：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/closure/closure.c:22：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/closure/closure.c:25：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/closure/closure.c:291：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/closure/closure.c:309：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrGarbageCollectMode
 * @brief EZrGarbageCollectMode：区分分代与增量 collector 调度入口。
 *
 * ## Summary
 * 区分分代与增量 collector 调度入口。
 *
 * ## Value model
 * GENERATIONAL/INCREMENTAL 为互斥模式，MAX 是边界值而非可运行模式。
 *
 * ## Invariants and lifecycle
 * 模式切换由 collector 入口负责，不能只改对象颜色或据此断言回收线程正在运行。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:28：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:454：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:3273：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_full.c:41：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_mark.c:1746：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrGarbageCollectIncrementalObjectStatus
 * @brief EZrGarbageCollectIncrementalObjectStatus：记录对象在增量扫描和释放中的颜色/阶段。
 *
 * ## Summary
 * 记录对象在增量扫描和释放中的颜色/阶段。
 *
 * ## Value model
 * PERMANENT 跳过普通回收，INITED/WAIT_TO_SCAN/REFERENCED/UNREFERENCED/RELEASED 对应初始化、待扫描、已扫描及释放状态。
 *
 * ## Invariants and lifecycle
 * 颜色只有在当前 collector 周期与写屏障协议下才有意义；枚举本身不保活对象，也不允许无锁修改对象头。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:38：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/raw_object.h:59：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/debug/debug_heap.c:110：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/debug/debug_heap.c:111：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:457：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:418：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrGarbageCollectGenerationalObjectStatus
 * @brief EZrGarbageCollectGenerationalObjectStatus：记录 minor/major 扫描中的年龄与扫描经历。
 *
 * ## Summary
 * 记录 minor/major 扫描中的年龄与扫描经历。
 *
 * ## Value model
 * NEW、SURVIVAL、BARRIER、ALIVE、LONG_ALIVE、SCANNED、SCANNED_PREVIOUS 是状态标签，不是位集。
 *
 * ## Invariants and lifecycle
 * 与 heapGenerationKind、storageKind 及 A/B 周期代号分开解释；不能仅凭一个标签推断对象可搬迁或已生存多少轮。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:58：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/raw_object.h:60：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:2164：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:2177：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:2189：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:2199：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrGarbageCollectStatus
 * @brief EZrGarbageCollectStatus：区分 collector 的运行许可、用户停用、自动停用和退出停用。
 *
 * ## Summary
 * 区分 collector 的运行许可、用户停用、自动停用和退出停用。
 *
 * ## Value model
 * RUNNING、STOP_BY_USER、STOP_BY_SELF、STOP_BY_EXITING 为不同停止意图。
 *
 * ## Invariants and lifecycle
 * 不能把退出停用当作任意恢复的用户暂停；状态修改须由实际调度/域协议安排，类型不提供锁。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:73：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:455：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:681：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:771：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:1020：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrGarbageCollectRunningStatus
 * @brief EZrGarbageCollectRunningStatus：记录一轮增量 GC 的推进阶段，供后续 safepoint 继续。
 *
 * ## Summary
 * 记录一轮增量 GC 的推进阶段，供后续 safepoint 继续。
 *
 * ## Value model
 * 传播、atomic 前后、分段 sweep、结束和 PAUSED 依次编号；MAX 不可当有效阶段。
 *
 * ## Invariants and lifecycle
 * IsInvariant/IsSweeping 使用枚举顺序区间；增删成员须同步所有区间判断，不能任意重排。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:84：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:989：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:185：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:444：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:456：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:597：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrGarbageCollectGeneration
 * @brief EZrGarbageCollectGeneration：在增量周期之间用 A/B 标记区分本轮存活对象。
 *
 * ## Summary
 * 在增量周期之间用 A/B 标记区分本轮存活对象。
 *
 * ## Value model
 * INVALID 是未加入有效周期的初值；A 与 B 是交替的周期代号。
 *
 * ## Invariants and lifecycle
 * 不是 young/old/permanent 堆代，也不是对象年龄。初始化、复制和周期翻转应使用 collector 当前代号。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:101：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/raw_object.h:61：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:458：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_full.c:13：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_object.c:1180：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_object.c:1214：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrGarbageCollectHeapGenerationKind
 * @brief EZrGarbageCollectHeapGenerationKind：将对象归入调度和统计使用的堆代。
 *
 * ## Summary
 * 将对象归入调度和统计使用的堆代。
 *
 * ## Value model
 * YOUNG/OLD/PERMANENT 是堆代标签，MAX 为域边界。
 *
 * ## Invariants and lifecycle
 * 与 regionKind 和 storageKind 分开保存；选择堆代不单独建立 pin 或移动许可。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:110：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/raw_object.h:63：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_remembered_set.c:23：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_remembered_set.c:24：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_remembered_set.c:25：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_remembered_set.c:26：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrGarbageCollectRegionKind
 * @brief EZrGarbageCollectRegionKind：为分配区域及 used/live 统计指定区域类别。
 *
 * ## Summary
 * 为分配区域及 used/live 统计指定区域类别。
 *
 * ## Value model
 * INVALID、EDEN、SURVIVOR、OLD、PINNED、LARGE、PERMANENT 为互斥标签。
 *
 * ## Invariants and lifecycle
 * regionId/regionDescriptorIndex 必须与实际 domain region 表一致；不能把枚举值当作 region 实例身份。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:120：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/raw_object.h:64：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:108：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:113：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:118：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:123：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrGarbageCollectStorageKind
 * @brief EZrGarbageCollectStorageKind：说明对象采用可移动、固定或大对象持久存储策略。
 *
 * ## Summary
 * 说明对象采用可移动、固定或大对象持久存储策略。
 *
 * ## Value model
 * YOUNG_MOVABLE、OLD_MOVABLE、OLD_PINNED、LARGE_PERSISTENT 为独立类别。
 *
 * ## Invariants and lifecycle
 * pin 路径先确保目标 region 可用再修改头和记账；reserve 失败不得把对象标成已成功 pinned。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:134：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:943：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:964：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:428：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:429：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:430：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrGarbageCollectEscapeKind
 * @brief EZrGarbageCollectEscapeKind：按来源累计对象逃逸事实，供晋升和根处理使用。
 *
 * ## Summary
 * 按来源累计对象逃逸事实，供晋升和根处理使用。
 *
 * ## Value model
 * NONE=0，其余 RETURN/CLOSURE_CAPTURE/MODULE_ROOT/GLOBAL_ROOT/HOST_HANDLE/OLD_REFERENCE/PINNED_REFERENCE/CROSS_THREAD/NATIVE_HANDLE 为独立位。
 *
 * ## Invariants and lifecycle
 * 可组合的事实不等于所有权计数或线程同步；设置 CROSS_THREAD 不替代跨线程传输的域和对象存活协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:145：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/raw_object.h:68：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/closure/closure.c:226：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/closure/closure.c:292：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/closure/closure.c:293：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/closure/closure.c:310：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrGarbageCollectPinKind
 * @brief EZrGarbageCollectPinKind：区分使对象保持 pin 的宿主、原生、持久根和大对象来源。
 *
 * ## Summary
 * 区分使对象保持 pin 的宿主、原生、持久根和大对象来源。
 *
 * ## Value model
 * NONE=0，其余来源为位集；多个来源可共存。
 *
 * ## Invariants and lifecycle
 * 撤销时只能撤销自身来源。nativeCallPinRefCount 与 nativeCallOwnsPinKind 区分共享引用和先前已存在的 pin，不能清掉别的持有者。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:161：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_roots.c:137：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:944：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:958：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:960：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc.c:962：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrGarbageCollectPromotionReason
 * @brief EZrGarbageCollectPromotionReason：为晋升记录单一诊断原因。
 *
 * ## Summary
 * 为晋升记录单一诊断原因。
 *
 * ## Value model
 * NONE、SURVIVAL、ESCAPE、OLD_REFERENCE、PINNED、MODULE_ROOT、GLOBAL_ROOT、HOST_HANDLE、LARGE_OBJECT 为单值编号。
 *
 * ## Invariants and lifecycle
 * 不是 escapeFlags/pinFlags 那样的可组合位集；记录原因不独立触发晋升，也不是统计完整因果链。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:172：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/raw_object.h:71：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/closure/closure.c:235：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/closure/closure.c:294：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/closure/closure.c:312：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/closure/closure.c:391：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrRawObjectType
 * @brief EZrRawObjectType：让 GC 扫描器以原始对象标签选择实际对象布局。
 *
 * ## Summary
 * 让 GC 扫描器以原始对象标签选择实际对象布局。
 *
 * ## Value model
 * STRING/BUFFER/ARRAY/FUNCTION/CLOSURE_VALUE/CLOSURE/OBJECT/THREAD/NATIVE_POINTER/NATIVE_DATA 显式借用对应 ZR_VALUE_TYPE 数值；INVALID 与末尾 sentinel 不表示可扫描对象。
 *
 * ## Invariants and lifecycle
 * 标签必须与实际分配布局一致；不得只改标签便把一种对象重解释成另一种。C enum 宽度不因此成为固定 wire 宽度。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:187：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/raw_object.h:34：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1164：定义、配套消费或失败边界证据。
 * - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c:882：定义、配套消费或失败边界证据。
 * - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c:888：定义、配套消费或失败边界证据。
 * - zr_vm_cli/src/zr_vm_cli/repl/repl_session.c:70：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrObjectPrototypeType
 * @brief EZrObjectPrototypeType：在模块、类、接口、结构、枚举、原生和联合原型之间选择解析/实例化规则。
 *
 * ## Summary
 * 在模块、类、接口、结构、枚举、原生和联合原型之间选择解析/实例化规则。
 *
 * ## Value model
 * 互斥标签含 UNION；Rust sys 的现有公开子集不包含所有 C 枚举项。
 *
 * ## Invariants and lifecycle
 * 原型对象必须匹配标签及其成员布局；不能据 Rust 薄封装推断完整一一对应或可传入任意整数。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:206：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/object.h:219：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:1211：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:1213：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:1215：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:1217：定义、配套消费或失败边界证据。
 */

/**
 * @struct SZrGarbageCollectionObjectMark
 * @brief SZrGarbageCollectionObjectMark：作为每个原始 GC 对象的状态头，协调初始化、扫描、逃逸、共享根、pin、区域归属及搬迁。
 *
 * ## Summary
 * 作为每个原始 GC 对象的状态头，协调初始化、扫描、逃逸、共享根、pin、区域归属及搬迁。
 *
 * ## Fields
 * status/generationalStatus/generation/minorScanEpoch 负责扫描；heapGenerationKind/regionKind/storageKind/regionId/regionDescriptorIndex 表示存储；survivalAge/escapeFlags/anchorScopeDepth/promotionReason 记录年龄与逃逸；pinFlags 与共享根计数、显式根标志及 registry 索引配合；forwardingAddress 指向搬迁副本，forwardingRefLocation 尚无有效地址写入证明。
 *
 * ## Invariants and lifecycle
 * 构造入口先初始化全部字段及无索引/无 scope 哨兵。共享 ignored-root/pin 计数是普通字段，必须遵守 domain mutation lock；先 reserve/登记再发布计数和 token；失败边界仅约束本次新增的 root/pin 引用与 token 发布，不保证回滚 registry 扩容或 marking 补标。RootLease 不等于 pin，也不改变区域类别。复制阶段的 forwardingAddress 只在当前修正窗口内有效，不能保存跨阶段裸地址。TODO: forwardingRefLocation 当前仅见初始化/清空；在 gc_cycle 的搬迁与引用重写入口确认是否仍属有效协议，不能据字段名承诺引用追踪。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_object_conf.h:223：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/raw_object.h:53：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_roots.c:21：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_roots.c:36：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_roots.c:115：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:964：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:2528：定义、配套消费或失败边界证据。
 * - tests/core/test_gc_root_leases.c:119：定义、配套消费或失败边界证据。
 * - tests/core/test_gc_root_leases.c:354：定义、配套消费或失败边界证据。
 * - tests/core/test_gc_root_leases.c:431：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/raw_object.h:38：定义、配套消费或失败边界证据。
 */
