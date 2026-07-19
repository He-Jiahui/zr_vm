# AOT 00：当前状态与缺口

## 已有基线

- AOT C backend 已具备 typed scalar local/thunk、部分 loop lowering 和工程级执行入口。
- core 已有 TypeLayout、metadata token、reflection、profile 与 GC/ownership runtime 基础。
- `.zro/.zrm`、binary-first 与 metadata stripping 已有可运行链路。
- parser/semantic facts 正在形成 Canonical Type graph、CFG/dataflow 与统一查询层。

证据见：[typed layout](./02-type-layout/2026-06-24-typed-layout-baseline.md)、[typed register/codegen](./07-codegen/2026-07-19-typed-register-codegen-baseline.md)、[generic sharing](./08-generics/2026-07-19-generic-sharing-runtime-baseline.md)、[memory](./09-memory/2026-06-25-gc-aot-memory-baseline.md)、[reflection](./10-reflection/2026-07-19-token-and-generic-reflection-baseline.md)、[metadata](./11-metadata/2026-07-19-metadata-runtime-baseline.md)、[stripping](./12-stripping/2026-07-03-metadata-stripping-baseline.md)。

## 结构性缺口

1. 部分 backend 路径仍从旧指令、AST shape 或 ad-hoc type flags推断值语义，Canonical TypeRef 与 Place尚未成为唯一入口。
2. typed scalar fast path 不能代表 struct/ref/owner/property/callable 全部布局；aggregate ABI 与 cleanup CFG仍需统一。
3. metadata、reflection、generic sharing、stripping和module loader尚未完全共享稳定 identity/hash。
4. native extern 仍存在运行时 object/string 签名重解析；目标是 artifact 中持久化 FfiSignature。
5. 旧计划把大量微切片执行日志混在设计正文，无法区分已验证 baseline 和新语法目标。

## 重写边界

- 本目录只描述 target architecture与实现顺序。
- 语言表层以 [syntax](../syntax/README.md) 为准。
- 完成记录不自动晋级新计划；必须由本目录 gate重新验证。
- 当前活动实现可以继续，但提交前必须说明它落在哪个新 contract/milestone。
