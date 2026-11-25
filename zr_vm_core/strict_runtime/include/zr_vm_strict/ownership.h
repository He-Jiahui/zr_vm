#ifndef ZR_VM_STRICT_OWNERSHIP_H
#define ZR_VM_STRICT_OWNERSHIP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ZrStrictStatus {
    ZR_STRICT_OK = 0,
    ZR_STRICT_INVALID_ARGUMENT,
    ZR_STRICT_OUT_OF_MEMORY,
    ZR_STRICT_EXPIRED,
    ZR_STRICT_REFCOUNT_LIMIT,
    ZR_STRICT_BUSY,
    ZR_STRICT_FAULT
} ZrStrictStatus;

typedef struct ZrStrictAllocation ZrStrictAllocation;
typedef struct ZrStrictBuilder { ZrStrictAllocation *record; } ZrStrictBuilder;
typedef struct ZrStrictUnique { ZrStrictAllocation *record; } ZrStrictUnique;
typedef struct ZrStrictShared { ZrStrictAllocation *record; } ZrStrictShared;
typedef struct ZrStrictWeak { ZrStrictAllocation *record; } ZrStrictWeak;
typedef struct ZrStrictManualBlock { ZrStrictUnique owner; } ZrStrictManualBlock;

typedef void *(*ZrStrictAllocate)(void *context, size_t size);
typedef void (*ZrStrictFree)(void *context, void *address);
typedef ZrStrictStatus (*ZrStrictDrop)(void *payload, void *context);

typedef struct ZrStrictAllocator {
    ZrStrictAllocate allocate;
    ZrStrictFree free;
    void *context;
} ZrStrictAllocator;

typedef struct ZrStrictRuntime {
    ZrStrictAllocator allocator;
    ZrStrictAllocation *allocations;
    size_t dropDepth;
    ZrStrictAllocation **dropAppend;
    unsigned closing;
} ZrStrictRuntime;

typedef struct ZrStrictLayout {
    size_t size;
    size_t alignment;
    const char *name;
    ZrStrictDrop drop;
    void *dropContext;
} ZrStrictLayout;

typedef struct ZrStrictSite {
    const char *source;
    unsigned line;
    unsigned column;
} ZrStrictSite;

typedef void (*ZrStrictReportSurvivor)(void *context, const char *typeName,
                                      ZrStrictSite site, size_t strongCount);

/* A runtime is single-threaded and stays at a stable address. A NULL allocator
 * uses malloc/free. Custom allocation must provide ordinary malloc alignment.
 * Allocation/report callbacks must not reenter the runtime. */
ZrStrictStatus ZrStrictRuntime_Init(ZrStrictRuntime *runtime,
                                    const ZrStrictAllocator *allocator);
/* After close, previously issued handles cannot be used. Surviving strong
 * owners are reported before raw memory is released; there is no cycle walk. */
ZrStrictStatus ZrStrictRuntime_Close(ZrStrictRuntime *runtime,
                                     ZrStrictReportSurvivor report, void *context);

/* Slots begin empty ({0}). Copy owners only through the clone operations, and
 * move by clearing the source. The caller/verifier enforces scoped borrows.
 * Layout names, drop contexts and site strings outlive their allocations. */
ZrStrictStatus ZrStrict_OwnBegin(ZrStrictRuntime *runtime, const ZrStrictLayout *layout,
                                ZrStrictSite site, ZrStrictBuilder *out, void **payload);
ZrStrictStatus ZrStrict_OwnCommit(ZrStrictBuilder *builder, ZrStrictUnique *out);
/* Generated cleanup drops initialized fields before aborting construction. */
ZrStrictStatus ZrStrict_OwnAbort(ZrStrictBuilder *builder);
void *ZrStrict_UniqueData(const ZrStrictUnique *owner);
ZrStrictStatus ZrStrict_UniqueDrop(ZrStrictUnique *owner);

ZrStrictStatus ZrStrict_Share(ZrStrictUnique *owner, ZrStrictShared *out);
ZrStrictStatus ZrStrict_SharedClone(const ZrStrictShared *owner, ZrStrictShared *out);
const void *ZrStrict_SharedData(const ZrStrictShared *owner);
ZrStrictStatus ZrStrict_SharedDrop(ZrStrictShared *owner);

ZrStrictStatus ZrStrict_Degrade(const ZrStrictShared *owner, ZrStrictWeak *out);
ZrStrictStatus ZrStrict_WeakClone(const ZrStrictWeak *observer, ZrStrictWeak *out);
/* Expiration leaves out empty. A live upgrade retains before publishing out. */
ZrStrictStatus ZrStrict_Wake(const ZrStrictWeak *observer, ZrStrictShared *out);
ZrStrictStatus ZrStrict_WeakDrop(ZrStrictWeak *observer);

/* The byte view has the lifetime of the block's borrow, including for size 0. */
ZrStrictStatus ZrStrict_ManualAlloc(ZrStrictRuntime *runtime, size_t size,
                                    ZrStrictSite site, ZrStrictManualBlock *out);
void *ZrStrict_ManualData(const ZrStrictManualBlock *block, size_t *size);
ZrStrictStatus ZrStrict_ManualFree(ZrStrictManualBlock *block);

#ifdef __cplusplus
}
#endif
#endif
