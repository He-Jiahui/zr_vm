# AOT 07-S2/S4 Generic Numeric Mixed u64/f64 Coverage and Mixed i64/u64 Live Proof

## Scope
- Slice: M1.5 / 07-S2/S4 generic numeric mixed u64/f64 constant-shape coverage plus mixed i64/u64 live-consumer proof completion.
- Affected layers: scalar-local proof, AOT C generic numeric smoke tests, source contracts, AOT plan/status docs.
- Behavior: proven `GET_CONSTANT uint -> GET_CONSTANT float -> ADD/SUB/MUL/DIV/MOD -> RETURN` shapes are now covered by
  executable smoke tests and verified to stay on the existing mixed-f64 scalar-local path. Mixed i64/u64 live-value
  consumer proof now consistently covers `ADD/SUB/MUL/DIV/MOD` for both i64 and u64 operands.

## Baseline
- The plan still listed mixed u64/f64 as an open proven-local numeric shape. Adding the five u64/f64 smoke tests did not
  expose a production lowering gap: the new tests passed on the first WSL GCC focused run, proving the existing mixed-f64
  path already handled this constant shape.
- The scalar-local proof had a separate mixed i64/u64 gap: the i64 and u64 live-consumer readers did not consistently
  include mixed i64/u64 proof across `SUB/MUL/DIV/MOD`; u64 `DIV/MOD` also fell through to a broad slot-mention check.
- RED command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_numeric_contracts_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_contracts_test"`
- RED result: generic numeric contracts reported 1 test, 1 failure because the source contract could not find the explicit
  `DIV/MOD` u64 consumer branch using `backend_aot_c_scalar_locals_generic_numeric_u64_binary_reads_slot(...)`.

## Test Inventory
- Focused executable smoke:
  `tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c`
  - `test_aot_c_generated_shared_library_compiles_generic_numeric_add_unsigned_int_float_local`
  - `test_aot_c_generated_shared_library_compiles_generic_numeric_sub_unsigned_int_float_local`
  - `test_aot_c_generated_shared_library_compiles_generic_numeric_mul_unsigned_int_float_local`
  - `test_aot_c_generated_shared_library_compiles_generic_numeric_div_unsigned_int_float_local`
  - `test_aot_c_generated_shared_library_compiles_generic_numeric_mod_unsigned_int_float_local`
- Contract tests:
  - `tests/parser/test_aot_c_generic_numeric_contracts.c`
  - `tests/parser/test_aot_c_source_contracts.c`
- Adjacent regression matrix:
  - generic numeric smoke/contracts/source contracts
  - typed/generic power smoke/contracts
  - generic LOGICAL_NOT numeric local smoke
  - generic bool equality local smoke
  - generic equality stack-copy local smoke
  - generic not-equal stack-copy jump-if local smoke
  - generic call-result stack-copy equality local smoke
- Boundary coverage:
  - u64/f64 operands use the mixed-f64 scalar-local path and preserve f64 result semantics.
  - `DIV` and `MOD` retain `"divide by zero"` / `"modulo by zero"` guards.
  - dynamic/unproven operands remain on runtime fallback paths.
  - mixed i64/u64 consumer proof is explicit for both source kinds across all generic numeric binary opcodes.

## Tooling Evidence
- WSL GCC mixed u64/f64 coverage confirmation:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test"`
  - generic numeric smoke 36/0 on the first run after adding the u64/f64 cases.
- WSL GCC focused GREEN:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_source_contracts_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
  - generic numeric smoke 36/0
  - generic numeric contracts 1/0
  - source contracts 24/0
- WSL GCC adjacent matrix:
  - build passed after splitting from an earlier timeout-limited combined command.
  - run results: 36/0, 1/0, 24/0, 1/0, 2/0, 8/0, 5/0, 4/0, 1/0, 1/0.
- WSL Clang adjacent matrix:
  - build passed with clean output after parenthesizing the multi-line contract string needles.
  - run results: 36/0, 1/0, 24/0, 1/0, 2/0, 8/0, 5/0, 4/0, 1/0, 1/0.
- MSVC Debug compatibility:
  - focused targets built with `build-msvc-aot-stack-copy --config Debug -- /m:1`.
  - generic numeric smoke: 36 tests, 0 failures, 36 ignored as Unix-only.
  - generic numeric contracts 1/0.
  - source contracts 24/0.
  - power smoke: 1 test, 0 failures, 1 ignored as Unix-only.
  - power contracts 2/0.
  - generic LOGICAL_NOT: 8 tests, 0 failures, 8 ignored as Unix-only.
  - generic bool equality: 5 tests, 0 failures, 5 ignored as Unix-only.
  - generic equality stack-copy: 4 tests, 0 failures, 4 ignored as Unix-only.
  - generic not-equal stack-copy jump-if: 1 test, 0 failures, 1 ignored as Unix-only.
  - call-result stack-copy equality: 1 test, 0 failures, 1 ignored as Unix-only.
- Static checks:
  `git diff --check -- .codex/sessions/20260620-2321-aot-07-12-codegen.md docs/parser-and-semantics/csharp-value-type-semir-aot.md docs/plans/aot/07-codegen-register-model-and-environment-isolation.md docs/plans/aot/index.md tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-mixed-u64-f64-coverage-and-mixed-i64-u64-live-proof.md tests/parser/test_aot_c_generic_numeric_contracts.c tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c`
  - exit 0; only line-ending normalization warnings were reported.
  `Select-String -Path tests/acceptance/2026-07-05-aot-07-s2-s4-generic-numeric-mixed-u64-f64-coverage-and-mixed-i64-u64-live-proof.md,docs/plans/aot/07-codegen-register-model-and-environment-isolation.md,docs/plans/aot/index.md,docs/parser-and-semantics/csharp-value-type-semir-aot.md,.codex/sessions/20260620-2321-aot-07-12-codegen.md -Pattern '[ \t]+$'`
  - no matches.

## Results
- `tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c` now proves all five mixed u64/f64 constant binary
  operators compile through the scalar-local mixed-f64 path.
- `tests/parser/test_aot_c_generic_numeric_contracts.c` now guards the scalar-local source proof shape that prevents
  mixed i64/u64 u64 operands from falling back to broad slot-mention checks.
- `backend_aot_c_scalar_locals.c` unifies i64 and u64 generic numeric live-consumer proof for `ADD/SUB/MUL/DIV/MOD`:
  same-kind i64/u64 proof, mixed i64/u64 proof, and mixed-f64 proof are checked consistently for the relevant local kind.
- Generated u64/f64 C contains mixed-f64 local markers, direct expressions, optional zero guards, and no targeted generic
  numeric runtime boundary or f64 sync helper for these proven-local shapes.

## Acceptance Decision
- Accepted for this slice: proven mixed u64/f64 constant-shape coverage is now explicit, and mixed i64/u64 live-consumer
  proof is complete across `ADD/SUB/MUL/DIV/MOD`.
- This closes the previously listed mixed u64/f64 constant-shape coverage item and the mixed i64/u64 live-proof gap found
  while auditing it.
- Remaining 07 work: dynamic/unproven operands, broader value-copy migration, GC roots/exports/frame cleanup,
  byte-frame narrowing, performance counters, and complete zero-frame typed bodies.
- Broader AOT 07~12 objective remains active and incomplete.
