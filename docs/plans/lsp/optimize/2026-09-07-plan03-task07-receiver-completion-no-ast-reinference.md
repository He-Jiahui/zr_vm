---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - tests/language_server/test_lsp_source_contract_canonical_completion_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_receiver_completion_projection_cases.h
related_module_docs:
  - docs/cli-and-tooling/lsp-receiver-completion-capability-boundary.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
  - docs/plans/astra/lsp/review.md
doc_type: milestone-record
---

# Plan 03 Task 7.67: Receiver Completion Has No AST Reinference

## Goal

Remove the two completion-local receiver prototype scans and expression type
inference fallbacks. Preserve member projection from published reference types
and existing symbol, type-environment, and metadata inputs.

## Contract

- `TryCollectReceiverCompletions` first consumes the published reference type
  fact for ordinary receivers and retains native/type-symbol projection when
  that fact has no local prototype members.
- The bounded function does not call
  `find_receiver_variable_prototype_recursive`,
  `try_infer_receiver_type_text_from_ast`, or
  `ZrParser_ExpressionType_Infer`.
- Explicit type bindings, imported native metadata, symbol/type-environment
  projections, and the existing class/import fallback remain available.
- A native local whose semantic context and projected symbol type are missing
  yields no members through the exported completion query, even though its
  constructor initializer AST remains present.
- Existing class constructor discovery and imported/name/type-environment
  resolution are explicitly outside this deletion; their canonical identity
  and stale-fact matrices remain pending.

## RED/GREEN

The source contract first reported both forbidden helper names inside the
receiver completion function. Removing those completion-only branches
converted the same contract to GREEN. The
canonical reference-fact path and existing symbol, type-environment, class, and
metadata projections remain in the function. The runtime regression checks
Vector3 x/y/z members with the published type, then removes the analyzer's
semantic context and projected symbol type and checks an empty completion
result. It restores the borrowed pointers before cleanup and frees every
request-owned completion item.

## Verification

- GCC and Clang rebuilt the source-contract and interface targets. MSVC rebuilt
  the production and test sources; source-contract execution passes on all
  three toolchains.
- GCC and fresh static MSVC both pass the structured receiver completion and
  new initializer regression. Their full interfaces exit `1` with exactly the
  same eight failure names as the frozen
  `current-gcc-aggregate-20260906-1429` log: class fixture diagnostics, closed
  generic hover, explicit exact-type failure hover, extern type hover, extern
  layout hover, local symbol query, reference payload hover, and container
  foreach hover.
- Clang's full interface exits `1` at the existing
  `semantic_scope_facts.c:799` heap-use-after-free before this regression. A
  separate GDB invocation after registry initialization runs only the new
  regression and releases global state; it reports PASS,
  `TASK767_FOCUSED_FAILURES:0`, and debugger exit `0`. ASan/UBSan access checks
  stay enabled; `detect_leaks=0` is required for this ptrace run and provides no
  leak acceptance.
- The older MSVC shared interface configuration has an existing unexported
  `ZrLanguageServer_Lsp_StringsEqual` dependency; the new regression uses the
  exported completion query and adds no internal API export requirement. The
  old static build exited `0xc0000005` before emitting test output. The fresh
  static Debug build completes and runs through the entire interface suite;
  the old-cache crash does not reproduce there. Its source contract exits `0`.

Build commands from the repository root (the Linux commands run under WSL):

```text
cmake --build .codex/build-lsp-opt-gcc --target zr_vm_language_server_lsp_source_contracts_test zr_vm_language_server_lsp_interface_test --parallel 4
cmake --build .codex/lsp-optimize-validation/clang-asan-current --target zr_vm_language_server_lsp_source_contracts_test zr_vm_language_server_lsp_interface_test --parallel 4
cmake --build .codex/lsp-optimize-validation/task767-msvc-static --target zr_vm_language_server_lsp_source_contracts_test zr_vm_language_server_lsp_interface_test --parallel 4
```

Each executable is run from that build's `bin` directory. The isolated Clang
check uses these GDB commands against its interface executable:

```gdb
set pagination off
set confirm off
set environment ASAN_OPTIONS detect_leaks=0
break test_lsp_context_create_and_free
run
up
call test_lsp_receiver_completion_does_not_reinfer_initializer(state)
set $task767_result = (int)g_failed_test_count
call ZrCore_GlobalState_Free(global)
printf "TASK767_FOCUSED_FAILURES:%d\n", $task767_result
quit $task767_result
```

## 状态与产出记录

- 开始时间：2026-09-07 06:35 +08:00（本记录建立时间）。
- 实际完成时间：2026-09-07 06:55 +08:00。
- 状态：本删除边界、三工具链源码契约与 focused 回归完成；
  Task 7、Task 3、Task 8 及完整矩阵继续进行中。
- 完成项目：删除两处 completion-local receiver prototype/type inference
  fallback；保留 reference fact 的 native/type-symbol 投影；增加源码边界与
  exported completion query 缺失类型回归。
- 源码版本：基于 `27b3c259` 后的共享工作树；本记录与明确列出的代码和测试同提交。
  并发 core/parser/AOT 等修改不属于本子项，也未视为冻结验收版本。
- 产出路径：本记录、`docs/cli-and-tooling/lsp-receiver-completion-capability-boundary.md`、
  `tests/language_server/test_lsp_receiver_completion_projection_cases.h`；本地运行日志位于
  `.codex/lsp-optimize-validation/task767-*.log`。
- 剩余门槛：class/import receiver identity、完整 source/binary/native/stale/unresolved
  consumer 矩阵、Task 3 sourceless/provider generation、Task 8 的 16-target 与
  stdio/CLI smoke，以及已登记的 baseline failures 和 sanitizer 问题。
