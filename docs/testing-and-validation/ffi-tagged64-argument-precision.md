---
related_code:
  - tests/ffi/test_ffi_module.c
  - tests/ffi/test_ffi_integer_precision_contract_cases.h
  - tests/ffi/ffi_fixture.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c
implementation_files:
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/ffi/test_ffi_module.c
  - tests/ffi/test_ffi_integer_precision_contract_cases.h
  - tests/ffi/ffi_fixture.c
  - tests/acceptance/2026-10-16-macro-m1-token-foundation.md
doc_type: testing-guide
---

# FFI Tagged64 Argument Precision

This focused gate supports accepted macro M1 validation; it is not platform or
M1 acceptance. GCC storage-adjacent R5 is a valid RED; the 13-line production
repair is now applied and focused R6 is GREEN; all full gates remain pending.

## Observed Loss

GCC storage-adjacent R3 failed `test_zr_ffi_preserves_64_bit_integer_precision`.
Its u64 identity input `9007199254740993` (`2^53 + 1`) passed through
`double` and became `9007199254740992` (`2^53`) before the native identity
function could return it. The fixture's C identity function does not round it.
The failed boolean is in the raw test log; the conversion path explains the loss.
This is a source-path diagnosis, not a captured native-argument trace.

The old signed input `-9007199254740991` is `-(2^53 - 1)`, which binary64
represents exactly. It cannot establish large-i64 preservation; the old combined
boolean also cannot isolate the signed path when its u64 conjunct fails first.

## Actual Call Path

1. Source compilation and normal execution load the real fixture shared library.
2. `SymbolHandle.call` enters `zr_ffi_symbol_invoke_array` in `ffi_runtime_invoke.c`.
3. That function gives the resolved parameter and value to
   `zr_ffi_build_scalar_argument` in `ffi_runtime_callback.c`.
4. Before the repair, both I64/U64 branches used `zr_ffi_extract_numeric_value`,
   converting tagged integers to `double`, then casting into native storage.
5. `zr_ffi_invoke_native_symbol` performs the native call; the fixture's
   `zr_ffi_identity_i64` or `zr_ffi_identity_u64` returns its input unchanged.
6. `zr_ffi_set_result_from_scalar` maps the native result back to a VM value.

Applied callback SHA256:
`28BA219A5B92CB77F2E94F1B4065C211996771C50ED37087AFDC686452F55B74`.

## Applied Tagged Rules

The applied 13-line repair changes only the tagged integer argument fast paths:

| Target | Direct path before the existing numeric fallback |
| --- | --- |
| I64 | Copy `nativeInt64` only for a `SIGNED_INT` value. |
| U64 | Copy `nativeUInt64` for an `UNSIGNED_INT` value. |
| U64 | Cast `nativeInt64` directly only when tagged `SIGNED_INT` and nonnegative. |

All other tags retain the current fallback. Result marshalling, callback
dispatch, wrapper lowering and invocation cleanup are outside this repair.
Retaining a fallback is not proof that its range or conversion semantics are safe.

## Discriminating Signed Oracle

The 43-line `test_ffi_integer_precision_contract_cases.h` adds
`ffi_assert_signed64_precision_contract` at the beginning of the existing fourth
case, `test_zr_ffi_preserves_64_bit_integer_precision`. The 30 Unity case names,
count and order remain unchanged; this helper is not a new `RUN_TEST`.

Its independent fixture uses the normal compile/execute path and only the real
i64 identity symbol, requesting `9007199254740993` and `-9007199254740993`.
Both exact comparisons must hold. The original combined fixture also retains its
old u64/signed checks and adds those two signed values.

The helper copies compile, execution and exact-result booleans, frees a compiled
function, and destroys its VM before any of its three Unity assertions. It does
not retain a VM-owned result or string after destruction. This cleanup statement
is local to the new helper, not a claim about every assertion in the old fixture.

## Evidence Boundary

R3 receipt `1BE966AFA7A4D0F40A05F3C6A4A166195EA5719B55FFC92509720CBB363DF620`
records FFI 28 PASS / 2 FAIL / native exit 2. Source/HEAD drift independently FAIL;
later focused results do not waive R3 or its other failures.

R5 receipt `DB147784DA01C9E9CEACE5EAA025FB927D24BD11D194B142E4452CC1B124B7A8`
records 319 PASS / 1 FAIL / 0 IGNORE: only the fourth FFI case's signed oracle fails.
Union 69, native-extern 30 and compiler 127 PASS; all 18 direct children terminal.
Closure PASS: HEAD `33c3f43e` and 4890 source inputs / `318A9714` stayed stable.
R5 is valid RED, not GREEN. Fresh R6 receipt
`511E8B48C1186187E73AAE413D7F52032EFB9D2CE49499331F4B711213F6F168`
records320 PASS/0 FAIL/0 IGNORE, eight native exits0 and all18 direct children
naturally terminal. HEAD `33c3f43e` and4890/E36006EF source stayed stable;
support/environment/cache/ELF closure PASS. The sole production change from R5
is the applied tagged64 repair. This is focused normal-call proof, not full-range,
callback, platform or M1 acceptance.
Historical Windows successes cannot certify this GNU run or the current source.

## Open Boundaries

Unsigned-tag to i64, negative signed to u64, floating conversion ranges and
nonfinite inputs remain OPEN. U64 results still use `ZrLib_Value_SetInt`; values
above `INT64_MAX` remain a separate issue. Shared scalar helpers also serve
callback paths, but this normal-call fixture does not prove complete callback
coverage. Full Windows, GCC/Clang matrices, fresh docs validation and M1 remain OPEN.

See [FFI test harness contract](../library-and-builtins/ffi-test-harness-contract.md)
and the [macro validation ledger](macro-foundation-validation.md) for surrounding
resource and milestone gates.
