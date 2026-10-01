---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_roots.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_core/include/zr_vm_core/execution_frame_layout.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_roots.c
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_observation.c
  - tests/core/test_execution_add_stack_relocation.c
  - tests/core/test_execution_add_stack_relocation_aot_roots.inc
  - tests/core/test_aot_gc_root_frame_exception.inc
  - tests/acceptance/ssa-stack-root-frame-relocation.md
  - tests/acceptance/aot-root-frame-protected-unwind.md
implementation_files:
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/state.h
plan_sources:
  - docs/plans/ssa/04-frame-native/04-roots-observation.md
tests:
  - tests/core/test_execution_add_stack_relocation.c
  - tests/core/test_execution_add_stack_relocation_aot_roots.inc
  - tests/core/test_ssa_roots_observation.c
  - tests/core/test_aot_gc_root_frame.c
  - tests/acceptance/ssa-stack-root-frame-relocation.md
doc_type: runtime-contract
status: in-progress
---

# ExecIR frame roots and observation

The parser-owned frame-root adapter projects packed logical values to physical
frame offsets. Only explicitly classified managed, derived, or inline-field
entries are visited; uninitialized entries are skipped and scalar slots are
never scanned by accident. Derived entries retain their base slot and offset
metadata for moving collectors and stack relocation. The visitor preflights all
root metadata, then visits managed/inline roots before derived roots regardless
of map order so a moving collector can update bases first. After the derived
callback, the adapter reloads the possibly moved base and checked-recomputes the
derived slot as `base + derivedOffset`; managed/derived storage must be at least
pointer-sized.
Root-map construction and visitation both reject unknown root kinds before an
address is formed or a callback is invoked.
Construction also validates slot count/capacity relationships and required
physical and logical tables before allocating a candidate map; failure leaves
the caller's existing map intact.
Managed and derived roots are accepted only for packed REF/BOXED values, while
inline-field roots require INLINE_SPAN/BOXED storage. Scalar bit patterns are
therefore never promoted to roots solely because they resemble an address.
The base of a derived root must independently be REF/BOXED storage.

`ZrParser_ExecIr_ObserveFrame` provides a bounded materialization/writeback
boundary for debugger and deoptimization consumers. It validates the descriptor
before reading storage, copies supplied payloads only into slots classified as
scalar, returns logical values, and records physical slots whose optimization
facts must be invalidated. Reference and inline storage is never overwritten by
the scalar input channel. Invalidation capacity is preflighted before any output
changes, and reused logical values emit each physical slot only once. Missing
precise metadata is an error, not a whole-frame or whole-heap scan fallback.
Runtime GC integration and native pin lifetimes remain owned by later core
adapters.

Observation rejects malformed slot count/capacity and storage-count
relationships before changing frame bytes, writeback values, or invalidation
state. Unknown packed slot-class enum values fail in the same preflight.

The core adapter exposes the same boundary to runtime-neutral ExecBC, AOT and
JIT consumers through `ZrCore_ExecutionFrameRootMap_Build` and
`ZrCore_Execution_VisitFrameRoots`. Root-map validation checks the layout hash
and physical offsets before a callback can run. Managed roots are visited
before derived roots; after a callback updates a base, the derived address is
recomputed with checked pointer arithmetic. `ZrCore_Execution_ObserveFrame`
preflights every destination, materializes only scalar slots, returns bounded
writeback values, and reports unique physical slots to invalidate. A short
frame or capacity error leaves both frame bytes and invalidation state
unchanged. The `ssa_core_roots_observation` test covers relocation, inline
fields, scalar-looking pointer bits, and this atomic failure boundary.

## AOT root frames during VM stack growth

An AOT root-frame node is linked from `SZrState` until `ZrCore_Gc_AotRootFramePop`.
The node, root map, and root descriptors therefore use host-stable storage for that
whole interval. `ZrCore_Gc_AotRootFramePush` rejects a node that overlaps the
entire movable VM stack allocation, including its allocator-reserved extra
slots. `frameBase` may still point into the VM stack; growth rebases those
addresses by saving their byte offsets and restoring them against the new
`stackBase`. A `LOCAL_ADDRESS` base remains at its original host address. Push
also rejects a node already active in the chain, so repeated use of one node
cannot create a self-cycle.

Stack growth keeps GC stopped while stack pointers and active AOT root bases
are encoded or restored. The focused OOM fixture uses an allocator that returns
`NULL` without releasing the old block, so rollback restores the root chain and
frame bases against that still-valid allocation before restoring the prior GC
stop state. This fixture does not define failure behavior for every allocator.
On success the call-info and root-frame pointers are restored against the moved
allocation before GC resumes. A compile-time alignment assertion protects the
low-bit tag used to preserve root-chain links while an in-stack `frameBase`
temporarily contains an offset; the public frame ABI is unchanged.

