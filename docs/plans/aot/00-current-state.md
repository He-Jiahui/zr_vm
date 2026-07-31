# AOT 00：当前状态与缺口

## 已有基线

- AOT C backend 已具备 typed scalar local/thunk、部分 loop lowering 和工程级执行入口。
- core 已有 TypeLayout、metadata token、reflection、profile 与 GC/ownership runtime 基础。
- `.zro/.zrm`、binary-first 与 metadata stripping 已有可运行链路。
- parser/semantic facts 正在形成 Canonical Type graph、CFG/dataflow 与统一查询层。

证据见：[typed layout](./02-type-layout/2026-06-24-typed-layout-baseline.md)、[typed register/codegen](./07-codegen/2026-07-19-typed-register-codegen-baseline.md)、[generic sharing](./08-generics/2026-07-19-generic-sharing-runtime-baseline.md)、[memory](./09-memory/2026-06-25-gc-aot-memory-baseline.md)、[reflection](./10-reflection/2026-07-19-token-and-generic-reflection-baseline.md)、[metadata](./11-metadata/2026-07-19-metadata-runtime-baseline.md)、[stripping](./12-stripping/2026-07-03-metadata-stripping-baseline.md)。

## Syntax 对齐状态

- 语言与依赖权威是 [Syntax 01-14 索引](../syntax/README.md)，AOT 的逐节点状态位于[完整追踪矩阵](./syntax-contract-traceability.md)。
- Syntax 01-05 的 canonical type/Place/borrow/layout/ownership/property 基础已有里程碑记录；AOT 仍需按每一行独立证明 machine lowering、artifact 与四执行路径闭合。
- `06A` 只建立 migration inventory/frontend/dry-run，`06B` 才执行 writer/backend cutover；不得把 06A 完成解释为 AOT 已删除 legacy fallback。
- `07A` 只建立 reference fixture/manifest 骨架，`07B` 才证明 current reference 的 interp、binary-first、AOT C、AOT LLVM 等价。
- `10R`、`10F`、`10C` 分别冻结 resolver/artifact identity、FFI ABI 和 official provider convergence；任何 AOT 计划引用 Syntax 10 时必须写明具体阶段。
- Syntax 11-14 分别增加 generated declaration、Task/Scheduler、Iterator frame 与 TestManifest 的 AOT/metadata/trim 责任，不能继续沿用只覆盖 01-10 的旧投影。

## 结构性缺口

1. 部分 backend 路径仍从旧指令、AST shape 或 ad-hoc type flags推断值语义，Canonical TypeRef 与 Place尚未成为唯一入口。
2. typed scalar fast path 不能代表 struct/ref/owner/property/callable 全部布局；aggregate ABI 与 cleanup CFG仍需统一。
3. metadata、reflection、generic sharing、stripping和module loader尚未完全共享稳定 identity/hash。
4. native extern 仍存在运行时 object/string 签名重解析；目标是 artifact 中持久化 FfiSignature。
5. 既有 baseline 多为局部 verifier、runtime consumer 或单后端切片；它们不能替代 Syntax 01-14 对应行要求的完整 AOT 产物与四执行路径矩阵。

## 重写边界

- 本目录只描述 target architecture与实现顺序。
- 语言表层以 [syntax](../syntax/README.md) 为准。
- Syntax 与 AOT 的唯一状态映射位于[完整追踪矩阵](./syntax-contract-traceability.md)；未来出现未映射 Syntax 节点时，受影响 AOT 晋级 fail closed。
- 完成记录不自动晋级新计划；必须由本目录 gate重新验证。
- 当前活动实现可以继续，但提交前必须说明它落在哪个新 contract/milestone。
