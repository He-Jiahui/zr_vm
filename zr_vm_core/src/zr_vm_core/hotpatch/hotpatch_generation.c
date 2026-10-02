#include "zr_vm_core/hotpatch_generation.h"

#include <stdint.h>
#include <string.h>

/* 代际 API 共用可选诊断，以便 host 把期望代际与当前代际一起报告。 */
static EZrHotPatchGenerationStatus gen_fail(SZrHotPatchGenerationDiagnostic *d,
                                             EZrHotPatchGenerationStatus s,
                                             TZrUInt64 expected,
                                             TZrUInt64 actual,
                                             TZrUInt32 leases) {
    if (d) { d->status=s; d->expectedGeneration=expected; d->actualGeneration=actual; d->leaseCount=leases; }
    return s;
}
/* 记录槽位、状态与 lease 的复合更新在同一 manager 锁下串行化。
 * Resolve 的 public manager 参数为 const；该锁只是逻辑 const API 的内部
 * 同步状态，manager 本体仍由非 const Init 初始化和持有。 */
static atomic_flag *gen_lock_state(const SZrHotPatchGenerationManager *m) {
    return (atomic_flag *)&m->lock;
}
static void gen_lock(const SZrHotPatchGenerationManager *m) {
    atomic_flag *lock = gen_lock_state(m);
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {}
}
static void gen_unlock(const SZrHotPatchGenerationManager *m) {
    atomic_flag_clear_explicit(gen_lock_state(m), memory_order_release);
}
/* Pointer equality is defined for records from different manager arrays;
 * relational comparisons across those arrays are not. */
static TZrBool gen_belongs(const SZrHotPatchGenerationManager *m,
                           const SZrHotPatchVersionRecord *r) {
    if (!m || !r || !m->records) return ZR_FALSE;
    for (TZrUInt32 i = 0u; i < m->capacity; ++i) {
        if (&m->records[i] == r) return ZR_TRUE;
    }
    return ZR_FALSE;
}
/* 代际号在同一 manager 生命周期内单调递增，不复用已回收槽位的旧编号。 */
static TZrUInt64 gen_next(SZrHotPatchGenerationManager *m) {
    TZrUInt64 previous = atomic_load_explicit(&m->nextGeneration,
                                               memory_order_relaxed);
    if (previous == UINT64_MAX) return 0u;
    ++previous;
    atomic_store_explicit(&m->nextGeneration, previous, memory_order_relaxed);
    return previous;
}

/* caller 提供并持有 records 存储；manager 只初始化槽位，不接管其释放。 */
EZrHotPatchGenerationStatus ZrCore_HotPatch_GenerationManager_Init(SZrHotPatchGenerationManager *m,SZrHotPatchVersionRecord *records,TZrUInt32 capacity,SZrHotPatchGenerationDiagnostic *d) {
    TZrUInt32 i;
    if(!m||!records||capacity==0u) return gen_fail(d,ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT,0,0,0);
    atomic_flag_clear(&m->lock);
    atomic_init(&m->nextGeneration,0u);
    atomic_init(&m->active,ZR_NULL);
    m->records=records; m->capacity=capacity; m->count=0u;
    for (i = 0u; i < capacity; ++i) {
        records[i].generation = 0u;
        records[i].moduleHash = 0u;
        records[i].contentHash = 0u;
        records[i].publicContractHash = 0u;
        records[i].targetProfile = 0u;
        records[i].state = ZR_HOT_PATCH_VERSION_FREE;
        atomic_init(&records[i].leaseCount, 0u);
    }
    return gen_fail(d,ZR_HOT_PATCH_GENERATION_OK,0,0,0);
}
/* host 须先停止发布/读取并释放所有租约，再结束借用的 records 生命周期。 */
void ZrCore_HotPatch_GenerationManager_Deinit(SZrHotPatchGenerationManager *m) { if(m){ atomic_store_explicit(&m->active,ZR_NULL,memory_order_release); m->records=ZR_NULL; m->capacity=0u; m->count=0u; } }

