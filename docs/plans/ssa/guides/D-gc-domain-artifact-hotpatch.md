---
related_code:
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c
plan_sources:
  - docs/plans/ssa/index.md
  - docs/plans/ssa/architecture-design.md
doc_type: implementation-guide
status: planned
---

# 实现指南 D：GC 状态机、Domain 边界、Artifact 编解码与热更发布（06.x、08.x）

> 待实现草案。既有事实已核实：gc.h 已有 `pauseBudgetUs/remarkBudgetUs`（gc.h:104、233）与 `ZrCore_GarbageCollector_Barrier/BarrierBack`（gc.h:344-346）；artifact_schema.h 已有 `SZrArtifactDiagnostic`（:288）与 section registry；call_binding 磁盘投影是 84→96 字节逐字段编码。

## D.1 TLAB 快慢路（06.01）

```c
/* 热路径两比较一加法，进 ALLOC 指令的 fast lane： */
ZR_FORCE_INLINE TZrPtr zr_gc_tlab_allocate_fast(SZrGcTlab *tlab, TZrSize size) {
    TZrByte *result = tlab->cursor;
    TZrByte *next = result + ZR_ALIGN_UP(size, ZR_GC_ALLOC_ALIGN);
    if (ZR_UNLIKELY(next > tlab->limit)) return NULL;   /* → 慢路 */
    tlab->cursor = next;
    return (TZrPtr)result;
}

/* 慢路（publish 之后才可进入——分配可触发 GC）： */
TZrPtr zr_gc_tlab_allocate_slow(SZrState *state, SZrGcTlab *tlab, TZrSize size) {
    /* 1. size > tlabWasteLimit（如 TLAB 的 1/4）→ 直接从 region 分配，
          不换 TLAB（避免大对象打穿小 buffer）。
       2. 否则 retire 当前 TLAB（剩余空间填 filler 对象保持堆可解析——
          扫描器按对象步进，禁止空洞），换新 region 片段。
       3. region 耗尽 → 触发 minor GC 状态机推进；预算内完不成时
          本次分配仍必须成功（分配是不可失败点），从 major 空间借页
          并记 backpressure。 */
}
```

card table / remembered set（old→young 记录）：

```c
#define ZR_GC_CARD_SHIFT 9              /* 512B/卡 */
/* 写屏障在既有 ZrCore_GarbageCollector_Barrier 内追加 card 标记：
   仅当 object 在 old 且 valueObject 在 young 时
   cardTable[((TZrUIntPtr)object - heapBase) >> ZR_GC_CARD_SHIFT] = DIRTY。
   ExecIR 的 BARRIER 指令消除（02.02 证明 value 必 young→young 或
   必 null）后此调用整体不生成——静态消除，非运行期判断。 */
```

minor evacuation 的不可中断段（index 边界 5 的代码形态）：

```text
以 region 为切片单位推进：
for each youngRegion in budget:
    ── 从这里到本 region 完成是不可中断段 ──
    for each liveObject（按 mark 位图）:
        copy 到 to-space；旧位置写 forwarding pointer
    for each remembered card + 本次已疏散 region 的引用者:
        修复指向本 region 的引用
    ── 不可中断段结束，检查预算 ──
    预算耗尽 → 保存 nextRegionCursor 状态，返回 mutator
★ 半 region 状态永不暴露给 mutator：没有 read barrier 时，
  mutator 不能看到 forwarding pointer。predicate:
  resumeMutatorAllowed ⇔ 所有已开始 region 的引用修复已完成。
  这是 06.01 负测的核心断言（注入预算枯竭在 region 中途，
  断言状态机拒绝返回而是完成当前 region 再停）。
```

## D.2 GC 状态机与预算（06.02）

