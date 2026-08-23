---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_contract.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_semantic_query_parity.c
doc_type: milestone-record
---

# Plan 03 Task 1: Query Purity And Parity

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-24 00:31 +08:00 | 子里程碑已完成；Task 1 进行中 | 完成 exactness fail-closed、borrowed snapshot query view、以及 diagnostics 分析物化与只读查询分离。 | GCC、Clang、MSVC 的 focused query/diagnostics/LSP call-member matrix 均以真实 exit 0 通过；source/binary/native 同构 parity 仍由 Task 1 后续子里程碑验收。 |
| 2026-08-24 02:56 +08:00 | Task 1 已完成；Task 2 待开始 | 新增 source/binary/native standalone parity runner，覆盖 TypeAt、CanonicalTypeAt、CallAt、definition/declaration/references、diagnostics 与 PropertyAt 的重复查询及 borrowed snapshot 稳定性；修正 Windows GNU/Clang 测试兼容路径。 | GCC、Clang、MSVC 均完成 contract 3/3、query 29/29、compiler diagnostics 46/46、call/member 4/4、parity 3/3，所有测试进程真实 exit 0。 |

## Delivered Contract

- `ZrParser_SemanticQuery_TypeAt` 仅投影 `EXACT` expression fact；`UNKNOWN` 与
  `APPROXIMATE` 返回 `ZR_FALSE`。调用方不得在失败后根据文本、成员名或 AST 重建语义。
- `FactsAt`、`Diagnostics` 及其它指针结果都是 `SZrSemanticContext` 的借用 snapshot
  view。跨 snapshot 时只能保存稳定 id 与复制的 range，不能保存 AST、fact 或 diagnostic
  指针。
- `ZrParser_SemanticQuery_MaterializeDiagnostics` 是唯一允许重建 diagnostics cache 的
  分析生命周期操作。`ZrParser_SemanticQuery_Diagnostics` 不写 context；未物化时返回空
  borrowed view，scope 与物化 scope 不一致时 fail closed。
- compiler 与 LSP diagnostics bridge 都在完成 semantic fact 解析后显式 materialize，随后
  仅通过只读 query 投影 structured diagnostics。
- standalone LSP parity runner 直接消费 canonical query 与 PropertyDef identity：native 场景
  使用 `LinkedList.addLast` 的 resolved call 与 `PoolReadRef.value` 的 descriptor property，
  source/binary/native 均不按成员名、accessor 名或文本重建结果。

## Validation

- 新增 `zr_vm_semantic_query_contract_test` 覆盖 approximate `TypeAt` fail-closed、重复
  `FactsAt` 的 borrowed identity、以及 diagnostics query 无副作用。
- GCC Debug static、Clang Debug shared、MSVC Debug shared 均以真实 process exit 0 通过
  contract 3/3、semantic query 29/29、compiler semantic query diagnostics 46/46、LSP
  call/member semantic query 4/4，以及 LSP source/binary/native parity 3/3。
- Windows GNU 兼容路径以 `_WIN32` 选择 `_mkdir`；clang-cl 以优先 compiler identity 和
  C11 `_Noreturn` 处理 Windows CRT 宏，三工具链均复验该公共支持层。
- 另外重编译了 LSP diagnostics target；其现有最后一项 type-mismatch 断言的 return type
  expected range 与实际 source range 不一致，因未修改的基线断言失败。该项不计入本
  子里程碑通过证据，也不在此处用兼容逻辑或测试白名单掩盖。

## Next Gate

Plan 03 Task 2 待开始：将处理 analyzer-only semantic pass 收敛到 parser semantic context，
并继续维持 Task 1 的 borrowed snapshot、EXACT/UNKNOWN/APPROXIMATE 与 canonical identity
合同。后续 LSP consumer 不得用 property/accessor 名称配对兜底。
