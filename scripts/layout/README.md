---
related_code:
  - scripts/layout/apply_moves.py
  - scripts/layout/layout_map.tsv
  - scripts/layout/generated_includes.py
  - scripts/layout/dormant_contexts.py
  - tests/cmake/source-layout-private-consumers.cmake
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_core/src/zr_vm_core/function/function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_language_server/src/zr_vm_language_server/features/lsp_code_actions.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/breakpoint/debug_breakpoint_condition.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c
  - zr_vm_parser/CMakeLists.txt
  - tests/CMakeLists.txt
  - tests/parser/test_aot_c_source_contracts.c
  - tests/tooling/test_layout_moves.py
implementation_files:
  - scripts/layout/apply_moves.py
  - scripts/layout/layout_map.tsv
  - scripts/layout/generated_includes.py
  - scripts/layout/dormant_contexts.py
  - tests/cmake/source-layout-private-consumers.cmake
  - zr_vm_parser/CMakeLists.txt
  - tests/CMakeLists.txt
  - tests/tooling/test_layout_moves.py
plan_sources:
  - user: 2026-10-12 Reorganize source directories using an audited path map while preserving public include paths and dirty work
  - .codex/plans/20261012-layout-01a13033.md
  - user attachment: 3106251b-78e5-4004-901a-6ed9ea49a0be/pasted-text-1.txt
tests:
  - tests/parser/test_aot_c_source_contracts.c
  - tests/CMakeLists.txt
  - tests/tooling/test_layout_moves.py
  - tests/tooling/test_layout_generated_includes.py
  - tests/tooling/test_layout_dormant_contexts.py
doc_type: workflow-detail
---

# Source Layout Migration Workflow

## Purpose

This workflow groups implementation files by subsystem while keeping the source text and public include contract stable. The migration is driven by a reviewed old-path to new-path table. The table, rather than hand-written move commands, defines each destination and lets the tool update quoted includes and active source-path references consistently.

The migration groups implementation files in core, parser, language server, debugger, and library code, and places the AOT parser backend under the parser module. The applied map moved all 882 files. Its 1,054 active text rewrites are supplemented by target-specific CMake metadata and source-contract include assertions. All 309 existing documentation files touched by the initial apply were restored; later prose added by another session was preserved. Grouping follows filename prefixes and module boundaries, then consumer relationships for files whose names are ambiguous. File basenames remain stable and the migration does not change runtime behavior.

## Files and ownership

`scripts/layout/layout_map.tsv` is the authoritative, two-column mapping. Each non-comment row contains an old repository-relative path and its final repository-relative path, separated by a tab. The final mapping is the input to preview, apply, and check; audit fragments and reports are supporting review evidence and are not substitutes for the consolidated table.

`scripts/layout/apply_moves.py` owns the mechanical operation. The parser integration in `zr_vm_parser/CMakeLists.txt` now points its private backend include root at `zr_vm_parser/src/zr_vm_parser/backend_aot/`. Recursive module source discovery owns production backend compilation, with `backend_aot_disabled_stub.c` explicitly removed from both static and shared parser targets. The normal module source collection in `zr_vm_common/CommonMacros.cmake` uses recursive `CONFIGURE_DEPENDS` globs; target-specific source selection still needs explicit CMake review.

## Layout and semantic placement

The grouping keeps each basename. Core source families are separated into areas such as `function/`, `call/`, `reflection/`, `artifact/`, `execution/`, `exec_ir/`, `gc/`, and `module/`. Parser families are grouped under existing and new areas such as `compiler/`, `type_inference/`, `exec_ir/`, `semantic/`, `syntax/`, and `backend_aot/`. Language-server editor features and semantic queries are grouped by their role; debugger code is divided into breakpoint, evaluation, protocol, and inspection families. Small modules that already have useful package-like organization remain as they are.

