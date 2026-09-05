# Hash Set Pooled Removal

## Scope

The compiler integration fragmentation benchmark aborted when native array
removal passed a pooled hash-pair interior address to the allocator. The
allocation/removal implementation in `hash_set.h`, `hash_set.c`, and container
`module.c` is unchanged from baseline `c95e5387`; this prerequisite is separate
from the ownership pending-control repair.

Gdb against the existing old GCC integration binary confirmed
`object == &set->pairPoolTail->pairs[0]` immediately before the invalid free.
Calling only the benchmark test from a fresh process reproduced that path.
The value was a string with ownership kind NONE.

## Regression and Fix

`tests/core/test_hash_set_dense_paths.c` now checks pooled-only, standalone-only,
and mixed collision chains. Its real-allocator wrapper records exact pair
allocation bases and sizes, rejects invalid frees, and checks immediate
standalone release plus balanced final pool teardown.

The existing private pool-membership helper moved from `hash_set.c` to the
header's forced-inline implementation area. Removal skips individual frees
for pooled cells. Deconstruction uses the same helper and releases their
owning pool blocks. Public signatures, pool allocation cursors, and returned
key semantics are unchanged.

## Evidence

- RED: `.codex/logs/ownership-astra-hashset-red.log`, 4 tests, 2 failures.
  Pooled-only removal attempted 3 invalid frees; mixed removal attempted 2.
  Standalone removal and existing dense growth passed.
- GREEN GCC: `.codex/logs/ownership-astra-hashset-gcc-green.log`, 4/4 passed.
- GREEN Clang: `.codex/logs/ownership-astra-hashset-clang-green.log`, 4/4 passed.
- Both GCC and Clang strict C11 syntax checks of `hash_set.c` completed with
  no diagnostics after the shared helper move.

The test objects were compiled with the old GCC cache's recorded compile
commands, substituting Clang for the second test object. Both binaries linked
the existing old GCC runtime archive and harness objects from
`zr_vm_resource_shared_weak_test`. This directly verifies the changed inline
removal under both compilers, but is not a full Clang runtime rebuild.

All outputs stayed under `/home/hejiahui/.cache/zr-ownership-astra-gcc`.
The final GCC/Clang/sanitizer caches and frozen source were not changed by this
subtask. Full integration replay and MSVC validation remain with the parent
task and are not claimed here.

## Integrated Clang Replay

The parent subsequently rebuilt the complete Clang14 static Debug runtime
from the recorded task snapshot revision. Direct execution reports hash-set
4 Tests/0 Failures/0 Ignored and compiler integration127 Tests/0 Failures/
0 Ignored, both exit0. The former GC-fragment abort is gone without suppressing
or selecting away any compiler integration test. GCC/MSVC and final sanitizer
results continue in the parent Astra acceptance record.
