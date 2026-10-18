---
related_code:
  - lua/jdk/src/hotspot/share/classfile/classLoaderData.hpp
  - lua/mono/mono/metadata/image-internals.h
  - lua/mono/mono/metadata/image.h
  - lua/rust/compiler/rustc_data_structures/src/stable_hasher.rs
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/gc_domain_clone.h
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/src/zr_vm_core/artifact/artifact_call_binding.c
  - zr_vm_core/src/zr_vm_core/call/call_binding.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_contract_internal.h
  - zr_vm_core/src/zr_vm_core/call/call_binding_encoding.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_inline_frame.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_layout.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_link.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_member.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_signature.c
  - zr_vm_core/src/zr_vm_core/call/typed_call_binding.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_share.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/io/io_call_binding.c
  - zr_vm_core/src/zr_vm_core/module/module.c
  - zr_vm_core/src/zr_vm_core/module/module_call_binding.c
  - zr_vm_core/src/zr_vm_core/runtime/execution_contract.c
  - zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c
  - zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_call_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_module_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/gc_domain_clone.h
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/src/zr_vm_core/artifact/artifact_call_binding.c
  - zr_vm_core/src/zr_vm_core/call/call_binding.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_contract_internal.h
  - zr_vm_core/src/zr_vm_core/call/call_binding_encoding.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_inline_frame.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_layout.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_link.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_member.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_signature.c
  - zr_vm_core/src/zr_vm_core/call/typed_call_binding.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_share.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/io/io_call_binding.c
  - zr_vm_core/src/zr_vm_core/module/module.c
  - zr_vm_core/src/zr_vm_core/module/module_call_binding.c
  - zr_vm_core/src/zr_vm_core/runtime/execution_contract.c
  - zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c
  - zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_call_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_module_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c
plan_sources:
  - docs/plans/ssa/00-measurement-contracts/02-contract-freeze.md
  - .codex/plans/20261018-ssa-contract-freeze.md
  - user: 2026-10-18 Source42 contract dictionary text freeze
tests:
  - tests/core/test_gc_domain_bridge.c
  - tests/core/test_gc_domain_multimutator.c
  - tests/core/test_resource_cross_domain_transfer.c
  - tests/core/test_resource_cross_domain_transfer_races.c
  - tests/core/test_resource_cross_domain_transfer_review_cases.h
  - tests/core/test_resource_cross_domain_transfer_value_copy_cases.h
  - tests/core/test_resource_same_domain_handoff.c
  - tests/core/test_ssa_backend_admission.c
  - tests/core/test_ssa_backend_service.c
  - tests/core/test_ssa_cross_domain_clone.c
  - tests/core/test_ssa_cross_domain_clone_oom.c
  - tests/core/test_ssa_major_budget_runtime.c
  - tests/parser/test_ssa_compare_predicate_domain.c
  - tests/parser/test_ssa_contract_freeze.c
doc_type: module-detail
---

# Execution contract：唯一 owner 与运行期边界

## 导航与冻结范围

[运行期 owner](../core-runtime/execution-contract-ownership.md) · [持久字段](../module-system/persistent-contract-fields.md) · [接口矩阵](../testing-and-validation/ssa-interface-contract-matrix.md) · [兼容政策](../testing-and-validation/ssa-contract-compatibility.md)。四页共同构成 00.02 的文字字典；[47 叶依赖图](../plans/ssa/index.md)与各叶运行验收保持独立。文档冻结不授予实现、加载、安全测试或运行权限；planned API 不据此成为已实现。


## 权威与边界

Core 拥有低层执行契约、验证、解析与运行期生命周期；Parser 拥有 AST/语义事实到 Core IR 的生产，以及优化、投影、写出驱动。新增消费者复用同一声明，禁止复制平行 public struct 或向 Core 引入 Parser AST。下述 D1–D6 是本次明确的设计选择；它们冻结职责和落地合同，未改源码、语言语法、public ABI 或 generic checker。