The mapping preserves filenames such as `function_frame_place.c` when moving them to `function/`. This keeps basename-based source references, debugger breakpoints, and diagnostic `__FILE__` names stable, and avoids collisions between same-named files in different modules. Directory names follow the owning feature or the file prefix. Keep the hierarchy shallow and avoid creating tiny one-file groups when a related group is available.

Some destinations require semantic review rather than prefix matching. The current parser audit places `bound_expression.c`, `place.c`, `place_queries.c`, `receiver_call.c`, and `generic_instantiation.c` under `semantic/`, while `project_imports.c` remains with compiler/project setup. `execbc_patterns.def` and `execbc_fusion_patterns.def` belong with `exec_ir/execbc/` because the latter includes the former and the public generated-header contract identifies the source definition. The AOT backend keeps its current relative layout under the parser module. These are mapping decisions, not a license to infer further moves from names alone; the reviewed TSV is authoritative.

## Public and private headers

Public include paths are compatibility paths. Core and parser headers under `include/<module>/` are installed as a directory tree, included by other product modules and tests, and in some cases named in generated AOT C text. Moving a public header into a subdirectory would therefore affect installed consumers and generated source even when the implementation itself is unchanged. The current approach keeps retained public-header paths stable.

The migration audit separates direct product consumers from the transitive include closure of retained public headers. A header with no observed direct consumer can still be required by a public header that remains installed. Candidate demotions must move into the owning module's private source tree, rewrite dependent includes relative to that private root, and give every consuming test target the required private include directory. Recheck the closure until no retained public header depends on a candidate being moved.

The applied map contains 882 rows: 176 core moves, 368 parser source/header moves, 234 AOT backend moves, 79 language-server moves, 20 debugger moves, and 5 library moves. Header visibility analysis starts from top-level installed umbrella headers as well as direct product consumers, then follows the transitive include closure. The final map retains 92 of 102 core public headers and 64 of 90 parser public headers; 10 core and 26 parser headers moved into private source trees. These counts describe this migration, not a permanent API guarantee. The audit is textual: it cannot prove ABI or undocumented source compatibility, discover every macro-generated include, or account for generated output absent from the checkout. In particular, the AOT C backend continues to emit the retained public include spellings.

Moved private headers use paths rooted at the module's private source include directory, for example `"function/function_precall_internal.h"`. Avoid parent-relative forms such as `"../gc/gc_internal.h"`. The script resolves quoted includes against the including file and the module's source and public include roots before rewriting them, so a relative include is changed only when its resolved target is in the move map.

## Applying a reviewed map

Run commands from the repository root. With no action flag, the tool previews the default `scripts/layout/layout_map.tsv`; `--dry-run` states that intent explicitly. `--map` selects a different mapping table. Review both the move list and the exact files whose include or path text would change before applying.

```powershell
$build = 'E:/cargo-targets/zr_vm/build/layout-01a13033/msvc-static-debug'
$plan = 'E:/cargo-targets/zr_vm/reports/layout-01a13033/layout-plan.json'
$env:ZR_VM_LAYOUT_TMP_ROOT = 'E:/cargo-targets/zr_vm/tmp/layout-01a13033'
$env:ZR_VM_LAYOUT_FIXTURE_ROOT = 'E:/cargo-targets/zr_vm/tests/layout-01a13033'
python -B scripts/layout/apply_moves.py --dry-run --cmake-build $build --cmake-config Debug
python -B scripts/layout/apply_moves.py --map scripts/layout/layout_map.tsv --dry-run --cmake-build $build --cmake-config Debug
```

Use Python 3.10 or newer and `-B` so running the tool does not create bytecode caches in the checkout. The configured build must have a CMake File API codemodel-v2 reply for this checkout. `--cmake-build` and `--cmake-config` select that build and configuration; the tool reads per-source compile groups and include roots, including propagated header target contexts. It resolves quoted includes in the including file's directory, mapped full and module-local aliases, and actual target contexts. Multiple actual compile-context variants are checked for consistent resolution. Ambiguous or required includes that cannot be resolved are errors; the tool does not assign guessed private roots.

