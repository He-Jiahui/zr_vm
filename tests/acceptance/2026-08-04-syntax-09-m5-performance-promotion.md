# Syntax 09 M5 performance promotion acceptance

Date: 2026-08-04

## Status

- State: `proven`; M1-M5 are proven and Gate 09 is promoted.
- Plan: `docs/plans/syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md`.
- Scope: allocation-event counts, scan/pause work, high-volume handle/direct
  access, churn, and separate thread-local/concurrent observations.

## RED evidence

The new performance target first failed while compiling its five uses of
`SZrPoolStats.slabAllocationCount`. The public stats contract had only current
`slabCount`, so it could not state a cumulative successful slab-allocation
event count. The compiler diagnostics named the missing field; no unrelated
test or link failure was involved.

## Implemented contract

- `slabAllocationCount` is appended to `SZrPoolStats`, preserving offsets of
  every existing field. It increments only after slab storage and slot metadata
  both allocate successfully and the slab is attached to the pool.
- The target creates separate GcFree and GcMapped pools with 65,536 live
  elements, 256 slots per slab, and eight scan passes. A per-item baseline uses
  65,536 independently allocated class-storage blocks containing an actual
  `SZrRawObject` header plus the same payload.
- Allocation events, scan visits, scan bytes, and handle-validation counts are
  deterministic assertions. Wall-clock ticks are emitted for investigation but
  are not compared: scheduler and instrumentation variance changes their order
  across platforms.
- Thread-local non-atomic and concurrent atomic modes each execute and report
  one million handle validations. The existing multi-worker test remains the
  correctness proof for concurrent mutation; this matrix keeps performance
  observation distinct from that correctness test.

## Deterministic matrix

| Measure | GcFree slab | GcMapped slab | Per-item class storage |
|---|---:|---:|---:|
| live elements / allocations | 65,536 / 256 slabs | 65,536 / 256 slabs | 65,536 / 65,536 blocks |
| eight-pass visits | 0 | 524,288 | 524,288 |
| eight-pass bytes | 0 | 16,777,216 | 109,051,904 |

The per-item byte figure is an explicit object-storage traversal baseline, not
a fabricated full VM collection. Existing M3 tests separately prove real
external GC trace/rewrite, barrier cards, full/minor collection, and compact-safe
managed values.

## Existing stress evidence retained

- `zr_vm_generational_pool_test`: one million validate/reject operations,
  identity/ABA/exhaustion/alignment, and four-worker concurrent churn.
- `zr_vm_generational_pool_gc_stress_test`: partial initialization, separated
  GcFree/GcMapped/GcBarriered accounting, one million direct-field operations
  without repeat validation, and 100,000 recycle/reuse cycles.
- M2/M3 focused acceptance retains guard cleanup, deferred exactly-once Drop,
  canonical TypeLayout admission, and compact/barrier evidence.

## Verification

- WSL GCC 11.4 Debug performance matrix: 2/2.
- WSL Clang 14 Debug performance matrix: 2/2.
- MSVC 19.44 Debug static performance matrix: 2/2.
- Fresh final WSL GCC production pool: 14/14.
- Fresh final WSL GCC GC stress: 3/3.
- Fresh final WSL GCC performance matrix: 2/2.
- `git diff --check`: clean before staging.

## Review decision

M5 no longer substitutes a smoke result for unlike responsibilities. Allocation,
GcFree, GcMapped, per-item traversal, thread-local validation, concurrent-mode
validation, direct hot access, churn, and multi-worker correctness have separate
evidence. No timing threshold can introduce a platform-dependent promotion.
Gate 09 is promoted.