| 对象 | 唯一 owner | 实际 producer / consumer | 生存期 |
| --- | --- | --- | --- |
| SemanticIR/canonical callable | Parser semantic context | inference/compiler → BuildModule、retained-source lowering | context 持有节点/参数数组，TypeId 仅 context-local |
| ExecIR function/module | Core ExecIR storage | Parser builder/codec → Core verifier、Parser passes | owning pools/maps 由 Free 释放；reserve 后重新取得借用 |
| Execution contract | Core runtime contract | builder/codec/host → checker、后端、AOT | 标量复制不延长目标生存期 |
| CallBinding static contract | Core call binding | compiler/module → Check/Compare/codec | token/hash 可编码，无 entry/context 地址 |
| CallBinding target/cache | Core call binding runtime | link/typed/member helpers → tag 指定的 VM/native/AOT target | 浅复制不创建 root、不取得 code lease |
| backend service/job/code | Core backend service | host descriptor/request → provider callbacks | host arrays/userData、immutable input payload 借用 |
| code lease | Core execution code handle | provider/service publish → acquire/release | 延迟回收；不得落盘 |
| persistent encoding | Core artifact / legacy IO | Parser writer → Core reader | view 借用 bytes，decoded owning tree 另行释放 |
| load epoch/version entry | Core module loader/version manager | planned loader publication → runtime resolver/backend invalidation | D4 定义进程命名空间 |
| capability authority | host loader policy | signature/allowlist → Core admission | effects/operation mask 不是授权 |
| domain roots/clone/transfer | Core GC-domain/ownership | trusted proof、graph policy → share/clone | 同域 root 与跨域独立图分开 |
| budget/telemetry | Core GC budget | host config → collector → stats copy | 外部串行化；非硬实时 |

## D1：Domain_GetContract = 只读域策略快照

唯一 owner 为 Core GC-domain，落地叶 06.03，状态 **planned 新接口**。现有 GetIdentity 已实现，返回 id:u64/generation:u32；缺失或非 active 时零对，不改名冒充完整合同。

选择的逻辑快照包括 domain identity、active/lifecycle 状态、同域共享所需 Send/Sync 证明类别、跨域复制所需 schema/policy 身份和 budget 配置 presence。不返回长期 allocator/object/collector 指针，不生成证明或 capability，不创建 root。06.03 单点新增 C 声明；本文没有虚构现有函数原型或数字 ABI。

查询同步、锁内复制一致标量；参数或 inactive 域失败时输出无效快照。解锁后结果只是观察值，使用时重新验证 identity；调用方保证 state/domain 存活。字段不适用使用显式 presence，零 hash 不表示已验证。planned consumers 为 share/clone/transfer admission 和 host worker，现有 GetIdentity 调用不算新接口 consumer。

验收：空/注销中/失效域、同域与异域、identity 再用、并发配置快照、证明缺失和输出清空。该 planned 接口落地前，现有 share/clone 参数及诊断保持原样。

## D2：Domain_TransferValue = 跨域结构化复制事务

唯一 owner 为 Core cross-domain ownership，落地叶 06.04，状态 **planned 通用 facade**。Transfer 统一表示复制图到独立目标域；禁止共享源 GC 指针、ownership control、弱引用控制块或 runtime lease。unique/resource 的源侧提交/失效是显式 ownership 协议，不由复制成功自动发生。materialized resource 只由宿主许可创建独立目标资源。

已有 Clone_Prepare/Publish/Claim/Commit/Abort/Free 与 DomainTransferGraph_Prepare 都是真实实现，层次不同；未来 facade 不是二者的宏别名。输入合同必须含源/目标 identity、bytes/objects/depth quota、schema/policy、worker identity、非零 claim epoch。目标图归目标 allocator，源 state 至 envelope 释放前保持有效；prepare/publish/claim/commit/abort 分别可观察。

