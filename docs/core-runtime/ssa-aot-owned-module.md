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
tests:
  - tests/parser/test_ssa_source_aot_owned_module.c
  - tests/acceptance/ssa-aot-owned-module.md
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
the destination before freeing the old arena. Its 259-line owner TU is pinned at
`54B3179C21FEF38B1D6528CBE7D430F70FAE12139F92760B655CF188820AAD4A`.
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

The OOM branch and private checked arithmetic are implemented but have no injected
allocation-failure or direct SIZE_MAX boundary acceptance. Those need the following
independent actual-TU allocator/private-helper gate, rather than credit from ordinary
successful scalar cloning. The admitted scalar lifetime gate is GREEN; broader
schema/fault/platform/returned-program acceptance remains OPEN.

The authored 677-line test inventories all pointers, but real 9/8 inputs populate
only their actual scalar spans, have no GC header and have a present-empty logical
map. Generic multi-function/GC/CFG/deopt retention is unproved. Allocator injection,
the actual-TU OBJECT shim, private-helper arithmetic boundaries, Linux/full MSVC,
native lowering and normal returned-program retention are separate unfinished
gates. The full SSA47 goal and C/LLVM milestone remain OPEN.
