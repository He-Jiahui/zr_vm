#ifndef ZR_VM_LIB_CONTAINER_GENERATIONAL_POOL_H
#define ZR_VM_LIB_CONTAINER_GENERATIONAL_POOL_H

#include "zr_vm_lib_container/conf.h"

#include "zr_vm_core/type_layout.h"

#include <stdint.h>

/* 供编译期能力检查与 zr.pooling 常量共同识别当前稳定槽位协议版本。 */
#define ZR_POOL_STABLE_SLOT_CONTRACT_HASH UINT64_C(0x5a52504f4f4c0002)

/** @brief 池对象只通过创建、操作和销毁 API 管理；调用方不能直接访问其存储。 */
typedef struct SZrPool SZrPool;

/** @brief 操作结果区分调用错误、句柄身份失效、借用冲突与池关闭阶段。 */
typedef enum EZrPoolStatus {
    ZR_POOL_STATUS_OK = 0,
    ZR_POOL_STATUS_INVALID_ARGUMENT,
    ZR_POOL_STATUS_OUT_OF_MEMORY,
    ZR_POOL_STATUS_WRONG_POOL,
    ZR_POOL_STATUS_HANDLE_STALE,
    ZR_POOL_STATUS_ENTITY_RETIRED,
    ZR_POOL_STATUS_BORROW_CONFLICT,
    ZR_POOL_STATUS_CONSTRUCTION_FAILED,
    ZR_POOL_STATUS_GENERATION_EXHAUSTED,
    ZR_POOL_STATUS_POOL_BUSY,
    ZR_POOL_STATUS_POOL_DESTROYED
} EZrPoolStatus;

/** @brief 内部槽位生命周期；RETIRED 等待现有 guard 释放，EXHAUSTED 永不复用。 */
typedef enum EZrPoolSlotState {
    ZR_POOL_SLOT_FREE = 0,
    ZR_POOL_SLOT_LIVE,
    ZR_POOL_SLOT_RETIRED,
    ZR_POOL_SLOT_EXHAUSTED
} EZrPoolSlotState;

/** @brief guard 的读共享或写独占权限。 */
typedef enum EZrPoolBorrowMode {
    ZR_POOL_BORROW_READ = 0,
    ZR_POOL_BORROW_WRITE
} EZrPoolBorrowMode;

/** @brief 布局扫描策略；MAPPED 扫描全部活动槽位，BARRIERED 只扫描脏槽位。 */
typedef enum EZrPoolGcScanKind {
    ZR_POOL_GC_SCAN_FREE = 0,
    ZR_POOL_GC_SCAN_MAPPED,
    ZR_POOL_GC_SCAN_BARRIERED
} EZrPoolGcScanKind;

/** @brief THREAD_LOCAL 由调用方串行化；CONCURRENT 由池保护内部状态。 */
typedef enum EZrPoolConcurrencyMode {
    ZR_POOL_CONCURRENCY_THREAD_LOCAL = 0,
    ZR_POOL_CONCURRENCY_CONCURRENT
} EZrPoolConcurrencyMode;

/** @brief 池身份、稳定槽位和代数共同构成弱句柄；回收后的同槽新实体不会复活旧句柄。 */
typedef struct SZrPoolHandle {
    uint64_t poolId;
    TZrSize slotIndex;
    uint64_t generation;
} SZrPoolHandle;

/** @brief 把 source 初始化为池拥有的元素；失败时可由 abortInitialize 清理部分构造。 */
typedef TZrBool (*FZrPoolInitialize)(
        void *destination,
        const void *source,
        void *context);
/** @brief 仅在 initialize 失败时清理已获得的外部资源；回调需能处理部分构造对象。 */
typedef void (*FZrPoolAbortInitialize)(void *destination, void *context);
/** @brief 在最后一个 guard 释放后或销毁时析构池拥有的元素。 */
typedef void (*FZrPoolDrop)(void *element, void *context);
/** @brief 扫描池槽位中的 GC 引用；由布局扫描策略决定调用频率。 */
typedef void (*FZrPoolScan)(void *element, void *context);

/** @brief 约定槽位生命周期和 GC 扫描方式，供底层池调用方提供元素操作。
 * @note 池复制此描述符，但 context 指向的数据仍由调用方持有；回调需在池销毁前保持可用。
 */
typedef struct SZrPoolTypeLayout {
    TZrSize elementSize;
    TZrSize elementAlignment;
    EZrPoolGcScanKind gcScanKind;
    FZrPoolInitialize initialize;
    FZrPoolAbortInitialize abortInitialize;
    FZrPoolDrop drop;
    FZrPoolScan scan;
    void *context;
} SZrPoolTypeLayout;

/** @brief 控制扩容、代数上限与同步策略；零容量和零代数上限采用默认值。
 * @pre THREAD_LOCAL 模式要求调用方串行访问同一个池。
 */
typedef struct SZrPoolConfig {
    TZrSize slabCapacity;
    uint64_t generationLimit;
    EZrPoolConcurrencyMode concurrencyMode;
} SZrPoolConfig;

/** @brief 持有槽位借用期间的访问权；释放前即使实体被回收，槽位也不会复用。
 * @note 由 TryRead/TryBorrow 填充，仅调用一次 ZrPoolGuard_Release 结束借用。
 */
typedef struct SZrPoolGuard {
    SZrPool *pool;
    void *value;
    TZrSize slotIndex;
    uint64_t generation;
    EZrPoolBorrowMode mode;
    TZrBool active;
} SZrPoolGuard;

