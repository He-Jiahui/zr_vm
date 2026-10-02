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
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_roots.c
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_observation.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/state.h
plan_sources:
  - docs/plans/ssa/04-frame-native/04-roots-observation.md
tests:
  - tests/parser/test_ssa_roots_observation.c
  - tests/acceptance/2026-10-02-ssa-parser-inline-root-fields.md
  - tests/core/test_execution_add_stack_relocation.c
  - tests/core/test_execution_add_stack_relocation_aot_roots.inc
  - tests/core/test_ssa_roots_observation.c
  - tests/acceptance/2026-10-02-ssa-inline-root-bounds.md
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
Root specifications count reference locations, independently of the number of
logical packed values. One INLINE_SPAN value can therefore contribute several
inline-field roots at different offsets, including a pointer ending exactly at
the containing span boundary. Each specification still resolves its value and
physical slot, checks the root kind and storage class, and bounds the field.
The duplicate key remains `(valueId, kind, fieldByteOffset)`; repeating that key
is rejected. Allocation failure and later validation failures preserve the
existing map through candidate construction.
Managed and derived roots are accepted only for packed REF/BOXED values, while
inline-field roots require INLINE_SPAN/BOXED storage. Scalar bit patterns are
therefore never promoted to roots solely because they resemble an address.
The base of a derived root must independently be REF/BOXED storage.

The parser regression uses `LayoutPackedFrame` to construct one real
INLINE_SPAN with three pointer fields. It checks their distinct callback
addresses and values, the final field boundary, skipping an uninitialized
field, and unchanged map storage after duplicate, out-of-span, unknown-value
and unknown-kind failures. The existing managed/derived, relocation and
observation cases remain in the same test, with always-active checks under
`NDEBUG`. See the
[parser inline-field acceptance](../../tests/acceptance/2026-10-02-ssa-parser-inline-root-fields.md).
This adapter test does not establish source-language GC or safepoint/codegen
integration.

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

For an inline field, `frameByteOffset` and `byteSize` describe the containing
span, while `fieldByteOffset` locates a pointer inside that span. The root
visitor bounds the complete span from `frameByteOffset`, then bounds the
pointer-sized inline-field access from the computed field address. Managed
and derived roots retain their descriptor-sized access bounds from that
computed address, including existing legal nonzero offsets. An inline field
ending exactly at the frame boundary is valid; its address does not require another
complete span after it. Checked address overflow reports a frame-bounds
diagnostic with a defined zero offset and does not invoke the callback.

The core regression covers this final-field boundary, a one-byte-short frame,
out-of-span fields, an uninitialized root, a stationary root, null writeback,
unaligned host byte storage, malformed slot alignment, empty maps and null
visitor inputs. Its checks remain active when `NDEBUG` is defined.

The non-inline regression separately checks managed and derived roots whose
pointer alone would fit but whose complete access span exceeds the frame;
both reject before callback. Their legal nonzero-offset accesses still pass.
Native pin leases and source-language GC/codegen integration are outside this adapter
fixture; a stationary callback result does not establish a pin lifetime.
Scoped build and test evidence is recorded in the
[inline root bounds acceptance](../../tests/acceptance/2026-10-02-ssa-inline-root-bounds.md).

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
Generated-C shared-library throw integration is a separate Linux-only gate in
`tests/parser/test_aot_c_shared_library_throw_root.inc`. A test-only allocator
decorator delegates every request to the original allocator with its original
arguments. For the fixture's string payload, `NormalizeThrownValue` checks
whether the value is already an Error without allocating, then creates the
Error with `ZrCore_Object_New`; the observer latches that first `OBJECT`
allocation request before delegation and samples the root chain only after
the original allocator succeeds. It requires the generated THROW source slot
to contain the exact fixture payload and records the generated root-frame and
frame-base identities plus the mapped live object identity. The observed
`LOCAL_ADDRESS` map must point to a live object in the collector list, distinct
from the manually registered outer caller root. The built-in `object` source
spelling compiles on both current Linux toolchains. The emitted entry must
directly call the generated throw helper after `PrepareStaticDirectCall`; that
helper's unique payload and metadata are checked during Error normalization.
Flushed phase markers qualify compilation, emission, linking and execution.

The first qualified runs retained the outer object, but automatic entry GC
aged it from zero to two and promoted it before the requested final collection.
The final fixture therefore completes a real full GC after restoring the
caller's saved VM/native frame window. The collection kind and counter must
confirm completion, the restored caller C root must retain a live object and
an unrooted control object must be released. This tests retention even after
normal promotion, with chain top/depth restored and the external C root popped
during cleanup. It leaves automatic GC enabled. GCC 11.4 and Clang 14 each
passed the dedicated `ssa_aot_generated_throw_root` CTest (1/1) and their
direct core root suites passed 12/12. This does not
prove that the collector consumed or rewrote the generated map or collected
while the generated frame was active. All generated artifacts remain under
the D: cache's `tests_generated/aot_c_shared_library_throw_root` directory for
owned post-run cleanup.

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
registered CTests (2/2). The bounded generated-AOT integration smoke passed
on GCC and Clang; full GCC/Clang suites remain outside this record. Details are in
[the protected-unwind acceptance note](../../tests/acceptance/aot-root-frame-protected-unwind.md)
and [the generated-AOT integration note](../../tests/acceptance/ssa-generated-aot-throw-root.md).
