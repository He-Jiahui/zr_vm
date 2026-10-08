---
related_code:
  - zr_vm_core/include/zr_vm_core/aot_ir_owned_module.h
  - zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_core/src/zr_vm_core/aot_ir.c
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - tests/parser/test_ssa_source_aot_owned_module.c
  - tests/parser/test_ssa_aot_owned_module_faults.c
  - tests/parser/support/ssa_aot_owned_module_fault_shim.h
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-source-execbc-vm.cmake
implementation_files:
  - zr_vm_core/include/zr_vm_core/aot_ir_owned_module.h
  - zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_core/src/zr_vm_core/aot_ir.c
plan_sources:
  - .codex/plans/20261007-ssa-core-owned-module.md
  - .codex/plans/20261006-ssa-core-scalar-owner-design-v2.md
  - .codex/plans/20261007-ssa-owner-fault-gate.md
tests:
  - tests/parser/test_ssa_source_aot_owned_module.c
  - tests/acceptance/ssa-aot-owned-module.md
  - tests/acceptance/ssa-aot-owned-module-faults.md
doc_type: module-detail
status: finite-core-owner-lifetime-green
---

# Independent Core AOT module ownership

## Purpose and current implementation

The AOT IR is a structural representation with borrowed C array views and numeric
semantic references. Parser's projection descriptor does not independently retain
all those arrays. The owner interface provides the location for an independent
Core lifetime: consumers can retain a module after its descriptor,
projection, compiler, fixture and runtime state have been released.

**The frozen production implementation now performs independent copying.** Clone
validates the source, measures a checked arena, allocates once, copies and rebinds
the complete reachable schema, validates its candidate and atomically replaces
the destination before freeing the old arena. The historical 259-line lifetime
owner TU was pinned at
`54B3179C21FEF38B1D6528CBE7D430F70FAE12139F92760B655CF188820AAD4A`.
Its lifetime commit is `78ce8caf737ed7d0a4f1dce3e377e1bd2b0904b8`.
The current 272-line TU adds only conditional private fault-test forwarding
symbols; the following section records its new pin and separate fault gate.
Actual reviewed r3 proves finite lifetime GREEN: three selected no-argument
CTest suites passed 52 Unity cases (descriptor20, scalar22, owner10), with zero
failures/ignored and no observed UBSan diagnostic. The run reused the r3 binaries
after preserving/reviewing 17 Parser warnings; it did not rebuild or suppress
warnings. The independent MSVC r3 evidence is two-TU compile-only proof.

Historical RED used the validation stub: valid input returned UNSUPPORTED (14)
without allocation or destination writes. Actual r1 passed configure/build and
two prerequisites, then failed both Clone assertions (expected 0, observed 14).
That RED is committed as `511513d459ad4425ff0d0622444f28f137d078ab`.

The implementation includes only the Core public header and CRT headers. It introduces no
Core-to-Parser dependency. Parser and scalar emitters are test consumers.
See [acceptance](../acceptance/ssa-aot-owned-module.md) for evidence status and
[test guide](../../tests/acceptance/ssa-aot-owned-module.md) for the finite modes.

## Public API and caller obligations

The public header exposes an opaque `SZrAotIrOwnedModule` and these `ZR_CORE_API`
functions; its layout is private to the owner translation unit.

```c
EZrAotIrStatus ZrCore_AotIr_CloneOwnedModule(
    const SZrAotIrModule *source, SZrAotIrOwnedModule **destination,
    SZrAotIrDiagnostic *diagnostic);
const SZrAotIrModule *ZrCore_AotIr_OwnedModuleView(
    const SZrAotIrOwnedModule *owner);
void ZrCore_AotIr_OwnedModuleFree(SZrAotIrOwnedModule **owner);
```

Initialize a destination slot to NULL. The interface permits NULL or a unique live
owner produced by successful Clone. Source and every advertised array extent must remain readable and unchanged
through the entire call. Arithmetic checks cannot establish memory readability.
Destination must be a writable pointer slot. That slot and optional diagnostic
storage must not overlap source records/pools, the old arena, or each other.
There is no Init API, shallow-copy ownership, reference counting or concurrency
contract. Arbitrary pointers are not valid owners.

View(NULL) returns NULL. For a live owner, View borrows its inline module until
Free or successful replacement; it does not allocate or validate. Free(NULL) and
Free(&nullOwner) are no-ops. Free of a unique live owner releases its base once
and clears the caller's slot. Never individually free interior arrays or map
headers. Existing StateMapFree/GcMapFree own separately allocated storage and are
inappropriate for these arena interiors.

Clone is structural ownership. Module validity does not establish an executable
ABI or authorize execution. `ZrCore_AotIr_RequireExecutableAbi` remains a separate
existing check; UNKNOWN callable ABI can be structurally valid. Pure scalar C/LLVM
buffer admission remains the emitter's separate finite profile.

