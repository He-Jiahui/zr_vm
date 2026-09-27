---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
tests:
  - tests/parser/test_ownership_intrinsic_member_separation.c
  - tests/parser/test_ownership_optional_callable_cases.h
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_ssa_source_cleanup_cfg.c
  - tests/parser/test_ownership_receiver_guard_performance.c
status: partial
---

# SSA 01.02: Weak named-call owner lifetime

## Failure and RED evidence

The existing suffix-throw test combined `weak.explode()` and
`weak?.explode()`, so a single `Expected 1 Was 0` did not identify which path
retained the target. Splitting it into direct and optional tests established
independent RED cases. The fresh WSL GCC Debug
`zr_vm_ownership_intrinsic_member_separation_test` executable from
`D:/tmp/zr_vm/ssa-optional-member-gcc` reported 54 tests and five failures:
both new suffix-throw cases returned 0 instead of 1, the two existing
intrinsic-named method dispatch cases returned 1 instead of 3, and the
guarded missing-member fixture failed compilation. The neighboring native GC
pressure and normal weak receiver tests passed.

GDB stopped in `receiver_guard_emit_working_value` and
`emit_known_vm_member_call_cached` while compiling each suffix-throw case.
The marked `OWN_WAKE` owner and `KNOWN_VM_MEMBER_CALL` result were both slot 4
for the direct call and both slot 5 for the optional call. The direct guard
fact dominated two segments, a member expression followed by a function
call. The compiler previously created `OWN_VIEW_SHARED` only when the guard's
first segment itself was a call, so a named member call could overwrite its
registered owner slot. The VM's exception-unwind path reached
`execution_close_exception_scope_registrations`; the registration existed,
but its owner/result slot alias could no longer guarantee the original Shared
owner remained available for close.

## Repair boundary

The receiver guard now uses its existing borrowed Shared view when a weak
guard's dominated chain ends in a call, even if the guard begins on a named
member expression. `OWN_WAKE` and `MARK_TO_BE_CLOSED` stay in the cleanup
slot, and the ordinary member call may reuse only the view slot for its
result. Field-only chains retain their existing slot layout. No core unwind
or general member-call lowering rule changes.

The guarded missing-member failure has a separate type-resolution cause and
remains outside this repair.

## GREEN evidence (2026-09-27)

The WSL GCC 11.4.0 Debug cache rebuilt the four requested targets with
`cmake --build /mnt/d/tmp/zr_vm/ssa-optional-member-gcc --target
zr_vm_ownership_intrinsic_member_separation_test
zr_vm_pre_semantic_ir_test zr_vm_ssa_source_cleanup_cfg_test
zr_vm_ownership_receiver_guard_performance_test -j 4` (348/348 Ninja edges,
exit 0 after CMake regeneration). The changed receiver-guard object, parser
shared library, and ownership executable all have timestamps newer than the
edited source.

The freshly linked ownership executable reported 54 tests, one failure, and
process exit 1. Both split suffix-throw tests and both existing intrinsic-named
member dispatch tests passed. The sole failure remained
`test_live_weak_missing_member_is_not_null_reference_error` at the compile
assertion (`Expected Non-NULL`), exactly the separate unresolved case.
The GC-pressure case and neighboring normal weak calls also passed. An
independent ownership replay is saved as
`D:/tmp/zr_vm/ssa-optional-member-gcc/root-ownership-after-guard.log`.

GDB on the fresh executable found owner/result slots 4/5 for the direct
suffix call and 5/6 for the optional suffix call. This is the intended slot
separation from the RED 4/4 and 5/5 pairs. The same cache passed the direct
`zr_vm_pre_semantic_ir_test` runner 114/114, the direct
`zr_vm_ssa_source_cleanup_cfg_test` runner 52/52, its focused CTest 1/1, and
the direct `zr_vm_ownership_receiver_guard_performance_test` runner 1/1. All
four adjacent runs exited 0. This record covers the GCC interpreter path;
the missing-member compiler repair remains a separate acceptance item.