OOM、target Throw、竞争 abort/commit 按真实协议清理或保留可重试 owner，不能承诺任意 FALSE 无可见变化。现有 raw-array materialization 改源存储、Throw 清理、schema 指纹只验非零、冲突 Free 保留 wrapper 的 TODO 继续公开。06.04 验收覆盖 quota、循环/共享子图、两域关闭、schema mismatch、分配失败、Throw、重复/竞争 claim、释放重试与 ledger。

## D3：Gc_GetStats = 现有预算累计统计

唯一 owner 为 Core GC budget，所属 06.02，状态 **已实现**。保持 SZrGcBudgetStats = SZrGcBudgetStepResult typedef 及 GetBudgetStats 委托，不能静默改成通用堆统计。通用 GetStatsSnapshot 是另一真实入口。

maxAtomicPauseUs 当前是配置阈值，非实测最大暂停；bytesDone/objectsDone 为零，nextCursor 是 accepted cursor，debt/pressure 查询时采样。OVER_BUDGET 累计工作但当前 wrapper 保留旧 cursor 的兼容 TODO 必须可见。SetBudget 在 mutation lock 下整体替换并重置遥测；其他读取/评估仍由宿主串行化，不授予通用线程安全或硬实时保证。新增通用统计必须另定 shape/API，不能扩大此别名含义。

## D4：disk generation 与 runtime load epoch 分离

唯一签发 owner 为 Core module loader/version-entry registry，落地 08.03；10.01 backend 只消费命名空间。runtime epoch 不由磁盘 writer 签发，也不从磁盘 generation、metadataGeneration 或 backend registration 直接推导。

| 域 | 当前 producer | 冻结含义 |
| --- | --- | --- |
| serialized execution generation | ModuleAddFunction 初始化 1；scalar codec 编解码，有限 EIS profile 要求 1 | 持久语义值，不是进程寿命见证 |
| module.metadataGeneration | module.c 原子/局部 u32 递增、跳零/有 wrap | 元数据 cache invalidation，不充当不可复用 epoch |
| callBindingGeneration | CallBinding runtime invalidation | 目标缓存寿命，不能跨重载沿用 |
| HotPatch generation | manager-local u64 单调分配；rollback 新代 | manager 生存期内记录身份，Init 是新命名空间 |
| backend generationKey | host 输入前三部分、service 盖 selected registration | domain/module/generation/registration 共同匹配，独立于 contract.generation |
| loadEpoch | planned loader registry | 装载/重载新 runtime namespace，不序列化 |

选择进程内单调非零 u64 epoch，由一个 registry lock/atomic owner 串行签发；溢出拒绝 publication，绝不 wrap/复用。卸载、重载、manager 重建不得复用旧 epoch；进程重启旧 handle 全部无效。runtime version-entry 同时携带 epoch 与 disk semantic identity；同内容重新加载也产生新 epoch。新增 runtime witness/API 由 08.03 实现，本次不改 SZrExecutionGenerationKey public ABI 来假装已存在。

先 prepare entry，原子 publish，再禁止旧 namespace 新 Acquire；旧 frame/code/root leases 保持至最后 release。rollback 选旧语义内容但分配新 epoch/generation，不回拨签发器或迁移活 frame。验收依赖 08.03 leases、08.04 rollback、10.01 invalidation，覆盖重载、同内容、跨域、旧 ticket 晚完成、溢出、manager 重建与旧 frame 存活。

## 当前 backend 三 TU

main 保留 Init/Deinit/Register/Unregister、cancel/invalidate/shutdown/finalize/status/Resume；compile 持 request/candidate/queue/ProcessNext/Complete/Publish/QueryTicket；facade 只持默认 service 指针/锁及四 bool 门面；internal.h 共享 private helpers。execution_code_handle.c 保持独立 owner。

F3546F3D 静审 move-map 覆盖 40 个旧函数、36 个不变，四个 intentional gate 修改；随后 Complete 已修正独立 contract.generation mismatch 的 expected/actual 来源。当前源 pin 见本任务 implementation receipt，不能沿用修正前 compile pin。原 145 source pins 实际 143 当前匹配、main/internal 两项因重构变更；contract overlay 的 46 项为 44+同两变更，不是旧 pin 全部 current。

