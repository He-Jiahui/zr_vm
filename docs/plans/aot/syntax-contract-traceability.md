# AOT / Syntax 完整追踪矩阵

> 上游权威：[Syntax 01-14 索引](../syntax/README.md)。本文件只记录 AOT 投影，不复制或修改语言语义。

## 状态规则

| 状态 | 含义 |
|---|---|
| `blocked_by_syntax` | 所需 Syntax gate 尚未晋级，AOT 不得发明替代 contract。 |
| `ready` | Syntax contract 可消费，但尚无满足本行范围的 AOT evidence。 |
| `in_progress` | 本行 AOT 工作正在执行，完整退出门尚未满足。 |
| `partially_verified` | 有历史或子切片证据，但未覆盖本行全部产物和四执行路径。 |
| `completed` | 本行全部 AOT 产物、负例和 interp/binary/AOT C/AOT LLVM parity 已有链接证据。 |
| `not_applicable` | 没有机器后端行为；该行仍必须说明 artifact/trim 边界。 |

Syntax 状态与 AOT 状态独立。Syntax `completed` 只允许 AOT 从 `blocked_by_syntax` 转为 `ready` 或开始验证；不能直接得到 AOT `completed`。

## 01 Canonical TypeRef、Place、CFG 与 artifact

| Syntax 节点 | Canonical Syntax 输出 / gate | AOT 责任 | AOT 计划 | 必需 AOT 产物 / evidence | AOT 状态 |
|---|---|---|---|---|---|
| `01/M1` | [Canonical Type graph](../syntax/01-canonical-type-place-cfg-artifact/m1-type-graph.md) | 只按 TypeId/TypeUse 分类 machine value | [02](./02-typed-value-and-layout.md)、[08](./08-generic-sharing.md)、[11](./11-metadata.md) | type table roundtrip、open/invalid type 拒绝、四路径 identity parity | `partially_verified` |
| `01/M2` | [Place 与 CFG](../syntax/01-canonical-type-place-cfg-artifact/m2-place-cfg.md) | 把 Place projection 与 CFG edge 输入 ExecIR | [03](./03-instruction-set-refactor.md) | Place/Value golden、branch/join/exception edge verifier | `partially_verified` |
| `01/M3` | [前置 Semantic IR](../syntax/01-canonical-type-place-cfg-artifact/m3-pre-semantic-ir.md) | 只从 validated SemIR 生成 ExecIR/backend op | [03](./03-instruction-set-refactor.md)、[04](./04-semir-and-c-backend.md) | SemIR-to-ExecIR golden、unsupported op fail-closed | `partially_verified` |
| `01/M4` | [Artifact schema](../syntax/01-canonical-type-place-cfg-artifact/m4-artifact-schema.md) | 读取 versioned canonical sections | [11](./11-metadata.md)、[12](./12-code-stripping.md) | writer-reader-writer bytes、schema/hash mismatch diagnostics | `partially_verified` |
| `01/M5` | [Canonical consumers](../syntax/01-canonical-type-place-cfg-artifact/m5-canonical-consumers.md) | 与 VM/debug/LSP/reflection 消费同一 canonical projection | [04](./04-semir-and-c-backend.md)、[06](./06-implementation-blueprint.md) | interp/binary/AOT C/AOT LLVM parity，无 AST fallback | `partially_verified` |

## 02 Reference syntax 与 borrow checker

