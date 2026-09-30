---
related_code:
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/ownership_transfer.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_internal.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_core/src/zr_vm_core/gc/gc_concurrent_major.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_telemetry.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_cross_domain.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_lifecycle.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_value_copy.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_cached.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_internal.h
  - zr_vm_core/src/zr_vm_core/ownership_transfer.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
plan_sources:
  - docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md
tests:
  - tests/core/test_gc_domain_multimutator.c
  - tests/core/test_gc_nested_mutation.c
  - tests/core/test_gc_concurrent_major.c
  - tests/acceptance/2026-09-30-gc-nested-mutation-stw.md
  - tests/core/test_resource_same_domain_handoff.c
  - tests/core/test_resource_cross_domain_transfer.c
  - tests/core/test_resource_cross_domain_transfer_races.c
  - tests/core/test_object_call_known_native_fast_path.c
  - tests/library/test_native_binding_direct_call.c
doc_type: module
---

# GcDomain 多 mutator 与同域 owner handoff

本模块把 Syntax 04 的单 mutator `GcDomain` 扩展为 domain-local STW 基线，并提供
runtime-internal 的同域 `Unique<Resource>` handoff。它只定义 GC/owner 底层合同；不会提前
接入 `TaskScheduler`，也不提供跨 domain transport。

## Mutator registry 与状态机

每个 attached `SZrState` 在所属 domain 中对应一个稳定 `mutatorId`。registry 记录：

- 当前 safepoint epoch；
- VM execution depth 与 native depth；
- `AttachedInactive`、`Running`、`Parked`、`BlockingDetached`、
  `NoSafepointCritical` 状态；
- 当前 native safepoint mode。

`ZrCore_GcDomain_MutatorEnter/Leave` 是可嵌套的。只有 outer execution leave 才把 state
恢复为 inactive；native scope 有独立 depth，不能被 VM inner/outer scope 提前清零。若 nested
VM/native entry 正好遇到 active pause，且没有仍打开的 mutation scope，运行中的 `GcAware`
mutator 会先发布当前 epoch 并进入 `Parked`，再等待 collector 结束 pause，避免“保持 Running
等待 pause”形成自锁。并发标记中的 mutation scope 遵守单独的延迟停靠规则，见下节。

新 mutator 可以在 pause 期间注册，但只能保持 inactive；`MutatorEnter` 会等本 domain pause
结束后再进入运行态。detach 会从 registry 移除 exact state identity 并唤醒 collector。

### 并发标记期间的嵌套 mutation scope

每条 mutator record 在 `coordinationLock` 下维护 `mutationDepth`。只有成功取得
recursive `mutationLock` 的 `MutationBegin` 才增加深度；匹配的 `MutationEnd` 重新查找
record 后递减并释放一层锁。mutation table 扩容和删除会整体复制 record，因此深度随记录
一起搬移；Begin/End 不会跨协调锁释放保留旧 record 指针。

当深度大于零时，nested `MutationBegin`、执行入口和原生入口不能把当前线程停在 pause
等待上，因为它仍持有 mutation lock。`MutatorPoll` 也会在这个深度下延后停靠。最外层
`MutationEnd` 先释放 mutation lock；若 domain pause 仍在请求，再通过正常 poll 发布当前
epoch。这样 collector 不会把仍持锁的 mutator 当作已 parked。

### 嵌套 mutation 回归验证

当前 native static-core 与 Clang 构建的 direct fixture 和注册 CTest 均通过；fixture 日志确认
真实 STW pause、mutator parked、existing-key 写入完成、线程 join、`GetValue` 校验和 major
drain 完成。Clang 的 ten-target build 为 85/85，GC CTest group 为 8/8，major-depth suite
为 14/14。首次诊断 CTest 曾有一次 41.91 秒 timeout，但未复现，原因尚未确定。可选的
old-source D-only probe 因基础设施、parser、bootstrap 和 link 失败而放弃；只编译了一个旧源
object，没有运行旧源码测试，不构成额外 RED。原始 GDB RED 仍有效；其他 SSA 06.02 验收
门槛仍开放。GCC 的 107/107 构建和 13/13 定向 CTest 已通过；GCC 14 direct run 尚未验证，
不作通过声明。精确日志记录在
`tests/acceptance/2026-09-30-gc-nested-mutation-stw.md`。

## Domain-local STW handshake

`ZrCore_GcDomain_StopTheWorldBegin` 只增加当前 domain 的 safepoint epoch，并只等待该
domain 的 registered mutators：

1. `AttachedInactive` 和 `BlockingDetached` 不阻塞 pause；
2. `Running/GcAware` 在 poll 边界发布 epoch、转为 `Parked`；
3. `NoSafepointCritical` 保持 blocker，直到退出或 collector 超时；
4. nested collector scope 由 `pauseDepth` 配对；
5. end/timeout 清除 pause、广播并恢复 parked mutators。