/* 准备槽位只记录已验证候选的不可变身份，不改变新调用的 active 指针。 */
EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Prepare(SZrHotPatchGenerationManager *m,const SZrValidatedHotPatch *v,TZrUInt64 moduleHash,SZrHotPatchGenerationHandle *out,SZrHotPatchGenerationDiagnostic *d) {
    if(!m||!v||!out||!v->manifest||!v->artifact||!v->signatureVerified||!v->immutableContent ||
       v->contentHash == 0u || v->patchId == 0u || v->publicContractHash == 0u || moduleHash == 0u)
        return gen_fail(d,ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT,0,0,0);
    memset(out,0,sizeof(*out)); gen_lock(m);
    SZrHotPatchVersionRecord *slot=ZR_NULL;
    for(TZrUInt32 i=0u;i<m->capacity;i++) if(m->records[i].state==ZR_HOT_PATCH_VERSION_FREE){slot=&m->records[i];break;}
    if(!slot){gen_unlock(m);return gen_fail(d,ZR_HOT_PATCH_GENERATION_CAPACITY,0,0,0);}
    TZrUInt64 g=gen_next(m); if(!g){gen_unlock(m);return gen_fail(d,ZR_HOT_PATCH_GENERATION_OVERFLOW,0,0,0);}
    slot->generation=g; slot->moduleHash=moduleHash; slot->contentHash=v->contentHash; slot->publicContractHash=v->publicContractHash; slot->targetProfile=v->targetProfile; atomic_store_explicit(&slot->leaseCount,0u,memory_order_relaxed); slot->state=ZR_HOT_PATCH_VERSION_PREPARED; if(m->count<m->capacity)m->count++;
    out->record=slot; out->generation=g; out->leased=ZR_FALSE; gen_unlock(m); return gen_fail(d,ZR_HOT_PATCH_GENERATION_OK,0,g,0);
}

/* 取消只归还尚未发布的元数据槽位；编号不回退，借用 artifact 不由此释放。
 * 与 Acquire/Publish 共用 manager 锁，避免检查零租约后再被新 reader pin。 */
EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_DiscardPrepared(
        SZrHotPatchGenerationManager *m, SZrHotPatchGenerationHandle *h,
        SZrHotPatchGenerationDiagnostic *d) {
    if (!m || !h || !h->record || h->leased)
        return gen_fail(d, ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT, 0u, 0u, 0u);
    gen_lock(m);
    if (!gen_belongs(m, h->record)) {
        gen_unlock(m);
        return gen_fail(d, ZR_HOT_PATCH_GENERATION_NOT_PREPARED,
                        h->generation, 0u, 0u);
    }
    SZrHotPatchVersionRecord *r = h->record;
    TZrUInt64 generation = r->generation;
    if (generation != h->generation || r->state != ZR_HOT_PATCH_VERSION_PREPARED) {
        gen_unlock(m);
        return gen_fail(d, ZR_HOT_PATCH_GENERATION_NOT_PREPARED,
                        h->generation, generation, 0u);
    }
    uint_fast32_t leases = atomic_load_explicit(&r->leaseCount, memory_order_relaxed);
    if (leases != 0u || atomic_load_explicit(&m->active, memory_order_relaxed) == r) {
        TZrUInt32 reportedLeases = leases > UINT32_MAX ? UINT32_MAX : (TZrUInt32)leases;
        gen_unlock(m);
        return gen_fail(d, ZR_HOT_PATCH_GENERATION_INVALID_STATE,
                        h->generation, generation, reportedLeases);
    }
    r->generation = 0u;
    r->moduleHash = 0u;
    r->contentHash = 0u;
    r->publicContractHash = 0u;
    r->targetProfile = 0u;
    atomic_store_explicit(&r->leaseCount, 0u, memory_order_relaxed);
    r->state = ZR_HOT_PATCH_VERSION_FREE;
    memset(h, 0, sizeof(*h));
    gen_unlock(m);
    return gen_fail(d, ZR_HOT_PATCH_GENERATION_OK, 0u, generation, 0u);
}

