---
related_code:
  - tests/parser/test_ssa_metadata_string_heap_owner.c
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/cmake/ssa-metadata-string-heap-owner.cmake
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_string_heap.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_string_heap.c
implementation_files:
  - tests/parser/test_ssa_metadata_string_heap_owner.c
plan_sources:
  - .codex/plans/20261008-ssa-function-retention-next-finite-v4.md
tests:
  - tests/parser/test_ssa_metadata_string_heap_owner.c
doc_type: test-acceptance-record
status: metadata-string-heap-owner-finite-green
---

# Actual metadata string heap tests

| Default case | Actual observation |
| --- | --- |
| actual_nine_finalized_heap_owns_exact_live_bytes | Live allocated bytes equal published rows, complete real source9 destruction balances |
| actual_eight_finalized_heap_owns_exact_live_bytes | Same independent actual source8 lifecycle |
| compact_failure_preserves_builder_then_retry | One exact allocation fault preserves full builder/reserve/source; retry copies actual row and frees old capacity |
| compacted_builder_second_compact_allocates_nothing | Already exact builder retains owner and performs no allocator call |
| empty_live_compiler_builder_compact_allocates_nothing | Actual compiler with empty independent builder performs no zero allocation |
| zero_length_reserved_builder_releases_capacity | Builder's owned reserve releases original capacity with no allocation |

There are exactly6 default RUN_TEST calls. Current304-line test SHA256 is
`292E84442465F42F363337708CFE79DC9C3DC332E413D086242470B9124108C0`.
The current actual suite is6 PASS,0 FAIL,0 IGNORE with Debug/assertions/UBSan.
Evidence is in [acceptance](../../docs/acceptance/ssa-metadata-string-heap-owner.md).

The fixture builds the real9/8 SCRIPT pipeline. The independent builder calls
the actual Add/Sort with the actual published live string; it does not fabricate
source identity or manually populate a fake metadata row. OOM tests compare
all eight reserved rows and source digest while owners are live. After retry,
only numeric old record IDs/counts are inspected; retired pointer snapshots
are not read. VM realloc records whether the allocation is new before the call.

Partial builder and fixture ownership is registered before fallible helpers.
Teardown disarms faults, frees the builder against its live compiler, clears
borrowed fields, frees fixture, restores saved GC state and frees real Global.
Post-destruction diagnostics contain independent numeric fields. The constructor
is not fault-probed, and the test does not promise recovery of arbitrary corrupt
or partial bootstrap objects.

Normal and direct registration use explicit Parser/Core/Library dependencies,
zero profiling, unset output/trace and120-second timeout. No Oracle/source or
generated-native execution, providers, network/FFI or security tests occur.
The finite four-unit MSVC compilation has0 exits/0 warnings; whole Linux/MSVC,
normal retained output and all47 SSA leaves remain OPEN.
