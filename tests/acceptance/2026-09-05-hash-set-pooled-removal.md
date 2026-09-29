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

## Current-source CTest registration

On 2026-09-29, the existing `zr_vm_hash_set_dense_paths_test` target was
present, but this CTest discovery query returned `Total Tests: 0`:

```text
wsl.exe --exec ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red -N -R hash_set_dense_paths
```

The target is now registered as `hash_set_dense_paths`. It was rebuilt from
current source in the same D-drive cache using CMake 3.22.1 and WSL GCC 11.4.0:

```text
wsl.exe --exec cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_hash_set_dense_paths_test -- -j4
wsl.exe --exec /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_hash_set_dense_paths_test
wsl.exe --exec ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red -N -R hash_set_dense_paths
wsl.exe --exec ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red -R hash_set_dense_paths --output-on-failure --no-tests=error
```

The build exited 0, the direct Unity executable reported 4 tests / 0 failures,
CTest discovery listed one test, and CTest passed 1/1. The four existing cases
cover dense growth threshold plus pooled, standalone, and mixed-pair removal.
This registration closes the test-discovery gap only; it does not claim the
broader map/string optimization gates in SSA plan 05.03 are complete.

## Managed-object owned-key transfer

`test_hash_set_remove_transfers_owned_key_reference` covers an ordinary,
non-resource managed object. `HashSet_Add` copies its unique input into a
shared pair key and retains one strong reference. `HashSet_Remove` returns the
pair's key slot without another retain or release. The test releases the
original owner first and confirms the returned value still owns the last
strong reference; releasing the result clears that slot. The current
implementation passed this test without a production change, so this is
contract coverage for the existing shallow transfer behavior.

The test does not define direct-unique RESOURCE mirror-slot behavior or change
the separate rule that `HashSet_Deconstruct` does not release stored key/value
contents.

```text
wsl.exe --exec cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_hash_set_dense_paths_test -- -j4
wsl.exe --exec /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_hash_set_dense_paths_test
wsl.exe --exec ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red -R '^hash_set_dense_paths$' --output-on-failure --no-tests=error
```

The focused target rebuilt with WSL GCC 11.4.0. The direct Unity executable
reported 5 tests / 0 failures, and the registered CTest passed 1/1.