schema6/logical1、reserved0 由 owner 准入，ABI 匹配 selected registration，合法 ABI23 不应被全局17拒绝。保留 flags/immutable 及 Complete generation 优先级。ASYNC flag 不启动线程，host 驱动 ProcessNext，PENDING 不等于机器码完成。

query/compile/cancel/retire/destroy/resume 等回调锁外执行，in-flight 计数参与 shutdown；默认锁只保护指针，不 pin service 生存期。QUEUED 在 query 返回前可见、Complete snapshot→cleanup 注册 handoff、Resume shutdown 检查等既有 TODO 不被文字或串行 mock 消除。

## 两头文件补入的 15 runtime records

两头 named structs 为 23、原已列 8、补 15，仅是有限差集。字段全路径如下；匿名 union 以 tag 选择 active arm，不能把各 arm 当同时存活。嵌套 static contract 可编码不表示 live record 可落盘。

| record / owner | 字段 : C type | clone / free / wire 边界 |
| --- | --- | --- |
| `SZrCallBinding` / binding cache/caller owns record; function/closure/AOT metadata and code addresses are borrowed runtime targets | `contract : SZrCallBindingContract`; `generation : TZrUInt64`; `target : SZrCallBindingTarget` | shallow value copy only; copied object references require owner GC marking/barriers; no context or closure clone / Invalidate clears generation/target and preserves contract; does not free targets; cache array freed by function owner / This runtime record has no row encoding in the audited CBND/EIS codecs. Nested static contract fields can be separately encoded; excluding this record does not exclude those fields. |
| `SZrCallBindingCandidate` / binding cache/caller owns record; function/closure/AOT metadata and code addresses are borrowed runtime targets | `contract : SZrCallBindingContract`; `generation : TZrUInt64`; `target : SZrCallBindingTarget` | shallow value copy only; copied object references require owner GC marking/barriers; no context or closure clone / Invalidate clears generation/target and preserves contract; does not free targets; cache array freed by function owner / This runtime record has no row encoding in the audited CBND/EIS codecs. Nested static contract fields can be separately encoded; excluding this record does not exclude those fields. |
| `SZrCallBindingDiagnostic` / caller storage; scalar/value snapshots, no separately owned allocation | `status : EZrCallBindingStatus`; `targetMetadataToken : TZrMetadataToken`; `instructionIndex : TZrUInt32`; `candidateIndex : TZrUInt32`; `expected : TZrUInt64`; `actual : TZrUInt64` | ordinary value copy only; not a deep clone and not a lease/GC root acquisition / no per-record free; caller releases enclosing storage / This runtime record has no row encoding in the audited CBND/EIS codecs. Nested static contract fields can be separately encoded; excluding this record does not exclude those fields. |
| `SZrCallBindingTarget` / binding cache/caller owns record; function/closure/AOT metadata and code addresses are borrowed runtime targets | `targetKind : TZrUInt32`; `dispatchSlotCount : TZrUInt32`; `ownerLayoutGeneration : TZrUInt64`; `targetGeneration : TZrUInt64`; `vm.function : struct SZrFunction *`; `native.function : FZrNativeFunction`; `aot.thunk : FZrAotEntryThunk`; `aot.methodInfo : const struct SZrAotMethodInfo *`; `aot.invoker : FZrAotReflectionInvoker`; `callableObject : struct SZrRawObject *` | shallow value copy only; copied object references require owner GC marking/barriers; no context or closure clone / Invalidate clears generation/target and preserves contract; does not free targets; cache array freed by function owner / This runtime record has no row encoding in the audited CBND/EIS codecs. Nested static contract fields can be separately encoded; excluding this record does not exclude those fields. |
| `SZrExecutionBackendCompileInvocation` / caller storage; scalar/value snapshots, no separately owned allocation | `ticket : SZrExecutionCompileTicket`; `request : SZrExecutionCompileRequest` | ordinary value copy only; not a deep clone and not a lease/GC root acquisition / no per-record free; caller releases enclosing storage / This runtime record has no row encoding in the audited CBND/EIS codecs. Nested static contract fields can be separately encoded; excluding this record does not exclude those fields. |
| `SZrExecutionBackendDescriptor` / host-owned descriptor/registration slot; callback code and userData lifetime supplied by backend | `magic : TZrUInt32`; `schemaVersion : TZrUInt32`; `backendKind : EZrExecutionBackendTargetKind`; `flags : TZrUInt32`; `target : SZrExecutionBackendTarget`; `supportedOperations : TZrUInt32`; `reserved : TZrUInt32`; `vtable : SZrExecutionBackendVTable`; `userData : TZrPtr` | ordinary value copy only; not a deep clone and not a lease/GC root acquisition / service invokes descriptor.vtable.destroy(userData) on unregister/final shutdown when eligible; never raw-frees userData; callback absent leaves ownership with host / This runtime record has no row encoding in the audited CBND/EIS codecs. Nested static contract fields can be separately encoded; excluding this record does not exclude those fields. |
| `SZrExecutionBackendDiagnostic` / caller storage; scalar/value snapshots, no separately owned allocation | `status : EZrExecutionBackendStatus`; `sourceId : TZrUInt32`; `instructionId : TZrUInt32`; `expectedState : TZrUInt32`; `actualState : TZrUInt32`; `ticketId : TZrUInt64`; `codeIdentity : TZrUInt64`; `expected : TZrUInt64`; `actual : TZrUInt64`; `expectedKey : SZrExecutionGenerationKey`; `actualKey : SZrExecutionGenerationKey` | ordinary value copy only; not a deep clone and not a lease/GC root acquisition / no per-record free; caller releases enclosing storage / This runtime record has no row encoding in the audited CBND/EIS codecs. Nested static contract fields can be separately encoded; excluding this record does not exclude those fields. |
| `SZrExecutionBackendRegistration` / caller storage; scalar/value snapshots, no separately owned allocation | `serviceIdentity : TZrUInt64`; `registrationIdentity : TZrUInt64`; `backendKind : EZrExecutionBackendTargetKind` | ordinary value copy only; not a deep clone and not a lease/GC root acquisition / no per-record free; caller releases enclosing storage / This runtime record has no row encoding in the audited CBND/EIS codecs. Nested static contract fields can be separately encoded; excluding this record does not exclude those fields. |
| `SZrExecutionBackendRegistrationRecord` / host-owned descriptor/registration slot; callback code and userData lifetime supplied by backend | `registrationIdentity : TZrUInt64`; `active : TZrBool`; `reserved0 : TZrUInt8`; `reserved1 : TZrUInt16`; `descriptor : SZrExecutionBackendDescriptor` | ordinary value copy only; not a deep clone and not a lease/GC root acquisition / service invokes descriptor.vtable.destroy(userData) on unregister/final shutdown when eligible; never raw-frees userData; callback absent leaves ownership with host / This runtime record has no row encoding in the audited CBND/EIS codecs. Nested static contract fields can be separately encoded; excluding this record does not exclude those fields. |
| `SZrExecutionBackendService` / host owns service plus registrations/jobs/codes arrays; Init borrows arrays, zeroes them; capacities/counts delimit access | `magic : TZrUInt32`; `schemaVersion : TZrUInt32`; `lock : volatile TZrUInt32`; `serviceIdentity : TZrUInt64`; `nextRegistrationIdentity : TZrUInt64`; `nextTicketId : TZrUInt64`; `registrations : SZrExecutionBackendRegistrationRecord *`; `registrationCapacity : TZrUInt32`; `registrationCount : TZrUInt32`; `jobs : SZrExecutionCompileJob *`; `jobCapacity : TZrUInt32`; `jobCount : TZrUInt32`; `codes : SZrExecutionCodeRecord *`; `codeCapacity : TZrUInt32`; `codeCount : TZrUInt32`; `resumeInterpreter : FZrExecutionBackendResumeInterpreter`; `resumeUserData : TZrPtr`; `shuttingDown : TZrBool`; `destroyed : TZrBool`; `shutdownDestroyCalled : TZrBool`; `reserved : TZrUInt8`; `queryTargetInFlight : volatile TZrUInt32`; `backendCallbackInFlight : volatile TZrUInt32`; `invalidatedKeys : SZrExecutionGenerationKey[ZR_EXECUTION_BACKEND_INVALIDATION_CAPACITY]`; `invalidatedKeyCount : TZrUInt32`; `reservedInvalidation : TZrUInt32` | not cloneable by struct assignment: lock, service identity, slots, callbacks, leases and in-flight counts represent a single live service / FinalizeShutdown retires resources/destroys eligible owners; Deinit only clears a destroyed service and detaches arrays; host releases arrays/service / This runtime record has no row encoding in the audited CBND/EIS codecs. Nested static contract fields can be separately encoded; excluding this record does not exclude those fields. |
| `SZrExecutionBackendVTable` / host-owned descriptor/registration slot; callback code and userData lifetime supplied by backend | `queryTarget : FZrExecutionBackendQueryTarget`; `compileAsync : FZrExecutionBackendCompileAsync`; `cancelCompile : FZrExecutionBackendCancelCompile`; `lookupEntry : FZrExecutionBackendLookupEntry`; `queryMap : FZrExecutionBackendQueryMap`; `unregisterMaps : FZrExecutionBackendUnregisterMaps`; `retire : FZrExecutionBackendRetire`; `destroy : FZrExecutionBackendDestroy` | ordinary value copy only; not a deep clone and not a lease/GC root acquisition / service invokes descriptor.vtable.destroy(userData) on unregister/final shutdown when eligible; never raw-frees userData; callback absent leaves ownership with host / This runtime record has no row encoding in the audited CBND/EIS codecs. Nested static contract fields can be separately encoded; excluding this record does not exclude those fields. |
| `SZrExecutionCodeRecord` / host service array slot; slot identity/state and registrationSlot associate backend owner | `info : SZrExecutionBackendCodeInfo`; `state : EZrExecutionBackendCodeState`; `registrationSlot : TZrUInt32`; `jobSlot : TZrUInt32`; `leaseCount : TZrUInt32`; `dependencyLeaseCount : TZrUInt32`; `mapsRegistered : TZrBool`; `reserved0 : TZrBool`; `reserved1 : TZrUInt16` | query snapshots/value metadata only; copying active slot does not duplicate callback ownership, lease counters, maps, or code resources / state-machine recycle; CodeRecord maps are unregistered before retire; backend callback owns actual map/code resources / This runtime record has no row encoding in the audited CBND/EIS codecs. Nested static contract fields can be separately encoded; excluding this record does not exclude those fields. |
| `SZrExecutionCodeView` / caller storage; scalar/value snapshots, no separately owned allocation | `info : SZrExecutionBackendCodeInfo`; `state : EZrExecutionBackendCodeState`; `leaseCount : TZrUInt32`; `dependencyLeaseCount : TZrUInt32` | ordinary value copy only; not a deep clone and not a lease/GC root acquisition / no per-record free; caller releases enclosing storage / This runtime record has no row encoding in the audited CBND/EIS codecs. Nested static contract fields can be separately encoded; excluding this record does not exclude those fields. |
| `SZrExecutionCompileJob` / host service array slot; slot identity/state and registrationSlot associate backend owner | `ticket : SZrExecutionCompileTicket`; `request : SZrExecutionCompileRequest`; `result : SZrExecutionBackendCompiledCode`; `lastStatus : EZrExecutionBackendStatus`; `registrationSlot : TZrUInt32`; `codeSlot : TZrUInt32`; `cancelRequested : TZrBool`; `resultValid : TZrBool`; `reserved : TZrUInt16` | query snapshots/value metadata only; copying active slot does not duplicate callback ownership, lease counters, maps, or code resources / state-machine recycle; CodeRecord maps are unregistered before retire; backend callback owns actual map/code resources / This runtime record has no row encoding in the audited CBND/EIS codecs. Nested static contract fields can be separately encoded; excluding this record does not exclude those fields. |
| `SZrExecutionCompileTicketView` / caller storage; scalar/value snapshots, no separately owned allocation | `ticketId : TZrUInt64`; `state : EZrExecutionBackendJobState`; `target : EZrExecutionBackendTargetKind`; `fallback : EZrExecutionBackendFallback`; `lastStatus : EZrExecutionBackendStatus`; `codeIdentity : TZrUInt64`; `generationKey : SZrExecutionGenerationKey` | ordinary value copy only; not a deep clone and not a lease/GC root acquisition / no per-record free; caller releases enclosing storage / This runtime record has no row encoding in the audited CBND/EIS codecs. Nested static contract fields can be separately encoded; excluding this record does not exclude those fields. |