```c
typedef struct SZrGcBudgetSlice {
    TZrUInt64 microseconds;             /* 对接既有 pauseBudgetUs */
    TZrUInt32 objectCount;
    TZrUInt64 bytes;
} SZrGcBudgetSlice;

typedef struct SZrGcStateMachine {
    EZrGcPhase phase;
    /* 每 phase 的续点，全部是 index/cursor，无裸指针跨 slice 存活： */
    TZrUInt32 markStackCheckpoint;
    TZrUInt32 sweepRegionCursor;
    TZrUInt32 evacuateRegionCursor;
    TZrUInt64 debtBytes;                /* 分配速率 > 回收速率的欠账，驱动下轮预算放大 */
    SZrGcPauseReasonRecord lastPauseReasons[ZR_GC_PAUSE_REASON_RING];
} SZrGcStateMachine;

/* 每帧入口（宿主驱动）： */
void ZrCore_Gc_StepWithBudget(SZrState *state, const SZrGcBudgetSlice *slice) {
    /* phase 推进规则：
       IDLE → (youngPressure) → MINOR_MARK → MINOR_EVACUATE → IDLE
       IDLE → (oldPressure)  → MAJOR_CONCURRENT_MARK（后台线程）
       MAJOR mark 结束 → REMARK（STW，受 remarkBudgetUs 单独约束）
       → SWEEP（分段）→ 可选 COMPACT（选择性，per-region 决策）→ IDLE
       SATB 屏障只在 MAJOR_CONCURRENT_MARK 活跃时启用（全局 flag，
       屏障函数读一次）；与 minor 的 card 屏障叠加而非替换——
       两套屏障语义不混（06.02 "SATB 与 incremental-update 不混搭"）。 */
}
```

pause 归因记录：每次超预算/不可中断段超时记 `{phase, reasonKind, overshootUs}` 环形缓冲，`ZrCore_Gc_GetStats` 暴露——这是 00.01 "GC pause p95/p99" 与 06.02 "暂停原因" 的数据源，实现先于优化。

## D.3 Domain Send/Sync 证明与结构化复制（06.03、06.04）

Send/Sync 是类型级推断结果，缓存在 prototype 上：

```c
typedef enum EZrDomainShareClass {
    ZR_DOMAIN_SHARE_IMMUTABLE = 0,      /* 深不可变 → 直接共享指针（同域）*/
    ZR_DOMAIN_SHARE_SEND_SYNC,          /* 字段全 Send+Sync 且无内部可变 → 共享 */
    ZR_DOMAIN_SHARE_SYNCHRONIZED,       /* 经 mutex/atomic contract → 共享但入口受限 */
    ZR_DOMAIN_SHARE_FORBIDDEN           /* borrowed/stack alias/thread-affine native */
} EZrDomainShareClass;
/* 推断规则自底向上：叶类型查表（标量 Send+Sync；native handle 看
   registration 声明），聚合体取字段最小值；含 mutable 共享字段而无
   sync contract → FORBIDDEN。结果随 layoutGeneration 失效重推。 */
```

跨域 Transfer 是两阶段事务（06.04 的"事务清理"落地）：

```c
TZrBool ZrCore_Domain_TransferValue(const SZrDomainTransferRequest *request,
        SZrDomainTransferResult *result, SZrDomainDiagnostic *diagnostic) {
    /* 阶段 0  预检遍历（只读源图）：DFS 收集对象集，任一对象
              FORBIDDEN/native 不可物化/超深度限制 → 整体拒绝，
              diagnostic 定位到具体对象的路径（identity map 记录父边）。
       阶段 1  目标域分配全部克隆壳（未填充）；OOM → 释放已分配壳，
              源域无任何改动。
       阶段 2  填充与重指向：identity map（源指针 → 目标索引，仅存活于
              本调用栈）保证共享子图/环克隆后拓扑同构。
              ★ 每个字段写入目标域对象走目标域的写屏障；
              ★ materialized handle（如文件/socket）调用 host 注册的
                materializer 创建独立资源，禁止复制源 handle 位模式。
       提交    结果 root 写入 result；两域各自 GC 独立回收，
              全程无一刻存在跨域可达指针——这是 06.04 断言
              "no shared managed/control pointer" 的实现基础。 */
}
```

## D.4 Artifact section codec（08.01）

沿用 writer_call_binding.c 的逐字段小端编码惯例，每个 section 三段式（header/rows/trailer hash）：

```text
section header（固定 24B）：
    u32 magic          （每 section 独立 magic，参照 ZR_CALL_BINDING_SECTION_MAGIC）
    u32 sectionVersion
    u32 rowSize        （编码后行宽，如 binding 的 96）
    u32 rowCount
    u64 contentHash    （rows 区 xxHash64，读侧先验后解）

读侧防御顺序（每步失败 → SZrArtifactDiagnostic 精确字段）：
    1. bounds：header 声称的 rowSize*rowCount 经 checked mul 后 ≤ 剩余文件长
    2. hash：contentHash 匹配（先于任何字段解析——不解析不可信字节）
    3. 逐行逐字段解码到 C 结构，每个 index/token 字段验证目标表界限
       （如 bindingRow 必 < bindingRowCount，layoutId 必 < layoutTableCount）
    4. 语义验证：挂 codec.validate（ExecBC section 在这里进 execbc_verify）
```