| Syntax 节点 | Canonical Syntax 输出 / gate | AOT 责任 | AOT 计划 | 必需 AOT 产物 / evidence | AOT 状态 |
|---|---|---|---|---|---|
| `02/M1` | [Syntax/canonical contract](../syntax/02-reference-syntax-borrow-checker/m1-syntax-canonical-contract.md) | 消费 passing form、TypeUse 与 callable contract | [04](./04-semir-and-c-backend.md)、[07](./07-codegen-register-model-and-environment-isolation.md) | ref/in/out/readonly ABI classification golden | `partially_verified` |
| `02/M2` | [Place/out definite assignment](../syntax/02-reference-syntax-borrow-checker/m2-place-out-definite-assignment.md) | 生成正确 load/store/out slot，不重做 definite assignment | [03](./03-instruction-set-refactor.md)、[07](./07-codegen-register-model-and-environment-isolation.md) | out success/failure CFG golden、call frame slot parity | `partially_verified` |
| `02/M3` | [Loan 与 NLL](../syntax/02-reference-syntax-borrow-checker/m3-shared-mutable-loan-nll.md) | 保留 provenance/lifetime map，不运行时重做 borrow checker | [03](./03-instruction-set-refactor.md)、[05](./05-ownership-gc-and-bridge.md) | ref provenance、move/drop conflict verifier、无 borrow fallback | `partially_verified` |
| `02/M4` | [Receiver/read-only boundary](../syntax/02-reference-syntax-borrow-checker/m4-receiver-readonly-call-boundary.md) | receiver effect 进入 call ABI 与 devirtualization proof | [04](./04-semir-and-c-backend.md)、[07](./07-codegen-register-model-and-environment-isolation.md) | readonly/mutable call matrix、无 defensive-copy drift | `partially_verified` |
| `02/M5` | [Escape/closure/suspension](../syntax/02-reference-syntax-borrow-checker/m5-escape-closure-suspension.md) | frame/environment 只承载已验证 capture/region | [05](./05-ownership-gc-and-bridge.md)、[07](./07-codegen-register-model-and-environment-isolation.md) | closure/frame provenance、suspension refusal、cleanup parity | `partially_verified` |
| `02/M6` | [Artifact/LSP consumers](../syntax/02-reference-syntax-borrow-checker/m6-artifact-lsp-consumers.md) | source/binary 与 machine ABI 使用同一 callable projection | [07](./07-codegen-register-model-and-environment-isolation.md)、[11](./11-metadata.md) | callable hash/TypeId roundtrip、四路径 signature parity | `partially_verified` |

## 03 Struct、ref struct、Span 与 layout

| Syntax 节点 | Canonical Syntax 输出 / gate | AOT 责任 | AOT 计划 | 必需 AOT 产物 / evidence | AOT 状态 |
|---|---|---|---|---|---|
| `03/M1` | [Struct layout/copy maps](../syntax/03-struct-ref-struct-span-layout/m1-struct-layout-copy-maps.md) | 按 TypeLayout 生成 aggregate ABI 与 destination-first construct | [02](./02-typed-value-and-layout.md)、[04](./04-semir-and-c-backend.md) | offset/hash/map golden、C/LLVM static assertions、无 boxing | `partially_verified` |
| `03/M2` | [Receiver effect](../syntax/03-struct-ref-struct-span-layout/m2-receiver-effect.md) | 将 canonical receiver effect 降入 method ABI | [04](./04-semir-and-c-backend.md)、[10](./10-reflection.md) | direct/interface/generic receiver parity | `partially_verified` |
| `03/M3` | [ref struct restrictions](../syntax/03-struct-ref-struct-span-layout/m3-ref-struct-restrictions.md) | 栈/frame 表示与 GC map 遵守 ref-like 限制 | [02](./02-typed-value-and-layout.md)、[05](./05-ownership-gc-and-bridge.md)、[07](./07-codegen-register-model-and-environment-isolation.md) | no-box/no-heap/no-suspend negative、legal frame map | `partially_verified` |
| `03/M4` | [Span core](../syntax/03-struct-ref-struct-span-layout/m4-span-core.md) | fat-ref layout、bounds/provenance op 与 proof-driven check elimination | [02](./02-typed-value-and-layout.md)、[09](./09-memory-management.md) | empty/max/slice bounds、check-on/off parity、profile | `partially_verified` |
| `03/M5` | [Buffer/pool/FFI](../syntax/03-struct-ref-struct-span-layout/m5-buffer-pool-ffi.md) | pin/guard/native window 按结构化 contract lowering | [05](./05-ownership-gc-and-bridge.md)、[09](./09-memory-management.md)、[11](./11-metadata.md) | pin cleanup、pool guard、FFI layout mismatch negative | `partially_verified` |

## 04 Resource ownership、Drop 与 GC domain

