# ZR AOT 重设计计划索引

> 状态：与当前 [Syntax 01-14 权威索引](../syntax/README.md)建立完整追踪；已有 AOT 实现只作为 evidence-scoped baseline，不自动代表目标 contract 已完成。

## 目标与权威边界

AOT 的职责是把 Syntax 已冻结的 Canonical TypeRef、TypeLayout、Place/Value/CFG facts、Semantic IR 和 versioned artifact 确定性降低为 native code，并与 VM 保持可观察语义一致。AOT 不定义语法、ownership、property、module、Task、Iterator 或 TestManifest 语义，也不得从 AST 拼写、`%xxx`、类型名字符串或 runtime object shape 反推这些语义。

[AOT / Syntax 完整追踪矩阵](./syntax-contract-traceability.md)是本目录的同步账本。Syntax 新增或重命名稳定阶段/里程碑后，矩阵缺行即阻断受影响 AOT 计划晋级。

## AOT 子系统计划

| AOT 计划 | 文档 | 核心交付 |
|---:|---|---|
| 00 | [当前状态](./00-current-state.md) | baseline、缺口与重写边界 |
| 01 | [设计原则](./01-design-principles.md) | backend-neutral contract、traceability 与 parity gate |
| 02 | [类型与布局](./02-typed-value-and-layout.md) | Canonical TypeRef、ABI/Layout、GC/ownership maps |
| 03 | [Semantic/ExecIR](./03-instruction-set-refactor.md) | Place/Value/CFG/cleanup 到 ExecIR |
| 04 | [C/LLVM backend](./04-semir-and-c-backend.md) | target lowering、thunk、异常与调用路径 |
| 05 | [ownership/GC bridge](./05-ownership-gc-and-bridge.md) | Drop、barrier、pin、owner/GC bridge |
| 06 | [总实施路线](./06-implementation-blueprint.md) | 后端分层 gate 与 Syntax readiness |
| 07 | [寄存器与调用 ABI](./07-codegen-register-model-and-environment-isolation.md) | typed register、call frame、environment |
| 08 | [泛型共享](./08-generic-sharing.md) | canonical instantiation、dictionary、specialization |
| 09 | [内存管理](./09-memory-management.md) | allocation、stack map、pool、no-scan proof |
| 10 | [反射](./10-reflection.md) | token roots、dynamic construction、invoke |
| 11 | [metadata/module/FFI](./11-metadata.md) | artifact schema、ModuleIdentity、FfiSignature |
| 12 | [裁剪](./12-code-stripping.md) | 可证明可达性与 metadata preserve |

## Syntax 01-14 主题投影

