# LSP Untyped Color Withdrawal

## Scope

Plan 00 Task 4 Sub04 removes the untyped native color scanner, capability
declaration and request routes. Ordinary strings, symbol navigation and
comments preserve their existing semantic contracts.

## Baseline

The regression fixture failed 12/30 on GCC and MSVC before production changes.
It observed color publication and successful color request responses where
the planned unsupported capability requires MethodNotFound. The retained
18 semantic/lifecycle checks already passed.

## Test Inventory

Two client profiles test exact capability absence, documentColor rejection,
four colorPresentation ranges, diagnostics, string and identifier hover,
exact definition ranges, comment reference absence and clean shutdown.
Adjacent capability, optional allocation, protocol, document sync and workspace
tests protect retained behavior. Browser tests execute worker callbacks.

## Tooling Evidence

GCC 11.4, Clang 14 and MSVC 19.44 builds exit 0. Focused CTest suites each pass
12/12, including all 30 color checks. Extension Node tests pass 40/40 and the
configured TypeScript noEmit exits 0. Source-contract executables on all three
toolchains retain exactly the two constructor signature ordering failures in
the Task 1 Sub02 committed failure ledger. They are not counted as passes.

Exact commands, source composition, paths and review results are recorded in
[the milestone record](../../docs/plans/lsp/optimize/2026-09-05-plan00-task04-sub04-untyped-color.md).

## Results

Registry, initialize, dispatch, constants, private declarations and CMake agree
that the color scanner is unavailable. No stale production path remains.
Independent review found no actionable issue; runtime checks were performed
separately by the root. Frozen code/test copies match all 28 exported paths.

## Acceptance Decision

The color capability withdrawal is accepted. Plan 00 as a whole remains open,
and no later phase is promoted. Full semantic behavior, compiled runtime
inventory, WASM/editor parity and memory/performance acceptance remain pending.
