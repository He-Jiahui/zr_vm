---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/function_aot_ir.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_aot_ir.c
  - zr_vm_core/src/zr_vm_core/function_aot_ir_internal.h
  - zr_vm_core/include/zr_vm_core/aot_ir_owned_module.h
  - zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/src/zr_vm_core/gc/gc_internal.h
  - zr_vm_core/src/zr_vm_core/string.c
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - tests/core/test_ssa_function_aot_owner.c
  - tests/core/support/ssa_function_aot_owner_fixture.h
  - tests/core/support/ssa_function_aot_owner_fixture.c
  - tests/gc/gc_test_utils.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/function_aot_ir.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_aot_ir.c
  - zr_vm_core/src/zr_vm_core/function_aot_ir_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_core/src/zr_vm_core/gc/gc_internal.h
plan_sources:
  - docs/plans/ssa
  - .codex/plans/20261008-ssa-function-owner-integration-design.md
  - .codex/plans/20261008-ssa-function-owner-tests-design.md
  - .codex/plans/20261008-ssa-function-owner-source-handoff-v2.md
tests:
  - tests/core/test_ssa_function_aot_owner.c
  - tests/acceptance/ssa-function-aot-owner.md
doc_type: module-detail
status: function-owner-functional-red-validated
---

# Function ownership of immutable Core AOT metadata

## Scope and current state

The independent [Core owner](ssa-aot-owned-module.md) already copies and rebases
the reachable AOT module schema into one uniquely owned arena. Function now has
a runtime-only owner slot and lifecycle hooks. The Snapshot entry point is
currently safe functional RED scaffolding: it clears the complete diagnostic,
returns INVALID_ARGUMENT for a NULL Function/source, otherwise returns
UNSUPPORTED, and neither allocates nor changes the owner slot. Source review is
not functional acceptance. Two actual twelve-case attempts exposed a lower
short-string allocation-size mismatch during real GlobalState shutdown. The
common allocation-size helper is repaired; a third actual run established
functional RED with three prerequisites passing and nine UNSUPPORTED feature
failures. The strict support assertion remains unchanged. GREEN publication
remains pending. See [acceptance](../acceptance/ssa-function-aot-owner.md).

This finite gate concerns metadata lifetime in actual Core Function records.
It does not attach executable entries, perform calls, check source callable
identity or install managed references. Ordinary Source_Compile retention,
default returned SSA execution, native parity and all forty-seven full SSA plan
gates remain OPEN.

## Representation and API

`function.h` forward-declares `SZrAotIrOwnedModule` and appends `ownedAotIr` to
the Function record. New explicitly initializes the slot to NULL. The new public
header declares:

```c
EZrAotIrStatus ZrCore_Function_SnapshotAotIr(
    SZrFunction *function, const SZrAotIrModule *source,
    SZrAotIrDiagnostic *diagnostic);
const SZrAotIrModule *ZrCore_Function_GetAotIr(const SZrFunction *function);
void ZrCore_Function_ForgetCopiedAotIrOwnerAliases(
    SZrFunction *copiedChildren, TZrUInt32 count);
```

All non-NULL Functions must be live, initialized by the current runtime,
current-sized and rebuilt with the current headers. Raw record headers carry no
allocation extent or schema tag that can reliably recognize older, smaller
external records. This API does not claim compatibility with those records.
The slot is runtime-only and is not serialized; artifact schema 6 and AOT
metadata ABI 17 are unchanged. Runtime modules using Function records must be
rebuilt together.

The target Snapshot protocol delegates directly to CloneOwnedModule using the
Function's unique destination slot. The source arrays remain readable and
unchanged during the call. Caller diagnostics must not overlap the source,
destination slot or old arena. Mutation and destruction require external
serialization. Successful replacement publishes a validated independent copy
before releasing the old arena; validation or allocation failure preserves the
old owner. Exact self-source is allowed because the clone is complete before
the source owner is released. These publication guarantees require the future
GREEN implementation and actual gate evidence.

Getter borrows the actual immutable owner view, or returns NULL for absent
metadata/NULL Function. The borrow ends on successful replacement, destruction
or a Function record move. Reacquire the view from the destination after a move.
No concurrent borrow lease is provided.

## Initialization, move and destruction

The large Function implementation receives only a private header and three tiny
hooks; owner behavior is isolated in the new module. The new module does not
introduce a Core-to-Parser or Core-to-Library link dependency.