| Syntax 节点 | Canonical Syntax 输出 / gate | AOT 责任 | AOT 计划 | 必需 AOT 产物 / evidence | AOT 状态 |
|---|---|---|---|---|---|
| `04/M1` | [resource/Unique + Drop](../syntax/04-resource-ownership-drop-gc-bridge/m1-resource-unique-drop.md) | move 清源、exactly-once Drop 与 partial-init cleanup | [05](./05-ownership-gc-and-bridge.md)、[12](./12-code-stripping.md) | Drop thunk/root、normal/throw/finally count parity | `partially_verified` |
| `04/M2` | [Shared/Weak](../syntax/04-resource-ownership-drop-gc-bridge/m2-resource-shared-weak.md) | retain/release/wake 与 thread capability ABI | [05](./05-ownership-gc-and-bridge.md)、[09](./09-memory-management.md) | strong/weak lifecycle、cycle policy、wake failure | `partially_verified` |
| `04/M3` | [Owner borrow/receiver](../syntax/04-resource-ownership-drop-gc-bridge/m3-resource-owner-borrow-receiver.md) | owner reborrow/provenance 与 call frame lifetime | [05](./05-ownership-gc-and-bridge.md)、[07](./07-codegen-register-model-and-environment-isolation.md) | borrow receiver ABI、move/drop conflict、no name fallback | `partially_verified` |
| `04/M4` | [Domain identity/single mutator](../syntax/04-resource-ownership-drop-gc-bridge/m4-domain-identity-single-mutator-bridge.md) | domain root/barrier 与 explicit intoGc bridge | [05](./05-ownership-gc-and-bridge.md)、[09](./09-memory-management.md) | GcBox bridge、cross-domain edge rejection、root rewrite | `partially_verified` |
| `04/M5` | [Domain-local STW/handoff](../syntax/04-resource-ownership-drop-gc-bridge/m5-domain-local-stw-owner-handoff.md) | safepoint poll 与 same-domain owner handoff ABI | [04](./04-semir-and-c-backend.md)、[05](./05-ownership-gc-and-bridge.md) | multi-mutator pause scope、handoff linearization、Drop=1 | `partially_verified` |
| `04/M6` | [Cross-domain transport](../syntax/04-resource-ownership-drop-gc-bridge/m6-cross-domain-transport.md) | TransferEnvelope encode/decode/abort cleanup | [05](./05-ownership-gc-and-bridge.md)、[11](./11-metadata.md) | transport schema、failure ownership、no raw pointer edge | `partially_verified` |
| `04/M7` | [Concurrent major/artifact/AOT/LSP](../syntax/04-resource-ownership-drop-gc-bridge/m7-concurrent-major-artifact-aot-lsp.md) | 发布完整 domain/ownership artifact 与 AOT safepoint parity | [05](./05-ownership-gc-and-bridge.md)、[07](./07-codegen-register-model-and-environment-isolation.md)、[11](./11-metadata.md) | concurrent major matrix、versioned maps、四路径 parity | `partially_verified` |

## 05 Unified property

| Syntax 节点 | Canonical Syntax 输出 / gate | AOT 责任 | AOT 计划 | 必需 AOT 产物 / evidence | AOT 状态 |
|---|---|---|---|---|---|
| `05/M1` | [Unified AST/Symbol](../syntax/05-property-unified-ast/m1-unified-ast-symbol.md) | 只消费 PropertyDef/Symbol identity，不读旧 AST shape | [03](./03-instruction-set-refactor.md)、[11](./11-metadata.md) | canonical property row、legacy shape rejection | `partially_verified` |
| `05/M2` | [Explicit field/init](../syntax/05-property-unified-ast/m2-explicit-field-init.md) | field Place 与 initializer cleanup lowering | [03](./03-instruction-set-refactor.md)、[04](./04-semir-and-c-backend.md) | field/init Place golden、partial-init failure | `partially_verified` |
| `05/M3` | [Access lowering/receiver effect](../syntax/05-property-unified-ast/m3-access-lowering-receiver-effect.md) | get/set/compound access 降为 canonical call/Place | [03](./03-instruction-set-refactor.md)、[04](./04-semir-and-c-backend.md) | access op golden、readonly/mutable receiver parity | `partially_verified` |
| `05/M4` | [Ref-return Place/region](../syntax/05-property-unified-ast/m4-ref-return-place-region.md) | ref-get 保留 Place/provenance/region 与 ABI | [03](./03-instruction-set-refactor.md)、[07](./07-codegen-register-model-and-environment-isolation.md) | ref-get slot/address stability、escape negative | `partially_verified` |
| `05/M5` | [Consumers/reflection/migration](../syntax/05-property-unified-ast/m5-property-consumers-reflection-migration.md) | binary/reflection/trim 使用同一 accessor identity | [10](./10-reflection.md)、[11](./11-metadata.md)、[12](./12-code-stripping.md) | accessor roots、source/binary hash、trim visibility | `partially_verified` |

