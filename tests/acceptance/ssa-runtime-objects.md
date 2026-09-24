# Runtime aggregate reconstruction

## Scope

This stage implements the object preparation part of plan 01.04 using actual
`SZrObject` allocations and the existing collector. It consumes the logical
aggregate recipes from `d9ff8388`. Runtime-only bindings map type/layout IDs and
logical field indices onto resolved prototypes/member descriptors. Bindings
never become serialized ExecIR pointers.

Prepare one shell per logical identity before connecting fields, so forward
references, shared aliases and cycles retain exact identity. Native hash maps
are prepared separately from movable shells. Uninitialized fields are absent;
an initialized-null field still has a cell. Restoration bypasses constructors,
initializers, property getters/setters and value-struct copy semantics.

Plain GC/scalar values are supported. Resource prototypes and ownership-bearing
field values fail closed because their transfer requires a joint owner/frame
commit. This stage does not complete native frame switching, automatic SROA
recipe production, inline stacks or conditional ownership cleanup.

## Source evidence

- JDK `lua/jdk/src/hotspot/share/runtime/deoptimization.cpp:1228`,
  `realloc_objects`, allocates identities before `reassign_fields` restores
  links. The existing `TestRematerializeObjects.java` tests compare restored
  field values with interpreter execution.
- CPython `lua/cpython/Lib/copy.py:172`, `_deepcopy_list`, memoizes an empty
  object before recursive restoration. `test_copy.py:test_deepcopy_memo`
  explicitly checks repeated references preserve alias identity.
- Rust `lua/rust/compiler/rustc_mir_transform/src/elaborate_drops.rs:170`,
  `drop_style`, distinguishes uninitialized/dead storage from initialized and
  conditional cleanup. Reconstruction must not infer initialization from null.
- Zr's ordinary `Value_Copy` can clone a STRUCT object. Its existing-pair
  field writer preserves exact identity for normalized plain values; fresh
  reserved cells avoid that copy path. Heap/local temporary roots use
  `LOCAL_ADDRESS`, because `FRAME_BYTE_OFFSET` scans only the VM stack.

## Validation

The first eight tests built against the failing stub and all eight failed on
WSL GCC. They demonstrated the missing real graph, GC protection, Nth-object
failure handling, ambient exception restoration, precise diagnostics and empty
graph publication. The fault wrapper compiles the production implementation
under a distinct name, replacing only selected allocation boundaries; object
storage, collection and exception handling remain the real runtime.

The ignored-registration regression failed with three original registrations
reduced to zero. Preparation now snapshots and roots the original registry and
restores it after field barriers, including indirect closure captures. Storage
for the remembered-object registry is reserved before attaching any fields.

Independent specification review found two further regressions. Throwing OOM
cleared an already active caller's nested mutator/native scopes (expected one
running mutator, observed zero); a raw resource source with neither an owner
handle nor lifecycle initialization was incorrectly accepted. Both cases were
reproduced before their fixes. The test suite also covers native-only nesting
and rejection of detached/critical native modes at this safepoint operation.

The runtime cases exercise real CLASS/STRUCT graphs, duplicate aliases,
forward references, a 257-object cycle rebuilt three times, absent versus null
fields, stale layouts, invalid field descriptors, resource/owner rejection,
overlapping outputs, non-replayed constructors/accessors, forced GC at object
and field reservation boundaries, caller-rooted post-publication collection,
each native preparation allocation, Nth object allocation, detached storage
and remembered-registry failures, old graph preservation, ambient exceptions,
and root/ignore/scope balance.

Quality review identified a missing collection pause around field attachment:
nested barriers may park even inside the marking lock. The transaction now
owns a checked pause and the marking lock through publication and heap cleanup;
remembered-registry reservation occurs inside that pause so peers cannot
consume the reserved capacity. Two guard regressions failed before the fix.
Additional tests cover pause acquisition failure, attachment exceptions, nested
caller pauses and a real competing collector started at the first barrier.
The worker must acquire the pause only after materialization returns, perform
a real full collection, and leave the rooted graph intact.

Physical relocation is not claimed by these tests. The current collector's
`gc_cycle.c:2222` minor policy and `gc_cycle.c:2735` compaction path reassign
ordinary objects' region metadata in place. An initial test assertion requiring
an address change failed while GDB confirmed that collection and graph
preparation succeeded. The final test checks the available collector behavior
and pause ordering instead; actual pointer relocation remains unexercised.

## Final validation (2026-09-24)

| Environment | Build directory | Result |
| --- | --- | --- |
| WSL GCC 11.4 Debug | `build/ssa-gcc-debug` | 5/5 suites passed |
| WSL Clang 14 Debug | `build/ssa-clang-debug` | 5/5 suites passed |
| MSVC 19.44.35228 Debug, static | `build/ssa-msvc-debug` | 5/5 suites passed |
| WSL GCC ASan/UBSan | `build/ssa-gcc-asan-phase80` | 5/5 suites passed |
| MSVC 19.44.35228 Debug, shared | `build/ssa-msvc-shared-debug` | 32/32 runtime cases passed |

The five suites contain 121 Unity cases, including 32 new runtime cases. The
sanitizer run sets `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`; it reports no sanitizer findings or leaks.

Build and run the selected suites in each existing matrix directory:

```sh
cmake --build <directory> --target zr_vm_ssa_runtime_objects_test \
  zr_vm_ssa_deopt_aggregates_test zr_vm_ssa_state_maps_test \
  zr_vm_ssa_oracle_resume_test zr_vm_aot_gc_root_frame_test -j 8
ctest --test-dir <directory> --output-on-failure --no-tests=error \
  -R '^(ssa_runtime_objects|ssa_deopt_aggregates|ssa_state_maps|ssa_oracle_resume|aot_gc_root_frame)$'
```

Windows commands run through the `using-vsdevcmd` environment wrapper. The
extra shared-library build uses `BUILD_SHARED_LIB=ON`, `BUILD_STATIC_LIB=OFF`
and runs only `ssa_runtime_objects`. Its allocation-fault copy is compiled
inside the test-enabled core DLL so private GC helpers stay private; the
ordinary API is also exercised independently in the same test executable.
Existing MSVC warning-level overrides and unrelated object-helper/path-length
warnings remain. The changed production/test sources introduce no new warnings.

Independent specification review passed after the reproduced scope/resource
findings were fixed. Independent quality review passed after collection-pause
protection and reservation ordering were corrected; the reviewer also reran
the final 32-case GCC runtime suite successfully.