`generated_includes.py` classifies each ordered compile variant independently. Only the current build's explicit `tests_generated/zr_vm_ffi_fixture_path.h` may resolve outside the checkout. The check uses file existence without reading generated contents, returns no repository-relative rewrite target for that file, and rejects unsafe ancestors or conflicting repository/generated resolutions. Other external roots are not added to the migration resolver.

For read-only checks, `dormant_contexts.py` recovers a test source's ordered roots from its literal CMake registration and the exact owner's C compile groups. A literal common-test helper is a limited fallback when its wrapper call is proved. Recovered sources and headers occupy a separate graph; they do not widen actual File API variants. Sources without complete owner evidence remain unverified, and unresolved diagnostics identify actual, dormant, or unverified context. This is a bounded declaration model rather than a CMake or preprocessor interpreter.

For a review that must remain pinned between preview and apply, save the plan JSON under the task report directory, inspect it, then pass the same file with `--apply`. The plan records the mapping hash, source-file hashes, and before/after hashes for text rewrites. The tool recomputes these inputs immediately before the first rename and rejects stale plans or files changed after preview. Atomic rewrite files belong under the registered task temporary directory; the tool defaults there and constrains `--tmp-dir` to that task root.

```powershell
python -B scripts/layout/apply_moves.py --plan $plan --cmake-build $build --cmake-config Debug
python -B scripts/layout/apply_moves.py --apply --plan $plan --cmake-build $build --cmake-config Debug
```

Apply uses filesystem renames rather than `git mv`, so it does not rewrite the user's Git index or stage unrelated edits. It validates repository-relative paths, duplicate and chained destinations, symlink/reparse-point components, source/destination conflicts, and mixed partially moved trees before changing files. Text rewrites are written atomically. The tool preserves file basenames and does not normalize untouched file bytes or line endings. Files absent from the mapping are not rewritten merely because they contain similar names; path tokens are matched on explicit boundaries.

Literal path rewriting also preserves a complete, longer file token when it resolves to a regular existing file inside this checkout and is absent from both sides of the map. An unmoved nested AOT library file can contain a suffix that resembles a mapped library path while referring to a different file. Existing mapped sources still receive their declared rewrite. Missing files, external paths, parent traversal and reparse paths receive no protection from this rule. The complete-token recognizer covers ASCII path components; it does not establish equivalent handling for spaces or non-ASCII filenames. Quoted-include resolution and File API verification remain separate and strict.

The rewrite scan covers supported text files under product modules, `tests/`, and `scripts/`, plus the root CMake and README files. Every `docs/` tree and every nested `tests/acceptance/` tree is excluded. Existing documentation keeps its original path spellings. Vendored dependencies and exactly `tests/tooling/test_layout_moves.py`, whose old-path samples are regression fixtures, are also excluded. `--check` proves only the tool's defined scan, not that every binary, external checkout, or generated file has no old path.

`--check` is read-only. After a complete apply it verifies that mapped source paths are gone, mapped destinations exist, scanned text has no permitted old-path aliases, and resolvable quoted includes are found using the modeled module roots plus any supplied File API target contexts. A nonzero result is a migration issue to investigate; it is not permission to broaden replacements blindly. Review each residue against the table and scan exclusions.

```powershell
python -B scripts/layout/apply_moves.py --check --cmake-build $build --cmake-config Debug
```

## Test and validation boundaries