## 06 Migration 与 repository cutover

| Syntax 节点 | Canonical Syntax 输出 / gate | AOT 责任 | AOT 计划 | 必需 AOT 产物 / evidence | AOT 状态 |
|---|---|---|---|---|---|
| `06A/M1` | [Migration inventory](../syntax/06-percent-migration-lsp-fixtures/m1-migration-inventory.md) | 分类旧 backend/artifact input，不执行语义切换 | [06](./06-implementation-blueprint.md) | AOT fallback inventory、owner plan 与解除条件 | `ready` |
| `06A/M2` | [Migration frontend/LSP](../syntax/06-percent-migration-lsp-fixtures/m2-migration-frontend-lsp-fixes.md) | 消费 edit/report schema，拒绝 targetNotPromoted 输入 | [06](./06-implementation-blueprint.md)、[11](./11-metadata.md) | old artifact/source admission report、无隐式 upgrade | `ready` |
| `06A/M3` | [Repository dry run gate](../syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md) | 提供 AOT writer/backend allowlist 与重建预案 | [06](./06-implementation-blueprint.md) | zero-unknown dry-run、golden rebuild plan | `blocked_by_syntax` |
| `06B/M4` | [Atomic cutover gate](../syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md) | current writer/backend 只输出新 schema/IR | [06](./06-implementation-blueprint.md)、[11](./11-metadata.md) | parser-to-AOT full matrix、old writer rejected | `blocked_by_syntax` |
| `06B/M5` | [Legacy cleanup gate](../syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md) | 删除旧 opcode/AST/string fallback 与旧 roots | [06](./06-implementation-blueprint.md)、[12](./12-code-stripping.md) | production allowlist empty、legacy symbol scan | `blocked_by_syntax` |

## 07 Comprehensive syntax reference fixture

| Syntax 节点 | Canonical Syntax 输出 / gate | AOT 责任 | AOT 计划 | 必需 AOT 产物 / evidence | AOT 状态 |
|---|---|---|---|---|---|
| `07A` | [Fixture/manifest skeleton](../syntax/07-comprehensive-syntax-reference-fixture/m1-reference-fixture-manifest.md) | 为 AOT case 建立 feature manifest 与预期状态 | [04](./04-semir-and-c-backend.md)、[06](./06-implementation-blueprint.md) | manifest schema、unsupported/blocked 分类 | `partially_verified` |
| `07B` | [Current reference promotion](../syntax/2026-07-19-07-comprehensive-syntax-reference-fixture-design.md) | 同一 fixture 运行 interp/binary/AOT C/AOT LLVM | [04](./04-semir-and-c-backend.md)、[06](./06-implementation-blueprint.md) | output/exception/profile/artifact 全量 parity | `blocked_by_syntax` |

## 08 Reflection library 与 runtime type system