| Syntax 主题 | 阶段边界 | AOT 责任 | 主 AOT 计划 | 必须产物 |
|---:|---|---|---|---|
| [01 Canonical TypeRef/Place/CFG/artifact](../syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md) | M1-M5 | 只消费规范类型、Place/Value、flow 与 schema | 02、03、07、11 | Type/Layout table、ExecIR、versioned artifact |
| [02 ref/in/out/scoped/readonly](../syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md) | M1-M6 | 将已验证 passing/region/receiver contract 降为 ABI | 03、04、05、07 | call frame、ref provenance、cleanup edge |
| [03 struct/ref struct/Span/layout](../syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md) | M1-M5 | target layout、aggregate ABI、stack/ref-like 限制 | 02、04、05、09 | LayoutHash、GC map、bounds/provenance op |
| [04 resource/Drop/GC bridge](../syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md) | M1-M7 | copy/move/drop glue、domain safepoint、root/barrier/transport | 05、07、09、11、12 | ownership map、Drop thunk、GC stack map、transport ABI |
| [05 unified property](../syntax/2026-07-18-05-property-unified-ast-design.md) | M1-M5 | lower get/set/ref-get 与显式 field Place | 03、04、10、11、12 | PropertyContract、call/Place lowering、roots |
| [06 migration](../syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md) | `06A/M1-M3`、`06B/M4-M5` | 06A 只提供 inventory/gate；06B 删除旧 backend/artifact fallback | 06、11、12 | dry-run 输入、schema cutover、删除清单 |
| [07 reference fixture](../syntax/2026-07-19-07-comprehensive-syntax-reference-fixture-design.md) | `07A` skeleton、`07B` current promotion | 建立四执行路径和 artifact 的纵向验收 | 04、06 | checksum、异常、artifact golden、profile |
| [08 reflection](../syntax/2026-07-19-08-reflection-library-type-system-design.md) | M1-M5 | 保留 token/roots 并生成 construct/invoke thunk | 08、10、11、12 | reflection table、binder/invoker、preserve roots |
| [09 pooling](../syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md) | M1-M5 | 按 TypeLayout 支持 slab 与 guarded direct ref | 05、09 | GcScanKind、StableSlot、guard contract |
| [10 native/module/package](../syntax/2026-07-19-10-native-ffi-module-package-design.md) | `10R/M1-M2`、`10F/M3`、`10C/M4-M5` | ModuleIdentity、`.zrm`、FfiSignature、provider closure | 04、11、12 | dependency/native table、AOT thunk、provider roots |
| [11 comptime/metadata/typed generation](../syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md) | M1-M5 | 编译生成声明按普通 canonical contract 进入 codegen、metadata 和 trim | 03、08、10、11、12 | generated contract rows、phase roots、deterministic artifact |
| [12 Task/Job/Scheduler](../syntax/2026-07-20-12-async-task-job-scheduler-design.md) | M1-M6 | task frame/state machine、scheduler/domain ABI、transport与cleanup | 03、04、05、07、09、11、12 | FrameLayout、safepoints、host policy、transport schema |
| [13 Iterator/Enumerator/yield](../syntax/2026-07-20-13-iterator-enumerator-yield-design.md) | M1-M4 | iterator frame/state、yield/resume cleanup、sync/async ABI | 03、04、05、07、09、11、12 | iterator frame layout、resume thunk、artifact roots |
| [14 test metadata/harness](../syntax/2026-07-20-14-test-function-harness-design.md) | M1-M4 | TestManifest、test-only roots、sync/async runner artifact | 04、10、11、12 | versioned manifest、production trim、runner parity |

## 阶段依赖规则

- `06A` 只建立 inventory、migration frontend 与 dry-run，不证明目标 AOT lowering 已存在；`06B` 必须等待 08、10-14 及其 AOT 投影闭合后才可切换 current writer/backend。
- `07A` 只建立 fixture/manifest 骨架；只有 `07B` 可以在 06B、08-14 和四执行路径一致后晋级 current reference。
- `10R` 可先冻结 resolver、manifest 和 provider phase；`10F` 独立冻结 FFI ABI；`10C` 必须汇聚 08、09、11-14 的 provider contract，不能用占位 provider 代替 owner gate。
- Syntax 里程碑完成只解除 AOT 上游阻塞，不改变 AOT 行状态。AOT 状态和证据以[详细矩阵](./syntax-contract-traceability.md)为准。

## 每个 AOT 里程碑的必填证据

1. 明确 Syntax 节点、schema 版本、输入 TypeId/PlaceId/SymbolId 以及拒绝的旧输入。
2. 列出新增或修改的 IR、artifact 与 runtime ABI，不以“backend 支持”笼统代替。
3. 覆盖成功、边界、失败、异常 cleanup 和跨模块/target 不兼容。
4. 先通过 leaf/core，再通过 source、binary-first、AOT C、AOT LLVM 项目矩阵。
5. 记录性能计数与允许的 runtime check；未经 CFG/layout 证明不得声称消检。
6. 完成记录写明验证命令、结果、未完成边界和对应 Syntax/AOT 稳定节点。

## 共同晋级门

- interp、binary-first、AOT C、AOT LLVM 对同一 fixture 输出、异常分类和生命周期计数一致。
- backend 只消费 versioned artifact/IR；没有 AST token、旧 `%xxx` 或类型名 fallback。
- LayoutHash、CallableContractHash、ModuleIdentity、FfiSignature、FrameLayout 和 TestManifest 在适用的 writer/reader/backend roundtrip 后一致。
- borrow、move、readonly、out definite assignment、ref-struct escape 和 owner capability 在 AOT 前静态完成；AOT 只实现已验证操作与必要 runtime guard。
- 完成结论写入对应 `plan-id/detail.md`；正文只链接，不追加不可审计的滚动日志。

## 历史完成记录

可复用 baseline 位于 `02-type-layout/`、`07-codegen/`、`08-generics/`、`09-memory/`、`10-reflection/`、`11-metadata/`、`12-stripping/`。记录中的“完成”只对其 evidence scope 成立，不能覆盖追踪矩阵中尚未闭合的 Syntax 节点。
