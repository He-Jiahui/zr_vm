# LSP Optimize Baseline Contract Record

## Scope

This record covers only Task 1 of
[`00-baseline-and-contract.md`](./00-baseline-and-contract.md). It freezes
evidence and adds an inventory test; it does not claim protocol lifecycle,
capability-registry, editor-correctness, or native/WASM parity completion.

## 状态与产出记录

| Time | Status | Completed items |
| --- | --- | --- |
| 2026-08-22 19:27 +08:00 | completed | Captured committed snapshot `130907d` with L8 commit `ceadabbf` as an ancestor; recorded dirty-worktree exclusion, toolchain versions, direct interface/project/advanced Unity results, CTest stdio results, extension unit/TypeScript results, the inline-value semantic failure, and the aggregate-suite build gap. Added `language_server_stdio_protocol_inventory`, which checks initialize response coverage against explicit native and WASM mapping states. |

## Open Work

- Task 2 must replace the temporary JavaScript mapping with a canonical C
  capability registry and registry unit tests.
- Task 3 must add negative JSON-RPC lifecycle/conformance coverage.
- Task 4 owns the declared identity-resolve, alias, and workspace-notification
  overclaims surfaced by the inventory.
- Task 6 must build and run the full aggregate language-server matrix, then
  replace the recorded infrastructure gap with actual suite evidence.
