---
plan_id: optimize
task: plan01-task06-sub09
status: completed
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/module_init_analysis.c
  - zr_vm_parser/src/zr_vm_parser/compiler/module_init_analysis.h
  - zr_vm_parser/src/zr_vm_parser/compiler/module_init_binary_metadata.c
  - tests/parser/test_binary_metadata_source.c
  - tests/language_server/test_lsp_cast_operand_facts.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
doc_type: plan-record
---

# Plan 01 Task 6 Sub09: Binary Metadata Source

## 状态与产出记录

- Started: 2026-09-07 23:49 +08:00
- Completed: 2026-09-08 00:09 +08:00
- Source: a3e043fb plus shared working-tree changes.
- Scope: parser binary metadata loading and its parser/LSP regressions.
- Status: binary metadata identity fix accepted; parent full gates remain open.
- Outputs: binary IO module, decoder/API deletion, parser/LSP tests and module docs.
- Remaining gates: Clang full-smoke peak memory, imported compile-time source leaks
  and the separate project-import signature-count assertion.

## Evidence and Plan

The Linux full stdio smoke fails binarySeed diagnostics and navigation for CLI
artifacts written with --intermediate. The direct-writer fixture passes. GDB
confirms the member call producer executes, but the imported member already has
zero metadata token, signature token and signature hash. Both cast and plain
calls fail the same way.

The file loader correctly chooses graph_binary_stage.zro. The parser IO adapter
then preferentially reads its .zri sidecar and creates a synthetic IO source,
which lacks artifact identities and uses intermediate-text declaration positions.
This path also accepts stale .zri content without validating it against the .zro.

Add direct identity tests with absent, current and stale intermediate files.
Replay the same cases through canonical LSP call queries and navigation. Replace
the sidecar path with the existing structured binary reader, remove the obsolete
text decoder and its unused path API, and move binary IO ownership into a small
module. Validate GCC, Clang sanitizers, MSVC, Valgrind and full protocol smoke.
Parent plan and sanitizer gates remain open until separately accepted.

## RED

GCC focused CTest failed both targets. The no-intermediate parser case passed;
the current-intermediate case changed metadata token 50331649 to zero, and the
stale-intermediate case returned the wrong export name. LSP plain/cast binary
and native controls passed, while both intermediate-file cases lacked canonical
symbol identity. Logs: .codex/lsp-optimize-validation/plan01-task06-sub09-gcc-red-*

## Implementation

Removed the sidecar text decoder and synthetic source allocation/free path.
The IO adapter and matching destructor now live in module_init_binary_metadata.c
and use the core structured reader and source destructor. The path API had no
other caller and is removed. module_init_analysis.c loses about 1,200 lines;
its remaining summary-analysis responsibilities are unchanged in this slice.

The module remains larger than the usual file-size guideline because this change
only removes its obsolete decoder responsibility. Splitting summary construction,
effect traversal and SCC validation is a separate boundary; this fix adds no code
to the remaining large file.

## Validation

All logs use `.codex/lsp-optimize-validation/plan01-task06-sub09-`.

| Check | GCC 11.4 Debug | Clang 14 ASan/UBSan/LSan | MSVC Debug |
| --- | --- | --- | --- |
| Binary source regression | 3/3, exit 0 | 3/3, exit 0 | 3/3, exit 0 |
| LSP plain/cast identities | 4/4, exit 0 | 4/4, exit 0 | 4/4, exit 0 |
| Focused CTest | 2/2 | 2/2 | 2/2 |
| Manifest / IO lifetime | 3/3, exit 0 | 3/3, exit 0 | 3/3, exit 0 |
| Extended project import | 34/35, exit 1 | 34/35 and LSan, exit 1 | 34/35, exit 1 |
| Original full stdio smoke | exit 0, 36.98 MiB | final peak-memory assertion, exit 1 | exit 0, 44.91 MiB |

Commands use each build's existing compiler configuration:

```text
cmake --build <build> --target zr_vm_binary_metadata_source_test zr_vm_language_server_cast_operand_facts_test zr_vm_test_manifest_roundtrip_test zr_vm_project_import_canonicalization_test zr_vm_language_server_stdio zr_vm_cli_executable -j 10
ctest --test-dir <build> -R "^(binary_metadata_source|language_server_cast_operand_facts)$" --output-on-failure
<build>/bin/zr_vm_test_manifest_roundtrip_test
<build>/bin/zr_vm_project_import_canonicalization_test
node tests/language_server/stdio_smoke.js <build>/bin/zr_vm_language_server_stdio <build>/bin/zr_vm_cli
```

GCC build: `/home/hejiahui/.codex-builds/lsp-plan01-task06-sub03-gcc`.
Clang build: `/home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang`.
MSVC build: `E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc-current`, invoked
through `Invoke-VsDevCommand.ps1`; Windows executable names include `.exe`.
WSL Node is the configured 22.13.1 toolchain.

GCC Valgrind uses `--leak-check=full --show-leak-kinds=all
--errors-for-leak-kinds=all --error-exitcode=99`. Parser source regression:
4,432 allocations and frees; LSP regression: 605,968 allocations and frees.
Both exit 0 with zero remaining bytes and zero errors.

The extended import failure is the same on all three toolchains:
`test_project_compile_records_using_import_guard_dependencies` expects five
SIGNATURE records but gets six. GDB stops at its line 2004 and confirms zero
binary metadata adapter calls during that source-only case. Clang additionally
reports 650,787 bytes in 2,665 allocations in this extended executable, including
failure-path fixture ownership and an unreleased source in its direct roundtrip
test. These are not accepted as passing checks.

Clang full smoke passes all preceding protocol assertions and reaches line 4174,
where peak RSS is 711,630,848 bytes (678.66 MiB), above the unchanged 512 MiB limit.
Because this assertion follows shutdown but precedes explicit exit, this run does not certify
normal full-smoke sanitizer teardown. No memory limit or sanitizer option was
relaxed. The standalone focused sanitizer tests remain clean.

## Review and Commit Boundary

Independent read-only review found no actionable production defect and confirmed
that all three consumers still close their streams once and release completed
graphs. A P3 documentation claim about empty inputs was corrected: only a
callback-free empty IO is rejected by the adapter; callback-backed empty input
retains the core decoder's error behavior.

Stage only this subtask's parser source/header, tests, CMake fragment and include,
module documentation, plan record and plan/index entries. Other shared core,
parser, generation-publication and call-binding changes are not part of this
commit. The user-owned astra.md is not staged.
