# Syntax 12 M3 Acceptance: Job/Scheduler Runtime

## Scope

This acceptance covers only Syntax 12 M3 from
`docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md`. It
establishes the cold `zr.task.Job<T>` handoff and local cooperative
`Scheduler` contract. It does not claim ThreadScheduler, Send/Sync,
cross-domain transfer, duration-provider, artifact/AOT, debug/LSP, or legacy
`TaskRunner` migration coverage.

## Accepted Contract

- The `zr.task` native descriptor exports canonical Job/Scheduler protocol
  masks, generic signatures, and stable member contract roles without using
  legacy TaskRunner or coroutine names as evidence.
- `init Job<T>(callable)` creates a cold single-consumption Job.
  `Scheduler.schedule(job)` consumes it and returns the Task completion
  handle; a second source submission is rejected by canonical ownership
  facts.
- `currentScheduler`, `yieldNow`, and `delay` share the Task completion ABI.
- Parser semantic facts preserve a native resolved member's contract role and
  ownership qualifier even when native metadata has no source `SymbolId`.
- A standalone Task expression is rejected by the Task Handle protocol; it
  must be awaited, returned, or stored.

## RED To Green

The initial focused test allowed the same Job source to be submitted twice and
accepted a discarded Task expression. Native metadata also produced a
contract-role fact with no source `SymbolId`, which the call-fact publisher
incorrectly treated as unresolved. The completed implementation recognizes a
non-zero native contract role as an exact resolved call fact, consumes only
argument zero for the canonical Scheduler schedule role, and publishes the
Task must-use compiler error from the Task Handle protocol.

## Validation Evidence

The final source was an isolated `c497860 + M3 exact staged overlay` snapshot
with initialized read-only submodules. Each toolchain built its own directory
from that identical snapshot.

| Toolchain | Job/Scheduler target | Process result |
|---|---:|---|
| GCC 11.4 | 5/5 | exit 0 |
| Clang 14 | 5/5 | exit 0 |
| MSVC 19.44 | 5/5 | exit 0 |

GCC also built the isolated CLI target and ran `zr_vm_cli --help` with exit 0.
The test output intentionally contains the duplicate-consume and discarded
Task compiler diagnostics; those are the negative assertions, while Unity
reports 5 tests and 0 failures on every toolchain.

The snapshot retains unrelated warnings in legacy TaskRunner/await descriptor
initializers, parser helper functions, and a zrm const-qualified call. M3's
new yield/delay descriptor rows explicitly initialize `dispatchFlags`; no M3
specific warning remains in the final replay.
