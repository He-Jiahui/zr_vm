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
plan_sources:
  - .codex/plans/20261007-ssa-core-owned-module.md
  - .codex/plans/20261006-ssa-core-scalar-owner-design-v2.md
tests:
  - tests/parser/test_ssa_source_aot_owned_module.c
  - tests/acceptance/ssa-aot-owned-module.md
doc_type: module-detail
status: finite-core-owner-genuine-red
---

# Independent Core AOT module ownership

## Purpose and current implementation

The AOT IR is a structural representation with borrowed C array views and numeric
semantic references. Parser's projection descriptor does not independently retain
all those arrays. The owner interface provides the location for an independent
Core lifetime: consumers should eventually keep a module after its descriptor,
projection, compiler, fixture and runtime state have been released.

**Current implementation is a validation stub.** Clone rejects NULL source or
destination, invokes the existing Core module validator, propagates its failures,
and returns `ZR_AOT_IR_UNSUPPORTED` (14) for valid input. It performs no allocation
and never writes the destination slot. Its own argument/unsupported diagnostics
are zeroed and then receive the returned status; a NULL diagnostic is accepted.
It has no successful independent-copy path. Actual r1 proved finite genuine RED:
configure/build and two prerequisites passed; both isolated feature cases failed
only at the Clone-success assertion (expected 0, observed 14). This proves the
missing feature with working prerequisites, not successful ownership.

The stub includes only the Core public header and CRT headers. It introduces no
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

Initialize a destination slot to NULL. The full interface contract permits NULL
or a unique live owner produced by successful Clone; the stub currently produces
none. Source and every advertised array extent must remain readable and unchanged
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
inappropriate for future arena interiors.

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

The following table is the frozen implementation contract, **not implemented by
the stub**. Each span uses its actual `sizeof(*source.field)` and element alignment.

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

## Planned arena geometry and publication

The frozen design specifies one CRT allocation. Its first aligned object is the
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
alignments, with compile-time assertions that they fit `alignof(max_align_t)`;
pointerSize and a guessed eight-byte alignment do not define arena alignment.
Zero-count arrays consume no storage or padding; present-empty headers do.
No arbitrary count cap replaces checked arithmetic. Nonzero spans must have
aligned starts and disjoint exact extents inside the measured allocation.

The intended sequence rejects invalid arguments, establishes safe geometry and
validates the source, measures, allocates, zeroes/copies/rebases a private candidate,
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

Currently NULL source/destination returns INVALID_ARGUMENT; source validation
failure and diagnostic are propagated unchanged; valid input returns UNSUPPORTED.
The current enum ends at UNSUPPORTED and has no OUT_OF_MEMORY member. Future
production work may append OUT_OF_MEMORY and its StatusName case while preserving
existing numeric values. The frozen design assigns arithmetic overflow to
INVALID_RANGE and failed arena allocation to OUT_OF_MEMORY, expected=required
bytes and actual=0. Neither allocation failure nor geometry behavior is implemented
or accepted in this phase.

The authored 677-line test inventories all pointers, but real 9/8 inputs populate
only their actual scalar spans, have no GC header and have a present-empty logical
map. Generic multi-function/GC/CFG/deopt retention is unproved. Allocator injection,
the actual-TU OBJECT shim, private-helper arithmetic boundaries, Linux/full MSVC,
native lowering and normal returned-program retention are separate unfinished
gates. The full SSA47 goal and C/LLVM milestone remain OPEN.
