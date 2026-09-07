---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
related_module_docs:
  - docs/parser-and-semantics/semantic-assignment-fact-ownership.md
tests:
  - tests/parser/test_assignment_reference_diagnostic_cases.h
  - tests/parser/test_reference_fact_emission.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
doc_type: milestone-record
---

# Plan 03 Task 7.72: Assignment Reference Roles

## Failure and Correction

The local reference parity case resolved the correct canonical symbol and
returned three locations, but projected two reads and one write. LSP analysis
inferred assignment children separately, so the left identifier was a read.
The assignment branch now submits the complete node to parser inference and
consumes the resulting structured diagnostic through the existing helper.

Parser assignment inference previously performed a smaller compatibility check
than the LSP path. The migration therefore also reuses the detailed assignment
checker in the parser. This preserves ownership and type diagnostics, exact
source and expected-declaration ranges, and rejection of incompatible nominal
objects and null to nonnullable owners. Expected declaration coordinates come
from the canonical binding; the unused LSP name-based helper is deleted.

## Reproduction

The original parity runner exited 1 with the local-reference case failing. The
initial complete-node change restored all 18 parity cases, while the broad
analyzer exposed the parser's whole-assignment ownership diagnostic range.
The precise source range is now preserved by the shared detailed checker.

Four parser regression cases retain target Write and source Read facts even
when compatibility rejects the assignment. Before the detailed checker change,
the runner reported `11 Tests 3 Failures`: scalar diagnostic range was too wide,
incompatible object types were accepted, and a nonnullable owner accepted null.
The LSP test also checks role counts after replacing the document snapshot.

## 状态与产出记录

- 开始时间：2026-09-07 18:30:00 +08:00。
- 实际完成时间：2026-09-07 19:45:00 +08:00。
- 状态：Task 7.72 修复、确定性 RED/GREEN 回归及三工具链窄验证完成；
  Plan 03 Task 3/7/8 完整验收继续进行中。
- 完成项目：完整赋值事实生产路径；parser 统一详细兼容性检查；删除旧 LSP
  expected-type lookup；新增 parser 负向角色/范围回归及 updated-snapshot 高亮断言。
- 验证命令及结果：GCC、Clang ASan/UBSan、MSVC static Debug 的 reference-fact
  `11/11`、local semantic hover `12/12` 均真实 exit 0；parity 功能断言均为
  `18/18`，GCC/MSVC 真实 exit 0，Clang 因既有 544 字节/4 分配 LSan 泄漏
  真实 exit 1（依据 `.codex/task772-clang-parity.log` 修正原 exit 0 记录）；GCC/MSVC
  type-inference `124/124` 真实 exit 0。GCC/Clang/MSVC broad semantic analyzer
  均只保留已登记的九项功能失败；完整 LSP interface 均只保留已登记的六项失败。
  GCC/MSVC local semantic query 保留两项历史缓存依赖失败，Clang 同样两项并报告
  464 字节/6 分配既有 LSan 泄漏。GCC/MSVC source-contract 仅受并发 stdio
  `cJSON_CreateString("declaration")` 缺失影响；Clang source-contract 同样未改变
  该并发失败。Clang 完整 type-inference runner 在
  `tests/parser/test_type_inference.c:7152` 的固定文件名字符串触发当前
  `xxHash` `global-buffer-overflow`，因此不能作为本片的完整 sanitizer 通过证据；
  本片新增 reference-fact target 在 Clang ASan/UBSan 下仍为 `11/11` 且无 ASan/UBSan
  报告。Clang analyzer/interface 分别报告既有 448 字节/8 分配、18,472 字节/390
  分配 LSan 泄漏，ownership diagnostics 仍为冻结的 13 项失败（Clang 报告 896
  字节/12 分配 LSan），未出现本片相关 UAF。原始日志保存在 `.codex/task772-*`。
- 源码版本：基于 `b6c7086e` 后的共享 main 工作树；并行 stdio、core、AOT、
  call-binding 和 benchmark 修改不属于本子项。
- 产出路径：本记录、`semantic-assignment-fact-ownership.md`、两层赋值实现及相关测试。
- 剩余门槛：三工具链窄验证、既有 analyzer/interface 失败与 sanitizer 泄漏；
  Plan 03 Task 3/7/8 完整矩阵。
