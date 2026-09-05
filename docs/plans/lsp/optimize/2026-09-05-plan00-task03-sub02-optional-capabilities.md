---
related_code:
  - zr_vm_language_server/stdio/stdio_initialize.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
implementation_files:
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/04-editor-feature-correctness.md
tests:
  - tests/language_server/test_stdio_optional_capability_allocations.c
  - tests/language_server/stdio_optional_capabilities_smoke.js
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_protocol_inventory.js
  - tests/language_server/stdio_smoke.js
doc_type: milestone-record
---

# Optional 3.18 Capability Negotiation

## 状态与产出记录

| 开始时间 | 实际完成时间 | 状态 | 完成项目 | 验证结果 |
| --- | --- | --- | --- | --- |
| 2026-09-05 18:48 +08:00 | 2026-09-05 19:21 +08:00 | completed (protocol subitem; parent pending) | Independently negotiate inline completion and ranges formatting; preserve 3.17 ordinary providers; reject unnegotiated requests; validate known capability field types; handle allocation failure without leaked JSON or inconsistent flags. | RED 16/19, follow-up 4/25 and allocation RED 24 assertions reproduced. GCC/Clang/MSVC focused suites each 11/11 pass, including 25 protocol checks and control + 12 allocation fault checks. Specification and quality reviews approved. |

Start time records the first persisted regression fixture. The leaf follows
registry metadata commit `966ada6d` and does not change its descriptor table.

## Repair And Evidence

The old server advertised inline completion to every client and served both
optional methods without client support. New tests against the frozen old GCC
binary produced 3 passes and 16 failures, exit 1. The disabled-request cases
received real successful results instead of the required `-32601`; the
both-enabled request fixture already passed exact inline and formatting results.
Raw log: `.codex/lsp-optional-capabilities-red-final.log`.

The initial repair exposed a further malformed-field case: object-valued
capabilities still accepted ill-typed `dynamicRegistration`. Six additional
fixtures preserved valid dynamic/static registration and unknown extension
fields while reproducing four invalid-field failures (21/25 pass, exit 1).
Raw log: `.codex/lsp-optional-capabilities-dynamic-registration-red.log`.
The final helper accepts that known optional field only when absent or boolean.

Quality review then identified unchecked cJSON publication failures in the new
range-object branch. A focused C fixture reproduced 24 assertion failures,
exit 1, before the repair: optional flags remained enabled without successful
publication, ordinary fallback was missing, and four fault runs detected live
allocations after deleting the outputs. Its control discovers the allocation
ordinals of the range object, ranges-support value/key, parent key and inline
value/key from actual output pointers. Each site receives a transient and a
persistent failure. The first build needed the same private include directory
as adjacent stdio targets; after that harness setup correction, the recorded
RED is the actual runtime assertion failure rather than a compiler error.

The repair checks every newly introduced range allocation/attachment, deletes
unattached JSON, clears the ranges flag and attempts the ordinary boolean
fallback. Failed inline publication clears its flag as well. The C fixture's
normal control and all 12 fault cases pass on GCC, Clang and MSVC. Final
multi-toolchain replay and independent re-review are recorded below.

Initialize now stores two server-owned flags, publishes the corresponding
capabilities and dispatches optional methods using those same flags. No client
JSON pointer survives initialization. Repeated initialize is rejected before
negotiation can run again. New instances start with both flags false.

## Plan Interpretation

The stable protocol remains 3.17. Installed official protocol 3.17.5 types
declare inline completion and `rangesSupport` as 3.18 proposed additions:

- `zr_vm_language_server_extension/node_modules/vscode-languageserver-protocol/lib/common/protocol.inlineCompletion.d.ts`
- `zr_vm_language_server_extension/node_modules/vscode-languageserver-protocol/lib/common/protocol.d.ts`

The historical optimize index section 2.2 names a nonexistent
`documentRangesFormattingProvider`. The adopted rule is
`documentRangeFormattingProvider: {rangesSupport: true}` only when
`textDocument.rangeFormatting.rangesSupport` is exactly true. The ordinary
3.17 capability remains boolean true otherwise. Full snapshot assertions and
independent request fixtures test this correction. No syntax design conflicts
or language semantic changes are introduced.

## Verification

```text
cmake --build <build-dir> --target zr_vm_language_server_stdio zr_vm_language_server_stdio_server_lifecycle_test zr_vm_language_server_stdio_optional_capability_allocations_test --parallel 4
ctest --test-dir <build-dir> --output-on-failure -R '^language_server_(lsp_capability_registry|stdio_(optional_capability_allocations|optional_capabilities_smoke|protocol_inventory|protocol_conformance|resolve_capabilities_smoke|navigation_capabilities_smoke|file_operation_capabilities_smoke|document_sync_conformance|server_lifecycle|workspace_folders_smoke))$'
node --check tests/language_server/stdio_optional_capabilities_smoke.js
git diff --check -- <owned-paths>
```

| Toolchain | Configuration | Result |
| --- | --- | --- |
| GCC 11.4 | Debug shared, WSL | final build exit 0; 11/11 pass, 8.49 s |
| Clang 14 | Debug shared, WSL | final build exit 0; 11/11 pass, 9.55 s |
| MSVC 19.44 | Debug static, VS developer environment | final build exit 0; 11/11 pass, 15.97 s |

The new focused entry runs all 25 cases. Node 12 and 22 syntax checks pass.
MSVC retains existing command-line warning D9025 and unrelated long CMake
object-path warnings. Changed GCC/Clang source reports no compiler diagnostics.

WSL build directories are
`/home/hejiahui/.codex-builds/lsp-optimize-20260905-root/gcc` and `clang`;
Windows uses `E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc` with
`Invoke-VsDevCommand.ps1`. CTest `Testing/Temporary/LastTest.log` captures the
case-level output; subsequent runs may replace that generated log, so results
and RED evidence are retained here.

## Source, Outputs And Remaining Gates

Frozen source is original `c95e5387` plus exact gitlinks and this session's
owned corrections through `966ada6d`, followed by the ten source/test paths
in this leaf. Concurrent ownership/AOT commits and active semantic overlays
are excluded. The containing commit identifies the final code, tests, module
documentation and record together. Integrated acceptance still requires one
fully committed source tree after the active semantic commits arrive.

Module documentation is
[Optional LSP Capability Negotiation](../../../cli-and-tooling/lsp-optional-capability-negotiation.md).
The broad stdio smoke now explicitly opts into its optional requests but still
has earlier known semantic failures; its later assertions are not claimed as
passing. Focused fixtures reach the required retained behavior independently.
Formatter options, idempotence, semantic equivalence, grammar-based inline
completion and actual Web parity remain Plan 04/05 obligations. Complete
registry/dispatch/export inventory and Phase 00 acceptance remain pending.
