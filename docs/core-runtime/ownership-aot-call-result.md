---
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_call_result.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_call_result.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_stack_copy.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_call_boundaries.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_cleanup_registration.c
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_arithmetic.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/include/zr_vm_core/value.h
  - tests/parser/aot_c_ownership_call_result_projection_cases.h
  - tests/parser/test_aot_c_method_info_signature.c
  - tests/parser/test_ownership_abrupt_parity_cases.h
  - tests/parser/test_aot_receiver_guard_shared_library.c
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_call_result.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_call_result.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_cleanup_registration.c
  - zr_vm_core/src/zr_vm_core/closure.c
plan_sources:
  - docs/plans/syntax/astra.md
  - docs/plans/astra/syntax/ownership-object-member-separation.md
tests:
  - tests/parser/aot_c_ownership_call_result_projection_cases.h
  - tests/parser/test_aot_c_method_info_signature.c
  - tests/parser/test_ownership_abrupt_parity_cases.h
  - tests/parser/test_aot_receiver_guard_shared_library.c
doc_type: module-detail
---

# Ownership Across AOT Call Results

Generated C and LLVM must preserve the same Shared and Weak lifetime as the VM
when a function result passes through a temporary call slot and then becomes a
local. This document covers the result boundary. Pending return replacement and
exception-handler ownership are governed by the common execution-control
lifecycle and its separate regressions.

## Instruction-Specific Scalar Evidence

The C emitter allocates scalar C locals from a union of the primitive types seen
for each stack slot anywhere in a function. This is a declaration inventory, not
proof that the slot contains a primitive at a particular instruction. Compiler
temporary slots can be reused for a constructor integer argument, a callable,
and a Weak result within the same function.

Before the fix, `backend_aot_c_scalar_locals_record_exec_instruction_write`
queried the callee and SemIR for a primitive return kind. Both an unknown type and
a known ownership type produced `NONE`. The fallback then used the declaration
inventory of the reused destination slot. A following `SET_STACK` consequently
copied an old `zr_aot_sN` value instead of the returned Weak handle. Runtime
scalar synchronization cannot make this valid: a noninteger result leaves the
integer C local unchanged.

`backend_aot_c_scalar_call_result_has_nonprimitive_type` inspects only SemIR
records with the matching execution instruction index and destination slot.
An ownership qualifier or a known nonprimitive base type blocks scalar return
inference. An unknown base type remains eligible for the existing scalar
inference rules.

There are two consumers of this negative type evidence:

1. Declaration collection excludes the known nonprimitive call from inferred
   scalar result contributions. Primitive uses in other lifetimes can still
   declare C locals for the same numerical slot.
2. Reaching-write analysis sets the destination's primitive kind to `NONE` at
   that call. Subsequent stack copies therefore read the materialized value,
   unless a later primitive write independently establishes a new scalar value.

The type query lives in its own small backend module. The existing large scalar
analysis file only calls it at the two relevant inference boundaries; this task
does not reorganize unrelated scalar dataflow or emitter responsibilities.

## Returning Into the Generated Frame

`ZrLibrary_AotRuntime_Return` stages an ordinary generated result at the callee's
function base. `ZrLibrary_AotRuntime_FinishDirectCall` then closes remaining open
upvalues, invokes `ZrCore_Function_PostCall`, and restores the caller frame.

The core return path can store the result in the caller's physical VALUE place,
which is not necessarily the same address as the dense slot read by generated
code. For a materialized ownership handle, the AOT finish path transfers that
physical holder to the generated slot. It snapshots the complete `SZrTypeValue`,
resets the physical source without releasing its ownership, and writes the
snapshot into the generated slot. The snapshot is required because these two
value regions can overlap.

Shared and Weak staging at the callee function base owns an additional counted
reference. That base lies outside the callee's local cleanup range, which starts
at function base plus one. The finish path saves its stack offset before closing
or returning, restores the caller result, then releases this counted staging
holder. Using an offset allows the cleanup address to be reconstructed after
stack relocation. Identical staging and destination pointers are excluded from
the extra release.

This path implements the current generated direct-call ABI, which expects one
result. It does not introduce multiple-result ownership transfer semantics.

## Materializing a Local