The path rewrite accounts for active CMake source lists, tests that compile individual implementation files, scripts, debugger command files, and source-contract tests that read C text. `tests/parser/test_aot_c_source_contracts.c` is one such textual contract. The focused `tests/tooling/test_layout_moves.py` suite covers preview immutability, full and local quoted-path aliases, path and include rewrites, preservation of BOMs, line endings and file modes, collisions, stale plans, ambiguous contexts, ordered dormant fallback behavior, all-depth documentation and module-acceptance exclusions, exact self-fixture exclusion, and per-source File API contexts. Its 36 tests passed, including destination-path idempotence, rejection of another checkout's File API, generated-header integration, mixed-variant resolution, and preservation of real unmapped nested file tokens. Separate helper suites exercise generated-output and dormant-registration boundaries. Together, the suites ran 55 cases: 54 passed and one real Windows symlink-capability test was skipped. When a moved private header is consumed by a test, its target must receive the module-private include root. The explicit consumer table contains 60 targets in 7 root groups and 91 target/root entries. Generated C emitted by AOT continues to use public include names.

Run the focused safety suite with:

```powershell
python -B tests/tooling/test_layout_moves.py
```

The parser's recursive production source glob now excludes `backend_aot_disabled_stub.c`, so both parser library variants select the real backend. The WebAssembly and standalone SSA source collectors exclude the entire relocated `backend_aot/` directory to preserve their previous source sets. The standalone scalar-text contract continues to list its required backend support translation units explicitly. Recursive discovery makes ordinary new files available, but it does not remove these special-target boundaries automatically.

Static validation starts with `--check`, review of residual old-path references, and an include-path audit after the move. Then configure the affected build tree again so CMake refreshes recursive source discovery, build the supported static and shared variants, and run the relevant source-contract tests followed by the planned full test suite. Compare failures with the recorded pre-migration baseline so pre-existing failures are not attributed to the path change. Do not report a validation phase as passed until its command has completed and its result is recorded.

WSL distro enumeration was denied with `Wsl/EnumerateDistros/Service/E_ACCESSDENIED`, so this session has no passing Linux validation result. Native validation used CMake/CTest 3.31.6 with MSVC 19.44 and clang-cl 19.1.5, directly against this checkout and the three registered build trees under `E:/cargo-targets/zr_vm/build/layout-01a13033/`. All three final configurations completed with exit 0. Full builds retained failures; the MSVC failure identities matched their recorded pre-migration baselines, while Clang has only earlier post-migration diagnostic references. Clang missing-header build actions fell from 201 to zero; SDK and libffi failures remained.

The final complete CTest runs used `--parallel 2` and each returned exit 8:

| Configuration | Passed | Failed | Timeout | Not Run |
|---|---:|---:|---:|---:|
| MSVC static, 407 registrations | 282 | 57 | 49 | 19 |
| MSVC shared, 397 registrations | 270 | 57 | 15 | 55 |
| Clang, 407 registrations | 231 | 93 | 39 | 44 |

Static and shared introduced no ordinary failed-test state or normalized assertion identity relative to their pre-migration baselines; 46 and 15 additional timeouts remain unexplained. Clang's comparison uses an earlier post-migration run: two previously unrun SSA tests failed and 39 tests timed out. These results do not prove all configurations pass. The [final report](E:/cargo-targets/zr_vm/reports/layout-01a13033/final-report.md) contains per-test comparisons, raw evidence, strict-check diagnostics and validation limits.

## Current documentation and history

This workflow lives in `scripts/layout/` as requested. Migration edits to existing `docs/` were restored to their original content, and acceptance records retain their historical references. Later changes made by other sessions are preserved and recorded by SHA rather than reverted. The migration changes source locations, quoted include spellings, build source lists and textual source-contract expectations; it preserves unrelated dirty changes and the Git index. Reviewers can use the pinned plan, hash reports and original Git diffs in the task report directory to separate these changes.

## Plan and follow-up

The migration plan is [20261012-layout-01a13033](../../.codex/plans/20261012-layout-01a13033.md). Source moves and path metadata updates are applied, and native configure, build and full CTest commands have completed. Their nonzero build/test results and unresolved dormant or unverified include contexts remain visible in the final report. The [artifact manifest](E:/cargo-targets/zr_vm/reports/layout-01a13033/artifacts.json) records retained build trees, evidence, exact cleanup receipts and unknown shared artifacts. Review these records before reusing a build tree or treating a configuration as validated.
