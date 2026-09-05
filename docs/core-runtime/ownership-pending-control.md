---
related_code:
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_resource.c
  - zr_vm_core/src/zr_vm_core/ownership_resource_internal.h
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_resource.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
plan_sources:
  - docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md
  - docs/plans/astra/syntax/ownership-object-member-separation.md
  - docs/plans/astra/using/review.md
tests:
  - tests/parser/test_resource_shared_weak.c
  - tests/parser/test_ownership_pending_control_cases.h
  - tests/parser/test_ownership_drop_failure_cases.h
  - tests/parser/test_ownership_aot_pending_cases.h
  - tests/parser/test_ownership_loop_finally_cases.h
  - tests/parser/test_ownership_abrupt_roundtrip_cases.h
  - tests/parser/test_ownership_abrupt_parity_cases.h
  - tests/parser/test_aot_receiver_guard_shared_library.c
  - tests/parser/gdb_pending_owner_return.gdb
doc_type: module-detail
---

# Ownership Across Pending Control

## Scope

Pending control is runtime storage for an abrupt transfer crossing `finally`.
Its `value` is an owning value, not a borrowed view of a frame slot. This
contract is shared by interpreter and generated-code helpers. It introduces
no language syntax, exported ownership operation, or serialized ABI change.

The ownership intrinsics remain `share`, `degrade`, `wake`, `intoGc`, and
`drop`. Member access and optional access operate on their targets, including
ordinary members with these spellings. Direct null access still raises
`NullReferenceError`.

## Pending Value Lifecycle

Installing a valued transfer copies/retains its input before clearing the old
record. This ordering also permits input to alias `pendingControl.value`.
Copy and release can allocate, invoke resource code, or report a native host
failure; their temporary roots must be removed on both normal and nonlocal exit.

Clearing first detaches all visible metadata and value storage, then releases
the detached value. A repeated or reentrant clear therefore sees an empty
record. Valueless transfers use this same path, as do thread reset, exception
replacement, and completed resume. State initialization only initializes
storage and never attempts to release an uninitialized value.

An exception active before successful cleanup is restored after cleanup. Drop
executes with a clean exception context so an internal try/catch cannot consume
the outer exception. Stack-local GC root frames protect saved exceptions and
replacement values without allocating a pin-registry entry. A top-level source
object is temporarily pinned during copying because value-copy code may hold
its raw C pointer across allocation.

## Resource Callback Boundary

The private `ZrCore_OwnershipResource_DropProtected` completes lifecycle work
before returning a captured native status. It retains the existing ownership
root while the object is `DROPPING`, executes the custom body and managed field
cleanup, marks `DROPPED`, and unregisters the root. Reentrant Drop on the same
object does not execute the body twice.

Each callback has anchored stack base/top and return storage, saved handler
and native-yield boundaries, and a saved GC-root frame boundary. A callback's
pending transfers are isolated from the caller's pending transfer and drained
before the caller record is restored. On host failure, abandoned registrations
are closed before restoring call state. The first native failure remains the
reported failure; subsequent cleanup does not replace it.

The final Shared release must run `FinishFinalStrong` before propagating a
native failure. Otherwise the implicit weak reference and `dropInProgress`
would remain even after strong count reached zero. Source destructors remain
nonthrowing; defensive host cleanup is not a new source exception-aggregation
feature.

## Domain Rejection

`ZrCore_Ownership_ReleaseValue` validates the control block's isolation domain
before resetting Shared or Weak storage. Rejected calls leave the value kind,
object/control identity, strong count, and weak count unchanged. The origin
domain can still release the handle, including an expired weak handle. No
function signature changes are required.

## Frame And Compiler Contracts

Publishing an owned call result into a distinct physical frame slot transfers
ownership from the dense staging slot. Keeping an additional retained staging
copy would postpone Drop until caller-frame teardown, even after explicit drop.

Throw normalizes its payload before pending cleanup can run callbacks. Catch
and end-finally refresh cached frame addresses after cleanup because nested
calls may relocate the value stack. The AOT regressions use a deterministic
moving allocator and overwrite a source payload during Drop to verify ordering.

Loop transfers only enter finally handlers whose protected scope they exit.
TRY contexts carry their scope-stack depth for this decision. Pending absolute
destinations are patched for unresolved and already-resolved loop labels.
Quickening treats those destinations as block boundaries and remaps them on
both compaction and insertion. Runtime resume keeps an enclosing try active
when the pending target remains within that protected region.

## Validation And Maintenance

Direct tests cover counts, weak expiry, exactly-once Drop, self-alias,
replacement, reset, host failure, nested catch, stack relocation, and isolation
domain rejection. One shared source supplies VM, binary readback, generated C,
and LLVM abrupt-cleanup tests with exact trace and Drop-count assertions.
Current execution results and outstanding gates belong to the linked Astra
plan and acceptance record, not to historical test totals.

The large dispatch/function/compiler files retain narrowly scoped changes in
their existing call-result and branch-remapping responsibilities. This work
does not reorganize the interpreter or absorb concurrent frame optimizations.
