---
related_code:
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_core/src/zr_vm_core/call_info.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
plan_sources:
  - docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md
  - docs/superpowers/plans/2026-08-10-ownership-object-member-separation-implementation.md
tests:
  - tests/core/test_execution_dispatch_callable_metadata.c
  - tests/parser/test_ownership_intrinsic_member_separation.c
  - tests/acceptance/2026-08-10-ownership-object-member-separation.md
doc_type: module-detail
---

# State Lifecycle

## Purpose

VM execution reuses call frames and compiled metadata to avoid allocations on
hot paths. Reuse does not transfer ownership to the garbage collector: raw
arrays and call-info nodes remain allocator-owned storage and must be released
when their state or function reaches teardown. This document records those
boundaries so optimization caches cannot silently become process-lifetime
leaks.

## Reusable Call-Info Chain

`ZrCore_CallInfo_Extend` allocates an `SZrCallInfo` after the current tail and
increments `callInfoListLength`. Returning from a call moves the active cursor
backward but deliberately retains the `next` chain for reuse. Consequently,
`ZrCore_State_Free` walks from `baseCallInfo.next`, releases every extended
node with the same native allocation type, and resets the base link, active
cursor, and length before the state allocation itself is released.

The base call-info is embedded in `SZrState` and is never freed separately.
Stack-local call-info values used by native fast paths are temporary overlays;
their helpers restore the retained chain link and do not insert those stack
addresses into the owned chain.

## Compiled Function Prototype Storage

`SZrFunction::prototypeInstances` is a raw array of prototype pointers sized by
`prototypeInstancesLength`. The pointed-to prototypes are GC-managed objects,
but the pointer array is owned by the function. Function teardown therefore
releases only the array and then resets the function to its GC-safe tombstone.
It must not recursively free any prototype object.

Module prototype loading grows this array through the runtime allocator. Its
per-prototype `inheritTypeNames` value starts in the constructed, allocation-free
state; the metadata parser performs the single initialization. Initializing it
twice would overwrite the first allocation before normal cleanup could observe
it.

## Validation Boundary

Behavior regressions are covered by the callable-metadata and ownership suites
on GCC, Clang, and MSVC. Allocation ownership is additionally checked with an
ASan+UBSan build using LeakSanitizer and with Valgrind full leak checking. The
accepted ownership target ends with zero live blocks and matching allocation
and free counts; disabling leak detection is not sufficient evidence for this
lifecycle contract.
