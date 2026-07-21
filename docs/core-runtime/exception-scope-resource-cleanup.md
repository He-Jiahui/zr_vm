---
related_code:
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
plan_sources:
  - user: 2026-07-19 按 docs/plans/syntax 严格执行并逐里程碑提交
  - docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md
tests:
  - tests/parser/test_buffer_pool_ffi.c
doc_type: module-detail
---

# Exception Scope Resource Cleanup

## Purpose

`%using` resources are represented by the VM's to-be-closed stack chain. Normal
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

## Verification

`zr_vm_buffer_pool_ffi_test` throws from inside `%using (lease)`, catches outside,
then rents the same size again. The expected generation and return/reuse counters
prove that cleanup ran exactly once before catch and did not corrupt adjacent VM
state. Parent using/escape tests protect normal close and structured cleanup
behavior.
