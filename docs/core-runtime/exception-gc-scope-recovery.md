---
related_code:
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/src/zr_vm_core/exception_internal.h
  - zr_vm_core/src/zr_vm_core/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_internal.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c
plan_sources:
  - docs/plans/ssa/06-gc-domain/03-domain-sharing.md
  - docs/plans/ssa/06-gc-domain/04-cross-domain-clone.md
tests:
  - tests/core/test_exception_gc_scopes.c
  - tests/core/test_aot_gc_root_frame_exception.inc
  - tests/core/test_gc_nested_mutation.c
  - tests/acceptance/ssa-exception-gc-scope-recovery.md
doc_type: module
---

# Protected Exception GC Scopes

`ZrCore_Exception_TryRun` captures the registered mutator's execution, native
and locked mutation depths at entry. A local `ZrCore_Exception_Throw` restores
these depths before its C11 non-local jump. This preserves a worker or native
caller's outer scopes when a nested job fails.

## Snapshot And Identity

`SZrGcDomainScopeSnapshot` stores values copied under the domain coordination
lock. It owns no registry pointer or root handle. Domain pointer, domain
identity and mutator ID must still match at restoration, so table growth is
safe and detach/reattach cannot reuse another registration's lock depths.
Callbacks must retain their entry domain registration and caller-owned scopes.
If the registration changes or a callback has released caller-owned mutation
levels, restoration does not invent replacement lock ownership.

The private TryRun context has the public recovery point as its first member.
The automatic context and callback-return flag are volatile across `setjmp`;
the recovery status can change during Throw without making saved fields
indeterminate after `longjmp`. Normal return retains ordinary Begin/End and
Enter/Leave behavior, including the existing FINE result contract.

## Restoration Order

Local Throw first normalizes its error and writes the recovery status. It then
restores the saved AOT root-chain top and depth without traversing abandoned
callback nodes. Under the coordination lock, it releases exactly the recursive
mutation levels added since TryRun entry, restores caller depths and native
mode, and derives the surviving running/inactive/detached/critical status.
The current domain epoch is used rather than a stale parked status or epoch.
Only then is the status broadcast to collectors and other mutators.

Retained caller mutation levels stay locked until their caller ends them.
An empty entry releases every callback-owned level, allowing another mutator
to enter the mutation lock after catch. Nested TryRun restores each distinct
entry snapshot. Catch repeats root-chain restoration without releasing another
thread's locks on the legacy worker forwarding path.

## Files And Bounds

`exception_try_run.c` owns the protected context and local restoration helper;
`gc_domain_scopes.c` owns registry snapshots and depth restoration. The shared
private exception header preserves the existing C/native-C++ unwind selection.
The extraction reduces `exception.c` from 1,053 to 989 lines.

The verified guarantee is for local Throw caught on its owning native thread
in the C11 configuration. Forced-C++ destructor reentry, raw native exceptions,
legacy cross-thread forwarding and abandoned transfer resources are separate
contracts. In particular, this change releases mutation locks but does not
release a clone transaction's temporary root handles or native allocations.

Validation and failure evidence are in
[the acceptance record](../../tests/acceptance/ssa-exception-gc-scope-recovery.md).
