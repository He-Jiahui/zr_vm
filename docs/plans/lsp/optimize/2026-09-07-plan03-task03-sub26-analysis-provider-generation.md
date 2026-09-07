---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
tests:
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_semantic_external_provider_generation_cases.h
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.26: Analysis Provider Generation

## Contract

The compiler previously published generation zero for every external member,
even when its host knew the provider epoch used for analysis. Add an explicit
`SZrSemanticContext.externalProviderGeneration` input. The host sets it before
metadata projection and analysis. New and reset contexts initialize it to zero,
which continues to mean unavailable. A host must rebuild the semantic context
to adopt another provider epoch; changing the input does not update old facts.

The call, callable-value, and non-call member producers copy this input into
their external reference facts. Read-only queries project the published value.
The compiler's script reset preserves the explicitly supplied input while
discarding the previous facts. A host must rebuild the semantic facts when its
admitted provider set changes; consumers neither restamp existing facts nor
derive an epoch from metadata names.
Metadata owner, tokens, signature hash, target kind, and snapshot-local IDs keep
their existing meaning. This input represents the provider set admitted to one
analysis; it does not invent a per-module version or load-order identity.

This is a producer prerequisite. The LSP's provider counter currently has no
production reload caller. Wiring host epochs, cache invalidation, actual binary
and descriptor reloads, and multiple projects remains a dependent milestone.

## Reference Evidence

- `lua/roslyn/src/Compilers/Core/Portable/Compilation/SemanticModel.cs:17`
  binds semantic questions to a tree in a particular Compilation, and line 45
  exposes the owning Compilation. ZR likewise keeps facts in their analysis
  context; it carries an explicit numeric provider epoch in the existing API.
- `lua/rust/compiler/rustc_middle/src/ty/typeck_results.rs:32` defines the owner
  relative to which its local IDs are interpreted. Its query accessors validate
  that owner. ZR retains local SymbolId/TypeId semantics and carries external
  metadata identity separately across snapshots.

## Verification

The focused regression compiles actual native module-link calls and
callable values for epochs 0, 9, and 4294967305. It checks exact `SymbolAt` and
`ExternalReferences` epochs, stable metadata identity across independent
compilations, all three publication paths, and the reset-to-unavailable boundary.

MSVC RED: 36 existing tests passed and the new case failed with `Expected 9 Was
0`. The first producer change exposed a second reset boundary: `compile_script`
discarded the supplied epoch before publishing facts. Preserving the explicit
input across this internal reset makes the complete symbols runner pass 37/37
on GCC, Clang ASan/UBSan, and MSVC. Calls pass 32/32, relations 29/29, and the
76 source contracts pass on every toolchain. LSP parity passes 20/20 on every
toolchain. All five final executables exit 0, including Clang with ASan, UBSan,
and leak detection enabled.

The final shared-source validation includes peer Plan 01 Task 6 Sub10's binary
IO cleanup and Sub11's parity-fixture ownership fixes. Before those changes,
Clang parity passed all 20 functional cases but exited 1 with 5069 bytes in 41
allocations, then 544 bytes in four allocations. Its final clean run is
`.codex/task326-final-clang-parity-with-cleanup.log`. Those peer changes are not
part of this commit. Full plan acceptance still requires one committed source
version and the complete prescribed matrix.

Validation uses `.codex/build-lsp-opt-gcc`,
`.codex/lsp-optimize-validation/clang-asan-task773`, and
`.codex/lsp-optimize-validation/task767-msvc-static`. Targets are
`zr_vm_semantic_query_symbols_test`, `zr_vm_semantic_query_calls_test`,
`zr_vm_semantic_query_relations_test`,
`zr_vm_language_server_semantic_query_parity_test`, and
`zr_vm_language_server_lsp_source_contracts_test`. Build each target with
`cmake --build <build> --target <targets> -j 6`, then execute each binary;
Clang uses `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. MSVC builds use `Invoke-VsDevCommand.ps1`.
Logs are `.codex/task326-*-build.log`, `.codex/task326-msvc-red.log`, and
`.codex/task326-final-<compiler>-<target>.log`.

The original GCC directory retained an obsolete `semantic_type_use.c.o` whose
Ninja dependency record had zero headers. GDB showed its old context layout
reading the reference-array address as `0x1`; deleting this generated object
and rebuilding removed both symbols and parity SIGSEGVs. The MSVC linker also
reported an invalid COFF section in `module_init_analysis.c.obj`; rebuilding
that generated object resolved it. These build artifacts are not source fixes.

The production edits add context configuration and forward it at existing
publication sites. The large compiler and publication files gain no new
responsibility; the regression is a separate header included by the existing
symbols runner. Only the four-line generation hunk belongs to this milestone
in `compiler.c`; unrelated call-binding hunks remain with their owner.

## 状态与产出记录

- 开始时间：2026-09-07 23:44 +08:00。
- 实际完成时间：2026-09-08 00:39 +08:00。
- 状态：Task 3.26 已完成；Plan 03 Task 3、7、8 整体门槛保持未完成。
- 完成项目：宿主代际输入、调用及成员事实捕获、script reset 输入保留、三工具链查询与 LSP 窄门禁。
- 源码基线：`eacdee1f` 加共享工作区已有修改及上述 Sub10/Sub11 验证支持，仅提交本项拥有的代码、测试、文档。
- 下一步：在真实 LSP 分析及 provider reload 生命周期接入代际，补齐缓存失效和跨项目矩阵。
- 剩余门槛：实际 provider reload、多项目和无源码 virtual URI；完整 16-target、stdio/CLI 与最终平台验收。
