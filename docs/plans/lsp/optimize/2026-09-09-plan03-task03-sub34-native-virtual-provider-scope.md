---
plan_id: optimize
task: plan03-task03-sub34
status: completed
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_virtual_document_identity.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_virtual_document_identity.h
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_virtual_documents.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_virtual_documents.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_document_links.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_cross_snapshot_references.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_cross_snapshot_references.h
tests:
  - tests/language_server/test_lsp_native_virtual_provider_scope_cases.h
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/lsp_native_virtual_fixture.h
  - tests/language_server/test_lsp_virtual_document_identity_cases.h
  - tests/language_server/test_lsp_compile_tool_projection_cases.h
  - tests/language_server/test_lsp_multi_project_provider_generation_cases.h
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/stdio_smoke.js
  - zr_vm_language_server_extension/test/virtualDocuments.test.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.34: Native Virtual Provider Scope

## Evidence And Design

Source baseline: `444b22fa`, with the existing shared worktree overlay preserved.
Plugin definition still returns its DLL/SO file URI. Bare native virtual URIs
contain only a module name, so they cannot bind two projects' same-named providers.

The new two-project fixture starts with int and float `answer()` providers.
It requires exact rendered declaration ranges, distinct URIs, alternating reads,
real reload, rejection of both old-generation URIs and fresh declarations.
GCC RED has the prior seven failures plus this new failure, exit 1:
`round=0 project=0: definition must publish one virtual declaration`.
Commands: `cmake --build /home/hejiahui/.codex-builds/l8-callable-value-gcc
--target zr_vm_language_server_lsp_project_features_test --parallel 16`, then
the matching `bin/zr_vm_language_server_lsp_project_features_test` executable.
Logs: `.codex/lsp-optimize-validation/plan03-task03-sub34-red{,-build}-gcc.log`.

The URI encodes a module path and one structured JSON query containing exact
project URI, provider origin URI and decimal-string provider generation. cJSON
handles serialization/parsing; the existing core StringBuilder owns temporary
encoding buffers. The JSON query is URI-escaped as one component. This choice
was checked with the installed `vscode-uri` library: normal key/value query
separators change encoding during `URI.parse(uri).toString()`, while the single
JSON component round-trips reserved path characters and the full uint64 value.
The JSON spelling remains canonical to reject lossy decoding, including NUL.

Scoped reads must validate project and the current nonzero generation before
loading that project's descriptor, then compare the actual provider origin.
Builtin native URIs retain their established unscoped shape. Identity strings
are GC-owned; descriptor rows are borrowed only from the live selected provider.
This URI identity is not a replacement for canonical SymbolId/TypeId or metadata
tokens, and does not admit stale semantic facts.

## Reference And Ownership Evidence

Migrating the old physical-plugin URI assertions exposed three reference
regressions. The `query-project-gcc` and `validation-project-gcc` runs each had
ten failures, comprising the prior seven and these three:

- LSP Descriptor Plugin Member Completion Definition And References.
- LSP Descriptor Plugin Type Member Navigation.
- LSP Native Virtual Documents Preserve Project And Generation.

Two GDB traces break at the native candidate resolver. The first shows an outer
non-NULL LSP context becoming NULL at the receiver call. The second, after fixing
that call, shows a module FUNCTION query sent to the type-member scan, returning
false with an empty candidate. Native field/method initialization also discarded
context. Logs: `plan03-task03-sub34-reference-{context,candidate}-gdb.log` under
the validation directory. Commands use `gdb -q -batch <GCC project executable>`,
break `lsp_semantic_query.c:1075` conditional on native-plugin source kind, then
`step`, `finish` and print the context/candidate/query, with a short backtrace.

Reverse virtual module-member references now read canonical ExternalReferences
through the existing metadata identity validator. The owning project's URI and
exact projected range select the declaration. Receiver acquisition propagates
context to its native metadata provider. In `owner-red-project-gcc`, all three
reference regressions pass, including exact source document links.

That run separately proves missing-project-provider leakage: after loading one
project's provider, another project without a plugin can render a forged URI
using that registered origin. The new negative test fails, for eight total
failures. Both URI production and consumption now require the existing
project-local plugin loader to confirm ownership; ambient registry state cannot
prove an origin belongs to a project. Unassociated global plugins remain
unresolved at this boundary. The final positive test detaches the source AST
during reverse references, then restores it before further requests/cleanup.

## Validation

