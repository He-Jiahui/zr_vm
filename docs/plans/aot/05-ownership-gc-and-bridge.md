# AOT 05：Ownership、Drop 与 GC Bridge

## 世界边界

| 类别 | AOT责任 |
|---|---|
| GC class/ref | root/stack map、safepoint、write barrier、pin contract |
| Unique resource | move-only availability、exactly-once Drop glue |
| Shared/Weak | retain/release/upgrade runtime ABI；线程能力按类型区分 |
| GcBox/Gc handle | 显式ownership-to-GC bridge，不隐式改变生命周期 |
| ref/ref struct | 已验证region内地址访问，不跨heap/suspend/escape |

## Lowering规则

- frontend facts决定copy/move/drop合法性；AOT按TypeLayout的copyKind/dropKind发射操作。
- cleanup CFG保证正常退出与异常退出都执行exactly-once Drop/Close。
- `Unique<T>.intoGc()`是显式bridge op，生成GcBox语义；不能优化成改变ownership tag。
- resource持有GC对象只经`Gc<T>` root handle；AOT必须让GC看到该root。
- Shared第一版非原子；跨线程只接受AtomicShared/明确Send-Sync contract。
- PoolHandle是ordinary weak identity；PoolRef是scoped direct ref + guard。retire/reuse逻辑在`zr.pooling` runtime，backend只执行通用Drop和ref-like规则。
- 只有GcFree slab可NoScan；GcMapped/GcBarriered保留pointer map/barrier/card。

## 优化边界

允许消除配对retain/release、证明无逃逸的短生命周期分配、冗余barrier和bounds check，但证明必须来自共享facts并在debug verifier中可关闭。Weak upgrade、external native pointer和动态cast不能凭惯例删除检查。

## Syntax 上游追踪

| Syntax 节点 | 本计划消费的稳定输入 | 本计划退出责任 |
|---|---|---|
| [02/M3-M5](../syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md) | loan/NLL、escape、closure/suspension facts | ref provenance 与 cleanup 保真，不在 runtime 重做 borrow checker |
| [03/M3-M5](../syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md) | ref-like restriction、Span/pool/FFI lifetime | stack/root/pin/guard lowering 与非法跨界拒绝 |
| [04/M1-M7](../syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md) | Unique/Shared/Weak、Drop、GcDomain、handoff/transport contracts | exactly-once Drop、root/barrier/safepoint/transport runtime ABI |
| [09/M1-M5](../syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md) | StableSlot、PoolRef guard、deferred reuse、GcScanKind | handle validation、guard Drop、reuse/scan/barrier behavior |
| [10F/M3](../syntax/2026-07-19-10-native-ffi-module-package-design.md) | owner/ref-like pin/marshaller rules | native success/failure/callback cleanup 与 unsupported ABI 拒绝 |
| [12/M2-M5](../syntax/2026-07-20-12-async-task-job-scheduler-design.md) | Task frame roots、domain policy、Job handoff 与 transport | suspension roots、same/isolated-domain cleanup、failure Drop=1 |
| [13/M2-M4](../syntax/2026-07-20-13-iterator-enumerator-yield-design.md) | iterator frame ownership、yield suspension 与 dispose | frame roots、early dispose/throw cleanup、artifact projection |

本计划只处理已由 Syntax 验证的 ownership operation；逐节点状态见[完整追踪矩阵](./syntax-contract-traceability.md)。

## Syntax 04 M4 已落地基线

- `Gc<T>` / `GcBox<T>` 使用独立 canonical bridge kind，不复用 ownership qualifier。
- `Unique<Resource>.intoGc()` 在 SemIR 中保留 source Place，并以
  `ZR_SEMANTIC_OWNERSHIP_INTO_GC_BOX` 表示消费；active loan 和 Shared 输入在 frontend
  被拒绝。
- ExecBC 继续使用稳定 `OWN_DETACH` 槽，但 VM helper 优先执行
  `ZrCore_Ownership_IntoGcBoxValue`。C/LLVM AOT 共用的 OwnDetach runtime helper按同一顺序先
  尝试 IntoGcBox，再进入 legacy Detach；合同测试精确验证 helper 定义、调用与顺序。
- 单 mutator runtime 通过 domain root slot 追踪 host `SZrGcRootHandle` 与 ownership roots，
  minor/major/compact rewrite 更新 slot target。AOT 不得把它降级为 hidden ignore set。
- 当前不宣称完整 source `Gc<T>` constructor、AOT stack-map schema、多 mutator safepoint
  handshake 或跨 domain transport；这些仍由后续里程碑提供。

## 验收

压力测试覆盖branch/loop/throw中的owner、partial construction、shared cycle policy、GC compaction、pin/unpin、pool deferred reuse和callback异常。profile分别报告allocation、scan bytes、barrier、retain/release与Drop，不混成单一“内存性能”。

## 实施与验收矩阵

交付物包括TypeLayout ownership/GC maps、Drop/retain/release/upgrade/bridge thunks、cleanup plan、stack/root map与profile事件schema。

| 场景 | 静态输入 | AOT动作 | 必须保持的runtime检查 |
|---|---|---|---|
| Unique move/drop | Place availability + DropKind | move清源、cleanup exact-once | external/native Drop contract |
| Shared/Weak | owner TypeRef + thread capability | retain/release/upgrade thunk | weak liveness、atomic/thread policy |
| GC field/write | GC map + field Place | root/stack map、write barrier | dynamic heap relation未证明时barrier |
| owner-to-GC | explicit bridge op | GcBox/root registration | allocation/finalization failure |
| ref/ref struct | region/provenance facts | direct address/fat-ref access | native lifetime和未证明bounds |
| pooling | StableSlot + GcScanKind + guard | guard Drop、barrier/no-scan投影 | handle validation只在borrow入口 |

leaf证据从`tests/gc/gc_tests.c`、`tests/core/test_aot_gc_root_frame.c`、`tests/acceptance/2026-06-06-aot-ownership-direct-core.md`、`tests/acceptance/2026-06-25-aot-09-s5b-ffi-native-call-pin.md`开始。目标测试必须覆盖return/throw/finally中的Drop顺序、Shared环策略、Weak失败、compacting GC、callback reentry和PoolRef active guard。

退出条件：所有正常/异常edge拥有可审计cleanup plan；debug verifier可检测重复/遗漏Drop与root map；profile能分别证明GC scan、barrier、retain/release和pool hot access成本，而不是只给总时间。
