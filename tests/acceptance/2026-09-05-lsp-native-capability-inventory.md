# Native Capability Inventory Acceptance

## Scope

This acceptance record covers the compiled native side of Plan 00 Task 2
Sub02. The probe uses the actual capability registry and the production stdio
dispatcher; the Node contract compares initialize output, route selection,
optional negotiation and registered CTest IDs.

## Reproduction

Build the stdio server and
`zr_vm_language_server_lsp_capability_inventory_probe`, then run:

```text
node tests/language_server/stdio_protocol_inventory.js \
  <stdio-server> <inventory-probe> <build-directory> <absolute-ctest> Debug
```

The CTest entry invokes the same command with the configured absolute CTest
executable. The runner starts profiles `3.17`, `inline-only`, `ranges-only` and
`both-3.18`.

## Baseline

Before this subitem, the source-marker inventory could not prove compiled
dispatch coverage and the first compiled run exposed the unregistered
`workspace/executeCommand` route. The command withdrawal is recorded in
[Task 4 Sub06](../../docs/plans/lsp/optimize/2026-09-05-plan00-task04-sub06-client-commands.md).
The broad stdio smoke and strict worker typecheck failures listed below are
pre-existing baseline failures, not acceptance failures introduced here.

## Test Inventory

- Registry and native dispatch probe: 30 descriptors, 43 routes, 13 token types.
- Negotiation profiles: `3.17`, `inline-only`, `ranges-only`, `both-3.18`.
- Negative mutations: metadata, route ownership, duplicate/orphan rows, token
  legend, encoding, capability shape and CTest registration.
- Lifecycle and withdrawal checks: initialization, shutdown, withdrawn methods
  and client-owned `workspace/executeCommand` requests.
- Cross-toolchain focused matrix: the same 14 CTest IDs on GCC, Clang and MSVC.

## Tooling Evidence

The isolated WSL builds use GCC 11.4 and Clang 14; the Windows build uses MSVC
19.44 under the Visual Studio developer environment. CTest is passed by an
absolute path discovered during CMake configuration (`/usr/bin/ctest` for WSL,
`D:/Tools/development/cmake/bin/ctest.exe` for MSVC). JavaScript syntax checks
run with the repository Node.js runtime. The probe replaces only handler bodies;
method selection remains compiled production dispatcher code.

## Acceptance Result

The compiled inventory reports 30 registry descriptors and 43 native routes.
Three descriptors are intentionally metadata-only controls. Every profile
reports `native-contract-mapped`; `orphaned` and `failures` are empty. The
mutation checker rejects 26, 26, 27 and 27 deliberately invalid cases in the
same profile order. GCC 11.4, Clang 14 and MSVC 19.44 each pass the focused
14-test matrix, including the inventory test.

The initialize snapshot contains 13 semantic token types and the single
`declaration` modifier. The optional inline-completion and range-formatting
fields are absent unless the corresponding client capability is negotiated.
Withdrawn methods and `workspace/executeCommand` return the exact `-32601`
MethodNotFound envelope.

Detailed commands, RED evidence and source boundaries are in the
[Plan 00 milestone record](../../docs/plans/lsp/optimize/2026-09-05-plan00-task02-sub02-compiled-native-inventory.md).

## Results

All three toolchains pass the focused 14-test matrix. Direct inventory output
is `native-contract-mapped` for every profile, with empty `failures` and
`orphaned` arrays. The configured CTest command is present in generated test
metadata, and all deliberate mutation cases are rejected. No production
semantic or syntax code changed.

## Open Gates And Owners

| Failure or remaining gate | Owner / next plan | Status |
| --- | --- | --- |
| WASM export and worker route mapping | Plan 05 Tasks 1-4, with the Web bridge owner | Pending; native strings are not treated as linked-export proof |
| Control and notification behavior | Plan 00 Task 5 and Plan 01 lifecycle work | Pending; three controls remain metadata-only in this inventory |
| Generic completion detail in broad stdio smoke | Plan 03 completion consumer and its linked compiler/query leaf | Existing baseline failure; focused inventory is unaffected |
| Missing `possibly_uninitialized_read` in diagnostic smoke | Plan 03 Task 6 diagnostic producer/projection | Existing baseline failure; not promoted by this record |
| 17 strict worker type diagnostics outside extension tsconfig | Plan 05/06 Web parity and packaging gate | Baseline/current delta is zero; explicit strict gate remains open |

The native inventory does not promote Plan 00, Plan 05 or any later phase. It
only closes the compiled native subitem and supplies the machine-readable
boundary for the next WASM and semantic work.

The follow-up Web static inventory is recorded in
[Plan 00 Task 2 Sub03](../../docs/plans/lsp/optimize/2026-09-06-plan00-task02-sub03-wasm-static-inventory.md).
It reports 30 CMake runtime exports, 28 bridge calls and 22 worker routes, and
the Web legend now matches the 13 native token types plus `declaration`. Its
optional linked-asset mode remains pending: after the validation-only overlay
was supplied with another session's uncommitted `semantic_scope_facts.h`, the
serial Emscripten build was system-killed at `execution_dispatch.c` under memory
pressure and produced no linked asset.

## Acceptance Decision

Accepted for Plan 00 Task 2 Sub02's native inventory scope. Plan 00 remains
pending until control/notification behavior, linked WASM exports and worker
routes, and the existing semantic baseline gates are separately verified.