All logs have prefix `.codex/lsp-optimize-validation/plan03-task03-sub34-`.
The affected target set is project features, semantic-query parity, interface,
semantic snapshot, source contracts and stdio. Final build command:
`cmake --build <build> --target zr_vm_language_server_lsp_project_features_test
zr_vm_language_server_semantic_query_parity_test
zr_vm_language_server_lsp_interface_test
zr_vm_language_server_lsp_semantic_snapshot_test
zr_vm_language_server_lsp_source_contracts_test zr_vm_language_server_stdio
--parallel 16`.

Build roots are GCC `/home/hejiahui/.codex-builds/l8-callable-value-gcc`, Clang
`/home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang`, and MSVC
`E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc-current` with `--config Debug`
through `Invoke-VsDevCommand.ps1`. Direct test executables are under each
`bin` directory, without a Windows `Debug` subdirectory. Clang tests use
`ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1`.

Full smoke command: `node tests/language_server/stdio_smoke.js
<build>/bin/zr_vm_language_server_stdio <build>/bin/zr_vm_cli` (Windows `.exe`).
Clang smoke alone retains the established sanitizer-only 1 GiB ceiling via
`ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES=1073741824`; production remains 512 MiB.
Extension commands are `npm run test:unit` and `npx tsc --noEmit -p ./` in
`zr_vm_language_server_extension`.

All three final builds exit 0. Final direct runner results:

| Check | GCC | Clang ASan/UBSan/LSan | MSVC |
| --- | --- | --- | --- |
| Semantic-query parity | 34/34, exit 0 | 34/34, exit 0 | 34/34, exit 0 |
| Semantic snapshot | exit 0 | exit 0 | exit 0 |
| Source contracts | exit 0 | exit 0 | exit 0 |
| Project features | 57 PASS / 7 FAIL, exit 1 | 57 PASS / 7 FAIL, exit 1 | 57 PASS / 7 FAIL, exit 1 |
| Interface | 114 PASS / 2 FAIL, exit 1 | 114 PASS / 2 FAIL, exit 1 | 114 PASS / 2 FAIL, exit 1 |
| Full stdio smoke | exit 0 | exit 0 | exit 0 |
| Stdio peak bytes | 38985728 | 719388672 | 48066560 |
| Stdio peak MiB | 37.18 | 686.06 | 45.84 |

Both new scope cases pass on all three compilers, including AST-detached reverse
references. Extension unit 43/43 and noEmit exit 0. Logs are
`final-{build,parity,snapshot,contracts,project,interface,stdio}-{gcc,clang,msvc}.log`,
`extension-unit.log` and `extension-noemit.log` under the stated prefix.

All three project runners retain these exact baseline failures:

- LSP Auto Discovers Project From Source File.
- LSP Imported Constructor And Meta Call Infer Through Module Type.
- LSP Relative And Alias Import Literal Navigation And Hover.
- LSP Network Native Members Semantic Tokens Cover Chain And Receivers.
- LSP Semantic Tokens Cover External Metadata Members.
- LSP Semantic Tokens Cover Native Value Constructor Members.
- LSP Pooling Hover Completion And Projection Expose Guard Contract.

All three interface runners retain `LSP Class Member Navigation And Completion`
and `LSP Hover And Completion Surface Explicit Exact Type Failures`.
Clang project LSan is now **18968 bytes / 473 allocations**, not the prior
19160/481 count. Interface LSan remains **20144 bytes / 422 allocations**.
These are real exit-1 results; no aggregate feature or memory gate is accepted.

## 状态与产出记录

- Started: 2026-09-09 12:33 +08:00.
- Completed: 2026-09-09 13:35 +08:00.
- Status: completed for this provider-scope submilestone. Both REDs, GDB evidence,
  three-toolchain positive/negative regression results and full failing runner
  baselines are recorded. All command sessions ended before completion.
- Outputs: scoped URI codec/provider resolution, virtual lookup and project
  admission, canonical reverse module-member references, context propagation,
  document links, project/codec/protocol/extension regressions and module docs.
- Source: `444b22fa` plus only the scoped implementation/tests recorded above;
  code, tests and documentation are committed together. Shared peer changes and
  the user-owned `astra.md` input are excluded from staging.
- Documentation: this record, the original Plan 03, optimize index, and
  `docs/parser-and-semantics/lsp-semantic-resolution-and-native-imports.md`.
- Remaining: binary virtual documents, parser origin producers, project-module
  summary ranges, canonical receiver migration, multi-definition matrices,
  the full 16-target gate and native/WASM/desktop-Web acceptance remain open.