## Schema and complete required copy inventory

The current AOT schema version is 7 and target ABI version is 1. Module target,
execution contract, hashes and flags are inline. Function contract, callable ABI,
frameLayout, identities, hashes and relocation count are also inline. Constants,
primitive layouts, source coordinates and deopt records contain numeric facts;
their semantic references are IDs/ranges. Pointer addresses are storage views,
not ABI hashes or serializable semantic references.

The implementation visits the complete inventory below using explicit actual
record types for sizeof/alignof. General schema coverage in the copy traversal
does not establish acceptance for every nonempty combination of those pools.

| Level | Pointer and matching count |
| --- | --- |
| Module | functions/functionCount; constantPool/constantCount; layoutPool/layoutCount |
| Function | frameSlots/frameSlotCount; valueSlotPool/valueSlotCount |
| Function | blocks/blockCount; instructions/instructionCount |
| Function | operandPool/operandCount; resultPool/resultCount |
| Function | phiIncomingPool/phiIncomingCount; successorPool/successorCount |
| Function | memoryTokenPool/memoryTokenCount; sourceMaps/sourceMapCount |
| Function | gcRootPool/gcRootCount; deoptStates/deoptStateCount |
| Function | deoptValuePool/deoptValueCount; deoptAggregates/deoptAggregateCount |
| Function | deoptAggregateFields/deoptAggregateFieldCount |
| Present gcMap | One SZrExecIrGcMap header; entries/entryCount; slotIndexPool/slotIndexCount; inlineRefOffsetPool/inlineRefOffsetCount |
| Present logicalStateMap | One SZrExecIrStateMap header; entries/entryCount; valuePool/valueCount; rootPool/rootCount; ownerStatePool/ownerStateCount |

Preserve absent versus present-empty optional headers. A non-NULL empty logical
map retains functionToken, signatureHash and generation. GC headers retain
safepointId, sourceId and rootRange. Preserve every live record, range, source
coordinate and inline fact. Copied map capacities equal copied counts; spare
capacity is not semantic content. Zero-count array views become NULL, even when
the source has an allocated empty view. Do not copy capacity-tail bytes.

Every pointer in a copied header must be rebased explicitly. No descriptor.owner,
compiler, semantic context, fixture, projection opcode array or projection
slotValues pointer belongs in the owner: none is reachable through this AOT schema.
The CONSTANT instruction's layoutId is its constant-pool index; it must not be
confused with the primitive layout record's id.

## Arena geometry and publication

The implementation uses one CRT allocation. Its first aligned object is the
private owner containing an inline module; the functions, constants, layouts,
all function pools and present map headers/pools follow in that same arena.
Owner and map headers must never be temporary stack objects behind published
pointers. Measurement and placement share one checked geometry traversal in the
same order: owner, module arrays, then each function's table-order arrays, GC map
and pools, and logical map and pools.

Geometry uses size_t without narrowing totals to TZrUInt32. Check multiplication
before forming `count * width`; calculate alignment padding without unchecked
`offset + align - 1`; check padding and final addition separately against SIZE_MAX.
Width is positive and alignment is a positive power of two. Use actual record
alignments, with compile-time assertions that they fit `alignof(TOwnedModuleMaxAlign)`;
pointerSize and a guessed eight-byte alignment do not define arena alignment.
Zero-count arrays consume no storage or padding; present-empty headers do.
No arbitrary count cap replaces checked arithmetic. Nonzero spans must have
aligned starts and disjoint exact extents inside the measured allocation.

`TOwnedModuleMaxAlign` is private: double under _MSC_VER, max_align_t otherwise.
The Microsoft-ABI branch matches the actual MSVC cstddef and clang 19
__stddef_max_align_t.h definitions; MSVC C did not expose max_align_t in the first
compile attempt. All per-record alignment assertions remain active. This is a
toolchain-specific ordinary-alignment definition, not removal of the alignment gate.

`owned_module_checked_span` validates its cursor/offset arguments, positive width,
power-of-two alignment and the ordinary-alignment upper bound before zero-count
handling. Failure preserves cursor and offset. Zero count reports the current
offset and consumes no bytes/padding. `owned_module_copy_span` uses a local void*
slot, so it never writes a typed pointer field through an incompatible void**.
`owned_module_layout` runs the same traversal for measurement and placement.

The sequence rejects invalid arguments, validates the source using existing Core
validation, measures, allocates, zeroes/copies/rebases a private candidate,
then validates that candidate using existing Core validation. All source reads
and candidate validation precede publication. On success, save the old owner,
publish the new owner, and free the old base. On any failure, preserve destination
identity, contents and lifetime; free only an allocated failed candidate.

Source may be the old owner's View or a caller module borrowing its arrays.
Exact self-source success must allocate a distinct arena while old storage is
still live, fully copy it, then replace/free old storage. A no-op success would
not prove replacement. Saved old views become invalid after that replacement.
Cloning between owners must leave the new owner usable after freeing the first.

