---
plan_id: optimize
task: plan03-task03-sub35
status: completed
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_scope_cache.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/language_server/test_lsp_interface.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.35: Parser-Owned Virtual Import Origin Publication

## Failure And Change

The existing relation publisher copied an imported symbol's external origin,
but left `virtualDeclarationUri` empty. A metadata provider already knew the
canonical native declaration URI, so the missing value was a producer/lifetime
boundary defect rather than an LSP navigation defect. The first focused GCC
run was RED: the parser relation regression completed 29 tests with one
failure because the virtual URI was `NULL`.

The parser semantic context now accepts an optional host resolver. During
`PublishImportOrigins`, the resolver receives the canonical external origin
and may return a metadata-owned declaration URI; the relation append boundary
clones that string into the snapshot. The compiler's script reset clears
request-scoped callbacks to avoid retaining stack user data, then explicitly
restores the callback and provider generation for the active compilation.
The analyzer stores the callback, propagates it to scoped query analyzers and
the fresh compiler context, and project analysis installs it only around the
analysis call. Both project and no-project document paths use the same wrapper,
so ordinary files cannot bypass the metadata projection.

The resolver accepts only a provider result that has a declaration and an
admitted virtual declaration URI. It therefore does not invent a URI from a
module spelling or turn a physical binary path into a declaration. Builtin and
project-scoped native descriptors are covered; binary virtual documents and
source/binary parser-origin producers remain later work.

## Verification

The focused GCC validation build is `/mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc`.
The build command was:

```text
wsl.exe -e bash -lc 'ninja -C /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  zr_vm_semantic_query_relations_test \
  zr_vm_language_server_lsp_interface_test \
  zr_vm_language_server_lsp_project_features_test -j 8'
```

The parser relation executable finished with exit 0 and `29 Tests 0 Failures
0 Ignored`. The GCC interface executable finished with exit 1, but the new
`LSP Native Import Alias Definition Uses Canonical Origin Without AST` case
passed. The runner still reports the two pre-existing failures:

- `LSP Class Member Navigation And Completion`;
- `LSP Hover And Completion Surface Explicit Exact Type Failures`.

The project-feature executable was started for baseline comparison and was
stopped after several minutes while repeatedly emitting its known circular
import fixture diagnostics; no result from that interrupted run is accepted
as a gate. A separate Clang ASan/UBSan build reran the parser target with the
same `29 Tests 0 Failures 0 Ignored` result. Its interface runner also exited
1 with the same two functional baseline failures, and reported the existing
`20144 byte(s) leaked in 422 allocation(s)` LSan result; the new case passed.
MSVC was not rerun in this slice. The parent three-toolchain and 16-target
gates remain open.

## 状态与产出记录

- Started: 2026-09-11 21:47 +08:00.
- Completed: 2026-09-12 00:14 +08:00.
- Status: completed for the parser-origin publication submilestone; parent
  Task 3 and Tasks 7/8 remain in progress.
- RED: focused parser test observed one null `virtualDeclarationUri`.
- GREEN: GCC and Clang parser relation tests are 29/29; both interface runs'
  new canonical-origin case passes, while the fixed two-case baseline remains
  exit 1.
- Outputs: callback API/lifecycle, project/no-project wiring, parser and LSP
  regressions, GCC/Clang evidence, module documentation, this record and the
  acceptance evidence.
- Remaining: binary virtual declaration URIs, source/binary origin producers,
  project summary ranges, multi-definition matrices, canonical receiver
  acquisition, full cross-toolchain/16-target acceptance and Tasks 7/8.
