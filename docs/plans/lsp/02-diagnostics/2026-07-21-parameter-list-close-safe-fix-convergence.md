---
related_code:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
plan_sources:
  - user: strict LSP semantic inference plan execution with per-submilestone records and commits
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_advanced_editor_features.c
  - tests/language_server/test_lsp_diagnostic_safe_fix_cases.h
  - tests/language_server/stdio_diagnostic_fix_smoke.js
doc_type: milestone-detail
plan_id: lsp-02-diagnostics
record_id: 2026-07-21-parameter-list-close-safe-fix-convergence
status: completed
completed_at: 2026-07-21 06:51 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: parameter-list-close-safe-fix-convergence
---

# Parameter-List-Close Safe-Fix Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-21 06:51 +08:00 | 已完成 | `missing_parameter_list_close`从共享declaration producer到unexpected-token起点零宽`)` machine edit、通用LSP quickfix和stdio apply/rebind清除闭环；consumer不按declaration kind/code/message/source重建；GCC/Clang/MSVC十八目标矩阵、三套stdio/CLI与三套diagnostic-fix smoke真实exit 0、marker为0 |

## 已实现契约

- `ZrParser_DiagnosticBuilder_BuildMissingParameterListClose`先构造既有structured diagnostic，再保留primary location并在`location.start`发布`Insert missing ')'`、edit text `)`、`ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE`的零宽fix。
- Function、class method/meta/setter、interface method/meta、extern function/delegate、union等既有producer都在确认parameter list缺少`)`后把current unexpected token range传给同一reporter；本阶段不新增declaration-specific分支。
- `func pick(value: int: int { ... }`的primary继续覆盖第二个`:`，edit只在该token前插入`)`，不替换return-type colon或其他源码。
- Fix分配失败会释放已构造的diagnostic并返回失败，不发布半初始化fact。
- LSP production无需新增parameter-list-specific逻辑；既有structured-fix consumer复制title/range/text并复用code-action snapshot，stdio从同一fix数组序列化`Diagnostic.data.fixes`。修正后的document version不再发布该code。

## TDD与根因证据

- Parser RED在稳定`95fa842`生产基线上为22 Tests / 1 Failure；新增builder case只在`diagnostic.fixes.isValid`断言失败。
- LSP RED为44项 / 1 failure；既有`missing_parameter_list_close` diagnostic存在，但没有可供通用code-action consumer投影的machine fix，其余43项通过。
- Stdio RED真实exit 1，精确失败为`Expected one serialized parameter-list-close diagnostic fix`。
- Producer-only GREEN后builder为22 Tests / 0 Failures，advanced-editor为44项0 failure，stdio diagnostic-fix smoke真实exit 0；未修改任何LSP production文件。
- Exact protocol fixture在line 0 character 20发布零宽`)`；version 2改为`func pick(value: int): int { return value; }`后该code清除。

## 工具链与回归证据

- 正式隔离source为`HEAD 95fa842ad9aa68ad27ef97be394fa02242f6297d`加5个code/test exact overlays；并发Syntax 03 M4中间态未进入快照。
- GCC 11.4.0、Clang 14.0.0、MSVC 19.44.35228 / `VSCMD_VER=17.14.36`使用指向同一source snapshot的独立static cache。
- 每套执行相同18个测试目标并取得18/18真实process exit 0；54个测试日志全部非空，`Fail -`、`FAIL:`和`:FAIL:` marker为0。
- 三套parser builder均22 Tests / 0 Failures；三套advanced editor均44项 / 0 failure。
- GCC、Clang和MSVC分别运行主`stdio_smoke.js`与`stdio_diagnostic_fix_smoke.js`，共6个Node进程真实exit 0。

## Snapshot、Schema与协议边界

- 本阶段不改变diagnostic registry、semantic query generation、artifact/binary schema、public-contract hash或cache key。
- `SZrStructuredDiagnostic`、fix结构和builder public API不变；只让既有builder填充已有fix数组。
- Code-action snapshot opaque data和resolve规则不变；本leaf复用已验收的version/generation/open-state/length/hash门禁。

## 未完成边界

- `missing_call_close`、`missing_group_close`及其他delimiter producer尚未完成machine-fix收敛；delimiter replacement/mix仍需独立语义边界。
- Registry-wide applicability审计、migration edit、多document fix和workspace command仍未完成。
- 没有采集p50/p95/p99、峰值内存、cancellation latency或100次乱序race，因此不晋级完整L3/L6。
