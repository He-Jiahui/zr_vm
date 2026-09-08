---
related_code:
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_library/include/zr_vm_library/native_binding.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/README.md
  - docs/plans/aot/index.md
tests:
  - tests/parser/test_canonical_type_graph.c
  - tests/parser/test_semir_pipeline.c
  - tests/core/test_gc_domain_bridge.c
doc_type: reference
---

# 术语表

| 术语 | 含义 |
| --- | --- |
| AST | parser 的语法树；只表达表层结构，不是最终类型/调用事实 |
| Canonical Type / TypeId | 归一化后的类型身份，含 owner module、generic 参数和 generation |
| Place | 可被读写/借用的存储位置；变量、字段、索引和投影都有稳定 PlaceId |
| ValueId | SemIR 中一次值产生/使用的身份，用于 loan liveness 和 bounds proof |
| Region | 借用或 cleanup 的生命周期区域 |
| Loan | 对 Place 的借用约束；可为 mutable/shared，按 NLL last use 结束 |
| TypeLayout | inline 值的 size/alignment/field/GC/ownership/copy/drop 描述 |
| SemIR | 编译前语义中间表示，保存事实和 cleanup/dispatch 边界 |
| Provider | 通过 native descriptor 提供模块/协议/类型能力的组件 |
| Contract role/hash | provider 或成员的稳定语义身份；不等同于名称字符串 |
| NativeRegistry | 宿主挂在 `SZrGlobalState` 上的 descriptor 注册表；负责 ABI、phase、identity 和 contract 校验 |
| CompileTool | 只在编译器阶段运行的 provider；读取 BuildFacts、发出 typed diagnostic 或提交声明 Patch |
| BuildFacts | `.zrp` profile 产生的规范编译事实；`zr.compile.build.feature` 只能读取已声明 feature |
| Declaration view | `zr.compile.declaration` 提供的不可变 `TypeView`/`DeclarationView` 快照；不能直接改写 AST |
| Patch | 声明 transform 的 typed 事务；先校验全部操作，全部成功后一次性提交 |
| Module identity | domain + canonical segments + package；不等同于文件路径 |
| Generation | module/metadata/cache 的单调代数；reload 后阻止 stale handle 命中 |
| GcDomain | 一组可协作 mutator 共享的 GC heap/屏障边界 |
| Global / State | Global 是 VM 实例 owner；State 是线程/mutator 执行上下文 |
| Job / Task | Job 是一次消费的 cold callable；Task 是完成/故障句柄 |
| AttachedDomain / IsolatedDomain | thread provider 的同 heap mutator / 独立 heap 传输策略 |
| Ref-like | 只在短生命周期内借用 source 的 inline view，不可持久存储或跨 await |
| Deopt | AOT/快速路径无法证明时退回 VM/runtime helper 的显式路径 |
| TestManifest | Test phase 编译产物，描述可运行 test case 而非源码扫描结果 |
| Provider phase | descriptor 的运行阶段：Runtime、CompileTool 或 Test；错误阶段不得注册 |
| `.zro/.zri/.zrs/.zrm` | binary、intermediate、syntax projection、package archive |
