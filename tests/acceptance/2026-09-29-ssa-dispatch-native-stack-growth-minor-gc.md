---
related_code:
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
tests:
  - tests/core/test_ssa_dispatch_boundaries.c
  - tests/core/test_ssa_dispatch_native_callback.inc
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/01-dispatch-boundaries.md
doc_type: acceptance-record
---

# SSA 03.01 Native callback stack growth and minor GC

## Scope

This slice characterizes a real `KNOWN_NATIVE_CALL` boundary while its native
callback grows the VM stack and runs a generational minor collection. It checks
the caller's saved PC and frame base during the callback, then checks that the
next VM load and return still read the rooted object. The ordinary `Object`
used by this fixture is promoted in place by the current minor collector, so
this acceptance does not claim that its address moves.

No production change was needed: the current dispatch, native-frame anchoring,
stack reallocation, and minor-collection paths satisfy this boundary.

## Baseline

The first fixture draft incorrectly required `metadataFunction` on the manually
installed top-level caller frame. GDB showed that field is null in this valid
entry-frame setup, so the callback returned before attempting growth. The test
was corrected to compare the PC against its fixture-owned instruction list.
The next draft also incorrectly required an ordinary `Object`'s address to
change during minor collection. `gc_cycle.c` documents that common individually
allocated objects are promoted in place; the final test checks the actual
EDEN-to-SURVIVOR transition instead.

## Test Inventory

`test_native_callback_growth_and_minor_gc_reloads_frame_and_pc` builds and runs
a three-instruction function:

1. `KNOWN_NATIVE_CALL` invokes the test native callback.
2. `GET_STACK` reads the object rooted in the caller frame.
3. `FUNCTION_RETURN` returns that loaded object.

The callback grows logical stack capacity from 64 to 128 slots, checks its
caller frame via a saved stack offset, and runs one minor collection. The test
checks the continuation PC remains at offset 1 during the callback and
collection, the rooted object reaches SURVIVOR with young-movable storage, the
next load returns that object, and execution finishes at return offset 2.
Offset 2 is expected because `FUNCTION_RETURN` executes while PC still names
that instruction; it is not a failed continuation restore.
Frame-pointer inequality is asserted only when the host allocator actually
relocates the stack allocation.

## Tooling Evidence

All compiled outputs and test logs were kept in the D-drive GCC cache
`/mnt/d/tmp/zr_vm/close-proxy-core-red`.

```bash
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --target zr_vm_ssa_dispatch_boundaries_test \
    zr_vm_execution_dispatch_callable_metadata_test \
    zr_vm_precall_frame_slot_reset_test -j 4
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_dispatch_boundaries_test
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_execution_dispatch_callable_metadata_test
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_precall_frame_slot_reset_test
ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red \
  -R '^ssa_dispatch_boundaries$' --output-on-failure --no-tests=error
```

GDB 12.1 was used with the diagnostic command file
`/mnt/d/tmp/zr_vm/dispatch_native_gc_trace.gdb` against the focused executable.
It observed stack base and caller frame base relocation, capacity 64 to 128,
PC offset 1 before and after collection, minor count 0 to 1, and object region
EDEN to SURVIVOR with young-movable storage unchanged.

## Results

- Focused dispatcher Unity executable passed 8/8 tests.
- `ssa_dispatch_boundaries` CTest passed 1/1.
- Callable-metadata Unity executable passed 18/18 tests.
- Precall-frame-reset Unity executable passed 18/18 tests.
- The GCC target build succeeded. It reported an unused-function warning for
  `query_get_bool_field` in unchanged `reflection_member_query.c`.
- The initial draft failures were fixture assumptions described above; the
  corrected test passes without a production change.

## Acceptance Decision

Accepted as a characterization of the 03.01 native callback boundary for stack
growth and generational minor collection. It proves the caller's frame and PC
remain usable across the callback, the collection, and the next VM load. The
collector currently promotes this ordinary object in place; physical GC object
relocation and major compaction remain outside this slice.