| Syntax 节点 | Canonical Syntax 输出 / gate | AOT 责任 | AOT 计划 | 必需 AOT 产物 / evidence | AOT 状态 |
|---|---|---|---|---|---|
| `08/M1` | [TypeId/descriptor category gate](../syntax/2026-07-19-08-reflection-library-type-system-design.md) | 保留 Type identity/category，不把 symbol presence 当 visibility | [10](./10-reflection.md)、[11](./11-metadata.md) | Type/TypeOf rows、stale/open token negative | `blocked_by_syntax` |
| `08/M2` | [Member/property/callable gate](../syntax/2026-07-19-08-reflection-library-type-system-design.md) | 生成 canonical member/property/invoke descriptors | [10](./10-reflection.md) | overload/accessibility/property role query parity | `blocked_by_syntax` |
| `08/M3` | [Metadata graph/preservation gate](../syntax/2026-07-19-08-reflection-library-type-system-design.md) | 把 reflection roots/edges 纳入 artifact 和 trim graph | [10](./10-reflection.md)、[11](./11-metadata.md)、[12](./12-code-stripping.md) | deterministic roots、trimmed-not-found semantics | `blocked_by_syntax` |
| `08/M4` | [Runtime construction/AOT gate](../syntax/2026-07-19-08-reflection-library-type-system-design.md) | 生成 constructor/invoke thunk 与 generation-aware cache | [08](./08-generic-sharing.md)、[10](./10-reflection.md) | class/boxed-struct success、illegal category/throw cleanup | `blocked_by_syntax` |
| `08/M5` | [LSP/migration/stress gate](../syntax/2026-07-19-08-reflection-library-type-system-design.md) | full/trim/preserve 模式与其他 consumer 结果一致 | [10](./10-reflection.md)、[12](./12-code-stripping.md) | source/binary/AOT/trim query parity、cache invalidation | `blocked_by_syntax` |

## 09 PoolHandle、PoolRef 与 pooling

| Syntax 节点 | Canonical Syntax 输出 / gate | AOT 责任 | AOT 计划 | 必需 AOT 产物 / evidence | AOT 状态 |
|---|---|---|---|---|---|
| `09/M1` | [Handle identity/state gate](../syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md) | lowering StableSlot/generation identity operations | [09](./09-memory-management.md) | stale/wrong-pool/wrap negative、no strong retention | `blocked_by_syntax` |
| `09/M2` | [Guarded ref/property gate](../syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md) | PoolRef guard、ref property 与 scoped Drop | [05](./05-ownership-gc-and-bridge.md)、[09](./09-memory-management.md) | active guard blocks reuse、hot access no revalidation | `blocked_by_syntax` |
| `09/M3` | [Reclamation/slab/GC gate](../syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md) | GcFree/Mapped/Barriered slab maps 与 deferred reuse | [05](./05-ownership-gc-and-bridge.md)、[09](./09-memory-management.md) | scan/barrier maps、GC pressure、ABA stress | `blocked_by_syntax` |
| `09/M4` | [Artifact/reflection/native gate](../syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md) | 持久化 pool layout/capability，生成 native thunk | [09](./09-memory-management.md)、[11](./11-metadata.md) | layout hash roundtrip、provider/reflection parity | `blocked_by_syntax` |
| `09/M5` | [Stress/performance gate](../syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md) | 证明 guard、reuse、scan 与 allocation 成本 | [09](./09-memory-management.md) | target/backend 分项 profile 与语义对照 | `blocked_by_syntax` |

## 10 Native FFI、module 与 package

| Syntax 节点 | Canonical Syntax 输出 / gate | AOT 责任 | AOT 计划 | 必需 AOT 产物 / evidence | AOT 状态 |
|---|---|---|---|---|---|
| `10R/M1` | [Specifier foundation](../syntax/10-native-ffi-module-package/m1-specifier-foundation.md) | 只消费 domain-aware ModuleIdentity | [11](./11-metadata.md)、[12](./12-code-stripping.md) | canonical dependency identity、alias/path spelling exclusion | `partially_verified` |
| `10R/M2` | [Manifest/artifact/provider phase](../syntax/10-native-ffi-module-package/m2-artifact-provider-phase.md) | 加载 `.zro/.zrm/.zrp` provider/phase contract | [11](./11-metadata.md) | manifest/entry/dependency hash、phase mismatch negative | `partially_verified` |
| `10F/M3` | [Native contract gate](../syntax/2026-07-19-10-native-ffi-module-package-design.md) | 由 FfiSignature 生成 import/callback/marshal thunk | [04](./04-semir-and-c-backend.md)、[11](./11-metadata.md) | VM/libffi/AOT ABI vector、corrupt contract rejection | `blocked_by_syntax` |
| `10C/M4` | [Native provider convergence gate](../syntax/2026-07-19-10-native-ffi-module-package-design.md) | 汇聚 08、09、11-14 provider roots 与 canonical TypeIds | [11](./11-metadata.md)、[12](./12-code-stripping.md) | provider inventory/phase/identity closure | `blocked_by_syntax` |
| `10C/M5` | [Consumers/migration gate](../syntax/2026-07-19-10-native-ffi-module-package-design.md) | source/binary/AOT consumer 只读 canonical provider schema | [06](./06-implementation-blueprint.md)、[11](./11-metadata.md) | no-name fallback、provider drift fail-closed | `blocked_by_syntax` |

