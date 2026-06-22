# AOT M1.5 07-S2 I64 Constant Cross-Block Local-Only

Date: 2026-06-21 07:24:00 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice removes the frame destination write from the focused i64 primitive constants when the
constant slot has an i64 scalar local and every reachable later use can consume or kill that local.

For the focused typed scalar source, the first two constants now emit only scalar-local assignments:

- `slot 0`: `zr_aot_s0 = (TZrInt64)21;`
- `slot 1`: `zr_aot_s1 = (TZrInt64)2;`

The generated blocks no longer immediately fall through into `zr_aot_value_exec_primitive_constant`
or write `frame.slotBase[0].value` / `frame.slotBase[1].value` with the same integer payload.

Affected layers:

- i64 primitive constant scalar-local liveness and result-skip proof
- focused generated-product parser/AOT fixture
- AOT semantic contract documentation and plan records

## Baseline

Before this slice, the constants were mirrored into scalar locals but still wrote the frame slot
payload immediately afterward:

- `/* zr_aot_scalar_constant_i64_local */`
- `zr_aot_s0 = (TZrInt64)21;`
- `/* zr_aot_value_exec_primitive_constant */`
- destination assignment through `frame.slotBase[0].value`

Existing repository baseline:

- The worktree is dirty with unrelated parser/runtime/docs/build-output changes.
- `git diff --check` reports LF/CRLF conversion warnings from the existing dirty worktree but exits 0.
- `backend_aot_c_scalar_locals.c` is above the modularization threshold; this slice tightens the
  existing liveness proof instead of mixing in a structural split.

## Test Inventory

- Focused subsystem case: `tests/parser/test_aot_c_typed_scalar.c` verifies generated C for the
  first two i64 primitive constants.
- Generated-product boundary: inspected
  `build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c`.
- Source-contract target: `zr_vm_aot_c_source_contracts_test`.
- Registered CTest case: `aot_c_typed_scalar`.
- Negative cases: forbidden generated C tokens for the old primitive-constant frame writes for
  slots 0 and 1.

## Tooling Evidence

Tooling:

- WSL GCC Debug build directory: `build/codex-aot-07-wsl-gcc-debug`
- CMake/Ninja target rebuild for focused typed scalar and source contracts
- Direct generated C inspection
- CTest focused filter
- `git diff --check`

Commands and key outputs:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"
```

- RED before implementation: failed 1/1 because the generated C still contained the old
  `slot 0` scalar-local constant followed by `zr_aot_value_exec_primitive_constant`.
- GREEN after implementation: direct run reports `1 Tests 0 Failures 0 Ignored OK`.

Generated C evidence:

```c
/* zr_aot_scalar_constant_i64_local */
zr_aot_s0 = (TZrInt64)21;
/* zr_aot_scalar_constant_i64_local */
zr_aot_s1 = (TZrInt64)2;
```

The generated source still materializes direct-return result slots before return, because the current
direct-return lowering reads `frame.slotBase + resultSlot`.

Source-contract target:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test"
```

- GREEN: target linked successfully and the test binary reported 19 tests, 0 failures.

Registered CTest:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm/build/codex-aot-07-wsl-gcc-debug && ctest -R 'aot_c_typed_scalar' --output-on-failure"
```

- GREEN: 1/1 tests passed.

Formatting check:

```powershell
git diff --check
```

- GREEN: exit code 0; output only reported existing LF/CRLF conversion warnings.

## Results

- The RED generated-product assertion first proved the old primitive-constant frame write was still
  present after the scalar-local assignment.
- The primitive-constant skip proof now scans reachable consumers through the scalar result-skip
  machinery before eliding frame materialization.
- `FUNCTION_RETURN` remains a conservative boundary for the skip proof, so result slots used by the
  existing frame-based direct-return lowering are still materialized.
- The focused generated C now contains only scalar-local constant assignments for slots 0 and 1.
- The focused typed scalar executable, source-contract target, and registered CTest pass.

## Acceptance Decision

Accepted for the focused 07-S2 i64 primitive constant cross-block local-only slice.

This is not full 07-S2 and not full M1.5/07 completion. Remaining work includes remaining primitive
constants outside the focused proof, generic float copy/type checks, direct return/result frame
fallback removal, prologue/frame setup, reset-stack-null frame writes, and boundary local restoration.

`backend_aot_c_scalar_locals.c` remains above the modularization threshold. The smallest follow-up
split is extracting scalar result-skip/liveness proof helpers into a dedicated module.