/* 发布使后续 AcquireActive 取得新代际；旧 frame 的租约仍可读取退役槽位。 */
EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Publish(SZrHotPatchGenerationManager *m,SZrHotPatchGenerationHandle *h,SZrHotPatchGenerationDiagnostic *d) {
    if(!m||!h||!h->record||h->leased) {
        return gen_fail(d,ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT,0,0,0);
    }
    gen_lock(m);
    if (!gen_belongs(m, h->record)) {
        gen_unlock(m);
        return gen_fail(d, ZR_HOT_PATCH_GENERATION_NOT_PREPARED,
                        h->generation, 0u, 0u);
    }
    if (h->record->generation != h->generation ||
        h->record->state != ZR_HOT_PATCH_VERSION_PREPARED) {
        TZrUInt64 actualGeneration = h->record->generation;
        gen_unlock(m);
        return gen_fail(d, ZR_HOT_PATCH_GENERATION_NOT_PREPARED,
                        h->generation, actualGeneration, 0u);
    }
    SZrHotPatchVersionRecord *old=atomic_load_explicit(&m->active,memory_order_relaxed); if(old&&old!=h->record&&old->state==ZR_HOT_PATCH_VERSION_ACTIVE) old->state=ZR_HOT_PATCH_VERSION_RETIRED;
    h->record->state=ZR_HOT_PATCH_VERSION_ACTIVE; atomic_store_explicit(&m->active,h->record,memory_order_release); gen_unlock(m); return gen_fail(d,ZR_HOT_PATCH_GENERATION_OK,0,h->generation,0);
}

/* 回滚复制目标代际的身份到新槽位，再由 Publish 切换 active；
 * 不复活旧编号，已持有旧 lease 的调用继续看到原代际。 */
EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Rollback(
        SZrHotPatchGenerationManager *m, TZrUInt64 target,
        SZrHotPatchGenerationHandle *out, SZrHotPatchGenerationDiagnostic *d) {
    if (!m || !out || target == 0u)
        return gen_fail(d, ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT, target, 0u, 0u);
    memset(out, 0, sizeof(*out));
    gen_lock(m);
    SZrHotPatchVersionRecord *source = ZR_NULL;
    for (TZrUInt32 i = 0u; i < m->capacity; ++i) {
        if (m->records[i].generation == target &&
            m->records[i].state != ZR_HOT_PATCH_VERSION_FREE) {
            source = &m->records[i];
            break;
        }
    }
    if (!source) {
        gen_unlock(m);
        return gen_fail(d, ZR_HOT_PATCH_GENERATION_STALE_LINK, target, 0u, 0u);
    }
    SZrHotPatchVersionRecord *slot = ZR_NULL;
    for (TZrUInt32 i = 0u; i < m->capacity; ++i) {
        if (m->records[i].state == ZR_HOT_PATCH_VERSION_FREE) {
            slot = &m->records[i];
            break;
        }
    }
    if (!slot) {
        gen_unlock(m);
        return gen_fail(d, ZR_HOT_PATCH_GENERATION_CAPACITY, target, 0u, 0u);
    }
    TZrUInt64 generation = gen_next(m);
    if (!generation) {
        gen_unlock(m);
        return gen_fail(d, ZR_HOT_PATCH_GENERATION_OVERFLOW, target, 0u, 0u);
    }
    slot->generation = generation;
    slot->moduleHash = source->moduleHash;
    slot->contentHash = source->contentHash;
    slot->publicContractHash = source->publicContractHash;
    slot->targetProfile = source->targetProfile;
    slot->state = ZR_HOT_PATCH_VERSION_PREPARED;
    atomic_store_explicit(&slot->leaseCount, 0u, memory_order_relaxed);
    if (m->count < m->capacity) ++m->count;
    out->record = slot;
    out->generation = generation;
    out->leased = ZR_FALSE;
    gen_unlock(m);
    return gen_fail(d, ZR_HOT_PATCH_GENERATION_OK, target, generation, 0u);
}