## 11 Compile-time metadata 与 typed declaration generation

| Syntax 节点 | Canonical Syntax 输出 / gate | AOT 责任 | AOT 计划 | 必需 AOT 产物 / evidence | AOT 状态 |
|---|---|---|---|---|---|
| `11/M1` | [Build facts/comptime-if gate](../syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md) | 只接收已选择 declaration/function graph | [11](./11-metadata.md)、[12](./12-code-stripping.md) | inactive declaration absence、deterministic build facts | `ready` |
| `11/M2` | [Typed comptime/check gate](../syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md) | host-only comptime code与runtime/AOT roots隔离 | [04](./04-semir-and-c-backend.md)、[12](./12-code-stripping.md) | phase separation、sandbox failure、production trim | `ready` |
| `11/M3` | [AttributeUsage/Conditional gate](../syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md) | static metadata rows进入 artifact/hash/reachability | [10](./10-reflection.md)、[11](./11-metadata.md)、[12](./12-code-stripping.md) | role/target validation、conditional call roots | `ready` |
| `11/M4` | [DeclarationTransform/Patch gate](../syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md) | generated declarations走普通 Type/Layout/ExecIR pipeline | [02](./02-typed-value-and-layout.md)、[03](./03-instruction-set-refactor.md)、[11](./11-metadata.md) | deterministic generated identity、source/binary/AOT parity | `ready` |
| `11/M5` | [Consumers/migration gate](../syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md) | reflection/LSP/trim 与 AOT 共享 generated contract | [10](./10-reflection.md)、[12](./12-code-stripping.md) | consumer hash parity、stale patch rejection、06B readiness | `blocked_by_syntax` |

## 12 Task、Job 与 Scheduler

| Syntax 节点 | Canonical Syntax 输出 / gate | AOT 责任 | AOT 计划 | 必需 AOT 产物 / evidence | AOT 状态 |
|---|---|---|---|---|---|
| `12/M1` | [Explicit Task syntax/effect](../syntax/12-async-task-job-scheduler/m1-explicit-task-syntax-effect.md) | canonical async effect/callable 进入 codegen，不改写返回类型 | [03](./03-instruction-set-refactor.md)、[04](./04-semir-and-c-backend.md) | callable hash/Task TypeId parity、no hidden carrier | `partially_verified` |
| `12/M2` | [Task frame/runtime](../syntax/12-async-task-job-scheduler/m2-task-frame-runtime.md) | 生成 state/frame layout、resume/complete/fault cleanup | [07](./07-codegen-register-model-and-environment-isolation.md)、[09](./09-memory-management.md) | frame/root map、resume ABI、Drop/fault parity | `partially_verified` |
| `12/M3` | [Job/Scheduler](../syntax/12-async-task-job-scheduler/m3-job-scheduler-runtime.md) | Job handoff、scheduler callsite 与 must-use roots | [04](./04-semir-and-c-backend.md)、[12](./12-code-stripping.md) | one-time handoff、scheduler thunk、required roots | `partially_verified` |
| `12/M4` | [AttachedDomain scheduler](../syntax/12-async-task-job-scheduler/m4-attached-domain-thread-scheduler.md) | same-domain safepoint/policy 与 capture ABI | [05](./05-ownership-gc-and-bridge.md)、[07](./07-codegen-register-model-and-environment-isolation.md) | domain policy manifest、poll map、Send/Sync rejection | `partially_verified` |
| `12/M5` | [IsolatedDomain transport](../syntax/12-async-task-job-scheduler/m5-isolated-domain-transport.md) | transport encode/decode/abort 与 remote completion | [05](./05-ownership-gc-and-bridge.md)、[11](./11-metadata.md) | no cross-domain edge、failure Drop=1、policy mismatch | `partially_verified` |
| `12/M6` | [Artifact/debug/LSP migration](../syntax/12-async-task-job-scheduler/m6-artifact-debug-lsp-migration.md) | versioned task/frame/provider/debug projections | [07](./07-codegen-register-model-and-environment-isolation.md)、[11](./11-metadata.md)、[12](./12-code-stripping.md) | source/binary/AOT frame parity、roots、schema rejection | `partially_verified` |

