# Syntax 09 M2 guarded direct-ref acceptance

Date: 2026-08-04

## Status

- State: `proven` for M2; Gate 09 remains open at M4 and M5.
- Plan: `docs/plans/syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md`.
- Scope: `PoolRef<T>`/`PoolReadRef<T>` identity, reader/writer conflicts,
  property-reference projection, storage/escape rules, deterministic cleanup,
  and the single-validation direct-access contract.

## Contract review

- The provider publishes `PoolRef<T>` and `PoolReadRef<T>` as canonical
  `REF_LIKE` types. The compiler consumes that capability rather than comparing
  either concrete type name.
- `tryRead` and `tryBorrow` use the ordinary `out` contract. Failure leaves a
  default non-live view; success owns one guard that is released exactly once.
- Multiple readers coexist. Read/write and write/write acquisition conflicts
  fail deterministically, while recycle retires a slot without invalidating an
  already active guard.
- Writable `value` is a getter-only reference property. Nested member updates
  preserve Place identity and write back before guard close; ordinary value
  consumption does not retain the temporary reference shell.
- A `PoolRef<T>` is valid in scoped local flow and guard-moving call flow. Direct
  provider tests reject class and module/global fields, array elements and array
  captures, unconstrained generic boxing, opaque native ABI passage, closure
  capture, and uses after `await` or `yield` suspension.
- Normal block exit, return, throw, break, continue, and `out` replacement close
  the active view before the slot can be borrowed or recycled again.
- A successful borrow increments validation once. One million direct field
  updates through the acquired guard leave `handleValidationCount` unchanged.

## Direct evidence

`tests/container/test_generational_pool.c` contains the provider-specific
storage, closure, await, and yield cases plus the reader/writer and retirement
state machine. `tests/container/test_pooling_closed_type_runtime.c` executes
source-level reference projection, nested writeback, and every cleanup edge.
`tests/container/test_generational_pool_gc_stress.c` proves the no-repeat-
validation hot path. Property lowering/ref-return and type-inference suites cover
the shared compiler contracts consumed by the provider.

The fresh Gate 09 replay ran the same relevant targets under WSL GCC 11.4, WSL
Clang 14.0, and MSVC 19.44 Debug. Every toolchain passed pool lifecycle 13/13,
production closed-layout runtime 4/4, pool GC stress 3/3, property lowering
22/22, property ref-return 23/23, and type inference 122/122: 187/187 tests.
These targets are also contained in the broader 13-executable 271/271 replay.

## Review decision

The earlier upper-gate ledger described a remaining container/closure/
suspension matrix, but those direct provider cases are present and passed in all
three toolchains. No unresolved M2 behavior or memory-safety finding remains.
M2 is therefore `proven`; this does not satisfy the separate M4 consumer or M5
performance promotion gates.
