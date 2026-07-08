# AOT 07-S5 full-AOT i64/f64 scalar typed direct-call matrix guardrail

## Summary

Completed the focused guardrail slice that aligns i64 and f64 scalar typed direct-call smokes with the existing bool/u64
full-AOT matrix. The i64 suite now requires full-AOT direct-call markers and forbids metadata guard/deopt/sync for all
five cases. The f64 suite now does the same for all nineteen cases through shared smoke support.

## RED

Not applicable. This slice adds guardrails only; the current generator already satisfied the full-AOT direct-call shape.

## GREEN

- WSL GCC and Clang: i64 typed-direct smoke 5/0.
- WSL GCC and Clang: f64 typed-direct smoke 19/0.
- MSVC Debug: i64 typed-direct smoke 5 expected Unix-only ignores / 0 failures.
- MSVC Debug: f64 typed-direct smoke 19 expected Unix-only ignores / 0 failures.

## Generated C Evidence

- i64 two-arg generated C contains `zr_aot_static_i64_two_arg_direct_call_full_aot` and direct
  `zr_aot_sD = zr_aot_typed_i64_fn_N(...)` assignment.
- f64 three-arg generated C contains `zr_aot_static_f64_three_arg_direct_call_full_aot` and direct
  `zr_aot_fD = zr_aot_typed_f64_fn_N(...)` assignment.
- Focused generated C inspections did not find targeted `CanUseTypedDirectCall`, `DeoptTypedDirectCall`,
  `SyncSignedIntLocal`, or `SyncFloatLocal` in those full-AOT callsites.

## Changed Files

- `tests/parser/test_aot_c_typed_direct_call_shared_library_smoke.c`
- `tests/parser/aot_c_typed_direct_call_f64_smoke_support.h`

## Open Scope

This does not complete 07-S5. In/out writeback, GC roots/exports/frame cleanup, performance counters, and complete
07-S5 acceptance remain open.