## Errors and unfinished gates

NULL source/destination returns INVALID_ARGUMENT. Source/candidate validator
failures and diagnostics are propagated unchanged. Measurement/placement failure
or measured/filled size disagreement returns INVALID_RANGE; a failed candidate
is released. Failed allocation returns appended OUT_OF_MEMORY (15), with diagnostic
expected=required arena bytes and actual=0. StatusName returns `out_of_memory`;
existing values, including UNSUPPORTED=14, retain their values. The owner-created
diagnostic is zeroed before status/expected writes. NULL diagnostic is supported.
Success returns OK after publication and old-arena release.

The OOM branch and private checked arithmetic now have a separate actual-TU
allocation-failure and numeric boundary gate described below. Scalar lifetime
and the seventeen-case private fault gate are GREEN; broader schema/platform/
returned-program acceptance remains OPEN.

The authored 677-line test inventories all pointers, but real 9/8 inputs populate
only their actual scalar spans, have no GC header and have a present-empty logical
map. Generic multi-function/GC/CFG/deopt retention is unproved. Linux/full MSVC,
native lowering and normal returned-program retention remain unfinished gates.
The full SSA47 goal and C/LLVM milestone remain OPEN.

## Private allocator and geometry gate: finite GREEN

The current owner source is 272 lines, SHA-256
`312E94D857FE5B74610EF4C4894B55A65E2AF0648ABB0DDCE76FD605984EC3AC`.
Under `ZR_TEST_SSA_AOT_OWNED_MODULE_FAULTS`, two private symbols forward directly
to the real checked-span helper and return `alignof(TOwnedModuleMaxAlign)`.
They are absent from ordinary production objects and the installed public header.
No geometry formula or production allocation policy was replaced.

The 24-line shim reads real CRT/owner declarations before redirecting malloc/free.
Only the independent actual-source owner OBJECT receives the forced include and
private define. The 588-line Unity test includes it with NO_REDIRECT so its wrapper
bodies call CRT allocators. Parser, fixture, Core validator, support and ordinary
owner objects retain their normal allocators. No copied production .c or global
allocator hook is used. Link-map evidence must prove fault calls resolve solely
from the shimmed object, rather than the unshimmed Core archive member.

The authored 17-case inventory targets three single-site OOM/retry phases
(initial 9, independent 9-to-8 replacement, exact self-source), two zero-allocation
Core refusals, numeric checked-size/alignment boundaries and status stability.
Its fixed eight-record ledger observes only owner-TU direct allocator calls,
tracks request bytes/failure/free/live counts, and performs no heap bookkeeping.
Assertions disarm injection first; explicit release proves zero live owners and
one free for each successful arena before teardown fallback.

OOM expects one failed request with positive measured bytes, an otherwise zero
diagnostic containing OUT_OF_MEMORY and expected bytes, unchanged old/source
observations, zero free during failure and same-byte successful retry. Self retry
uses a value-only witness and never dereferences its invalidated old owner view.
Geometry tests call the real private helper with numeric SIZE_MAX arguments;
they allocate no huge arrays/arenas and independently test multiply, padding and
extent overflow, exact fit, zero count, invalid/null inputs and actual bound B
versus 2*B. Status checks target every old value/name 0..14, OOM=15 and unknown=999.

Actual MSVC fault r1 compiled the shimmed owner and test TUs with exit 0 and zero
warnings; receipt `FAC6B16062AA3474B41E0BF35DF67A67F871DD055EAF4F46CD13DA6A68C79951`
is compile-only. Direct r2 configure/build/CTest naturally exited 0: one suite,
17/17 passes, zero ignored, no UBSan diagnostic. Its receipt SHA-256 is
`6E153DAC65CAE4AB75EDA9D5569AEF5CAD3F4853F85598E26279A4E175A71700`.
Compilation confines the shim to its owner OBJECT; Root independently read the
pinned raw map to verify all three owner APIs and both private wrappers resolve
from that object. Source/header/settings/map/binary hashes remained unchanged.

R1 configure/build succeeded, but 231 warnings stopped its strict driver before
CTest and six foreign Parser inputs changed during that build. Its failed receipt
is preserved and receives no runtime credit. R2 reused the same build directory,
freshly configured current inputs and incrementally rebuilt; zero new warnings
do not establish whole-project warning acceptance.
See [fault acceptance](../acceptance/ssa-aot-owned-module-faults.md) and
[fault test guide](../../tests/acceptance/ssa-aot-owned-module-faults.md).
Normal registration is present but unrun. Candidate-corruption failure cleanup,
OS memory exhaustion, nonempty general maps/CFG/deopt/multiple-function retention,
Linux/full MSVC, default returned retention and full47 remain OPEN.