## 参考树与验证限制

JDK ClassLoaderData 的 runtime root tracing、_holder 和 is_alive 把运行期存活绑定 loader data；Mono image API 区分 data copy、open status 与 image lifetime；Rust HashStable 要求不依赖 session 分配身份/地址及 host 布局。这些先例支撑持久身份与运行期寿命分离，不证明本项目已实现对应能力，也不把 context-local TypeId hash 升格 stable hash。

只读依据：lua/jdk/src/hotspot/share/classfile/classLoaderData.hpp:50–107,249；lua/mono/mono/metadata/image.h:29–45；image-internals.h:16–25；lua/rust/compiler/rustc_data_structures/src/stable_hasher.rs:17–43。参考树未执行。

当前后端修正 admission33/33、service7 scenarios、CTest两项成功已由独审221A4D88接受；MSVC6个C的syntax阶段由394EB90F接受。GREEN范围仅clang-cl Debug UBSan metadata mock，MSVC仅syntax、无link/runtime信用。本次没有重复运行；全47叶与本叶完整退出仍需Root对全部证据/依赖收口。

## Sealed 证据坐标

报告根为 `E:\cargo-targets\zr_vm\reports\ssa-contract-freeze-01a0fe2b`。Git 内保留规则和完整族/record 索引；精确分支、调用参数与源 pin 由下列不可变 JSON 复核。