ExecIR section 的行编码：指令定长行（与 §2.2 内存布局字段一一对应但**独立声明**编码宽度，绝不 sizeof 内存结构——内存布局可变，磁盘契约冻结）；side pool 各自成 section，range 字段存 pool 内偏移。写侧与读侧共享一张 X-macro 字段表，保证 write/read/copy roundtrip 逐字节一致（08.01 roundtrip 断言的实现前提）。

## D.5 Generation 发布与 lease（08.03）

```c
typedef struct SZrFunctionVersionRecord {
    TZrUInt64 generation;               /* 单调，永不复用（防 ABA）*/
    /* 版本内容：ExecBC/binding rows/maps 的所有权归本 record */
    _Atomic(TZrUInt32) leaseCount;      /* 活跃 frame 持有数 */
    struct SZrFunctionVersionRecord *previousRetired;  /* 退休链，回收器遍历 */
} SZrFunctionVersionRecord;

typedef struct SZrFunctionEntryCell {
    _Atomic(SZrFunctionVersionRecord *) active;
} SZrFunctionEntryCell;

/* 调用入口获取版本——lease 协议禁止裸 load+refcount（08.03 明确要求）： */
SZrFunctionVersionRecord *zr_hotpatch_acquire_active_version(SZrFunctionEntryCell *cell) {
    for (;;) {
        SZrFunctionVersionRecord *record = atomic_load_explicit(&cell->active,
                                                                memory_order_acquire);
        TZrUInt32 leases = atomic_load_explicit(&record->leaseCount, memory_order_relaxed);
        if (leases == 0 && record != atomic_load(&cell->active))
            continue;                    /* 正在被替换回收，重读 */
        if (!atomic_compare_exchange_weak(&record->leaseCount, &leases, leases + 1))
            continue;                    /* CAS 加 lease；record 被并发退休则重试 */
        /* double-check：加 lease 后 active 仍指向 record 才算成功；
           否则减回并重试。★ 确定性 race 测试（08.03）要求在这两步之间
           注入替换并断言不泄漏、不悬垂。 */
        if (record == atomic_load_explicit(&cell->active, memory_order_acquire))
            return record;
        atomic_fetch_sub(&record->leaseCount, 1);
    }
}
/* frame 退出时 release lease；退休 record 的回收条件 =
   leaseCount==0 且已从 active 卸下 —— epoch 回收器周期扫退休链。 */
```

发布事务（Apply）：验证（08.02 能力交集）→ 构造新 record → `atomic_store(&cell->active, newRecord)` 逐 cell 原子切换 → 旧 record 挂退休链。旧 frame 经 `local->activeGeneration`（guide C §C.1 reload 从 callInfo 所持 record 读）继续用旧版本；PIC/binding 缓存的 generation guard 在下次检查时 STALE → 结构化 link error（guide C §C.2）。回滚 = 用已验证的旧内容**构造新 generation 的 record** 再走同一发布事务——代码路径唯一，幂等性由 patch registry 的 content hash 去重保证。

## D.6 能力交集校验（08.02）

```c
TZrBool ZrCore_HotPatch_Validate(const SZrHotPatchInput *input,
        SZrValidatedHotPatch *validated, SZrHotPatchDiagnostic *diagnostic) {
    /* 顺序固定，任一步失败即拒绝且后续不执行：
       1. 签名/信任根验证（宿主注册的验证回调）
       2. base identity：patch 声明的 base artifact hash == 当前加载的
       3. schema/ABI/module/signature/layout hash 全匹配
       4. 能力检查——集合语义必须是：
              allowed  = hostAllowlist ∩ baseAotManifest
              required = 对 patch 的 ExecIR/ExecBC 全量闭包分析结果
              if (required ⊆ allowed) 通过 else 拒绝(CAPABILITY_ESCALATION,
                  diagnostic 列出 required \ allowed 的每一项)
          ★ 绝不允许 "effective = required ∩ allowed 然后带残缺能力运行"
       5. TOCTOU 防护：步骤 4 分析的字节与最终注册执行的字节是同一份
          内存（validated 持有拷贝的所有权，Apply 只接受 validated）。 */
}
```