超时结果通过 `SZrGcDomainPauseDiagnostic` 返回 epoch、blocking mutator id、native mode、
exact state 与当时 `callInfoList` frame identity。调用方不能按线程名、native function 名或诊断
文本重建 blocker。

minor、major initial snapshot/remark 和 compact/full 的 pause work 都位于这套 domain-local
handshake 内。M7 的 major mark slices 在两次短 pause 之间与 mutator 并发执行，具体阶段、barrier
与budget compact合同见`gc-domain-concurrent-major.md`。另一个
`SZrGlobalState/GcDomain` 的 mutator 不属于等待集合，因而可以继续推进。

## VM、AOT 与 relocation

解释器 `ZrCore_Execute` 用 nested execution depth 包围完整 dispatch 生命周期。fetch 边界使用
命名预算周期 poll；实际 park 前把下一条 instruction 保存到 call-info，并发布当前 frame top。
恢复后重新解析 function/callable/frame cache，不能继续使用 collection 前的 movable object
地址。

AOT 继续在 emitted control/backedge safepoint 调用 `ZrCore_Gc_SafePoint`。collector 扫描每个
registered mutator 的 VM state、AOT root frames、call-info function、current exception 与 pending
control；minor forwarding 与 major relocation会重写同一组 roots。domain root slots 和 mutator
registry 在扫描/重写期间使用同一 coordination lock。

同域 write barrier仍先验证 owner/target 的 domain identity，再执行现有 generational barrier。
另一个 mutator不会获得绕过 domain validation 的写入口。

## Native safepoint mode

native descriptor 的既有 `dispatchFlags`（仍为 `TZrUInt32`，descriptor layout不变）新增：

- `GC_AWARE`：值为 0，是兼容默认；
- `BLOCKING_DETACHED`：callback期间不计入 pause blocker；
- `NO_SAFEPOINT_CRITICAL`：callback期间禁止当前 state启动 collection，并可被超时精确定位。

generic dispatch、cached dispatch、inline/pinned lanes、known-native direct callback 与 readonly
index-contract fast callback全部在实际 callback 前后走统一 `NativeEnter/NativeLeave`。cached
路径保留完整 resolved entry/descriptor identity，不能把 mode降级成裸 callback pointer。
同时设置两个非默认 mode bit 属于无效 descriptor，调用不会猜测优先级。

native recover-point 使用 `longjmp`/native throw 时，正常的 VM execution leave 与 native leave
不会执行。`ZrCore_Exception_Throw` 只在该 exact recover-point unwind 边界调用
`ZrCore_GcDomain_MutatorUnwindScopes`，把当前 state 的 execution/native depth、native mode 与
mutator status恢复为同一epoch的 inactive状态，再执行native unwind。普通脚本异常控制流不走
该reset；runtime也不按异常code、message或native function name判断是否清理scope。

## Same-domain TransferEnvelope

`SZrOwnershipTransferEnvelope` 是 opaque runtime carrier，状态为：

```text
Prepared -> Queued -> Claimed -> Committed
                         \\-> Aborted
Prepared/Queued ----------------> Aborted
```

当前 `PrepareSameDomain` 只接受当前 domain 的 direct resource `Unique`。prepare 用既有
`UniqueValue` move清空 source Place；payload对象/owner identity不复制、不clone、不创建Shared
control或refcount。publish/claim使用release/acquire state transition，claim记录 exact worker id
与 claim epoch；stale worker/epoch、重复commit/abort和cross-domain claim都失败。

queued/claimed envelope中的 direct Unique仍是 structured ownership root。commit把唯一 payload
move到target并清除envelope payload；之后由target Place cleanup Drop。prepared/queued取消或匹配
claimant的worker-exit/throw abort会在锁外release payload一次。terminal envelope free不会再次
Drop。因此queue close、claim race和commit/abort race中，source保持Moved且Drop count恰好为1。

## 当前边界

- 本阶段没有接入 `zr.thread`/Task scheduler，也不声明 `Send/Sync` public projection。
- same-domain direct resource Unique继续走O(1) owner handoff；M6的跨域`ValueCopy`、
  `StructuredClone`、`ImmutableHandle`、`ResourceMove`、quota与stale generation合同见
  `cross-domain-transfer-contracts.md`。两条路径共享envelope状态机但不会互相降级。
- envelope当前从runtime manager分配；未来scheduler可共分配或池化，但不得改变transfer identity
  或复制payload。
- `BlockingDetached`要求native在进入前已发布/固定其roots；`NoSafepointCritical`要求短时且禁止
  未pin interior pointer逃逸。本模块只执行descriptor mode与pause合同，不推断native实现行为。
- major pause、barrier、safepoint和transfer计数按domain identity/generation归因；宿主消费结构化
  `ZrCore_Gc_GetStats`字段，不按线程名或日志文本统计。