`test_stack_relocation_preserves_active_aot_frame_byte_offset_root` injects a
failed stack allocation, retries with a moving allocator, and rebases two
VM-stack frame bases while preserving their nonnull chain link and a C-local
base. Its minor-GC fixture verifies the `FRAME_BYTE_OFFSET` and
`LOCAL_ADDRESS` objects remain registered in the survivor region. The fixture
uses ordinary objects, which the current minor collector reassigns in place;
it does not assert pointer rewriting. The failed-growth injection uses a
test allocator that returns `NULL` while retaining the original block; this
tests rollback with a valid old base and does not define an allocator-wide
failure contract. Separate Push tests reject a frame node inside the stack's
extra slots and a duplicate active node, checking chain/depth integrity. These checks live in
`tests/core/test_execution_add_stack_relocation_aot_roots.inc`, included by
the existing direct-only relocation target. The target has no CTest
registration. The initial 21-test failure that expected the ordinary object
root slot to be rewritten was a false RED: this collector reassigns ordinary
objects in place during minor GC, so the fixture now verifies root retention
and survivor promotion instead. Final MSVC validation rebuilt the focused
target (2/2 Ninja steps, exit 0) and directly ran 23 tests (0 failures, 0
ignored, exit 0); the preceding four-target build completed 708/708 steps.
An isolated baseline linked the pre-fix `stack.c` object as
`stack_before_fix.obj`; the map names that object as the provider for
`ZrCore_Stack_GrowTo` and contains no fixed archive `stack.c.obj`. Its direct
run reported 23 tests, 1 failure, 0 ignored: only the active VM-frame-base
migration assertion failed, while the other 22 tests passed. The fixture skips
GC and cleans up the root chain before reporting that failure. This confirms
the old-stack RED for the migration contract. GCC/Clang focused runs remain
pending. The complete command and baseline correction are recorded in
[the acceptance note](../../tests/acceptance/ssa-stack-root-frame-relocation.md).

## AOT root-frame recovery at protected exception boundaries

`ZrCore_Exception_TryRun` owns a private context whose first member is the
thread-affine `SZrExceptionLongJump` exposed through `state->exceptionRecoverPoint`.
On the C11 `longjmp` path, a local `ZrCore_Exception_Throw` restores the AOT
root-chain top and depth captured at that TryRun entry after status
normalization and before restoring GC scopes and publishing their status.
Caller execution/native/mutation depths survive a nested Throw; an empty entry
returns to inactive after releasing callback-owned recursive lock levels.
This ordering keeps a concurrent stop-the-world scan from observing
callback-local nodes after their C lifetimes end. TryRun catch repeats the
assignment idempotently; neither path traverses abandoned nodes. A volatile
callback-return flag distinguishes normal return even when Throw carries
`ZR_THREAD_STATUS_FINE`. Normal callbacks retain their existing Push/Pop
behavior. Nested TryRun scopes restore to their active outer chain.

This ordering and lifetime guarantee is for the current C11 `longjmp`
configuration. In a forced-C++ build, Throw removes callback roots before C++
stack unwinding runs destructors; destructor reentry into VM/GC while unwinding
is not covered. A native C++ exception that bypasses Throw is restored only
when TryRun catch is reached, also without a pre-catch guarantee.

The root restoration contract covers local Throw paths caught by TryRun. The
legacy worker-forward path deliberately does not access another thread's
private snapshot; this change makes no new guarantee for its existing
cross-thread longjmp behavior. The change does not generalize allocator failure
guarantees or other GC-domain unwind behavior.
Generated-AOT throw integration remains a separate Unix shared-library smoke
gate.

`tests/core/test_aot_gc_root_frame_exception.inc` supplies real C-local
Push/Throw callbacks and covers empty-chain throws, `Throw(FINE)`, a preserved
outer `LOCAL_ADDRESS` young root through minor GC, nested TryRun, and a normal
TryRun Push/Pop control. A combined case pushes a callback-local C root, forces
a real moving `Stack_GrowTo` while an outer `FRAME_BYTE_OFFSET` root is active,
then throws and verifies the relocated outer `frameBase` still retains a young
object through minor GC. Before the first fix, the focused MSVC target built
6/6 steps and the direct binary reported 9 tests with 3 expected chain-top
failures (empty, outer, nested); the original six tests passed. Those RED paths
repaired the saved caller chain before cleanup and skipped GC while it was
unbalanced. The combined relocation case was added after that RED run.
The pre-inactive ordering is verified by a GNU real-unwind wrap probe: current code passes both
empty and caller-root cases, while catch-only and HEAD code fail the same
restoration invariant. Root independently reran the frozen MSVC standalone
suite (12/12) and checked its current exception provider in the link map.
Root also rebuilt the full current native Core target (final increment 5/5),
ran the 12-case root suite with no failures, and passed the root/capability
registered CTests (2/2). Full GCC/Clang suites and generated-AOT integration
smoke remain pending. Details are in
[the protected-unwind acceptance note](../../tests/acceptance/aot-root-frame-protected-unwind.md).
