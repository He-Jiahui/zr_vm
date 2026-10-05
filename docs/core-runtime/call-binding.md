---
related_code:
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/function_identity.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/call_binding.c
  - zr_vm_core/src/zr_vm_core/call_binding_encoding.c
  - zr_vm_core/src/zr_vm_core/call_binding_contract_internal.h
  - zr_vm_core/src/zr_vm_core/call_binding_link.c
  - zr_vm_core/src/zr_vm_core/call_binding_graph.c
  - zr_vm_core/src/zr_vm_core/call_binding_member.c
  - zr_vm_core/src/zr_vm_core/call_binding_signature.c
  - zr_vm_core/src/zr_vm_core/function_identity.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_graph.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
  - zr_vm_core/src/zr_vm_core/execution/execution_meta_access.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/src/zr_vm_core/call_binding.c
  - zr_vm_core/src/zr_vm_core/call_binding_encoding.c
  - zr_vm_core/src/zr_vm_core/call_binding_contract_internal.h
  - zr_vm_core/src/zr_vm_core/call_binding_link.c
  - zr_vm_core/src/zr_vm_core/call_binding_member.c
  - zr_vm_core/src/zr_vm_core/function_identity.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
plan_sources:
  - user: 2026-09-06 W2 / Call Binding M1 静态调用绑定与可重定位目标表
tests:
  - tests/core/test_call_binding_runtime.c
  - tests/library/test_call_binding_module.c
  - tests/library/test_call_binding_native_registry.c
  - tests/library/test_call_binding_relocation.c
  - tests/parser/test_call_binding_pipeline.c
  - tests/parser/test_typed_call_binding.c
  - tests/acceptance/2026-09-06-call-binding-m1.md
doc_type: module-detail
---

# Call Binding Runtime

Call Binding is the runtime contract shared by statically resolved VM calls,
native calls, AOT calls, accessors, meta-functions, virtual methods, interface
slots, and typed function values. The contract carries metadata tokens,
structural signature hashes, module identity, layout version/hash, operation,
and a dispatch slot. The resolved target is a separate tagged runtime witness;
it can contain a VM function, native callback, AOT entry, or a GC-traced
callable object.

## Binding and validation

Compiler facts initialize the cache entry before linking. The linker validates
the token tables, signature row, module hash, owner layout, relocation kind,
instruction coordinate, and target index. It builds a per-function instruction
map so a callsite cache is selected directly by instruction index. A failed
check invalidates the witness and reports a structured status such as
`signature-mismatch`, `layout-mismatch`, `target-not-found`, or
`stale-generation`.

Direct VM and native targets are resolved once and then checked by generation.
Virtual and interface entries keep their slot contract and select a concrete
descriptor after receiver shape/type validation. Getter, setter, and meta
operations use the same contract while preserving receiver provenance,
ownership cleanup, exceptions, and inline-struct writeback. Typed function
values retain a zero-target signature contract because the live value can be a
VM closure, native callback, or AOT callable.

## Function graph identity

Function constants that are also inline children are rebound using
`ZrCore_Function_HasSameDefinition` or an explicit `CALLABLE_CHILD` metadata
alias. The identity includes instruction bytes, constant-pool shape and literal
values (plus shared storage identity when available), source identity,
parameter count, and complete source/debug spans. This avoids
conflating same-name functions declared on one source line, a case that is
especially important when a binary provider contains both `answer()` and
`Math.answer()`.

The graph visitor follows constants and children with cycle protection. It is
used by linking, generation invalidation, GC marking/rewriting, and AOT table
construction. GC treats the callable witness and owner prototype as managed
edges; compaction rewrites those edges without exposing a process address in a
persistent record.

## Reload behavior

Removing or replacing a module advances the generation through the complete
function graph and clears resolved witnesses. Removal also clears the module
registry's hot string-pair cache before freeing the hash node. An old witness
fails with a structured stale-generation diagnostic. A subsequent import links
the replacement provider by token and contract; static sites do not fall back
to a member-name lookup.

## Guarded cache witnesses

`ZrCore_Execution_CheckBindingGuard` is a classification-only API. Its current
first-party callers are the guarded-cache unit fixture, not interpreter/native
or AOT dispatch. It checks a supplied nonzero generation and contract witnesses,
then the independent VM target generation before receiver shape/slot routing.
A slot fallback only permits later slot lookup and counts as a miss; it neither
looks up nor invokes the target. OK may retain TARGET_NONE for deferred typed
or polymorphic selection. This guard does not invalidate a failing binding.

