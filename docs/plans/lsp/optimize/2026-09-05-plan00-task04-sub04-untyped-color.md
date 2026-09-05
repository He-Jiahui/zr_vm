---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/04-editor-feature-correctness.md
tests:
  - tests/language_server/stdio_color_capability_smoke.js
  - tests/language_server/test_lsp_capability_registry.c
  - tests/language_server/stdio_protocol_inventory.js
  - zr_vm_language_server_extension/test/serverCapabilities.test.js
  - tests/acceptance/2026-09-05-lsp-untyped-color-withdrawal.md
doc_type: milestone-record
---

# Untyped Color Capability Withdrawal

## 状态与产出记录

| 开始时间 | 实际完成时间 | 状态 | 完成项目 | 验证结果 |
| --- | --- | --- | --- | --- |
| 2026-09-05 19:13 +08:00 | 2026-09-05 20:20 +08:00 | completed (capability subitem; parent pending) | Withdraw color declarations and both methods; delete the raw string scanner and private constants; retain ordinary string and symbol behavior. | GCC/Clang/MSVC focused each 12/12; browser unit 40/40 and configured noEmit pass; independent review found no actionable issue. |

Start time is the first retained RED evidence. The containing commit binds
the implementation, regression fixture, module documentation and this record.

## Baseline And Repair

The old native adapter interpreted ordinary `"#336699"` source strings as color
values without a compiler color type or expression fact. A presentation request
for an arbitrary identifier range also returned a successful empty array. Both
client profiles received an unconditional `colorProvider` declaration.

Before production removal, the focused fixture failed 12 of 30 checks on GCC
and MSVC: six unsupported declaration/request checks per profile. The other
18 checks passed, including exact hover contents/ranges, definition locations,
comment references, valid diagnostics and lifecycle. Raw evidence is retained
in `.codex/lsp-color-capability-red-20260905-gcc.log` and the corresponding
`msvc.log`. These are runtime assertion failures with a nonzero exit.

The repair removes the descriptor, initialize declaration, dispatcher branches,
private declarations and color-specific constants. The 401-line scanner is
deleted along with its CMake entry and obsolete snapshot-source assertion.
Explicit requests now receive the complete `-32601` MethodNotFound envelope.
Protocol snapshots and the broad smoke's current capability expectations are
updated; a dedicated fixture reaches all color assertions independently of
the broad smoke's known semantic failures.

Browser tests execute the actual transpiled worker callbacks. Empty and
color-aware clients see no color declaration or registration; hover and
definition callbacks remain available. No WASM color implementation existed.

## Contract And Plan Interpretation

This implements Plan 00's capability truth gate and Plan 04 Task 7's prohibition
on color inference from arbitrary source strings. It does not accept typed
color semantics or versioned presentation edits. Those require compiler-owned
facts and later consumer tests before capability publication can return.

Ordinary strings preserve canonical string types and literal constants. Symbol
navigation retains exact targets and ranges. This protocol removal adds no
semantic identity, query-result ownership or snapshot lifetime. No syntax
contract changes are introduced. Current contracts and the superseded June
color records are documented in
[Advanced Editor Features](../../../cli-and-tooling/lsp-advanced-editor-features.md)
and [Registry Metadata](../../../cli-and-tooling/lsp-capability-registry-metadata.md).

## Verification

```text
cmake --build <build-dir> --target zr_vm_language_server_stdio zr_vm_language_server_lsp_capability_registry_test zr_vm_language_server_lsp_source_contracts_test zr_vm_language_server_stdio_server_lifecycle_test zr_vm_language_server_stdio_optional_capability_allocations_test --parallel 4
ctest --test-dir <build-dir> --output-on-failure -R '^language_server_(lsp_capability_registry|stdio_(optional_capability_allocations|optional_capabilities_smoke|color_capability_smoke|protocol_inventory|protocol_conformance|resolve_capabilities_smoke|navigation_capabilities_smoke|file_operation_capabilities_smoke|document_sync_conformance|server_lifecycle|workspace_folders_smoke))$'
<build-dir>/bin/zr_vm_language_server_lsp_source_contracts_test
node --test test/*.test.js
node node_modules/typescript/bin/tsc -p . --noEmit
git diff --check -- <owned-paths>
```

GCC 11.4 and Clang 14 use Debug shared builds under
`/home/hejiahui/.codex-builds/lsp-optimize-20260905-root/{gcc,clang}`.
MSVC 19.44 uses the VS developer environment and Debug static build at
`E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc`.

| Toolchain | Focused result | Source-contract result |
| --- | --- | --- |
| GCC 11.4 | build exit 0; 12/12 pass, 13.85 s | exit 1, same two baseline failures |
| Clang 14 | build exit 0; 12/12 pass, 17.17 s | exit 1, same two baseline failures |
| MSVC 19.44 | build exit 0; 12/12 pass, 93.56 s | exit 1, same two baseline failures |

Each focused color entry reaches all 30 checks. Extension unit tests pass
40/40; the configured TypeScript noEmit command exits 0. These results do not
claim the separate strict worker gate or actual WASM/editor acceptance.

The frozen source remains `c95e5387` with exact gitlinks, this session's
committed corrections through `c518347e`, and the explicitly owned color paths.
All 28 exported files compare byte-for-byte between the workspace and both
validation sources. Concurrent runtime, parser and semantic overlays are not
part of this subitem's source. Full integrated acceptance must use a later
single committed tree.

The source-contract executable still reports exactly the two constructor and
super-constructor signature ordering failures in the committed Task 1 Sub02
failure ledger. The color snapshot test removal does not alter those causes;
the executable remains failed and Plan 03 owns their repair. Existing GCC
warnings in virtual documents and semantic analyzer sources, plus MSVC D9025
and object-path warnings, remain visible.

Independent read-only review found no actionable issue in the production or
test patch. It did not execute tests. The root's toolchain results provide
runtime evidence. The full semantic matrix, runtime inventory, memory tools
and native/Web acceptance remain open; no parent phase is promoted.