| 文件 | 字节 | SHA256 |
| --- | ---: | --- |
| `persistent-field-audit.json` | 720108 | `FCD3A6A7A0AC2F7A57D34108AEC4A57E2BC50E68D5C32BCE3873557C98ED0784` |
| `audit-coordinate-corrections.json` | 41112 | `1EE1DF88A21024DE2098C3858BA25F814D1BEAF7FA8C12724420B43BBFD7D1D2` |
| `legacy-io-field-audit.json` | 1115640 | `D50CDBB8AC5E02EA426EBC090B1A8A72D379898A0BB7BE44BB829F293CEFCA8E` |
| `dictionary-delta-review.json` | 15688 | `5B6BDCF47B9805623AE459538F092E66441DA343C072530E031B1FC53EE6DD3D` |
| `contract-inventory-delta.json` | 119852 | `FC771882967BABE650C97412E24691A2AF13EF1536AD0ABAAA048CA51A61FA61` |
| `contract-inventory-delta-review.json` | 18076 | `BD7404366C0A8A4ADF6D67C4609ACD26112C733C59905CD58D2128A0453FA16E` |
| `interface-family-audit.json` | 104659 | `1F7F35D4C229F4F41BD7068B5441689FB535136ED3E0E55C3B37478B240F26E6` |
| `backend-production-review.json` | 24837 | `F3546F3D070B8FC17B4C11E56E10D1F42B827F0960A454A06F55C4E76CCA1679` |
| `backend-green-review.json` | 22707 | `221A4D88BF309C2F11566A992B054E98B849D5F7229D651AC5E6ABDBE93E483D` |
| `msvc-syntax-review.json` | 15138 | `394EB90F40205BDE63D56C9AC1061B71317B7E6D36D12B65A5EEE7D2E8D0C00D` |
