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
    TZrSize ignoredRegistryIndex;
    TZrSize rememberedRegistryIndex;
    TZrSize regionDescriptorIndex;
    TZrPtr forwardingAddress;
    TZrPtr forwardingRefLocation;
};

typedef struct SZrGarbageCollectionObjectMark SZrGarbageCollectionObjectMark;


#endif // ZR_OBJECT_CONF_H
