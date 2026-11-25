#ifndef ZR_VM_STRICT_POOL_H
#define ZR_VM_STRICT_POOL_H

#include <stdint.h>
#include "zr_vm_strict/ownership.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ZrStrictPoolSlab ZrStrictPoolSlab;

typedef struct ZrStrictPoolHandle {
    uintptr_t runtime;
    uint64_t pool;
    uint64_t slab;
    uint64_t rental;
    size_t slot;
} ZrStrictPoolHandle;

typedef struct ZrStrictPool {
    ZrStrictRuntime *runtime;
    ZrStrictPoolSlab *slabs;
    ZrStrictLayout layout;
    uint64_t identity, lastSlab, lastRental;
    size_t stride, slabSize, capacity, hardCap, openViews;
    unsigned busy;
} ZrStrictPool;

typedef struct ZrStrictPoolView {
    ZrStrictPool *pool;
    ZrStrictPoolHandle handle;
    unsigned writable;
} ZrStrictPoolView;

typedef void (*ZrStrictReportRental)(void *context, const char *typeName, ZrStrictSite site);

/* Pools are optional, single-threaded owners. The pool and its runtime must
 * stay at stable addresses until the pool is closed. Slab size must be nonzero;
 * a hard cap of zero means no explicit cap. Layout metadata is borrowed.
 * Handles and views cannot outlive their runtime; views are not copyable. */
ZrStrictStatus ZrStrict_PoolInit(ZrStrictRuntime *runtime, ZrStrictPool *pool, const ZrStrictLayout *layout,
                                 size_t slabSize, size_t hardCap);
/* Moves an initialized value's bytes into the pool. On success the caller
 * transfers ownership of the value; on failure it retains ownership. */
ZrStrictStatus ZrStrict_PoolRent(ZrStrictPool *pool, const void *value, ZrStrictSite site, ZrStrictPoolHandle *out);
int ZrStrict_PoolContains(const ZrStrictPool *pool, ZrStrictPoolHandle handle);
/* Open views still see the payload after recycle, but every copy of the
 * handle expires immediately. The last view closes and drops the payload. */
ZrStrictStatus ZrStrict_PoolRecycle(ZrStrictPool *pool, ZrStrictPoolHandle handle);
ZrStrictStatus ZrStrict_PoolOpenView(ZrStrictPool *pool, ZrStrictPoolHandle handle, int writable, ZrStrictPoolView *out);
const void *ZrStrict_PoolViewData(const ZrStrictPoolView *view);
void *ZrStrict_PoolViewDataMut(const ZrStrictPoolView *view);
ZrStrictStatus ZrStrict_PoolCloseView(ZrStrictPoolView *view);
/* Free only entirely empty slabs. Returns the number of slabs released. */
size_t ZrStrict_PoolShrink(ZrStrictPool *pool);
/* Closes only after all views end. Reports and drops every remaining rental,
 * freeing all slabs even if a destructor faults. Callbacks may not reenter. */
ZrStrictStatus ZrStrict_PoolClose(ZrStrictPool *pool, ZrStrictReportRental report, void *context);

#ifdef __cplusplus
}
#endif
#endif
