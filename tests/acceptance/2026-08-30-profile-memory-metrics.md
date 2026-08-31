# M2 Task 1 Profile Memory Metrics

## Scope

The runtime profile now exposes allocation, value-copy, barrier, collection,
mark/rewrite, promotion, raw-array, member-cache, and GC scan-byte counters.
Counters are enum-indexed, disabled by default, and use relaxed atomic updates
for worker-visible memory events. A bounded pause ring and JSON report output are
available under `ZR_VM_PROFILE_MEMORY=1`.

## TDD Evidence

- RED: the focused profile test did not compile before the memory enum, runtime
  fields, and recording APIs existed.
- GREEN: `tests/core/test_value_construction_profile.c` contains 11 focused
  tests covering disabled recording, stable names, atomic accumulation, bounded
  pause storage, managed allocation, value-copy bytes, raw-int fast hits, full
  collection/scan bytes, and helper compatibility.
- `tests/benchmarks/scripts/hotspot_summary.py` now emits event-normalized
  allocation-bytes-per-allocation and scan-bytes-per-marked-object rates.

## Implementation Boundaries

The pause ring currently records concurrent-major remark and old-compaction
phases; minor and outer non-concurrent/full pauses remain a follow-up. Allocation
bytes currently cover GC-managed raw objects created through the raw-object and
region constructors, not auxiliary `GcMalloc` payloads or hash pairs. Derived
hotspot rates are event-normalized, not benchmark-operation-normalized. These
limits are intentionally reported rather than treated as complete process-wide
accounting.

## Verification

- The previously built focused profile binary passed 9 tests with 0 failures;
  the expanded 11-test source was syntax-checked and its relink was blocked by
  concurrent shared-tree builds, so no newer binary pass is claimed here.
- Strict GCC syntax checks passed for profile, object, member-access, and GC
  modules; MSVC-compatible atomic code remains guarded by `_InterlockedExchangeAdd64`.
- Python compilation and benchmark report consumer regressions passed.
- The raw-array canonical-storage regression covers three-element generic
  materialization, avoiding a power-of-two capacity assertion.

## Decision

Accepted as the observational M2 Task 1 slice. The counters are sufficient to
rank the next representation optimization, while the explicit accounting limits
remain exit conditions for a future GC instrumentation pass.
