# AOT 07-S2/S4 Stack-Copy Declaration Consumer Narrowing

- Time: 2026-06-28 13:14:53 +08:00
- Status: Completed focused slice; 07/M1.5 remains in progress.

## Scope

This slice removes stale i64 scalar-local declarations from bool-only stack-copy chains in the bool short-circuit logical AOT C path.

`backend_aot_c_scalar_locals_record_stack_copy_destinations()` now prefers the stack-copy SemIR destination kind and narrows propagated declaration kinds through the destination's later scalar consumers. This prevents source slots later reused for integer returns from leaking i64 declaration bits into bool-only destinations.

The follow-up regression fix makes that consumer narrowing source-kind aware for generic conversions: a stack-copy destination used as the source of `TO_INT`/`TO_UINT`/`TO_FLOAT` keeps the candidate source scalar kinds instead of being narrowed to the conversion result kind. Typed conversion consumers still narrow to their real source kind, so the unsigned mixed-literal frame-free path keeps its u64 stack-copy local.

## RED

The focused logical shared-library smoke added generated-C assertions that:

- forbid `TZrInt64 zr_aot_s3 =`
- forbid `TZrInt64 zr_aot_s5 =`
- forbid `TZrInt64 zr_aot_s15 =`
- require `TZrInt64 zr_aot_s6 =`
- require `TZrInt64 zr_aot_s16 =`

The old generator failed on `TZrInt64 zr_aot_s3 =`.

## GREEN

- WSL GCC: `zr_vm_aot_c_logical_shared_library_smoke_test` 5/0, `zr_vm_aot_c_source_contracts_test` 22/0, `zr_vm_aot_c_shared_library_smoke_test` 13/0, and `zr_vm_aot_c_frame_setup_contracts_test` 1/0.
- WSL Clang: `zr_vm_aot_c_logical_shared_library_smoke_test` 5/0, `zr_vm_aot_c_source_contracts_test` 22/0, `zr_vm_aot_c_shared_library_smoke_test` 13/0, and `zr_vm_aot_c_frame_setup_contracts_test` 1/0.
- Windows MSVC Debug: logical smoke built and ran as 0 failures / 5 ignored Unix-only, source contracts 22/0, shared-library smoke 0 failures / 13 ignored Unix-only, and frame setup contracts 1/0.

Generated C evidence from the GCC bool logical fixture:

- `TZrBool zr_aot_b3`, `TZrBool zr_aot_b5`, and `TZrBool zr_aot_b15` are declared.
- `TZrInt64 zr_aot_s3`, `TZrInt64 zr_aot_s5`, and `TZrInt64 zr_aot_s15` are absent.
- Real i64 return locals `TZrInt64 zr_aot_s6` and `TZrInt64 zr_aot_s16` remain.
- The unsigned mixed-literal fixture keeps `TZrUInt64 zr_aot_u2`, emits `zr_aot_scalar_stack_copy_u64 dstSlot=2 srcSlot=0`, and converts through `zr_aot_scalar_exec_to_i64` with no generated frame setup.

## Remaining Work

This does not complete 07-S2/S4. Value-level stack-copy migration, broader generic/dynamic/string boundaries, GC roots/exports/frame cleanup, wider byte-frame narrowing, performance counters, and full typed function-body zero-frame proof remain later 07 work.