/** @brief 容量、借用、构造/析构与扫描计数的诊断快照；不作为同步控制依据。 */
typedef struct SZrPoolStats {
    uint64_t poolId;
    TZrSize slabCount;
    TZrSize slotCount;
    TZrSize liveCount;
    TZrSize retiredCount;
    TZrSize freeCount;
    TZrSize exhaustedCount;
    TZrSize activeReadCount;
    TZrSize activeWriteCount;
    uint64_t deliverCount;
    uint64_t recycleCount;
    uint64_t reuseCount;
    uint64_t dropCount;
    uint64_t constructionFailureCount;
    uint64_t partialCleanupCount;
    uint64_t handleValidationCount;
    uint64_t barrierMarkCount;
    TZrSize dirtySlotCount;
    uint64_t scanPassCount;
    uint64_t scannedSlotCount;
    uint64_t scannedByteCount;
    uint64_t slabAllocationCount;
} SZrPoolStats;

/** @brief 创建按需分配 slab 的稳定槽位池；调用方最终用 ZrPool_Destroy 释放。
 * @pre layout 的元素大小非零、对齐为 2 的幂；需要 GC 扫描时必须提供 scan 回调。
 * @return 成功写入 outPool；outPool 有效时，失败会清空它并返回原因。
 */
ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_Create(
        const SZrPoolTypeLayout *layout,
        const SZrPoolConfig *config,
        SZrPool **outPool);

/** @brief 用核心类型布局接入对象复制、析构和 GC 追踪，供语言层 Pool<T> 复用同一槽位语义。
 * @pre 需要 VM 状态的布局只能在 THREAD_LOCAL 模式下使用，并传入有效 state。
 * @note state、嵌套布局、元数据表、registry 及回调数据均为借用，必须至少存活至池销毁。
 */
ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_CreateFromTypeLayout(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        FZrTypeLayoutGcValueVisitor visitor,
        TZrPtr visitorUserData,
        const SZrPoolConfig *config,
        SZrPool **outPool);

/** @brief 退役全部实体并释放池；成功后清空调用方指针。
 * @return 尚有 guard 时返回 POOL_BUSY，池保持可释放状态；先释放 guard 再重试销毁。
 */
ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_Destroy(SZrPool **pool);

/** @brief 将 source 初始化到稳定槽位并发布弱句柄；构造失败时回滚槽位。
 * @note 成功后的元素归池所有，调用方应通过 Recycle 或 Destroy 触发析构。
 * @note initialize 可能部分构造失败；若会获得外部资源，布局须提供 abortInitialize。
 */
ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_Deliver(
        SZrPool *pool,
        const void *source,
        SZrPoolHandle *outHandle);

/** @brief 立即使句柄失效，并在最后一个 guard 释放后回收元素。 */
ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_Recycle(
        SZrPool *pool,
        SZrPoolHandle handle);

/** @brief 同时校验池身份、槽位状态和代数，用于拒绝跨池或已失效句柄。 */
ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_Validate(
        const SZrPool *pool,
        SZrPoolHandle handle);

/** @brief 获取可与其他读 guard 共存的只读借用；失败时清空 outGuard。 */
ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_TryRead(
        SZrPool *pool,
        SZrPoolHandle handle,
        SZrPoolGuard *outGuard);

/** @brief 获取与所有读写 guard 互斥的可写借用；失败时清空 outGuard。 */
ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_TryBorrow(
        SZrPool *pool,
        SZrPoolHandle handle,
        SZrPoolGuard *outGuard);

/** @brief 仅对活动的可写 guard 暴露槽位地址；指针只在 guard 生命周期内有效。 */
ZR_VM_LIB_CONTAINER_API void *ZrPoolGuard_Value(SZrPoolGuard *guard);

/** @brief 对任一活动 guard 暴露只读槽位地址；指针只在 guard 生命周期内有效。 */
ZR_VM_LIB_CONTAINER_API const void *ZrPoolGuard_ReadOnlyValue(
        const SZrPoolGuard *guard);

/** @brief 结束借用，必要时完成延迟析构，并使 guard 不再可访问。 */
ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPoolGuard_Release(
        SZrPoolGuard *guard);

/** @brief 按布局的 FREE/MAPPED/BARRIERED 策略执行 scan 回调并返回本次扫描量。
 * @note 写 guard 活动期间，BARRIERED 槽位的脏标记保持到后续扫描。
 */
ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_Scan(
        SZrPool *pool,
        uint64_t *outScannedSlots,
        uint64_t *outScannedBytes);

/** @brief 对核心类型布局中的 GC 引用执行完整追踪，供持有池的 VM 对象扫描时调用。
 * @pre 池须由 ZrPool_CreateFromTypeLayout 创建并具备可扫描的规范布局。
 */
ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_TraceGcValues(
        SZrPool *pool,
        FZrTypeLayoutGcValueVisitor visitor,
        TZrPtr visitorUserData,
        uint64_t *outScannedSlots,
        uint64_t *outScannedBytes);

/** @brief 获取用于容量、借用和扫描诊断的当前统计快照。 */
ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_GetStats(
        const SZrPool *pool,
        SZrPoolStats *outStats);

/** @brief 将池状态映射为稳定的诊断名称。 */
ZR_VM_LIB_CONTAINER_API const TZrChar *ZrPool_StatusName(
        EZrPoolStatus status);

#endif // ZR_VM_LIB_CONTAINER_GENERATIONAL_POOL_H
