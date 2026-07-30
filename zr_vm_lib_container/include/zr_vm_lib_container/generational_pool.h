#ifndef ZR_VM_LIB_CONTAINER_GENERATIONAL_POOL_H
#define ZR_VM_LIB_CONTAINER_GENERATIONAL_POOL_H

#include "zr_vm_lib_container/conf.h"

#include <stdint.h>

#define ZR_POOL_STABLE_SLOT_CONTRACT_HASH UINT64_C(0x5a52504f4f4c0002)

typedef struct SZrPool SZrPool;

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

typedef enum EZrPoolSlotState {
    ZR_POOL_SLOT_FREE = 0,
    ZR_POOL_SLOT_LIVE,
    ZR_POOL_SLOT_RETIRED,
    ZR_POOL_SLOT_EXHAUSTED
} EZrPoolSlotState;

typedef enum EZrPoolBorrowMode {
    ZR_POOL_BORROW_READ = 0,
    ZR_POOL_BORROW_WRITE
} EZrPoolBorrowMode;

typedef enum EZrPoolGcScanKind {
    ZR_POOL_GC_SCAN_FREE = 0,
    ZR_POOL_GC_SCAN_MAPPED,
    ZR_POOL_GC_SCAN_BARRIERED
} EZrPoolGcScanKind;

typedef enum EZrPoolConcurrencyMode {
    ZR_POOL_CONCURRENCY_THREAD_LOCAL = 0,
    ZR_POOL_CONCURRENCY_CONCURRENT
} EZrPoolConcurrencyMode;

typedef struct SZrPoolHandle {
    uint64_t poolId;
    TZrSize slotIndex;
    uint64_t generation;
} SZrPoolHandle;

typedef TZrBool (*FZrPoolInitialize)(
        void *destination,
        const void *source,
        void *context);
typedef void (*FZrPoolAbortInitialize)(void *destination, void *context);
typedef void (*FZrPoolDrop)(void *element, void *context);
typedef void (*FZrPoolScan)(void *element, void *context);

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

typedef struct SZrPoolConfig {
    TZrSize slabCapacity;
    uint64_t generationLimit;
    EZrPoolConcurrencyMode concurrencyMode;
} SZrPoolConfig;

typedef struct SZrPoolGuard {
    SZrPool *pool;
    void *value;
    TZrSize slotIndex;
    uint64_t generation;
    EZrPoolBorrowMode mode;
    TZrBool active;
} SZrPoolGuard;

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
} SZrPoolStats;

ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_Create(
        const SZrPoolTypeLayout *layout,
        const SZrPoolConfig *config,
        SZrPool **outPool);

ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_Destroy(SZrPool **pool);

ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_Deliver(
        SZrPool *pool,
        const void *source,
        SZrPoolHandle *outHandle);

ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_Recycle(
        SZrPool *pool,
        SZrPoolHandle handle);

ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_Validate(
        const SZrPool *pool,
        SZrPoolHandle handle);

ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_TryRead(
        SZrPool *pool,
        SZrPoolHandle handle,
        SZrPoolGuard *outGuard);

ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_TryBorrow(
        SZrPool *pool,
        SZrPoolHandle handle,
        SZrPoolGuard *outGuard);

ZR_VM_LIB_CONTAINER_API void *ZrPoolGuard_Value(SZrPoolGuard *guard);

ZR_VM_LIB_CONTAINER_API const void *ZrPoolGuard_ReadOnlyValue(
        const SZrPoolGuard *guard);

ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPoolGuard_Release(
        SZrPoolGuard *guard);

ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_Scan(
        SZrPool *pool,
        uint64_t *outScannedSlots,
        uint64_t *outScannedBytes);

ZR_VM_LIB_CONTAINER_API EZrPoolStatus ZrPool_GetStats(
        const SZrPool *pool,
        SZrPoolStats *outStats);

ZR_VM_LIB_CONTAINER_API const TZrChar *ZrPool_StatusName(
        EZrPoolStatus status);

#endif // ZR_VM_LIB_CONTAINER_GENERATIONAL_POOL_H