Pointers are borrowed for the synchronous check; the caller keeps them valid
and owns exclusive writes to optional counters/diagnostics. Exact receiver type
identity and native/AOT callable-object generation are not fully checked here.
Reset drops dynamic witnesses and counters while preserving contract, relocation
and static callsite coordinates, without releasing referenced objects. See
[the detailed guard contract](core-binding-guard-generation.md) for field-level
constraints, branch-specific diagnostics, all eight states and finite tests.
Existing runtime graph GC behavior and historical runtime acceptance are separate
from this guard's static comment review.

## 静态契约、运行目标与线格式的边界

本节对应 `call_binding.c`、`call_binding_encoding.c` 与私有
`call_binding_contract_internal.h` 的完整注释审查：17 个函数、11 个有独立
目的的块、2 个宏及 3 处包含边界，共 33 单元。三文件没有类型、字段、枚举或
全局变量定义；公开类型仍由 `call_binding.h` 提供。以下是当前静态调用证据，
此次未执行构建或测试，也未扩大到消费者文件的全文审查。

私有 inline 检查器让公开 runtime 检查与 Core ExecIR 行校验复用同一组纯规则，
无需为行校验引入运行目标或函数图链接依赖。它检查 token 的表号和非零 RID、
非零签名哈希、保留位、owner/layout 配对及分派槽规则，不证明 metadata 中存在
对应实体，也不重新计算签名。typed 可以没有固定成员目标，虚/接口需要 owner
和槽；调用现场的对象类型、接收者布局及真正目标由后续消费者核验。

| 公开入口 | 当前消费目的与失败状态 |
| --- | --- |
| `CheckContract` | 编码、链接、编译事实及执行 guard 共用字段规则；每次检查先重置完整诊断，不修改 binding。 |
| `CompareContracts` | provider、metadata 与 Resolve 对照两份有效契约；按十个字段的固定次序返回首个差异，绝不回退名称查找。 |
| `Invalidate` | 失败、重载与 checkpoint 恢复撤销目标；只清 generation 和 target，保留 contract，不释放被引用对象。 |
| `AdvanceGeneration` | 图访问器先完成去重收集，再给每个节点推进非零代际并清缓存目标；收集失败尚未进入回调，外层模块事务不因此获得回滚保证。 |
| `Validate` | member、typed、导入及 Resolve 拒绝旧代际或错误目标；非空 binding 失败后目标失效，空 binding 的早拒绝只写诊断 status/expected/actual。 |
| `Resolve` | 先保存可能别名 binding.contract 的预期，再撤销旧目标；token 必须唯一，完成契约对照及目标核验后才成功。失败保留契约而目标失效。 |
| `StatusName` | 执行、模块与 AOT 错误包装读取静态短字符串；未知值返回 unknown-link-error，无分配。 |
| `EncodeContract` | writer 与 artifact 行按固定偏移写小端 64 字节；不持久化指针或 C 结构布局，检查失败未改本地输出缓冲，但不保证外层文件事务回滚。 |
| `DecodeContract` | 普通 .zro reader 与公开夹具只消费 bool；非空输出先清零，字段在局部检查成功后整体发布，输入和输出需避免重叠。 |

artifact reader 使用独立的内部详细状态 decoder，保留 INVALID_TOKEN 等分类，
公开 bool decoder 只委托给它并转换成功状态。纯诊断写入器只更新三个差异字段；
完整清零属于 contract 检查的初始化步骤，不能概括为所有早拒绝都清完整诊断。

候选数组、函数、闭包及 AOT shim 元数据均同步借用，调用者保持它们有效并协调
并发修改；这些入口没有 retain、锁或 GC pin。缓存纳入真实函数图后的 GC 标记和
搬迁会追踪/重写 VM function 与 callableObject，API 成功本身不建立额外根。
VM/AOT 的目标代际检查允许记录值为零跳过相应探针；native 闭包分支比较现场代际
时没有这项非零条件。NONE 目标对 typed 和多态 site 可以仍为有效的延迟选择状态。

当前生产 Resolve 调用均传单候选；公开夹具另有重复 token 双候选拒绝。
TODO：唯一命中非零候选索引时，Resolve 写 candidateIndex 后 Compare 的检查又会
清零诊断，尚需从该多候选公开入口明确失败诊断是否应保留位置。现有测试没有
证明这项用途，不据此新增 BUG。Unity 注册、CMake 提供者及代码中的断言仅为
本轮静态证据；不据此声称运行结果或整个调用模块已经验证。

## Test coverage

`call_binding_runtime` covers direct, virtual, interface, typed, generation,
GC, and diagnostic behavior. `call_binding_module` covers source and binary
module functions, static type methods, captured module state, same-line
same-name binary methods, and reload invalidation. Native registry, artifact,
AOT projection, and property suites cover the other target kinds and shared
cache paths. Opcode-prefix and SIMD instruction design remain later milestones.