`ZrLibrary_AotRuntime_CopyStack` implements the materializing `SET_STACK` boundary
with `ZrCore_Value_AssignMaterializedStackValue`. Shared, Weak, Unique, and Loaned
handles transfer their existing wrapper and reset the source temporary. Ordinary
values retain their established copy behavior. A `GET_STACK` read still uses
the separate runtime read path and can create a retained copy when required.

This distinction matters for call arguments as well as returned values. In the
Weak fixture, the compiler emits a read of `shared` into a temporary followed by
a materializing copy into the call argument slot. Copying that temporary again
would leave a hidden strong reference alive after `drop(shared)` and prevent
`wake(returned)` from reporting expiration.

No compatibility branch preserves the former retained temporary behavior.

## Cleanup Registration And Entry

Generated code reads dense slots while the core frame layout can keep a separate
physical VALUE slot. Cleanup registration retains the physical owner so an
aliased member read can replace the dense receiver with a returned field value.
Closing that registration releases both slots only when they still hold the same
ownership control. Core cleanup resolves the mirror in either direction for
physical storage beyond the function's dense logical slots. Explicit ownership
operations clear and refresh the registered copy around the operation.

VM call preparation has already initialized physical parameter slots. The LLVM
runtime entry now preserves those slots, matching generated C's native-call-only
initialization guard. Previously it reset the physical Shared parameter without
release; a GDB count watchpoint proved one missing decrement after the callee
returned and the caller dropped both visible handles.

LLVM arithmetic lowering maps specialized signed and floating negation to the
existing runtime Neg operation. Signed constant equality emits a checked runtime
comparison before materializing its boolean destination. Prepared-call failure
keeps an existing runtime error, which makes unsupported instructions observable
instead of replacing them with a generic call failure.

## Regression Coverage

The signature target includes a compiled Shared/Weak source fixture in
`aot_c_ownership_call_result_projection_cases.h`. Each caller first constructs a
resource with an integer argument and then receives an ownership-qualified call
result. At the copy into the `returned` local, the source must have no reaching
bool, i64, u64, or f64 value. This checks the actual compiler and execution IR,
including temporary-slot reuse.

The generated shared-library target runs the common abrupt-cleanup fixture
through both C and LLVM. Its expected result is `123456778908`: nested finally
trace `1234567789`, multiplied by 100, plus eight resource drops. The fixture
checks Shared and Weak returns through nested finally, a return replaced by a
throw, loop break and continue cleanup, and a Weak member chain returning a
Shared leaf.

## Validation Evidence

The root task owns the frozen source and build caches so concurrent workers do
not rebuild or modify the same snapshot. Targeted replay uses Ubuntu-22.04 with
the frozen source at `/home/hejiahui/.codex-snapshots/ownership-astra-final` and GCC
cache `/home/hejiahui/.cache/zr-ownership-astra-final-gcc`.

- Before the scalar fix, `zr_vm_aot_c_method_info_signature_test` reported
  13 tests, one failure, and zero ignored tests. The failure was the ownership
  result reusing an i64 kind. Evidence:
  `.codex/logs/ownership-astra-r6-gcc-aot-signature.log`.
- With the scalar fix, the initial Weak projection regression and all twelve
  surrounding signature tests passed, with zero ignored tests. Evidence:
  `.codex/logs/ownership-astra-r7-gcc-aot-signature.log`.
- Frozen manifest `9758b9cf68ec47c4fe533f1f76fd06801116c821cf35c7172f9466e5591a228e`
  passes Shared/Weak84/84 on GCC, Clang, MSVC and Clang ASan/UBSan, with zero
  ignored cases. The generated C/LLVM target passes10/10 on GCC and Clang;
  the C ownership library passes2/2, and signature projection passes13/13.
- MSVC signature projection passes10/10. Its Unix shared-library tests are
  capability-ignored (10 and2 respectively), not execution passes.
- Evidence is captured in `.codex/logs/ownership-astra-aot-entry-*-results.json`,
  `ownership-astra-aot-extra-gcc-results.json` and
  `ownership-astra-lifecycle-final-asan-results.json`. Whole-plan integrated
  acceptance remains separate from this defect-stage replay.