| Event | Owner action | Required result |
| --- | --- | --- |
| Function_New | Explicit NULL initialization | No arena allocation |
| Function_GetAotIr | Borrow actual OwnedModuleView | No allocation or transfer |
| By-value move followed by ZrCore_Function_DetachOwnedBuffers | Tombstone forgets slot without freeing | Destination remains sole owner |
| Function_Free | Free unique arena before descendant/buffer cleanup | Slot becomes NULL and arena releases once |
| Repeated Free during GC shutdown | Observe NULL slot | No duplicate arena release |
| Fresh immediate borrowed child copies | Clear only copied owner slots | Original child/grandchild owners remain live |

The tombstone helper is also used after destruction. It must only forget the
slot: freeing at that point would destroy an arena already transferred to a
copied destination. Actual Function_Free instead releases the owner's arena
first, including a borrowed-graph parent's own arena, before resetting the
record. Borrowed child-graph flags affect descendant cleanup rather than the
parent's unique arena.

## Borrowed child shells

Module-root `zr_vm_parser/src/zr_vm_parser/compiler.c` refreshes a borrowed graph
with one memcpy of immediate child Function records. Their nested child storage
still points into the original graph. The inserted helper immediately clears
only those fresh immediate owner aliases, without allocation, free or recursion.
Recursive clearing would mutate original grandchildren and lose their unique
owners. The original graph must remain alive while nested storage is borrowed.

The helper's precondition is deliberately narrow: freshly shallow-copied
immediate records whose owner slots still alias the source. Applying it to an
actual unique owner would discard ownership and leak. Other copied buffers remain
borrowed; arbitrary copied shells cannot be independently destroyed through
Function_Free. No fallible operation is inserted into the existing by-value
move/rebind/donor-detach sequences.

## Actual Core lifecycle validation design

The twelve-case fixture starts with a heap-backed bounded module containing an
i64 constant, one function, a two-instruction block, a real frame/slot layout,
two source-map rows and a present-but-empty logical state map. Core module
validation and strict no-argument i64 admission must pass before Function
features receive any credit. A separate prerequisite proves actual independent
Clone ownership after releasing producer storage.

The lifecycle fixture uses real GlobalState_New, its main State, Function_New
and real GlobalState_Free collector shutdown. It does not replace production
Function records, GC dispatch or constructors. C-local test Functions remain
protected by the saved stop-GC flag until publication/cleanup. Inline child
shells use actual typed child storage and owner links; only real New donors are
on the GC allocation list. State is not independently freed before GlobalState.

Separate fixed ledgers record VM allocation and arena allocation. Wrapper
bookkeeping does not allocate, assert or compare freed pointer values. VM
bootstrap allocation is not faulted. The existing private owner fault OBJECT
alone supplies the deliberate one-site arena failure. Witnesses after producer
release or replacement are bounded semantic values and record IDs, rather than
freed addresses or allocator reuse assumptions.

The explicit test target links static Core, actual Core vendors/Common and Unity.
It avoids the generic Unity harness and the direct guard loop, both of which add
unneeded Parser/Library/runtime helpers. Actual source membership, compilation
commands and a fresh raw link map are required to prove that restriction. Four
profiling inputs are zero; profile output and Core bootstrap tracing are unset.
Loader/provider callbacks remain NULL. Only approved metadata and lifecycle test
operations run; no SourceExecute, Oracle, generated native program, loader,
network, FFI or security probe is part of this gate.

### Actual shutdown allocation-size investigation

The diagnostic attempt ran all twelve cases and reported 38 allocator errors
on each of ten real GlobalState shutdowns, while no VM allocations remained
live. All 380 value-only diagnostics identify a free of a 335-byte allocation
reported as 212 through 226 bytes, with matching OBJECT allocation flags.
The short-string constructor reserves `sizeof(SZrString) +
ZR_VM_SHORT_STRING_MAX`; the GC base-size helper instead used logical string
length plus one. Parser string budgeting already uses the same fixed short
capacity as construction. GC evacuation also obtains its allocation and copy
size from this base-size helper, so the repair must reconcile that common
helper with actual construction. Long-string base objects and their separately
owned native buffers retain their existing allocation protocol.

The repaired helper now reports the constructor's fixed short capacity.
Independent review also found four existing synthetic GC callers whose helper
reserved only the base header for an empty short string. That helper's STRING
minimum now reserves the same fixed capacity. Its allocator does not enforce
exact-size arguments, and its broad Library/System GC suite was not executed
by this finite Core gate.

The strict ledger is the regression observation. No error, type, size or live
allocation check is weakened to obtain functional acceptance. Finite MSVC
compilation of the new Function module, test and fixture succeeded with zero
warnings; that compile evidence does not establish lifecycle GREEN.
