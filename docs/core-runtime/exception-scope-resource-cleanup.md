---
related_code:
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/ownership_shared.c
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/ownership_shared.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
plan_sources:
  - user: 2026-07-19 按 docs/plans/syntax 严格执行并逐里程碑提交
  - docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md
tests:
  - tests/parser/test_buffer_pool_ffi.c
  - tests/parser/test_resource_shared_weak.c
  - tests/core/test_type_layout_inline_copy.c
  - tests/parser/test_aot_c_call_shared_library_smoke.c
doc_type: module-detail
---

# Exception Scope Resource Cleanup

## Purpose

`using(resource)` resources are represented by the VM's to-be-closed stack chain. Normal
scope exit already closes registrations, but exception transfer must also close
the resources created inside the abandoned try scope before control reaches a
catch or finally block. Syntax03 M5 makes that ordering explicit for PoolLease and
other close-meta providers.

## Handler Checkpoints

Every pushed `SZrVmExceptionHandlerState` saves the current to-be-closed chain as
a stack-relative offset. The offset survives stack relocation. During exception
unwind, the VM resolves the saved boundary and repeatedly closes the top
registration while it is above the boundary.

Cleanup runs before:

- entering a matching catch;
- entering a finally block with a pending exception;
- popping a handler that cannot handle the exception;
- leaving a handler whose finally phase throws again.

Outer registrations remain linked because their stack positions are at or below
the saved boundary. Nested handlers therefore close only the resources whose
lexical scope is being abandoned, in LIFO order.

## Close Call Scratch Contract

A close meta call needs three values: callable, resource receiver, and error
argument. Scratch reservation may grow or relocate the stack, so the resource is
first saved as a stack offset and reloaded after reservation. The error object is
built directly in the third scratch slot. It is never constructed next to the
registered resource, where it could overwrite another live local or cached
callable.

The close function runs without yield during exception unwind. A normal close
receives null; exceptional close receives the current exception status projected
as an error value. The existing close-registration pop remains the single source
of truth, so the same registration cannot be invoked twice by one unwind.

## Ownership handles

The same chain directly closes `Unique`, `Shared`, `Weak`, and `Loan` values. Frame-layout locals
can keep their physical value outside the dense stack slot used by the close chain, so ownership
operations synchronize a retained cleanup mirror before and after overwrite, move, share, weak,
upgrade, release, and loan transitions. This prevents an exception from releasing a stale control
or leaving an extra strong/weak count alive.

Shared/Weak value parameters are also balanced across calls. After a callee successfully copies a
non-borrowed parameter, the caller staging owner is released. An exception then closes only the
callee copy plus other live lexical registrations. The final strong release marks the stable
control dead before resource Drop, which makes an upgrade attempted during Drop return empty.

## Distinct physical frame values

A frame-layout VALUE slot can have a dense registered cleanup cell and a distinct
byte-frame physical `SZrTypeValue`. Close processing resolves the physical cell
through the active `SZrCallInfo` chain. When both cells retain the same ownership
control, or the dense cell aliases a direct resource owner, cleanup clears the
dense registration before releasing the physical value. This ordering is
required because a resource destructor may re-enter the VM and grow or relocate
the stack. Re-entrant code must observe the registration as null and must not
retain a pointer that became stale during the destructor call.

The physical value is the release source when it is distinct and still owns the
resource. The dense mirror is then reloaded from its stack offset and reset, so
ordinary objects, resources, loans, Shared/Weak controls, overwritten physical
slots, and pre-close stack relocation all converge on one release without a
stale alias or duplicate Drop.

## Generated-call exception transfer

AOT C and LLVM calls complete through resume-aware runtime boundaries. A normal
return finishes the prepared call. A caught exception that resumes in the same
generated caller refreshes that caller frame and dispatches at the resume
instruction. An exception that has already unwound beyond that caller is left
unchanged for the outer handler; it is not rewritten as an AOT runtime failure.
This keeps nested direct/meta-call cleanup on the same exception-scope contract
as interpreter execution.

## Verification

`zr_vm_buffer_pool_ffi_test` throws from inside `using(lease)`, catches outside,
then rents the same size again. The expected generation and return/reuse counters
prove that cleanup ran exactly once before catch and did not corrupt adjacent VM
state. Parent using/escape tests protect normal close and structured cleanup
behavior.

`zr_vm_resource_shared_weak_test` throws after creating Shared clones and a Weak observer, then
asserts that unwind runs resource Drop once. It also covers value-parameter copies, nested
Shared/Weak fields, final-strong behavior, and wake failure after the last
strong owner is dropped.

`zr_vm_type_layout_inline_copy_test` covers distinct dense/physical ownership
cells, including a resource Drop callback that verifies the dense alias is
already null and then forces stack growth. The AOT call and receiver shared
library suites cover caught nested exceptions, tail callable propagation, and
post-call Weak expiry through generated C and LLVM.