/* AcquireActive/Acquire 在 manager 锁下建立 lease，回收器据此保留退役记录。 */
static EZrHotPatchGenerationStatus gen_acquire_locked(
        SZrHotPatchGenerationManager *m, SZrHotPatchVersionRecord *r,
        SZrHotPatchGenerationHandle *out, SZrHotPatchGenerationDiagnostic *d) {
    (void)m;
    if (!r || r->state == ZR_HOT_PATCH_VERSION_FREE)
        return gen_fail(d, ZR_HOT_PATCH_GENERATION_STALE_LINK,
                        0u, r ? r->generation : 0u, 0u);
    /* The atomic may be wider than the public UInt32 lease count. Keep its
     * full width until this lock-protected limit check has passed. */
    uint_fast32_t leases = atomic_load_explicit(&r->leaseCount,
                                               memory_order_relaxed);
    if (leases >= UINT32_MAX)
        return gen_fail(d, ZR_HOT_PATCH_GENERATION_OVERFLOW,
                        0u, r->generation, UINT32_MAX);
    atomic_fetch_add_explicit(&r->leaseCount, 1u, memory_order_relaxed);
    out->record = r;
    out->generation = r->generation;
    out->leased = ZR_TRUE;
    return gen_fail(d, ZR_HOT_PATCH_GENERATION_OK,
                    0u, r->generation, (TZrUInt32)(leases + 1u));
}
/* 新调用入口取得当前 active 的租约，调用完成后必须 Release。 */
EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_AcquireActive(SZrHotPatchGenerationManager *m,SZrHotPatchGenerationHandle *out,SZrHotPatchGenerationDiagnostic *d) { if(!m||!out)return gen_fail(d,ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT,0,0,0); memset(out,0,sizeof(*out)); gen_lock(m); SZrHotPatchVersionRecord *r=atomic_load_explicit(&m->active,memory_order_acquire); EZrHotPatchGenerationStatus s=gen_acquire_locked(m,r,out,d); gen_unlock(m); return s; }
/* 按编号取得仍在槽位中的代际，包括尚未回收的退役版本。 */
EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Acquire(SZrHotPatchGenerationManager *m,TZrUInt64 generation,SZrHotPatchGenerationHandle *out,SZrHotPatchGenerationDiagnostic *d) { if(!m||!out||!generation)return gen_fail(d,ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT,generation,0,0); memset(out,0,sizeof(*out)); gen_lock(m); SZrHotPatchVersionRecord *found=ZR_NULL; for(TZrUInt32 i=0u;i<m->capacity;i++)if(m->records[i].generation==generation){found=&m->records[i];break;} EZrHotPatchGenerationStatus s=gen_acquire_locked(m,found,out,d); gen_unlock(m); return s; }

/* 外 manager 检查只比较槽位地址，必须在任何记录解引用前完成。有效
 * handle 的字段快照随后与 Publish/CollectRetired 一样由 manager 锁保护。 */
EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Resolve(
        const SZrHotPatchGenerationManager *m,
        const SZrHotPatchGenerationHandle *h,
        SZrHotPatchVersionView *out,
        SZrHotPatchGenerationDiagnostic *d) {
    if (!m || !h || !out || !h->record || !h->leased)
        return gen_fail(d, ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT,
                        0u, 0u, 0u);
    if (!gen_belongs(m, h->record))
        return gen_fail(d, ZR_HOT_PATCH_GENERATION_STALE_LINK,
                        h->generation, 0u, 0u);
    gen_lock(m);
    if (h->record->generation != h->generation ||
        h->record->state == ZR_HOT_PATCH_VERSION_FREE) {
        TZrUInt64 actualGeneration = h->record->generation;
        gen_unlock(m);
        return gen_fail(d, ZR_HOT_PATCH_GENERATION_STALE_LINK,
                        h->generation, actualGeneration, 0u);
    }
    SZrHotPatchVersionView snapshot;
    snapshot.generation = h->record->generation;
    snapshot.moduleHash = h->record->moduleHash;
    snapshot.contentHash = h->record->contentHash;
    snapshot.publicContractHash = h->record->publicContractHash;
    snapshot.targetProfile = h->record->targetProfile;
    snapshot.state = (EZrHotPatchVersionState)h->record->state;
    snapshot.leaseCount = atomic_load_explicit(&h->record->leaseCount,
                                               memory_order_acquire);
    *out = snapshot;
    gen_unlock(m);
    return gen_fail(d, ZR_HOT_PATCH_GENERATION_OK, 0u, h->generation,
                    snapshot.leaseCount);
}

/* Release 结束一次代际租约；只有最后一个租约结束后 RetireCollect 才可回收。 */
EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Release(SZrHotPatchGenerationManager *m,SZrHotPatchGenerationHandle *h,SZrHotPatchGenerationDiagnostic *d) { if(!m||!h||!h->record||!h->leased)return gen_fail(d,ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT,0,0,0); gen_lock(m); if(!gen_belongs(m,h->record)||h->record->generation!=h->generation){gen_unlock(m);return gen_fail(d,ZR_HOT_PATCH_GENERATION_STALE_LINK,h->generation,0,0);} TZrUInt32 n=atomic_load_explicit(&h->record->leaseCount,memory_order_relaxed); if(n==0u){gen_unlock(m);return gen_fail(d,ZR_HOT_PATCH_GENERATION_INVALID_STATE,h->generation,h->generation,0);} n=atomic_fetch_sub_explicit(&h->record->leaseCount,1u,memory_order_release)-1u; h->leased=ZR_FALSE; h->record=ZR_NULL; gen_unlock(m); return gen_fail(d,ZR_HOT_PATCH_GENERATION_OK,0,h->generation,n); }
/* TODO: 回收将槽位置 FREE，但未递减 manager.count；确认 count 是累计高水位
 * 还是当前占用数，再决定是否在回收时同步。 */
EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_CollectRetired(SZrHotPatchGenerationManager *m,TZrUInt32 *collected,SZrHotPatchGenerationDiagnostic *d) { if(!m||!collected)return gen_fail(d,ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT,0,0,0); *collected=0u; gen_lock(m); SZrHotPatchVersionRecord *active=atomic_load_explicit(&m->active,memory_order_relaxed); for(TZrUInt32 i=0u;i<m->capacity;i++){SZrHotPatchVersionRecord *r=&m->records[i]; if(r!=active&&r->state==ZR_HOT_PATCH_VERSION_RETIRED&&atomic_load_explicit(&r->leaseCount,memory_order_acquire)==0u){r->generation=0u;r->moduleHash=0u;r->contentHash=0u;r->publicContractHash=0u;r->targetProfile=0u;atomic_store_explicit(&r->leaseCount,0u,memory_order_relaxed);r->state=ZR_HOT_PATCH_VERSION_FREE;(*collected)++;}} gen_unlock(m); return gen_fail(d,ZR_HOT_PATCH_GENERATION_OK,0,0,*collected); }
/* 仅供诊断显示；状态枚举仍是调用方的判定依据。 */
const TZrChar *ZrCore_HotPatch_Generation_StatusName(EZrHotPatchGenerationStatus s){switch(s){case ZR_HOT_PATCH_GENERATION_OK:return "ok";case ZR_HOT_PATCH_GENERATION_CAPACITY:return "capacity";case ZR_HOT_PATCH_GENERATION_STALE_LINK:return "stale-link";case ZR_HOT_PATCH_GENERATION_NOT_PREPARED:return "not-prepared";case ZR_HOT_PATCH_GENERATION_OVERFLOW:return "overflow";default:return "invalid-generation";}}
