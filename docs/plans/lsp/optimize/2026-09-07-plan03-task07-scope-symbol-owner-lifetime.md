---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_semantic_scope_symbol_lifetime_cases.h
related_module_docs:
  - docs/parser-and-semantics/semantic-scope-fact-ownership.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
doc_type: milestone-record
---

# Plan 03 Task 7.68: Preserve Scope Owners During Symbol Growth

## Failure and Ownership Contract

The Task 7.67 Clang ASan interface run stopped in
`semantic_scope_facts_visit_type` while updating the document for
`LSP Signature Help Displays Closed Generic Instantiation`. The visitor retained
a borrowed `SZrSemanticSymbolRecord *` across generic parameter publication.
`ZrParser_Semantic_RegisterSymbol` grew `context->symbols`, and the later child
visit read `symbol->id` from the retired allocation. The method visitor had the
same lifetime error before publishing ordinary parameters and its body.

Both visitors now copy the canonical owner ID before publication can grow the
symbol array. Scope, member, parameter, and body publication uses this value.
The ID remains local to its semantic context; no cross-snapshot identity,
display-text inference, or request-time semantic fallback is introduced.

## RED/GREEN

The new source-scope fixture fills the symbol array and forces its next growth
to move storage. The previous storage is cleared and quarantined until the
builder returns, making stale ID reads deterministic on all three allocators.
Six cases cover generic type declarations and generic methods in classes,
structs, and interfaces, checking exact field, regular parameter, and body owner
IDs. Test-owned allocations are released before final assertions.

Before the production change, GCC reports `30 Tests 6 Failures`: all three
generic type cases expect owner `1` but receive `0`, and all three generic
method cases expect owner `3` but receive `0`. The preceding 24 symbol tests
pass. With the two visitor corrections, the same six cases pass.

## Verification

| Toolchain | Symbols | Facts | Calls | Complete LSP interface |
| --- | --- | --- | --- | --- |
| GCC Debug | 30/30 | 17/17 | 31/31 | exit 1, same eight frozen failure names |
| Clang ASan/UBSan | 30/30 | 17/17 | 31/31 | exit 1, same eight names plus LSan |
| MSVC static Debug | 30/30 | 17/17 | 31/31 | exit 1, same eight frozen failure names |

All nine lower-layer runs exit `0`; Clang access and leak checks remain enabled.
The complete interfaces pass the original closed-generic signature case and
both the structured receiver completion and missing-type regression from
Task 7.67. The original scope-owner heap-use-after-free no longer occurs.

The first Clang replay had `BUILD_NETWORK_LIB=OFF` and
`BUILD_THREAD_LIB=OFF`, while GCC/MSVC enable both. After enabling these options
and rebuilding, its extra network virtual declaration, auto native import, and
scheduler artifact failures all pass. Its eight remaining functional failure
names exactly match the frozen
`current-gcc-aggregate-20260906-1429/zr_vm_language_server_lsp_interface_test.log`
and both other toolchains: class fixture diagnostics, closed generic hover,
explicit exact-type failure hover, extern type hover, extern layout hover,
local symbol query, reference payload hover, and container foreach hover.

The aligned Clang run still reports `18528 byte(s) leaked in 384 allocation(s)`.
The first replay also reports a null base pointer to `qsort` in
`zr_vm_library/src/zr_vm_library/file.c:1289`; this is absent from the second
replay but remains recorded for deterministic support-layer investigation.
Neither issue is accepted as part of this owner-lifetime correction. Full
sanitizer and interface acceptance remain pending.

Build and run commands from the repository root (Linux commands run in WSL):

```text
cmake --build .codex/build-lsp-opt-gcc --target zr_vm_semantic_query_symbols_test zr_vm_semantic_facts_test zr_vm_semantic_query_calls_test zr_vm_language_server_lsp_interface_test --parallel 4
cmake --build .codex/lsp-optimize-validation/clang-asan-current --target zr_vm_semantic_query_symbols_test zr_vm_semantic_facts_test zr_vm_semantic_query_calls_test zr_vm_language_server_lsp_interface_test --parallel 4
cmake --build .codex/lsp-optimize-validation/task767-msvc-static --target zr_vm_semantic_query_symbols_test zr_vm_semantic_facts_test zr_vm_semantic_query_calls_test zr_vm_language_server_lsp_interface_test --parallel 4
cmake -S . -B .codex/lsp-optimize-validation/clang-asan-current -DBUILD_NETWORK_LIB=ON -DBUILD_THREAD_LIB=ON
cmake --build .codex/lsp-optimize-validation/clang-asan-current --target zr_vm_language_server_lsp_interface_test --parallel 4
<build>/bin/zr_vm_semantic_query_symbols_test
<build>/bin/zr_vm_semantic_facts_test
<build>/bin/zr_vm_semantic_query_calls_test
<build>/bin/zr_vm_language_server_lsp_interface_test
```

MSVC commands run through the repository's VsDevCmd wrapper and use `.exe`
suffixes. Interface runs are serialized because their fixture files are shared.
The PowerShell runner records the executable's `$LASTEXITCODE` immediately;
failure-name comparison strips only the elapsed time and finds zero differences.

## 状态与产出记录

- 开始时间：2026-09-07 07:04:52 +08:00（回归文件建立时间）。
- 实际完成时间：2026-09-07 07:18:37 +08:00。
- 状态：本 scope-owner lifetime 修复及三工具链底层回归完成；Plan 03 Task 3、
  Task 7、Task 8 和完整 sanitizer/interface 门禁继续进行中。
- 完成项目：type/method visitor 在 symbol array 扩容前保存 canonical owner ID；
  六项确定性扩容回归；原 Clang signature 崩溃路径验证；生命周期模块文档。
- 源码版本：基于 `b16b22b0` 后的共享工作树；本记录、列出的代码、测试和模块文档同提交。
  并发 core/parser/AOT 等 overlay 不属于本子项，三工具链结果不是冻结提交的最终验收。
- 产出路径：本记录、`docs/parser-and-semantics/semantic-scope-fact-ownership.md`、
  `tests/parser/test_semantic_scope_symbol_lifetime_cases.h`；本地 RED/GREEN 与完整
  interface 日志位于 `.codex/lsp-optimize-validation/task768-*.log`。
- 剩余门槛：八项 interface 功能失败、Clang 泄漏、空目录排序 UBSan 的独立回归；
  Task 3 sourceless/provider generation、Task 7 完整 consumer 矩阵、Task 8 的
  16-target、stdio/CLI smoke 和后续 native/WASM/editor/performance 验收。
