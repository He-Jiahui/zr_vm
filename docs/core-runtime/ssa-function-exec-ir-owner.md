---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/include/zr_vm_core/function_exec_ir.h
  - zr_vm_core/src/zr_vm_core/function_exec_ir.c
  - zr_vm_core/src/zr_vm_core/function_ir_owners_internal.h
  - zr_vm_core/src/zr_vm_core/function_aot_ir.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/function_exec_ir.h
  - zr_vm_core/src/zr_vm_core/function_exec_ir.c
  - zr_vm_core/src/zr_vm_core/function_ir_owners_internal.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
plan_sources:
  - .codex/plans/20261009-ssa-function-execir-owner-design-v1.md
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
tests:
  - tests/core/test_ssa_function_exec_ir_owner.c
  - docs/acceptance/ssa-function-exec-ir-owner.md
doc_type: module-detail
status: finite-green-verified-commit-pending
---

# Function-associated Core ExecIR ownership

## Association API and validation boundary

The proposed runtime association appends an opaque `ownedExecIr` slot after
`ownedAotIr` in the current Function record. Each slot is a separate unique owner.
The generic ExecIR snapshot prerequisite is committed as `3496923f`; see its
[contract](ssa-execir-owned-module.md) and
[finite evidence](../acceptance/ssa-execir-owned-module.md).

`ZrCore_Function_SnapshotExecIr(function, source, diagnostic)` now delegates a
live Function's independent storage snapshot to the generic owner API. NULL
Function resets the complete optional diagnostic to INVALID_ARGUMENT; a live
Function's NULL source is refused by the generic API. Current GREEN completes
all20 finite cases and both measured61-site Function fault sweeps. Its historical RED constructor refused non-NULL
pairs with UNSUPPORTED, returned false, completely reset the optional diagnostic
and left all eight payload fields zero. NULL Function/source reported INVALID_ARGUMENT.
That stub allocated nothing, read no source contents and changed neither owner slot.
The historical five-path RED source freeze and six test/support paths supplied
accepted native r3's20 cases:4PASS/16 attachment-refusal FAIL/0ignored. The passing cases
establish the typed/generic prerequisite, empty slots, actual New/size behavior
and NULL Function diagnostic. The16 feature failures stop before successful
association/lifetime/fault assertions. RED was committed as `ba7e2f13`.

The public getter borrows the generic owner's const module view; NULL or
unattached Function returns NULL. The alias-clear API clears only ExecIR slots
in fresh immediate copied child shells, without allocation, free or recursion.
The new record initialization and aggregate lifecycle support are real RED
behavior. They are unchanged by the GREEN forwarding implementation. Actual
GREEN validates association, dual-slot isolation, moves, owned/borrowed trees
and real shutdown with balanced independent ledgers. Its commit is pending.

## Live records and independent slots

Every non-NULL Function must be initialized, live, current-sized and rebuilt
with current headers. Source and all reachable typed storage remain readable
and stable during the call. The exclusive owner slot and optional writable
diagnostic must not overlap source, reachable storage, old owner or each other.
The caller serializes mutation, destruction and borrowing.

The implementation delegates to `ZrCore_ExecIr_CloneOwnedModule`. Failure preserves
the source, old ExecIR owner and complete lower diagnostic. Success replaces
only ExecIR after full cloning; AOT remains unchanged. AOT replacement likewise
does not replace ExecIR. No cross-slot transaction is promised. Source may be
the current borrowed ExecIR view. The borrow ends on successful replacement,
record move or Function destruction; acquire a new view from the destination
after a move. Writes through nested pointers violate the borrow contract even
though top-level C const does not enforce deep immutability.

## New, move, detach and destruction

`Function_New` must guard a NULL RawObject result before casting/dereferencing,
then initialize both owner slots. `New(NULL)` exercises the actual RawObject
argument refusal. Valid VM creation uses actual `sizeof(SZrFunction)`.
`GcMalloc` allocation failure retries through GC and may throw MEMORY_ERROR;
this gate does not inject raw allocation OOM and treat it as a plain NULL return.

A proven full-record transfer moves both owner slots to the destination.
`DetachOwnedBuffers` tombstones the donor using aggregate forget hooks: both
slots clear without freeing. Detach is not a general owner-discard API and must
follow a real transfer. Aggregate release in `Function_Free` releases both
independent owners before the existing child-buffer cleanup. Owned descendants
are recursively freed; borrowed descendants retain their existing policy.
Actual GC shutdown invokes Function_Free using the rebuilt current layout.

Runtime sizeof/alignment may change. Every linked Core/Parser/Library consumer
must be rebuilt from current headers; old smaller records are unsupported.
No numeric oldsize-plus-pointer assumption substitutes for actual compiler and
VM ledger observations. The runtime-only slot is not serialized; this gate
does not introduce a persistent schema or executable ABI change.
The selected r3 compiler/runtime observes sizeof(Function)=1208, AOT owner
offset1192 and ExecIR owner offset1200. The actual GC allocation size and OBJECT
tag case passes. These are selected-build observations, not portable constants.
The MSVC header compile probe asserts field order; it does not print or establish
that Clang runtime size numerically.

## Borrowed immediate shells

A fresh borrowed child shell is a shallow copy, not a new unique owner. Before
publication/destruction, its creator must separately clear AOT aliases with the
existing helper and ExecIR aliases with the new helper. Neither helper recurses
or modifies the originals or grandchildren. The borrowed parent frees its own
owners and immediate shell storage while skipping descendant destruction.
Remaining copied buffers stay borrowed: arbitrary shells cannot independently
be passed to Function_Free. Never clear owners in actual moved destinations.

## Open scope

This association is a multi-allocation typed storage snapshot, not Verify_ALL,
canonical identity/hash proof, ID remapping, execution installation or a single
arena. Ordinary source/default output and full Function migration remain OPEN,
as do complete 01.01, Linux/full MSVC, backend/native parity and all47 SSA leaves.
No source, Oracle, generated native, network/FFI/provider/security execution is
part of the finite gate.

- [Planned twenty cases](../testing-and-validation/ssa-function-exec-ir-owner.md)
- [Pending actual evidence](../acceptance/ssa-function-exec-ir-owner.md)