## 13 Iterator、Enumerator 与 yield

| Syntax 节点 | Canonical Syntax 输出 / gate | AOT 责任 | AOT 计划 | 必需 AOT 产物 / evidence | AOT 状态 |
|---|---|---|---|---|---|
| `13/M1` | [Enumerator protocol](../syntax/13-iterator-enumerator-yield/m1-enumerator-protocol.md) | constrained/static dispatch 使用 canonical witness | [04](./04-semir-and-c-backend.md)、[08](./08-generic-sharing.md) | no boxing/name lookup、for disposal parity | `ready` |
| `13/M2` | [Yield syntax/SemIR](../syntax/13-iterator-enumerator-yield/m2-yield-syntax-semir.md) | yield/resume/state/cleanup 降为 ExecIR | [03](./03-instruction-set-refactor.md)、[07](./07-codegen-register-model-and-environment-isolation.md) | state golden、early dispose/throw cleanup | `ready` |
| `13/M3` | [Iterator frame runtime](../syntax/13-iterator-enumerator-yield/m3-iterator-frame-runtime.md) | frame layout、roots、sync/async resume ABI | [07](./07-codegen-register-model-and-environment-isolation.md)、[09](./09-memory-management.md) | frame/root map、allocation/Drop profile | `ready` |
| `13/M4` | [Artifact/AOT/debug/LSP gate](../syntax/2026-07-20-13-iterator-enumerator-yield-design.md) | 持久化 frame/state并完成四路径 parity/trim | [11](./11-metadata.md)、[12](./12-code-stripping.md) | artifact roundtrip、logical stepping、resume roots | `blocked_by_syntax` |

## 14 Test metadata 与 harness

| Syntax 节点 | Canonical Syntax 输出 / gate | AOT 责任 | AOT 计划 | 必需 AOT 产物 / evidence | AOT 状态 |
|---|---|---|---|---|---|
| `14/M1` | [Metadata role/TestManifest gate](../syntax/2026-07-20-14-test-function-harness-design.md) | 版本化 TestManifest 并区分 production/test roots | [11](./11-metadata.md)、[12](./12-code-stripping.md) | manifest roundtrip、production body/roots absent | `blocked_by_syntax` |
| `14/M2` | [Assertion library gate](../syntax/2026-07-20-14-test-function-harness-design.md) | assertion native calls使用 canonical provider/Ffi contract | [04](./04-semir-and-c-backend.md)、[11](./11-metadata.md) | AOT assertion pass/fail/throw vector、provider phase | `blocked_by_syntax` |
| `14/M3` | [Runner/isolation gate](../syntax/2026-07-20-14-test-function-harness-design.md) | test artifact 在 VM/AOT runner 中保持 module/case isolation | [04](./04-semir-and-c-backend.md)、[09](./09-memory-management.md) | sync/async/parameterized isolation、exit/result parity | `blocked_by_syntax` |
| `14/M4` | [LSP/debug/migration gate](../syntax/2026-07-20-14-test-function-harness-design.md) | debug map、trim 和 06B migration 与 runner artifact 一致 | [10](./10-reflection.md)、[11](./11-metadata.md)、[12](./12-code-stripping.md) | test-only debug roots、source/binary/AOT visibility parity | `blocked_by_syntax` |

## 同步与晋级规则

1. Syntax README 或设计新增稳定 M/phase 节点时，本矩阵必须在任何受影响 AOT 晋级前增加对应行。
2. Syntax contract 改变 TypeId、Place、ABI、artifact 或 runtime capability 时，相关 AOT 行先降为 `blocked_by_syntax` 或 `ready`，再重新验证。
3. `partially_verified` 只能链接真实 baseline，不得用单一 backend 或单一 fixture 宣称整行完成。
4. `completed` 必须同时有 leaf/core、artifact roundtrip、interp、binary-first、AOT C、AOT LLVM 和负例 evidence；不适用项必须写明理由。
5. 历史完成记录保留原始范围和时间；本矩阵只引用，不回写或扩大其结论。
